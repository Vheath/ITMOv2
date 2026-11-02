/*
 * c-guardrails — deterministic guardrails for the alpine bird simulator.
 *
 * OpenCode V2 plugin. V1 plugin code does not load in OpenCode 2.x (the
 * server log shows: "Plugin must export a default definition with an id
 * and an effect or setup function"), so the two guardrails documented in
 * AGENTS.md live on the V2 Plugin.define() entrypoint:
 *
 *   1. Isolated C syntax checks. Before edit/write may touch a .c/.h
 *      file, the proposed content is compiled with `gcc -fsyntax-only`
 *      in a throwaway temp directory. Unresolved #include headers never
 *      refuse an edit: the server can run outside the nix devshell, so
 *      a third-party header may be invisible in isolation. Real parse
 *      errors refuse the edit and the gcc diagnostics go back to the
 *      agent so the C can be fixed before it lands on disk.
 *
 *      Include flags come, in order of preference, from
 *      build/compile_commands.json (the exact flags cmake recorded for
 *      the file), from `pkg-config --cflags sdl3` (probed once at
 *      setup), and finally from the file's own directory.
 *
 *   2. Destructive shell protection. Commands that destroy work
 *      irreversibly are refused before they run: recursive rm against
 *      /, ~, $HOME, bare wildcards or .git; mkfs and raw device writes;
 *      fork bombs; force pushes to main/master; powering the machine
 *      off. Targeted work such as `rm -rf build` or `git rm` stays
 *      allowed on purpose: git-tracked content is recoverable.
 */

import { Plugin } from "@opencode/plugin"
import { execFile } from "node:child_process"
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises"
import { tmpdir } from "node:os"
import { basename, dirname, isAbsolute, join, resolve } from "node:path"
import { promisify } from "node:util"

const exec = promisify(execFile)

const COMPILER = process.env.CC || "gcc"
const SYNTAX_TIMEOUT_MS = 15_000
const PROBE_TIMEOUT_MS = 5_000
const C_SOURCE = /\.(c|h)$/i

/* Tools whose input names a file on disk. */
const FILE_TOOLS = new Set(["edit", "write"])

/* Tools whose input runs a shell command. */
const COMMAND_TOOLS = new Set(["bash", "shell"])

/*
 * Refuse commands that destroy work irreversibly. Deliberately
 * conservative: ordinary build, clean and git commands must keep
 * working. Extend the table with new entries; do not widen the regexes,
 * they are anchored on shell token boundaries for a reason.
 */
const DESTRUCTIVE_COMMANDS: readonly { name: string; test: RegExp }[] = [
	{
		name: "recursive rm against /, ~, $HOME or a bare wildcard",
		test: /(?<!git )\brm\s+(?:[^;&|]*\s)?(?:--recursive|-[a-z]*r[a-z]*)\b[^;&|]*(?<=[\s;&|("'])(?:\/|~|\$HOME|\*|\.\*|\.\.\/\*)(?=[\s;&|)"']|\*|$)/i,
	},
	{
		name: "recursive rm with command-substituted targets",
		test: /(?<!git )\brm\s+(?:[^;&|]*\s)?(?:--recursive|-[a-z]*r[a-z]*)\b[^;&|]*\$\(/i,
	},
	{
		name: "recursive rm against the .git directory",
		test: /(?<!git )\brm\s+(?:[^;&|]*\s)?(?:--recursive|-[a-z]*r[a-z]*)\b[^;&|]*(?<=[\s;&|("'])\.git(?=$|[\s;&|)"'\/])/i,
	},
	{ name: "filesystem mkfs", test: /\bmkfs(?:\.[a-z0-9]+)?\b/i },
	{ name: "dd onto a raw device", test: /\bdd\b[^;&|]*\bof=\/dev\//i },
	{ name: "write to a raw disk device", test: />\s*\/dev\/(?:sd|hd|nvme|vd|mmcblk|loop)/i },
	{ name: "device erase", test: /\b(?:blkdiscard|wipefs)\b/i },
	{ name: "fork bomb", test: /: *\(\) *\{ *: *\| *:& *\} *; *:/ },
	{
		name: "force push to main/master",
		test: /\bgit\s+push\b[^;&|]*\s(?:-f\b|--force(?![\w-]))[^;&|]*\b(?:main|master)\b/i,
	},
	{
		name: "recursive chmod/chown against /, ~ or $HOME",
		test: /\bch(?:mod|own)\b[^;&|]*\s-R\b[^;&|]*(?<=[\s;&|("'])(?:\/|~|\$HOME)(?=[\s;&|)"']|$)/i,
	},
	{ name: "powering the machine off", test: /\b(?:shutdown|reboot|halt|poweroff)\b/i },
]

function refuse_destructive(command: string): void
{
	for (const rule of DESTRUCTIVE_COMMANDS) {
		if (rule.test.test(command))
			throw new Error(`[c-guardrails] refused: ${rule.name}: ${command}`)
	}
}

/*
 * Probed once at setup. Outside the nix devshell pkg-config has no sdl3
 * module and the probe quietly returns no flags; the syntax check then
 * leans on compile_commands.json or the directory fallback.
 */
async function discover_sdl3_flags(): Promise<string[]>
{
	for (const argv of [
		["pkg-config", "--cflags", "sdl3"],
		["sdl3-config", "--cflags"],
	]) {
		try {
			const { stdout } = await exec(argv[0], argv.slice(1), { timeout: PROBE_TIMEOUT_MS })
			const flags = stdout.trim().split(/\s+/).filter(Boolean)
			if (flags.length > 0)
				return flags
		} catch {
			/* probe miss: try the next source */
		}
	}
	return []
}

/*
 * Flags cmake recorded for one translation unit, so the isolated check
 * sees the exact include paths and defines of a real build. Only flag
 * tokens are kept: the compiler path, -c, -o and dependency-file flags
 * are dropped, and -fsyntax-only writes no output anyway.
 */
async function cc_flags(
	root: string,
	file: string,
): Promise<{ compiler: string; args: string[]; cwd: string } | undefined>
{
	const db_path = join(root, "build", "compile_commands.json")
	let db: readonly { directory: string; file: string; command?: string; arguments?: string[] }[]
	try {
		db = JSON.parse(await readFile(db_path, "utf8"))
	} catch {
		return undefined
	}
	const entry = db.find((e) => e.file === file || e.file.endsWith("/" + basename(file)))
	if (!entry)
		return undefined
	const tokens = entry.arguments ?? (entry.command ?? "").split(/\s+/)
	const args: string[] = []
	for (let i = 1; i < tokens.length; i++) {
		const token = tokens[i]
		if (!token)
			continue
		if (/^(?:-I|-D|-U|-std=|-nostdinc|-include|-isystem|-iquote|-f|-W)/.test(token)) {
			args.push(token)
			if ((token === "-include" || token === "-isystem" || token === "-iquote") && i + 1 < tokens.length)
				args.push(tokens[++i] as string)
		}
	}
	return { compiler: tokens[0] || COMPILER, args, cwd: entry.directory }
}

/*
 * Compile the proposed source in a throwaway directory and return gcc
 * diagnostics, or undefined when the content is syntactically fine (or
 * the check cannot run: no compiler, timeout, only missing headers).
 */
async function check_c_syntax(
	root: string,
	file: string,
	source: string,
	sdl3_flags: string[],
): Promise<string | undefined>
{
	let tmp_dir: string | undefined
	try {
		const cc = await cc_flags(root, file)
		tmp_dir = await mkdtemp(join(tmpdir(), "c-guardrails-"))
		const tmp_file = join(tmp_dir, basename(file))
		await writeFile(tmp_file, source)

		const compiler = cc?.compiler ?? COMPILER
		const args = [
			...(cc?.args ?? [...sdl3_flags, "-I", dirname(file)]),
			"-fsyntax-only",
			tmp_file,
		]
		try {
			await exec(compiler, args, { cwd: cc?.cwd, timeout: SYNTAX_TIMEOUT_MS })
			return undefined
		} catch (err) {
			const failure = err as { code?: string; killed?: boolean; stderr?: string }
			/* Fail open: a guardrail that cannot run must not block work. */
			if (failure.code === "ENOENT" || failure.killed)
				return undefined
			const stderr = String(failure.stderr ?? "")
			/* Missing includes are an isolation artifact, not a syntax error. */
			if (/fatal error:.*No such file or directory/.test(stderr))
				return undefined
			const diagnostics = stderr.trim().split("\n").slice(0, 40).join("\n")
			return diagnostics || String((err as Error).message)
		}
	} finally {
		if (tmp_dir)
			await rm(tmp_dir, { recursive: true, force: true }).catch(() => {})
	}
}

/*
 * The content the file will hold once the tool succeeds, so the check
 * sees the edit's result instead of the current disk state.
 */
async function proposed_content(
	tool: string,
	file: string,
	input: Record<string, unknown>,
): Promise<string | undefined>
{
	if (tool === "write")
		return typeof input.content === "string" ? input.content : undefined

	const old_text = input.oldString
	const new_text = input.newString
	if (typeof old_text !== "string" || typeof new_text !== "string")
		return undefined
	let current: string
	try {
		current = await readFile(file, "utf8")
	} catch {
		return undefined /* new file: nothing to apply the edit to */
	}
	if (!current.includes(old_text))
		return undefined /* the edit tool will refuse on its own */
	return input.replaceAll === true
		? current.split(old_text).join(new_text)
		: current.replace(old_text, new_text)
}

export default Plugin.define({
	id: "c-guardrails",
	async setup(ctx) {
		const root = ctx.location.directory
		const sdl3_flags = await discover_sdl3_flags()

		/* One startup line: load state and guarded tool names in the log. */
		const tools = await ctx.tool.list()
		const guarded = tools.map((t) => t.id).filter((id) => FILE_TOOLS.has(id) || COMMAND_TOOLS.has(id))
		console.log(`[c-guardrails] active (sdl3 cflags: ${sdl3_flags.length}, tools: ${guarded.join(", ") || "none"})`)

		await ctx.tool.hook("execute.before", async (event) => {
			const input = (event.input ?? {}) as Record<string, unknown>

			if (COMMAND_TOOLS.has(event.tool)) {
				const command = input.command ?? input.cmd ?? input.script
				if (typeof command === "string")
					refuse_destructive(command)
			}

			if (FILE_TOOLS.has(event.tool)) {
				const raw = input.path ?? input.filePath
				const file = typeof raw === "string" && raw.length > 0
					? (isAbsolute(raw) ? raw : resolve(root, raw))
					: undefined
				if (!file || !C_SOURCE.test(file))
					return
				const source = await proposed_content(event.tool, file, input)
				if (source === undefined)
					return
				const diagnostics = await check_c_syntax(root, file, source, sdl3_flags)
				if (diagnostics) {
					throw new Error(
						`[c-guardrails] ${basename(file)} would not pass gcc -fsyntax-only, fix the C first:\n${diagnostics}`,
					)
				}
			}
		})

		/*
		 * Safety net: every shell the server creates passes here, no
		 * matter which tool or plugin asked for it.
		 */
		await ctx.shell.hook("create.before", (event) => {
			refuse_destructive(event.command)
		})
	},
})
