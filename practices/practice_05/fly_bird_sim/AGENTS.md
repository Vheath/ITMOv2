# AGENTS.md — Alpine Bird Flight Simulator Project Guide

Welcome! This file provides full architectural context, technology stack specifications, coding rules, and design guidelines for AI agents working in this repository.

---

## 1. Project Overview & Vision

- **Project Title:** Alpine Bird Flight Simulator (working title)
- **Concept:** A 3D third-person bird flight simulator where the player controls a soaring bird (chase POV camera positioned behind and slightly above the bird) gliding through procedural alpine mountain ridges, canyons, and valleys.
- **Core Aesthetic:** Retro low-poly, flat-shaded geometric styling (evoking *Star Fox*, *Race The Sun*, or early 90s flight simulators).
- **Vibecoding Constraint:** **Zero external 3D asset files or Blender models.** All 3D geometry (the bird mesh, terrain grid, trees, rings, obstacles) is procedurally generated in pure C code via mathematical formulas and vertex arrays.

---

## 2. Technology Stack & Environment

- **Language:** Pure C (C11 standard: `-std=c11`, `set(CMAKE_C_STANDARD 11)`)
- **Multimedia & Rendering:** **SDL3** (Simple DirectMedia Layer 3, version 3.4+)
  - Rendering pipeline: `SDL_RenderGeometry()` for flat-shaded 3D triangles projected onto screen coordinates with depth sorting / Painter's Algorithm.
- **Build System:** CMake 3.20+ with Ninja generator (`cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`)
- **Environment Management:** Nix flake (`flake.nix` in root repo provides `sdl3`, `clang-tools`, `cmake`, `ninja`, `gcc`)
- **LSP & Tooling:**
  - `clangd` with background indexing and compile commands
  - `clang-format` enforcing Linux Kernel style (`.clang-format`)
- **MCP Server:** Local Python MCP server (`.opencode/mcp-sdl3/server.py`) for querying SDL3 symbol declarations and headers.
- **Opencode Guardrail Hooks:** Deterministic hooks in `.opencode/plugins/c-guardrails.ts` performing isolated syntax checks (`gcc -fsyntax-only`) and protecting against destructive shell commands.

---

## 3. Project Architecture & Modules

The project follows a clean, modular C structure designed for readability and maintainability:

```
pract_3/
├── .clang-format           # 8-column tab Linux kernel style
├── .gitignore              # Git ignore rules for build/caches
├── .opencode/
│   ├── commands/           # /build, /run, /commit-feature, /feature-branch
│   ├── mcp-sdl3/           # Local SDL3 symbol query MCP server
│   ├── plugins/            # Deterministic safety & syntax check hooks
│   └── skills/             # sdl3-c skill guidance
├── CMakeLists.txt          # CMake project definition
├── docs/
│   └── coding-style/
│       └── kernel-style.md # Canonical C Linux kernel coding guide
├── opencode.json           # Opencode configuration (LSP, MCP, permissions)
├── src/
│   ├── main.c              # Application entrypoint & main loop
│   ├── math3d.h / .c       # Vec3, Mat4, projection, transformations, vector math
│   ├── camera.h / .c       # Third-person spring chase camera
│   ├── bird.h / .c         # Procedural low-poly bird mesh, flapping animation, flight physics
│   ├── terrain.h / .c      # Procedural alpine heightmap grid & canyon generation
│   └── render3d.h / .c     # Triangle buffer, flat shading, depth sorting, SDL_RenderGeometry bridge
└── AGENTS.md               # Context & guidelines for AI models (this document)
```

---

## 4. Coding Conventions & Standards

Agents working on this codebase **must** adhere strictly to the following conventions:

1. **Linux Kernel C Coding Style:**
   - **Indentation:** Tabs only, exactly 8 characters wide. Never spaces for indentation.
   - **Line Length:** 80-column limit where practical.
   - **Brace Placement:**
     - Functions: opening brace `{` on its own line after declaration.
     - Statements (`if`, `while`, `for`, `switch`): opening brace `{` on the same line.
     - Single-statement blocks: omit braces (`if (cond) action();`).
   - **Naming:**
     - Lowercase with underscores (`snake_case`) for functions, variables, and struct members.
     - Uppercase (`UPPER_SNAKE_CASE`) for `#define` constants and enum values.
   - **Structures & Types:** Avoid unnecessary typedefs for structs; use explicit `struct app_context`, `struct vec3`, etc.
   - **Error Handling & Cleanup:** Centralized exit using `goto` labels (`err_free_xyz:`, `out:`) to clean up resources in reverse order.
   - **Comments:** Explain *why*, not *what*. Use kernel multi-line comment format:
     ```c
     /*
      * Multi-line explanation of a non-obvious algorithm
      * or architectural decision.
      */
     ```

2. **SDL3 Specific Rules:**
   - Use SDL3 functions exclusively (NOT SDL2).
   - Functions returning boolean return `bool` (`true` on success, `false` on failure).
   - Error messages are obtained using `SDL_GetError()`.
   - Use `SDL_CreateWindowAndRenderer()` or `SDL_CreateWindow()` without position parameters (which were removed in SDL3).
   - Window coordinates and rendering primitives frequently use floats (`SDL_FRect`, `SDL_Vertex`, etc.).

---

## 5. Git Automation & Branching Policy

- **Branches:** Every non-trivial feature or refactor must be developed in a dedicated branch:
  - Feature branches: `feat/<feature-name>` (e.g. `feat/math3d-pipeline`)
  - Bug fixes: `fix/<bug-name>`
  - Refactors: `refactor/<scope>`
- **Commits:** Follow **Conventional Commits**:
  - `feat(scope): concise imperative description`
  - `fix(scope): concise imperative description`
  - `refactor(scope): concise imperative description`
  - `chore(scope): concise imperative description`
- **Verification Before Commit:**
  1. Source code must pass `clang-format -i` (8-column tabs).
  2. Project must compile cleanly without warnings using `cmake --build build`.
  3. Inspect `git status` and `git diff` to ensure only intended changes are staged.

---

## 6. Game Roadmap & Implementation Phases

When developing features, proceed according to the following milestones:

- [x] **Phase 0:** Project scaffolding, SDL3 build verification, MCP tools, deterministic hooks, Git workflow.
- [ ] **Phase 1: 3D Math & Software Rasterizer Pipeline**
  - Vectors (`vec3`), matrices (`mat4`), matrix multiplication, perspective projection matrix.
  - Camera view matrix (look-at / chase transformation).
  - Triangle definition with normals, directional sunlight flat-shading calculation.
  - Back-face culling and Painter's depth-sort.
  - Bridge to `SDL_RenderGeometry()` for drawing batches of colored triangles.
- [ ] **Phase 2: Low-Poly Bird Mesh & Flight Physics**
  - Procedural vertex buffer for bird body (head, beak, body, animated wings, tail).
  - Wing flap animation cycle (`sin(time * speed)`).
  - Flight aerodynamic physics (pitch, roll/bank, yaw, lift vs. dive acceleration).
  - Chase camera with smooth lag / lerp behind the bird.
- [ ] **Phase 3: Procedural Alpine Terrain & Canyons**
  - Scrolling heightmap chunk grid generated mathematically (layered harmonics / Perlin).
  - Alpine altitude color shading (snow peaks, slate rock, pine valley greens, river bed).
  - Distance depth-fog color blending.
  - Canyon walls and flight corridor variation.
- [ ] **Phase 4: Gameplay Accents & Polish**
  - Floating golden rings/draft currents to fly through for speed boosts.
  - Minimalistic cockpit / HUD elements (speed, altitude, compass) drawn cleanly.
  - Flight collision detection against terrain.
