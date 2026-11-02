<!--
Sync Impact Report (temporary scratch material for amendment review —
remove this block before committing the amended constitution).

Version change: (unratified template scaffold) → 1.0.0

Modified principles:
- None. The previous .specify/memory/constitution.md was the unmodified
  constitution-template with every placeholder unfilled; there was no prior
  ratified content to preserve or migrate.

Added sections:
- Core Principles I–V, populated from the existing project boundaries
  (AGENTS.md, CMakeLists.txt, docs/coding-style/kernel-style.md, src/
  module layout)
- "Technology And Asset Constraints" (template slot [SECTION_2_NAME])
- "Development Workflow" (template slot [SECTION_3_NAME])
- Governance rules and the version footer

Removed sections:
- None (template example comments were replaced by concrete content)

Deferred TODOs:
- None. Ratification date 2026-10-28 matches the day the project scaffold
  and the template file itself were created; no earlier adoption date
  exists to recover.
-->

# Alpine Bird Flight Simulator Constitution

## Core Principles

### I. Pure C with SDL3 Only (NON-NEGOTIABLE)

- All production code MUST be pure C11 (`-std=c11` with
  `CMAKE_C_STANDARD_REQUIRED`); C++ or any other implementation language
  MUST NOT be introduced.
- SDL3 (3.4+) is the only permitted multimedia and rendering dependency;
  SDL2 APIs MUST NOT be used anywhere.
- SDL3 boolean returns MUST be treated as `true` = success, `false` =
  failure, and failure text MUST be obtained via `SDL_GetError()`.
- The build MUST remain CMake (3.20+) with Ninja and
  `find_package(SDL3 REQUIRED CONFIG)`; the link set MUST NOT grow beyond
  `SDL3::SDL3` and `m` without a constitutional amendment.
- Rationale: a single-language, single-dependency boundary keeps the Nix
  flake toolchain deterministic and the codebase auditable.

### II. Procedural Geometry Only (NON-NEGOTIABLE)

- All 3D geometry (bird mesh, terrain, trees, rings, obstacles) MUST be
  generated in C code from mathematical formulas and vertex arrays.
- The repository MUST NOT contain imported meshes, Blender models,
  downloaded textures, or other binary 3D asset files.
- The retro low-poly, flat-shaded aesthetic MUST be produced by code, not
  by assets.
- Rationale: project vision — the entire game stays expressible,
  reviewable, and rebuildable as pure source.

### III. Software 3D Pipeline via SDL_RenderGeometry

- Rendering MUST flow through the project's own pipeline: `math3d`
  transforms, perspective projection, directional-light flat shading,
  back-face culling, painter's depth sort, then batched
  `SDL_RenderGeometry()` submission.
- Window and renderer creation MUST use SDL3 signatures
  (`SDL_CreateWindowAndRenderer()` or `SDL_CreateWindow()` without
  position parameters); geometry and rectangles MUST use the float forms
  (`SDL_Vertex`, `SDL_FRect`).
- Rationale: the software renderer is part of the project's identity;
  offloading it to an engine or another rendering library would dissolve
  the architectural boundary.

### IV. Linux Kernel C Coding Style (NON-NEGOTIABLE)

- Indentation MUST be tabs, 8 columns wide; 80-column limit where
  practical.
- Function braces open on the next line; statement braces stay on the same
  line; single-statement bodies omit braces.
- Names MUST be `snake_case` for functions, variables, and members, and
  `UPPER_SNAKE_CASE` for `#define` constants and enum values.
- Structs MUST stay explicit (`struct vec3`); typedefs for structs and
  pointers are forbidden except for exact-width or platform types
  (`uint32_t`, `size_t`) and opaque handles.
- Multi-stage failure paths MUST converge on `goto` cleanup labels that
  release resources in reverse allocation order, and every allocation
  return value MUST be checked.
- `clang-format -i` (repo `.clang-format`) MUST leave the tree unchanged.
- Rationale: uniform style keeps procedurally generated C readable at any
  repo size; the rules mirror docs/coding-style/kernel-style.md.

### V. Modular Structure and Verified Delivery

- Source MUST remain organized as focused `.h`/`.c` modules (`math3d`,
  `render3d`, `camera`, `bird`, `terrain`, `ring`, `audio`, `main`); each
  module MUST own one concern, and a module accumulating a second
  responsibility MUST be split.
- Every non-trivial change MUST be developed on a dedicated branch:
  `feat/<feature-name>`, `fix/<bug-name>`, or `refactor/<scope>`.
- Commits MUST follow Conventional Commits (`feat(scope): ...`,
  `fix(scope): ...`, `refactor(scope): ...`, `chore(scope): ...`).
- Before any commit: `clang-format` clean, a warning-free
  `cmake --build build`, and an inspected `git status`/`git diff`.

## Technology And Asset Constraints

- The toolchain comes from the Nix flake (sdl3, gcc, clang-tools, cmake,
  ninja); component versions are pinned by the flake, not by ad-hoc
  installs.
- Rendering is CPU-side flat shading onto `SDL_RenderGeometry()` batches
  with painter's-algorithm depth ordering; no GPU shader pipeline.
- Roadmap phases from AGENTS.md (math pipeline, bird and flight physics,
  alpine terrain, gameplay polish) define the delivery sequence; new
  features MUST map to a phase instead of bypassing the plan.

## Development Workflow

- Features are specified with Spec Kit (`/speckit.specify`, then plan,
  then tasks) before implementation, and each artifact MUST be checked
  against the Core Principles above.
- Tooling: clangd with `compile_commands.json` for navigation, the local
  SDL3 MCP server for symbol and API queries, and the guardrail hooks for
  isolated syntax checks before edits land.
- Quality gates, in order: deterministic syntax check, `clang-format`
  verification, warning-free `cmake --build build`, manual diff review.

## Governance

- This constitution is the supreme governance document for the project;
  on conflict with AGENTS.md, `.clang-format`, or any other practice file,
  this document prevails.
- Amendments MUST include documented rationale, project-owner approval,
  and a migration note for work affected by the change.
- Versioning follows MAJOR.MINOR.PATCH: MAJOR removes or redefines
  principles incompatibly, MINOR adds principles or materially expands
  guidance, PATCH applies clarifications and wording fixes.
- Compliance is reviewed at every feature boundary (spec, plan, tasks,
  implementation); reviewers MUST reject changes that breach a
  NON-NEGOTIABLE principle.
- AGENTS.md serves as runtime development guidance within the boundaries
  fixed here.

**Version**: 1.0.0 | **Ratified**: 2026-10-28 | **Last Amended**: 2026-10-28
