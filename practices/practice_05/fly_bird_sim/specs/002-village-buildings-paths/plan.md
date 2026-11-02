# Implementation Plan: Village Buildings and Field Paths

**Branch**: `feat/village-buildings-paths` | **Date**: 2026-10-28 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/002-village-buildings-paths/spec.md`

**Note**: The `setup-plan.sh` script derives the branch label
`002-village-buildings-paths` from the spec directory; the constitution
(principle V) mandates the `feat/<feature-name>` form, which the spec
already records. The branch is created at implementation start —
`/speckit.plan` itself does not create branches.

## Summary

Make the alpine valley feel inhabited: procedurally generated clusters
of small wooden 1–2 story buildings standing around the valley fields,
each cluster joined by simple dirt paths. Everything is visual-only
scenery, deterministically placed via the existing hash-lattice
technique, rendered as flat-shaded low-poly triangles through the
project's software pipeline. Implementation = one new focused module
(`src/village.h` / `src/village.c`) following the proven
`scenery_generate_mesh()` pattern, plus one call site in `main.c` and
one source-list entry in `CMakeLists.txt`. No new dependencies, no
gameplay changes, no stored state.

## Technical Context

**Language/Version**: C11 (`-std=c11`, `CMAKE_C_STANDARD 11`)

**Primary Dependencies**: SDL3 3.4+ (existing `SDL_RenderGeometry`
pipeline via `render3d`), libm (`sinf`). **No new dependencies** — the
link set stays `SDL3::SDL3` and `m`.

**Storage**: N/A — geometry is generated per frame from integer lattice
hashes; nothing is persisted, loaded, or allocated on the heap.

**Testing**: No automated test framework in this project. Validation =
the deterministic guardrail syntax check, `clang-format` verification,
warning-free `cmake --build build`, and the manual flight scenarios in
[quickstart.md](quickstart.md).

**Target Platform**: Linux desktop (Nix flake toolchain: gcc,
clang-tools, cmake, ninja, sdl3).

**Project Type**: desktop-app (retro low-poly 3D flight simulator).

**Performance Goals**: Retain the current interactive frame rate;
typical village load ≈ ≤10 visible clusters × ~100 triangles ≈ 1000
triangles per frame (~1.5% of the 65536-triangle `static_tris` buffer)
plus ~20 `terrain_get_height()` samples per cluster — negligible next
to the terrain mesh's 10k+ height samples.

**Constraints**: Determinism (same place, same farmstead, every
session); visual-only (no collision/scoring/audio); placement limited
to the valley meadow band with a 40 m canyon/river corridor margin;
buildings ≤ 20 triangles each; kernel C style; no imported assets.

**Scale/Scope**: Clusters of 2–4 buildings, ~10–15% of 96 m lattice
cells, ≤ ~10 clusters in view, ≤ 20 tris/building, 2 tris per path
segment, one new module (~200–300 lines), one call site in `main.c`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Pure C with SDL3 only | PASS | Pure C11; no new link dependencies; SDL3 float APIs only, called via the existing `render3d` bridge. |
| II | Procedural geometry only | PASS | All building/path vertices computed from math and hash bits in C; no asset files enter the repo. |
| III | Software 3D pipeline via SDL_RenderGeometry | PASS | Village triangles flow through `math3d` transforms, flat shading, back-face culling, painter's depth sort, and batched `SDL_RenderGeometry()` exactly like flora. |
| IV | Linux Kernel C coding style | PASS | Tabs (8 cols), statement/function brace rules, `snake_case`, explicit `struct` names; no heap allocation in the module, so the `goto` cleanup chain is empty by construction; budget checks mirror `add_scene_tri()`. |
| V | Modular structure and verified delivery | PASS | New focused module `village.h/.c` owns exactly one concern (settlement scenery); `terrain.c` gains no second responsibility. The constitution's module list is the *current* inventory, not a closed set — the normative rule (one concern per module) is what governs, and adding a focused module honors it. Delivery on `feat/village-buildings-paths` with Conventional Commits and the standard quality gates. |

**No violations — Complexity Tracking table not required.**

**Post-Phase-1 re-check (after research.md, data-model.md,
contracts/village-module.md, quickstart.md)**: still PASS on all five
principles. The design adds no dependency, no asset, no second
responsibility to any existing module, no stored state, and the module
contract keeps `village.c` free of SDL calls (renderer-agnostic pure
geometry producer). No gate failures, no justification entries needed.

## Project Structure

### Documentation (this feature)

```text
specs/002-village-buildings-paths/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output — 7 resolved design decisions
├── data-model.md        # Phase 1 output — Cluster / Building / Path entities
├── contracts/
│   └── village-module.md # Phase 1 output — internal C module contract
├── quickstart.md        # Phase 1 output — manual validation scenarios V1–V7
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── main.c              # MODIFIED: one call to village_generate_mesh() in app_render()
├── village.h           # NEW: public interface of the village module
├── village.c           # NEW: cluster lattice, building + path geometry, budget guards
├── terrain.h/.c        # UNCHANGED (terrain_get_height() reused read-only)
├── math3d.h/.c         # UNCHANGED (vec3, tris reused)
├── render3d.h/.c       # UNCHANGED (pipeline reused as-is)
├── bird.h/.c           # UNCHANGED
├── camera.h/.c         # UNCHANGED
├── ring.h/.c           # UNCHANGED
└── audio.h/.c          # UNCHANGED

CMakeLists.txt          # MODIFIED: village.c added to the sdl3_app source list
```

**Structure Decision**: single-project layout (the existing flat `src/`
module set). The feature is one new focused module plus two one-line
integration edits; no new top-level directories, no `tests/` tree (the
project validates manually per quickstart.md and via its guardrail
hooks).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitution violations — no entries.
