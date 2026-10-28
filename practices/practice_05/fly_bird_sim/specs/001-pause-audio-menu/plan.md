# Implementation Plan: Pause Menu with Audio Volume Controls

**Branch**: `feat/pause-audio-menu` | **Date**: 2026-10-28 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/001-pause-audio-menu/spec.md`

**Note**: The `setup-plan.sh` script derives the branch label
`001-pause-audio-menu` from the spec directory; the constitution
(principle V) mandates the `feat/<feature-name>` form, which the spec
already records. The branch is created at implementation start —
`/speckit.plan` itself does not create branches.

**Cross-feature coordination** (from the 2026-10-28 cross-feature
analysis of 001 × 002-village-buildings-paths):
- 002's `quickstart.md` V7 ("open the pause menu (ESC), change volume,
  resume") requires THIS feature to be implemented; it cannot run
  against the current build, where ESC quits instantly (`main.c`
  `app_handle_events()`). Recommended sequencing: land 001 first, then
  002 (or adapt 002's V7 if the order flips).
- Both features edit `src/main.c` and `CMakeLists.txt`; whichever
  merges second rebases onto the first.
- 002's assumption "audio … remain untouched" means untouched *by 002*;
  this feature legitimately extends `audio.h/.c` with a volume setter.

## Summary

Give the player a safe pause: ESC freezes the simulation (flight input
ignored, world animation frozen, wind faded) and opens a keyboard-driven
pause menu — "Continue", a master volume control (0–100%), and "Exit" —
replacing the current instant-quit ESC behaviour. Implementation = one
new focused module (`src/menu.h` / `src/menu.c`) owning the paused
state, item selection, and 2D overlay drawing, plus a thin volume
setter in the audio module (`audio_engine_set_volume()` wrapping the
SDL3 stream-gain API), integration edits in `main.c` (event routing,
update guard, one render call), and one CMake source-list entry. Volume
applies immediately to all four existing sound effects; nothing is
persisted across runs.

## Technical Context

**Language/Version**: C11 (`-std=c11`, `CMAKE_C_STANDARD 11`)

**Primary Dependencies**: SDL3 3.4+ (existing window/renderer, events,
`SDL_RenderFillRect`/`SDL_RenderDebugText` for the 2D overlay,
`SDL_SetAudioStreamGain` — available since SDL 3.2.0 — for volume),
libm. **No new dependencies** — the link set stays `SDL3::SDL3` and
`m`.

**Storage**: N/A — the paused flag, selected item, and volume level are
in-session `struct` fields; the spec excludes persistence across
restarts for v1.

**Testing**: No automated test framework in this project. Validation =
the deterministic guardrail syntax check, `clang-format` verification,
warning-free `cmake --build build`, and the manual pause/volume/exit
scenarios in [quickstart.md](quickstart.md).

**Target Platform**: Linux desktop (Nix flake toolchain: gcc,
clang-tools, cmake, ninja, sdl3).

**Project Type**: desktop-app (retro low-poly 3D flight simulator).

**Performance Goals**: Menu open/close and item navigation respond
within the same interactive moment as the key press (spec SC-004); the
paused frame renders at the same rate as flight (the world generators
are already deterministic per-frame functions, so the frozen frame is
the normal render path with `app_update()` skipped).

**Constraints**: Kernel C style (tabs, braces, `snake_case`, explicit
`struct` names); pause MUST NOT advance the simulation, gameplay input,
score, or animation clocks (`elapsed_time` only advances inside
`app_update()`); resume MUST NOT fast-forward through the paused
interval; ESC replaces instant quit; no imported assets — the menu uses
SDL3's built-in debug text, no font files; volume range [0.0, 1.0] with
no wrap-around.

**Scale/Scope**: One new module (~150–250 lines), one audio setter
(~10 lines), four integration points in `main.c` (event routing, update
guard, wind fade on pause, menu render call), one CMake entry. Menu =
3 rows (Continue / Volume N% / Exit), one panel, no sub-screens.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Verdict | Notes |
|---|-----------|---------|-------|
| I | Pure C with SDL3 only | PASS | Pure C11; no new link dependencies; SDL3 APIs only, including the stream-gain call whose `bool` result is checked with failure text via `SDL_GetError()` per the SDL3 boolean rule. |
| II | Procedural geometry only | PASS | The menu adds no 3D geometry and no asset files; text comes from SDL3's built-in `SDL_RenderDebugText` (code, not a font asset); panel rectangles are `SDL_FRect` fills. |
| III | Software 3D pipeline via SDL_RenderGeometry | PASS | The 3D scene keeps flowing through `math3d` → flat shading → painter's sort → `SDL_RenderGeometry()` untouched. The menu is a 2D UI overlay drawn after `render_present()` on the same `SDL_Renderer`, exactly like the existing HUD and game-over banner (`main.c` already draws both with `SDL_RenderFillRect` + `SDL_RenderDebugText`). Principle III governs the 3D scene pipeline; no scene content bypasses it. |
| IV | Linux Kernel C coding style | PASS | Tabs (8 cols), statement/function brace rules, `snake_case`, explicit `struct` names. The menu module does no heap allocation (drawing is immediate-mode 2D), so its `goto` cleanup chain is empty by construction; the audio setter checks its inputs and the SDL3 `bool` result. |
| V | Modular structure and verified delivery | PASS | New focused module `menu.h/.c` owns exactly one concern (pause state + menu UI); `main.c` gains only routing/guard/render call sites; `audio.c` gains only the volume setter consistent with its existing concern. The constitution's module list is the *current inventory*, not a closed set — the normative rule (one concern per module) is what governs; adding a focused module honors it (same interpretation recorded in the 002 plan). Delivery on `feat/pause-audio-menu` with Conventional Commits and the standard quality gates. |

**No violations — Complexity Tracking table not required.**

**Post-Phase-1 re-check (after research.md, data-model.md,
contracts/menu-module.md, quickstart.md)**: still PASS on all five
principles. The design adds no dependency (`SDL_SetAudioStreamGain` is
existing SDL3, no link-set change), no asset (text via the built-in
debug font), and no scene-rendering change (the menu is the project's
established 2D overlay layer; the 3D pipeline is untouched). No module
gains a second responsibility: `menu.c` owns pause/menu only, the
audio setter stays inside the audio engine's concern, and `main.c`
gains only routing, a guard, and single call sites. No persisted
state, no heap allocation, no globals; the contract keeps `menu.c`
free of engine and teardown calls (quit and volume application remain
in `main.c`). No gate failures, no justification entries needed.

## Project Structure

### Documentation (this feature)

```text
specs/001-pause-audio-menu/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output — 7 resolved design decisions
├── data-model.md        # Phase 1 output — Pause Menu / Volume / Action entities
├── contracts/
│   └── menu-module.md   # Phase 1 output — internal C module contracts (menu + audio)
├── quickstart.md        # Phase 1 output — manual validation scenarios P1–P8
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── main.c              # MODIFIED: ESC→toggle routing, menu key routing,
│                       #   app_update() pause guard, wind fade on pause,
│                       #   menu overlay render call, Exit handling
├── menu.h              # NEW: public interface of the pause menu module
├── menu.c              # NEW: paused state, item selection, volume level,
│                       #   2D overlay drawing (no heap, no 3D)
├── audio.h             # MODIFIED: audio_engine_set_volume() declaration
├── audio.c             # MODIFIED: volume setter wrapping SDL_SetAudioStreamGain
├── terrain.h/.c        # UNCHANGED
├── math3d.h/.c         # UNCHANGED
├── render3d.h/.c       # UNCHANGED (pipeline reused as-is)
├── bird.h/.c           # UNCHANGED
├── camera.h/.c         # UNCHANGED
├── ring.h/.c           # UNCHANGED

CMakeLists.txt          # MODIFIED: menu.c added to the sdl3_app source list
```

**Structure Decision**: single-project layout (the existing flat `src/`
module set). The feature is one new focused module plus small
integration edits to `main.c`, `audio.h/.c`, and `CMakeLists.txt`; no
new top-level directories, no `tests/` tree (the project validates
manually per quickstart.md and via its guardrail hooks).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitution violations — no entries.
