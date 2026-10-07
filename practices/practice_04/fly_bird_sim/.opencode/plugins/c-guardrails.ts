// Deterministic Opencode plugin for C + SDL3 development guardrails and hooks
import { existsSync } from "fs";

// Known Nix/system SDL3 include path fallback
const SDL3_INCLUDE_DIRS = [
  "/nix/store/vq7xnwxwil95kf6p2sjqxl87gfl7kqjg-sdl3-3.4.10-dev/include",
  "/usr/include",
  "/usr/local/include",
];

const resolvedSdlInclude = SDL3_INCLUDE_DIRS.find((d) => existsSync(d)) || "";

// Pure pattern-matching rules for dangerous commands
const FORBIDDEN_SHELL_PATTERNS = [
  // Root/Home or path traversal destructive deletion
  /\brm\s+-[^\s]*[rf][^\s]*\s+(\/|~|\.\.|\*)/,
  // Hard resets or purging git tree
  /\bgit\s+reset\s+--hard\b/,
  /\bgit\s+clean\s+-[^\s]*f[^\s]*\b/,
  // Overwriting raw devices
  /\bdd\s+if=.*of=\/dev\//,
];

export default (async ({ client, project, directory, $ }) => {
  return {
    // 1. Tool execution before: Pure, deterministic bash command safety gate
    "tool.execute.before": async (input) => {
      if (input.tool === "bash") {
        const cmd = String(input.args?.command || "").trim();

        for (const pattern of FORBIDDEN_SHELL_PATTERNS) {
          if (pattern.test(cmd)) {
            throw new Error(`[Deterministic Guardrail]: Command blocked by security policy: "${cmd}"`);
          }
        }
      }
    },

    // 2. Tool execution after: Stateless, isolated syntax check (no filesystem race, no disk mutation)
    // Formatting is natively handled by opencode's formatter config in opencode.json.
    "tool.execute.after": async (input, output) => {
      if (input.tool === "edit" || input.tool === "write") {
        const filePath = input.args?.filePath;
        if (filePath && typeof filePath === "string" && filePath.endsWith(".c") && existsSync(filePath)) {
          // Perform isolated, stateless syntax validation into /dev/null
          const includeFlag = resolvedSdlInclude ? `-I${resolvedSdlInclude}` : "";
          const checkCmd = includeFlag
            ? $`gcc -fsyntax-only -Wall -Wextra ${includeFlag} ${filePath}`
            : $`gcc -fsyntax-only -Wall -Wextra ${filePath}`;

          const res = await checkCmd.nothrow();
          if (res.exitCode !== 0) {
            const errorDetails = (res.stderr ? res.stderr.toString() : "").trim();
            if (output && typeof output.output === "string") {
              output.output += `\n\n[Deterministic Syntax Check Alert]: GCC syntax check detected errors in ${filePath}:\n${errorDetails}`;
            }
          }
        }
      }
    },
  };
});
