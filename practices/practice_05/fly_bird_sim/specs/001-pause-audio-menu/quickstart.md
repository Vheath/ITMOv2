# Quickstart: Validate Pause Menu with Audio Volume Controls

**Feature**: specs/001-pause-audio-menu/spec.md
**Date**: 2026-10-28

Manual flight-based validation scenarios that prove the feature works
end-to-end. The project has no automated test framework; these
scenarios are the acceptance procedure (see the spec's Success
Criteria). Entities and state rules live in
[data-model.md](data-model.md); the module interface and `main.c`
integration in [contracts/menu-module.md](contracts/menu-module.md).

## Prerequisites

- Nix flake environment active (`nix develop` from the repo root) so
  `cmake`, `ninja`, `gcc`, `clang-tools`, and `sdl3` are on PATH.
- A display/audio-capable Linux desktop (the game opens a window and
  synthesizes sound; unmute the output).

## Build & run

```sh
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
./build/sdl3_app
```

Gates before any scenario: the build completes warning-free, and
`clang-format` leaves the tree unchanged.

## Menu reference

ESC toggles the menu (pause/resume). Up/Down select a row
(CONTINUE / VOLUME: NN% / EXIT), Left/Right change the volume on the
VOLUME row, Enter activates the selected row.

## Validation scenarios

### P1 — Pause and resume a flight (SC-001, FR-001..FR-004)

**Steps**: Start a flight. Fly level for ~10 s, note score/speed.
Press ESC. Observe. Press ESC again (or select CONTINUE + Enter).
Repeat the round trip 10 times.

**Expected**:
- On pause: the world freezes (river/cloud motion stops, HUD score
  stops ticking), the wind rush fades out within a second, the menu
  panel with three rows is visible, and steering/boost keys do
  nothing.
- On resume: flight continues from the exact same position, speed,
  score, and run state; the wind ramps back with airspeed.
- Across 10 cycles: no lost or altered state; each round trip well
  under 2 s.

### P2 — No time jump after a long pause (Story 1.4)

**Steps**: Pause the game and leave it open for 30+ s. Resume.

**Expected**: The flight continues from the frozen instant — no burst
of speed, no score jump, no fast-forward of the river/cloud
animations; everything resumes exactly where it froze.

### P3 — Volume control (SC-002, FR-005..FR-007)

**Steps**: Open the menu, select the VOLUME row. Tap Left until the
display reads 0%. Resume and fly (collect a ring, flap, dive, crash).
Reopen the menu, set 100%, resume, repeat. Also try a mid value
(e.g. 50%) while a wind loop is audible.

**Expected**:
- 0%: all four effects (wind rush, wing flap, ring chime, crash thud)
  are completely inaudible.
- 100%: all four play at the pre-feature loudness.
- Changes apply immediately — including to the already-playing wind
  loop, with no game or audio restart; the percentage display always
  matches the current level and never shows values below 0% or above
  100%.

### P4 — Exit from the menu (SC-003, FR-008, FR-010)

**Steps**: Open the menu, select EXIT, press Enter. Separately, run
the game again, open the menu, and close the window with its close
button.

**Expected**: In both cases the process terminates cleanly — audio
stops, the window closes, the shell returns without hang or error
text; exit takes at most two interactions from flight (ESC, Enter on
EXIT).

### P5 — Menu on the game-over screen (edge case)

**Steps**: Crash into terrain. On the "CRASHED INTO TERRAIN" screen
press ESC. Select CONTINUE and press Enter. Then press R.

**Expected**: The menu opens on top of the game-over banner; run
state is not reset by opening or closing it; CONTINUE returns to the
game-over screen (not a new flight); R (restart) is ignored while the
menu is open and works normally after it is closed.

### P6 — Rapid ESC and held-key isolation (edge case)

**Steps**: Mash ESC quickly 5–6 times, ending open. Then close it.
Next: hold W (dive), open the menu while holding, release W while
paused, then resume.

**Expected**: Each ESC press flips the menu exactly once — no lock-up,
no double-flip, no stuck intermediate state. After resume, the bird
does not keep diving on its own (only keys still physically held
count); a key released during the pause has no phantom effect.

### P7 — Keyboard-only operation (SC-005, FR-011)

**Steps**: Unplug/disable the mouse. Perform every menu function:
open, navigate all rows, adjust volume up and down to both extremes,
continue, and (in a separate run) exit.

**Expected**: Everything is reachable with ESC/arrows/Enter alone; at
no point is the mouse needed.

### P8 — Instant menu response (SC-004)

**Steps**: Open and close the menu repeatedly during fast flight.

**Expected**: The menu appears and disappears within the same
interactive moment as the key press; the frozen frame behind it stays
stable with no flicker, half-drawn rows, or interface stutter.

## Regression checklist

- Warning-free `cmake --build build` and clean `clang-format` check.
- Terrain, river, flora, rings, bird animation, camera, and the
  terrain crash behave exactly as before (P1/P2 flights should feel
  identical to the pre-feature build).
- No new files outside `src/menu.h`, `src/menu.c`, and edits to
  `src/main.c`, `src/audio.h`, `src/audio.c`, `CMakeLists.txt`.
- Cross-feature note: after this feature lands, scenario V7 of
  `specs/002-village-buildings-paths/quickstart.md` ("open the pause
  menu (ESC), change volume, resume") becomes executable as written;
  until 002 is merged, villages will not be visible in the scene.
