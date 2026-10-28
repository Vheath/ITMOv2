# Quickstart: Validate Village Buildings and Field Paths

**Feature**: specs/002-village-buildings-paths/spec.md
**Date**: 2026-10-28

Manual flight-based validation scenarios that prove the feature works
end-to-end. The project has no automated test framework; these scenarios
are the acceptance procedure (see the spec's Success Criteria). Entity
and placement rules live in [data-model.md](data-model.md); the module
interface contract in [contracts/village-module.md](contracts/village-module.md).

## Prerequisites

- Nix flake environment active (`nix develop` from the repo root) so
  `cmake`, `ninja`, `gcc`, `clang-tools`, and `sdl3` are on PATH.
- A display/audio-capable Linux desktop (the game opens a window).

## Build & run

```sh
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
./build/sdl3_app
```

Gates before any scenario: the build completes warning-free, and
`clang-format` leaves the tree unchanged.

## Validation scenarios

### V1 — Buildings appear around the fields (SC-001, SC-002)

**Steps**: Start a flight. Fly level at moderate altitude (~60–100 m)
over the green valley, avoiding the canyon, for ~60 seconds.

**Expected**:
- At least 3 distinct building groups appear beside the green fields.
- Each group reads as man-made: wooden box walls + pitched roof; some
  clearly single-story, others clearly two-story.
- At typical cruising distance you never mistake a building for a tree,
  rock, or terrain bump.

### V2 — Buildings sit flush on the ground (SC-003)

**Steps**: Pick one cluster and fly a slow low circle around it,
descending carefully (stay above crash altitude, watch the AGL readout).

**Expected**:
- No building floats above the terrain; no building is more than
  half-buried; door quads are visible on walls; windows appear only on
  two-story buildings.

### V3 — Paths connect each cluster (SC-004, part 1)

**Steps**: Overfly each cluster seen in V1 at an angle where paths are
visible (paths are flat strips; view from above/aside works best).

**Expected**:
- Dirt strips connect every building to the cluster's middle; no
  unconnected building; paths stay on the ground over rolling terrain
  (no clipping through rises, no hovering over dips).

### V4 — Determinism on revisit (SC-004, part 2)

**Steps**: Fly past a cluster until it leaves the view behind, then turn
around and return to the same area. Repeat once more (two revisits).

**Expected**:
- The identical cluster reappears: same buildings, same positions, same
  story counts, same paths — byte-for-byte layout, twice.

### V5 — World rules hold (FR-006, edge cases)

**Steps**: Fly along the canyon/river corridor for ~60 s, then survey
snowy high ground and the riverbanks.

**Expected**:
- No building or path ever stands in the corridor, over the river, on
  the riverbanks, on steep slopes, or above the snow line — habitation
  exists only around the valley fields.

### V6 — Performance is unchanged (SC-005)

**Steps**: With buildings on screen, fly normally for ~60 s and compare
subjectively against a run of the pre-feature build (e.g. keep the old
binary or `git stash` the change, rebuild, fly, restore).

**Expected**:
- No perceptible stutter or frame-rate drop versus the pre-feature
  build; buildings appear and disappear at the view edge as whole
  shapes, without flicker or half-drawn geometry.

### V7 — Gameplay is untouched (SC-006, FR-008)

**Steps**: Fly the same run you would without the feature: collect
rings, build a combo, open the pause menu (ESC), change volume, resume,
and eventually crash into terrain.

**Expected**:
- Controls, HUD, scoring, ring collection, pause menu, and the terrain
  crash all behave exactly as before; buildings add no collision (diving
  through a building does not trigger a crash unless you hit the
  terrain) and no new sounds.

## Regression checklist

- Warning-free `cmake --build build` and clean `clang-format` check.
- Terrain, river, flora, rings, bird animation, audio, pause menu
  unaffected (V7).
- No new files outside `src/village.h`, `src/village.c`, `src/main.c`
  (one call site), and `CMakeLists.txt` (source list entry).
