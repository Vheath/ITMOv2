---
description: "Task list for feature 001: Pause Menu with Audio Volume Controls"
---

# Tasks: Pause Menu with Audio Volume Controls

**Input**: Design documents from `/specs/001-pause-audio-menu/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/menu-module.md, quickstart.md

**Tests**: Not requested — the project has no automated test framework; validation is the manual quickstart.md procedure (constitution: deterministic syntax check, `clang-format`, warning-free `cmake --build build`).

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: `src/` and `CMakeLists.txt` at repository root (per plan.md)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Branch and clean baseline before any edits

- [x] T001 Create feature branch `feat/pause-audio-menu` from the current HEAD (`git checkout -b feat/pause-audio-menu`) per constitution principle V — deviation: implemented on `practice_5` per user direction (work in this branch only)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The menu module skeleton and its wiring — every user story builds on these

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T002 Create `src/menu.h` with `struct pause_menu` and the public API per `specs/001-pause-audio-menu/contracts/menu-module.md`: `menu_init`, `menu_is_open`, `menu_toggle`, `menu_handle_key`, `menu_volume`, `menu_wants_exit`, `menu_render`. Struct fields with data-model constraints: `open` (boolean), `selected` (row index, 0..2), `volume` (float, 0.0..1.0), `wants_exit` (boolean, one-shot)
- [x] T003 [P] Implement the state core in `src/menu.c` and register it in the build: `menu_init` (zeroes state; volume = 1.0; row 0 selected; closed), `menu_is_open`, `menu_toggle` (flips `open` exactly once per call), `menu_volume`, `menu_wants_exit`; add `src/menu.c` to the `add_executable(sdl3_app ...)` source list in `CMakeLists.txt`
- [x] T004 [P] Wire the module into the app in `src/main.c`: add `#include "menu.h"`, add `struct pause_menu menu;` to `struct app_context`, call `menu_init(&app.menu)` in `main()` after `ring_manager_init(&app.rings)`

**Checkpoint**: Foundation ready — `cmake --build build` is warning-free and the app runs exactly as before with the menu module compiled in but inert

---

## Phase 3: User Story 1 - Pause and Resume the Flight (Priority: P1) 🎯 MVP

**Goal**: ESC freezes the simulation (world, score, input) and shows the menu; "Continue" or ESC resumes the run with position, speed, score, and run state preserved bit-for-bit; works on the game-over screen too

**Independent Test**: quickstart.md scenarios P1 (pause/resume round trip ×10), P2 (no time jump after 30 s pause), P5 (menu on game-over screen, R ignored while paused), P6 (rapid ESC, held-key isolation), P8 (instant appearance)

### Implementation for User Story 1

- [x] T005 [P] [US1] Replace instant-quit ESC with pause toggle in `app_handle_events()` in `src/main.c`: on `SDL_EVENT_KEY_DOWN` with `event.key.repeat == 0` and `SDLK_ESCAPE`, call `menu_toggle(&app->menu)`; when the menu just opened, call `audio_engine_set_speed(&app->audio, 0.0f, false)` so the wind loop fades (FR-009); route arrow/Enter KEY_DOWN events to `menu_handle_key()` while open; keep the `SDLK_R` restart path only when the menu is closed (FR-002); `SDL_EVENT_QUIT` keeps setting `running = false` (FR-010)
- [x] T006 [P] [US1] Implement `menu_handle_key()` in `src/menu.c`: Up/Down move `selected` with wraparound across rows 0..2 ("row index, 0..2; moved by Up/Down with wraparound"); Enter on row 0 (CONTINUE) sets `open = false`; Enter on row 1 (VOLUME) is a no-op placeholder for US2; Enter on row 2 (EXIT) is a no-op placeholder for US3; volume stays adjustable-not-yet (US2 adds Left/Right)
- [x] T007 [US1] Implement `menu_render()` in `src/menu.c`: immediate-mode 2D overlay — one semi-transparent dark `SDL_FRect` panel (blend mode, like the existing game-over banner) with three `SDL_RenderDebugText` rows ("CONTINUE", "VOLUME: NN%", "EXIT"), the selected row recolored; percentage derived as `(int)(volume * 100.0f + 0.5f)`; no-op when closed; no heap allocation, no 3D pipeline usage
- [x] T008 [US1] Add the pause guard and render call in `src/main.c`: first line of `app_update()` becomes `if (app->game_over || menu_is_open(&app->menu)) return;` (freezes `elapsed_time`, score, and all simulation); in `app_render()`, call `menu_render(&app->menu, app->renderer, w, h)` after the game-over banner and before `SDL_RenderPresent()`

**Checkpoint**: User Story 1 fully functional and testable independently (quickstart P1, P2, P5, P6, P8)

---

## Phase 4: User Story 2 - Change Sound Effect Volume (Priority: P2)

**Goal**: The VOLUME row adjusts the loudness of all four sound effects (wind rush, wing flap whoosh, ring chime, crash thud) from true silence (0%) to normal loudness (100%), applied immediately, displayed as a percentage

**Independent Test**: quickstart.md scenario P3 (0% silent, 100% normal, mid-range audible, immediate effect, display matches level, never out of range)

### Implementation for User Story 2

- [ ] T009 [P] [US2] Implement `audio_engine_set_volume()` in `src/audio.h` and `src/audio.c`: signature `void audio_engine_set_volume(struct audio_engine *ae, float volume);` — clamp to [0.0, 1.0], call `SDL_SetAudioStreamGain(ae->stream, volume)`, check the `bool` return and log failure text via `SDL_GetError()` (non-fatal); safe no-op when `ae->stream` is NULL (audio init failed)
- [ ] T010 [US2] Add volume adjustment to `menu_handle_key()` in `src/menu.c`: Left/Right on row 1 (VOLUME) step `volume` by ±0.1 — "float, 0.0..1.0; adjusted in 0.1 steps, clamped at both ends — never negative, never > 1.0, no wrap-around"
- [ ] T011 [US2] Apply the level after menu events in `src/main.c`: after event processing call `audio_engine_set_volume(&app->audio, menu_volume(&app->menu))` (idempotent — same value, no effect); called every frame or after menu key events per contracts/menu-module.md

**Checkpoint**: User Story 2 fully functional and testable independently (quickstart P3)

---

## Phase 5: User Story 3 - Exit the Game from the Menu (Priority: P3)

**Goal**: The EXIT row terminates the game cleanly through the existing teardown chain; the window close button keeps working while the menu is open

**Independent Test**: quickstart.md scenario P4 (Exit terminates cleanly; window close while paused terminates cleanly; at most two interactions)

### Implementation for User Story 3

- [ ] T012 [P] [US3] Activate the EXIT action in `menu_handle_key()` in `src/menu.c`: Enter on row 2 (EXIT) sets `wants_exit = true` (one-shot flag; the menu module performs no teardown itself)
- [ ] T013 [P] [US3] Extend the main loop exit condition in `src/main.c`: `while (app.running && !menu_wants_exit(&app->menu)) { ... }` — shutdown flows through the existing reverse-order cleanup chain (`free` buffers → `render_context_destroy` → `audio_engine_destroy` → destroy window/renderer → `SDL_Quit()`), unchanged

**Checkpoint**: All user stories independently functional (quickstart P1–P8 minus volume already covered)

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Full validation, quality gates, and delivery discipline

- [ ] T014 Run all quickstart.md validation scenarios P1–P8 from `specs/001-pause-audio-menu/quickstart.md` and confirm every expected outcome; this also proves the cross-feature dependency: 002's quickstart V7 ("open the pause menu (ESC), change volume, resume") is now executable as written
- [ ] T015 Quality gates per constitution IV/V: `clang-format -i` on `src/menu.h`, `src/menu.c`, `src/main.c`, `src/audio.h`, `src/audio.c` leaves the tree unchanged; warning-free `cmake --build build`; inspect `git status`/`git diff` for unintended changes
- [ ] T016 Commit the work on `feat/pause-audio-menu` in logical groups with Conventional Commits (`feat(menu): ...`, `feat(audio): ...`), one group per completed user story checkpoint

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **User Stories (Phases 3–5)**: All depend on Phase 2 completion
  - US1 must complete before US2/US3 (the menu is their host)
  - US2 and US3 are functionally independent but both edit `src/menu.c` (`menu_handle_key`) and `src/main.c` — run them sequentially (US2 → US3) to avoid same-file conflicts
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Phase 2 — no dependencies on other stories
- **User Story 2 (P2)**: Depends on US1 (menu host + volume row exists as of T007); independent of US3
- **User Story 3 (P3)**: Depends on US1 (menu host + exit row exists as of T007); independent of US2

### Within Each User Story

- State/behavior core before integration call sites
- `src/menu.c` tasks are sequential within a story (same file)
- `src/main.c` tasks are sequential within a story (same file)
- Cross-file pairs (main.c ∥ menu.c, menu.c ∥ audio.c) can interleave

### Parallel Opportunities

- T003 ∥ T004 (menu.c + CMakeLists ∥ main.c — after T002)
- T005 ∥ T006 (main.c event routing ∥ menu.c key handling)
- T009 ∥ T010 (audio.h/.c volume setter ∥ menu.c stepping logic)
- T012 ∥ T013 (menu.c exit flag ∥ main.c loop condition)

---

## Parallel Example: User Story 1

```bash
# Launch the cross-file pair together:
Task: "T005 [P] [US1] Replace instant-quit ESC with pause toggle in app_handle_events() in src/main.c"
Task: "T006 [P] [US1] Implement menu_handle_key() in src/menu.c"

# Then sequentially (same files as above):
Task: "T007 [US1] Implement menu_render() in src/menu.c"
Task: "T008 [US1] Add the pause guard and render call in src/main.c"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (branch)
2. Complete Phase 2: Foundational (module skeleton + wiring)
3. Complete Phase 3: User Story 1 (pause/resume)
4. **STOP and VALIDATE**: quickstart P1, P2, P5, P6, P8
5. Demo if ready — the run is already safe from accidental ESC quits

### Incremental Delivery

1. Setup + Foundational → module compiles, app unchanged
2. Add User Story 1 → validate → **MVP!**
3. Add User Story 2 → validate (P3)
4. Add User Story 3 → validate (P4)
5. Polish: full P1–P8 pass, format/build gates, commits

### Parallel Team Strategy

Not recommended beyond the cross-file pairs listed above: the feature
is small and concentrated in two files (`src/menu.c`, `src/main.c`).

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Tests were not requested; the quickstart scenarios are the acceptance procedure
- Commit after each task or logical group (constitution principle V)
- Stop at any checkpoint to validate a story independently
- The guardrail syntax hook and `clang-format` run automatically before edits land; T015 is the explicit pre-commit gate
- Cross-feature: once this feature merges, `specs/002-village-buildings-paths` can be implemented and its V7 validated; if 002 lands first, rebase it onto this branch (both touch `src/main.c` and `CMakeLists.txt`)
