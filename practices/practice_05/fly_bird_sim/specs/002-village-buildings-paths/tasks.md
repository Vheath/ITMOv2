---
description: "Task list for feature 002: Village Buildings and Field Paths"
---

# Tasks: Village Buildings and Field Paths

**Input**: Design documents from `/specs/002-village-buildings-paths/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/village-module.md, quickstart.md

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

- [x] T001 Create feature branch `feat/village-buildings-paths` from the current HEAD (`git checkout -b feat/village-buildings-paths`) per constitution principle V — deviation: implemented on `practice_5` per user direction (work in this branch only; feature 001 is already merged here, so the cross-feature ordering note is satisfied). Cross-feature note: feature 001 (pause menu) also edits `src/main.c` `app_render()` and `CMakeLists.txt` — branch from `feat/pause-audio-menu` if it is already merged, otherwise plan to rebase whichever feature lands second

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The village module skeleton, its build registration, and the render integration — every user story builds on these

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [x] T002 Create `src/village.h` with the public API per `specs/002-village-buildings-paths/contracts/village-module.md`: `int village_generate_mesh(struct vec3 center_pos, struct tri3d *tris, int max_tris);` — includes `math3d.h`; module owns settlement scenery only (constitution principle V)
- [x] T003 [P] Implement the lattice skeleton in `src/village.c` and register it in the build: `village_generate_mesh()` iterates 96 m integer lattice cells (snapped from `center_pos`, same loop pattern as `scenery_generate_mesh()` in `src/terrain.c`), with a local static integer hash of (ix, iz) in the same family as terrain's `hash_coord()`; returns 0 (empty world) for now; add `src/village.c` to the `add_executable(sdl3_app ...)` source list in `CMakeLists.txt`
- [x] T004 [P] Wire the call site in `app_render()` in `src/main.c` per the contract's integration snippet, submitted between the river and scenery batches: `int village_count = village_generate_mesh(app->bird.pos, app->static_tris, STATIC_MESH_TRIS); render_draw_triangles(&app->r_ctx, app->static_tris, village_count, NULL);`

**Checkpoint**: Foundation ready — `cmake --build build` is warning-free and the app runs exactly as before (village count 0, no visual change)

---

## Phase 3: User Story 1 - Wooden Buildings Around the Fields (Priority: P1) 🎯 MVP

**Goal**: Loose clusters of 1–2 story wooden buildings (box walls + pitched roof) stand flush on the valley fields, none in the canyon corridor, on the river, on steep ground, or above the snow line

**Independent Test**: quickstart.md scenarios V1 (≥3 distinct clusters in a 60 s valley cruise, clearly man-made, mixed story counts) and V2 (flush seating: no floating corner, no more than half-buried, windows only on two-story buildings)

### Implementation for User Story 1

- [x] T005 [US1] Implement cluster and building derivation with placement validation in `src/village.c`: a cluster exists in a cell only if "the hash threshold passes (10–15% of cells) AND at least 2 buildings survive validation" (data-model.md); per building derive from hash bits — count 2–4, center = "hub + hash-derived offset (±20 m)", yaw rotation orienting the door face toward the path hub, `stories` 1 or 2 varying within and across clusters, palette variant (wood tone pair + roof color); validate all four footprint corners per research.md D2: corners lie in the vegetated valley band (~5–31 m altitude), corner-to-corner height spread ≤ 1.5 m, and every corner keeps `|corner.x − sinf(corner.z · 0.01) · 35.0| ≥ 40 m` from the canyon/river centerline; drop failing buildings individually, drop the cluster when the seed fails or fewer than 2 buildings survive — implementation note: on this terrain a single hash-derived spot passes D2 only ~6% of the time (avg corner spread 5.3 m), so the hub and each building use a short deterministic candidate ladder (8/24 hash-derived spots inside their mandated discs) to reach the SC-001 density without touching any spec constant; determinism is unaffected (FR-007)
- [x] T006 [US1] Implement building geometry in `src/village.c` per research.md D4 and data-model.md: rectangular footprint "~6 m × 5 m half-extents", "3.2 m per story"; 4 wall faces (2 triangles each, warm wood-plank browns with lighter gable ends), pitched roof of 2 rectangular slopes (4 triangles) + 2 triangular gable ends, dark door quad (2 triangles) on one long face, window quads (2 triangles each) only when `stories == 2` ("has_windows | True only when stories == 2"); roof in dark slate-brown/red distinct from terrain greens; flush seating: `base_y = min(4 corner terrain heights) − 0.2 m sink` via `terrain_get_height()`; total ≤ 20 triangles per building
- [x] T007 [US1] Implement whole-building emission with the budget guard in `src/village.c`: before emitting each building check `count + building_tris > max_tris` and skip the building entirely (never a partial one — FR-011); write opaque triangles (`alpha = 1.0`) in the caller's `tris` buffer; return the written count with `0 <= count <= max_tris`

**Checkpoint**: User Story 1 fully functional and testable independently (quickstart V1, V2, and the V5 world rules for buildings)

---

## Phase 4: User Story 2 - Paths Between the Buildings (Priority: P2)

**Goal**: Every building cluster is joined by visible flat dirt strips from each building's door to the cluster hub, so the group reads as one inhabited farmstead

**Independent Test**: quickstart.md scenario V3 (dirt strips connect every building to the cluster's middle; no unconnected building; paths stay glued to rolling terrain — no clipping, no hovering)

### Implementation for User Story 2

- [ ] T008 [US2] Implement star-topology path derivation in `src/village.c` per data-model.md and research.md D5: one path per building from its door position to the cluster hub; "width | 2.2 m"; "straight line, tessellated every ~8 m"; each interior vertex samples `terrain_get_height()` and lifts the strip by "+ 0.08 m" (same anti-z-fighting trick as the flower patches); validate that no path vertex falls inside the 40 m corridor margin (FR-006); every surviving building gets a path (FR-004)
- [ ] T009 [US2] Implement path segment emission with the segment budget guard in `src/village.c`: "triangles | 2 per tessellation segment. Packed-earth brown, opaque (D7)" — alpha = 1.0; the same `count + segment_tris > max_tris` whole-unit guard as buildings; skip segments that do not fit, never emit partial geometry

**Checkpoint**: User Stories 1 and 2 both functional (quickstart V3; V4 revisits now include identical paths)

---

## Phase 5: User Story 3 - A Consistent, Unobtrusive Living World (Priority: P3)

**Goal**: The world stays believable and playable: the same place always shows the same farmstead, buildings appear and vanish as whole shapes, nothing stands in the corridor, and the frame rate is unchanged

**Independent Test**: quickstart.md scenarios V4 (two revisits show a byte-for-byte identical cluster), V5 (no building or path in the corridor/river/steep/snow), V6 (no perceptible stutter vs. the pre-feature build), V7 (gameplay, HUD, audio, crash behaviour untouched)

### Implementation for User Story 3

- [ ] T010 [US3] Guarantee purity and determinism in `src/village.c` (FR-007, contract): output depends only on `center_pos` and compile-time constants — no `elapsed_time`, no frame-time, no call-order or prior-call dependence, no heap allocation, no global mutable state, no SDL calls; the lattice range derives from `center_pos` alone so buildings appear and vanish as whole shapes at the view edge (FR-011)
- [ ] T011 [US3] Tune density and range constants in `src/village.c` (FR-001, SC-001, edge cases): 96 m cell size, cluster hash threshold in the 10–15% band, minimum 2 surviving buildings per cluster; tune so a 60 s valley cruise meets "at least 3 distinct building clusters" (SC-001) while avoiding both failure modes — no dense town clusters and no long flight without any farmstead
- [ ] T012 [US3] Audit the triangle load in `src/village.c` (FR-010, SC-005): ≤ 20 triangles per building, 2 per path segment, expected ceiling ≤ ~10 visible clusters × ~100 triangles ≈ 1000 triangles per frame (~1.5% of the 65536-triangle buffer); confirm the whole-unit budget guards from T007/T009 hold under load (busy screens skip gracefully, never render incomplete)

**Checkpoint**: All user stories independently functional; the world gains life without losing consistency, safety, or performance

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Full validation, quality gates, and delivery discipline

- [ ] T013 Run all quickstart.md validation scenarios V1–V7 from `specs/002-village-buildings-paths/quickstart.md` and confirm every expected outcome; V7's pause-menu steps ("open the pause menu (ESC), change volume, resume") require feature 001 to be merged — run them against a build that includes `feat/pause-audio-menu`, or defer those steps and note it
- [ ] T014 Quality gates per constitution IV/V: `clang-format -i` on `src/village.h`, `src/village.c`, `src/main.c` leaves the tree unchanged; warning-free `cmake --build build`; inspect `git status`/`git diff` for unintended changes (no files touched outside `src/village.h`, `src/village.c`, `src/main.c`, `CMakeLists.txt`)
- [ ] T015 Commit the work on `feat/village-buildings-paths` in logical groups with Conventional Commits (`feat(village): ...`), one group per completed user story checkpoint; rebase onto `feat/pause-audio-menu` if 001 landed first (both edit `app_render()` in `src/main.c` and the `CMakeLists.txt` source list)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **User Stories (Phases 3–5)**: All depend on Phase 2 completion; the feature is concentrated in one new file (`src/village.c`), so stories run sequentially in priority order (P1 → P2 → P3)
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Phase 2 — no dependencies on other stories
- **User Story 2 (P2)**: Depends on US1 (buildings are the path endpoints and door positions come from US1 geometry)
- **User Story 3 (P3)**: Depends on US1 + US2 (its scenarios V4–V7 verify paths and cluster layouts, not buildings alone)

### Within Each User Story

- All story tasks edit `src/village.c` → strictly sequential (T005 → T006 → T007; T008 → T009; T010 → T011 → T012)
- The only cross-file parallel pair is in Phase 2: T003 (`src/village.c` + `CMakeLists.txt`) ∥ T004 (`src/main.c`)

### Parallel Opportunities

- T003 ∥ T004 (module skeleton + build registration ∥ main.c call site)
- No other same-phase pairs: every story task lives in `src/village.c`

---

## Parallel Example: User Story 1

```bash
# The feature's only parallel pair is foundational (T003 ∥ T004).
# User story work itself is sequential in src/village.c:
Task: "T005 [US1] Cluster and building derivation with placement validation in src/village.c"
Task: "T006 [US1] Building geometry (walls, pitched roof, door, windows, flush seating) in src/village.c"
Task: "T007 [US1] Whole-building emission with budget guard in src/village.c"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (branch)
2. Complete Phase 2: Foundational (module skeleton + call site)
3. Complete Phase 3: User Story 1 (buildings)
4. **STOP and VALIDATE**: quickstart V1, V2 (buildings alone already deliver the user's goal)
5. Demo if ready — the valley is inhabited

### Incremental Delivery

1. Setup + Foundational → module compiles, world unchanged
2. Add User Story 1 → validate → **MVP!**
3. Add User Story 2 → validate (V3)
4. Add User Story 3 → validate (V4–V7)
5. Polish: full V1–V7 pass, format/build gates, commits

### Parallel Team Strategy

Not recommended: the feature is one focused module in a single new
file; one developer runs the story sequence end to end.

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Tests were not requested; the quickstart scenarios are the acceptance procedure
- Commit after each task or logical group (constitution principle V)
- Stop at any checkpoint to validate a story independently
- The guardrail syntax hook and `clang-format` run automatically before edits land; T014 is the explicit pre-commit gate
- Cross-feature: if 001 (pause menu) is merged first, branch/rebase on top of it; after both merge, run 002's V7 in full — its pause-menu steps depend on 001
