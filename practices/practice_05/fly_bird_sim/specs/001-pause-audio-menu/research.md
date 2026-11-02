# Research: Pause Menu with Audio Volume Controls

**Feature**: specs/001-pause-audio-menu/spec.md
**Date**: 2026-10-28
**Status**: All design decisions resolved (no NEEDS CLARIFICATION remained
in the plan's Technical Context; the decisions below resolve the design
space behind the spec's FRs)

Method: direct codebase analysis of the main loop and input path
(`src/main.c` `app_handle_events()` / `app_update()` / `app_render()`),
the real-time audio engine (`src/audio.c` stream callback and setter
protocol), plus the project constitution
(`.specify/memory/constitution.md`) and the SDL3 audio API
(`SDL_SetAudioStreamGain`, verified against the installed headers via
the SDL3 MCP server). No external dependencies or unknown technologies
are involved, so no external research was required.

---

## D1: Module split and ownership of the paused state (FR-001, FR-002)

**Decision**: A new focused module `src/menu.h` / `src/menu.c` owns the
whole pause-menu concern: the open/closed flag (the session's paused
state, per the spec's Key Entities), the selected item, the volume
level, and the 2D overlay drawing. `main.c` consults
`menu_is_open()` for its update guard, routes key events while the
menu is open, and calls one render function per frame while open. The
menu module never touches the audio engine or the process lifecycle;
volume application and quitting stay in `main.c`.

**Rationale**: Constitution principle V — one concern per module, and
`main.c` must not accumulate a UI responsibility. Keeping the quit
decision and audio calls out of `menu.c` leaves the module trivially
testable state logic plus drawing, with no engine side effects.

**Alternatives considered**:
- Paused flag in `struct app_context`, menu as a dumb view — rejected:
  the paused state and the menu are the same concern; splitting them
  across two files invites drift (flag open, menu closed).
- All menu logic inline in `main.c` — rejected: `main.c` already owns
  the loop; a 200-line menu body there violates the module rule and
  the kernel-style function-size guidance.

## D2: Pause mechanics in the main loop (FR-002, FR-003, SC-001)

**Decision**: The main loop keeps running at full rate while paused.
`app_update()` gains an early return
`if (app->game_over || menu_is_open(&app->menu)) return;` before any
simulation work. Because the loop keeps ticking, `dt` is consumed every
frame and `last_ticks` keeps advancing — there is no accumulated
interval to fast-forward on resume (spec Story 1.4). Freezing
`app_update()` also freezes `elapsed_time`, which is the only clock
that drives the river and scenery animations (`main.c`), so the world
visibly stops; the per-frame world generators are deterministic
functions of the (now constant) bird position, so the frozen frame is
byte-identical every frame. Rendering continues so the menu is drawn
over the frozen scene.

**Rationale**: Reuses the proven `game_over` early-return pattern
(`app_update()` already returns early for the crash screen); zero new
loop machinery; the "no time jump" edge case is satisfied structurally
rather than by clamping (the existing 0.05 s `dt` clamp remains a
second guard).

**Alternatives considered**:
- Skip the whole loop body while paused (event-wait) — rejected: a
  wake-up `dt` spike would need special handling, and the render path
  would need a separate "draw menu only" mode; strictly more code.
- Freeze `SDL_GetTicksNS` bookkeeping instead — rejected: the tick
  delta is consumed per frame anyway; touching it adds a state machine
  for no benefit.

## D3: ESC toggle and input routing (FR-001, FR-004, FR-011, edge cases)

**Decision**: In `app_handle_events()`, ESC on `SDL_EVENT_KEY_DOWN` (with
`event.key.repeat` filtered so held-key auto-repeat cannot flicker the
menu) toggles the menu: closed → open (and pause), open → closed (and
resume, identical to "Continue"). While the menu is open, discrete
KEY_DOWN events for arrows, Enter, and R are routed to the menu
(`menu_handle_key()`); R (restart) is ignored while paused per FR-002.
Continuous gameplay input (W/A/S/D/SPACE, read via
`SDL_GetKeyboardState` into `app->input_*`) is still refreshed every
frame but is inert because `app_update()` is guarded — and because
those fields derive from the *live* physical key state each frame, a
key held at pause time and released during the pause has no phantom
effect after resume (spec edge case), with zero extra code. Window
close (`SDL_EVENT_QUIT`) keeps setting `running = false` while the
menu is open (FR-010). "Exit" in the menu also just sets
`running = false` through a `menu_wants_exit()` flag, so shutdown
flows through the existing cleanup chain (`free` buffers → destroy
render context → destroy audio → destroy window → `SDL_Quit()`).

**Rationale**: One event, one toggle, exactly the spec's toggle
semantics; the existing early-return guard makes continuous-input
isolation free; exit reuses the already-correct teardown path instead
of duplicating cleanup.

**Alternatives considered**:
- Edge-detect ESC from the keyboard state array — rejected: the event
  queue already provides press semantics and the repeat flag.
- A dedicated "input disabled" mode that clears `input_*` — rejected:
  recomputation from live state each frame is already correct.
- Calling exit paths directly from the menu (e.g. `SDL_Quit`) —
  rejected: would bypass the kernel-style reverse-order cleanup chain
  in `main()`.

## D4: Menu rendering layer (FR-011, SC-004, constitution III)

**Decision**: The menu is a 2D immediate-mode overlay: one
semi-transparent dark `SDL_FRect` panel (blend mode, as the existing
game-over banner) plus `SDL_RenderDebugText` rows — "CONTINUE",
"VOLUME: NN%", "EXIT" — with the selected row recolored. It is drawn
in `app_render()` after the HUD/game-over banner and before
`SDL_RenderPresent()`, only while the menu is open. No fonts are
loaded: `SDL_RenderDebugText` is built into SDL3, so no asset files
enter the repo.

**Rationale**: Exactly matches the established HUD/game-over drawing
precedent in `main.c` (same APIs, same layer). Constitution principle
III governs the 3D scene pipeline, which remains untouched; a 2D UI
overlay on the presented frame is the project's existing convention.
Text via the debug font honors "retro legible" (spec assumption) and
the no-assets rule at zero cost.

**Alternatives considered**:
- 3D quads through the software pipeline — rejected: no user value at
  a menu's fixed screen position; wastes triangle budget and adds
  transform work for flat text.
- SDL_ttf with a bundled font file — rejected: new dependency (link
  set must stay `SDL3::SDL3` + `m`, principle I) and an imported asset
  (principle II).
- A separate SDL texture composition layer — rejected: extra state and
  lifetime management for 3 text rows.

## D5: Volume application mechanism (FR-005, FR-006, FR-007)

**Decision**: `audio_engine_set_volume(struct audio_engine *ae, float
volume)` in the audio module: clamp to [0.0, 1.0], then call
`SDL_SetAudioStreamGain(ae->stream, volume)` (SDL 3.2.0+, project
requires 3.4+), checking the `bool` result. SDL applies the gain
downstream of the synthesis callback during `SDL_GetAudioStreamData`,
holds its own stream mutex (thread-safe against the audio callback
thread), and supports continuous changes — so the new level takes
effect on already-playing and newly-triggered sounds alike within
audio-buffer latency, with no restart (FR-006). The menu stores the
level as a 0.0–1.0 float, displays `volume * 100` as a percentage
(FR-007), and `main.c` pushes it to the audio engine after menu key
events (idempotent — same value, no effect). Default 1.0 (100%, the
game's normal loudness).

**Rationale**: One SDL3 call replaces what would otherwise be a new
`master_gain` field, callback edits, and lock discipline in
`audio.c`; it is the documented SDL way to scale a stream and matches
the constitution's SDL3-first rule. The wrapper keeps `menu.c`
decoupled from audio internals (the menu never touches
`ae->stream`); the clamp guarantees the extremes and no wrap-around
(spec edge case), with 0.0 giving true digital silence.

**Alternatives considered**:
- `master_gain` field multiplied inside the synthesis callback —
  rejected: correct but strictly more moving parts (new field, mutex
  discipline, callback edit) for the same result.
- Adjusting per-voice amplitudes (four separate gains) — rejected: the
  spec mandates a single master control; per-effect scaling would also
  re-touch every voice's mixing code.
- SDL_ttf-styled audio "click" feedback on menu changes — rejected:
  new audio content is out of scope (spec assumption).

## D6: Menu items and keyboard navigation (FR-005, FR-007, FR-011)

**Decision**: Three rows: `CONTINUE`, `VOLUME: NN%`, `EXIT`. Up/Down
move the selection (wrapping top↔bottom); Left/Right adjust the volume
by 10% steps (0%…100%, clamped at both ends) while the VOLUME row is
selected; Enter activates the selected row (Continue → resume, Exit →
quit; on the volume row Enter is a no-op); ESC always toggles. Ten
steps of 10% make 0% and 100% exactly reachable — the two extremes the
spec's edge case demands (true silence, normal loudness).

**Rationale**: The spec's assumption ("arrow keys to select/adjust,
Enter to activate") maps one-to-one onto this scheme; navigation and
adjustment stay orthogonal (Up/Down = which row, Left/Right = how
much), which keeps the state machine a single selected-index plus a
float. Step adjustment is deterministic and instantly verifiable.

**Alternatives considered**:
- Up/Down adjusting the volume directly when its row is selected —
  rejected: overloads one axis with two meanings; the spec story's
  "moves a volume control up and down" describes changing the level,
  not a specific key axis; Left/Right fulfills it with cleaner modal
  separation.
- A separate volume sub-screen — rejected: violates "keep it simple";
  one panel is the whole menu.
- Analog/hold-to-slide volume — rejected: no measurable benefit over
  10% steps and harder to validate.

## D7: Audio behaviour while paused (FR-002, FR-009)

**Decision**: On menu open, `main.c` calls
`audio_engine_set_speed(&app->audio, 0.0f, false)` — the existing
setter drives `wind_target_intensity` to 0 and the callback's own
smoothing (12·dt tracking) fades the wind loop out naturally within a
fraction of a second, satisfying "stop or fade". One-shot envelopes
(flap, chime, crash) simply decay per their existing envelopes; no new
triggers can occur because they are only fired from `app_update()`,
which is guarded — this also covers the "menu opens at the exact
moment a sound triggers" edge case without special handling. On
resume, `app_update()` calls `audio_engine_set_speed()` with the live
airspeed on its next frame, restoring the wind. The audio stream keeps
running throughout, so volume changes made in the menu are audible
immediately (FR-006) and audio state stays valid across resume
(FR-009).

**Rationale**: Reuses the crash path's proven "wind to zero" mechanic
(`audio_engine_trigger_crash()` already does exactly this); zero new
audio-thread state; the fade is the softer of the two FR-009-sanctioned
behaviours and needs no restore logic beyond the existing per-frame
set_speed call.

**Alternatives considered**:
- `SDL_PauseAudioStreamDevice()` while paused — rejected: pauses
  audible volume feedback in the menu (the player could not hear
  volume changes take effect, weakening FR-006's immediate-feedback
  intent) and adds device state transitions to rapid ESC toggling.
- Forcing gain to 0 while paused and restoring on resume — rejected:
  conflates pause-silence with the user's volume setting; a crash
  during restore bookkeeping would eat the user's chosen level.
- Silencing by muting each voice's trigger functions — rejected: the
  guard on `app_update()` already prevents triggers; belt-and-braces
  muting adds dead code.
