# Data Model: Pause Menu with Audio Volume Controls

**Feature**: specs/001-pause-audio-menu/spec.md
**Date**: 2026-10-28

Unlike the scenery features, this feature owns **mutable session
state**: a paused flag, a selected menu row, and a volume level. All
state is in-memory only (the spec excludes persistence for v1); the
concrete C representation follows kernel style: explicit `struct`
names, no typedefs for structs, fields in `snake_case`.

---

## Entity: Pause Menu

The overlay screen and the session's paused/active state — one
concern owned by `src/menu.c` (`struct pause_menu`).

**Fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| open | boolean | `true` = paused, menu visible; toggled only by ESC key-down (non-repeat) or Continue (FR-001, FR-004); each press flips it exactly once (edge case: rapid ESC) |
| selected | row index, 0..2 | 0 = CONTINUE, 1 = VOLUME, 2 = EXIT; moved by Up/Down with wraparound; clamped to the row count (FR-011) |
| volume | float, 0.0..1.0 | Master loudness; 1.0 by default; adjusted in 0.1 steps, clamped at both ends — never negative, never > 1.0, no wrap-around (FR-005, edge case) |
| wants_exit | boolean | Set when "Exit" is activated; read once by the main loop, which then shuts down through its existing cleanup chain (FR-008) |

**Relationships**: reads/writes the Volume Setting (its `volume`
field *is* the setting; display derives as `volume * 100`, rounded);
exposes the Menu Actions; consulted by `main.c` for the
`app_update()` guard and event routing.

**Validation rules** (from spec FRs):
- FR-002: while `open`, gameplay input and simulation updates are
  skipped — enforced by the caller's guard, not by the menu itself.
- FR-007: the displayed percentage is a pure function of `volume`.
- SC-004: open/close is a state flip with no animation state, so
  appearance is within the same frame as the key press.

## Entity: Volume Setting

The single master loudness level applied to all four sound effects
(wind rush, wing flap whoosh, ring chime, crash thud).

**Fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| level | float, 0.0..1.0 | Stored in `struct pause_menu.volume`; applied to the audio engine as a stream gain by `main.c` after every menu key event (idempotent) |
| display | integer, 0..100 | `(int)(volume * 100.0f + 0.5f)`; shown on the VOLUME row at all times while the menu is open |

**Relationships**: owned by Pause Menu; consumed by the audio engine
via `audio_engine_set_volume()` (clamps again defensively; the SDL3
stream gain is 1.0 = normal loudness, 0.0 = true silence).

**Validation rules**:
- FR-005: 0.0 maps to digital silence (gain 0), 1.0 maps to the
  game's normal loudness (gain 1) — no other scaling is introduced.
- FR-006: application is a stream-gain call, so the level affects
  already-playing and newly-triggered sounds without restart.
- Edge case: 0% is true silence because gain multiplies the mixed
  output to exact zero; 100% is bit-identical to pre-feature output.

## Entity: Menu Action

The selectable commands exposed by the pause menu.

**Fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| CONTINUE | action on row 0 | Enter on row 0 → `open = false` (resume); ESC anywhere behaves identically (FR-003, FR-004); from the game-over screen it returns to the game-over screen — no reset (edge case) |
| VOLUME | action on row 1 | Left/Right on row 1 → ±0.1 step, clamped; Enter on row 1 is a no-op (FR-005) |
| EXIT | action on row 2 | Enter on row 2 → `wants_exit = true`; the main loop performs the standard reverse-order teardown (FR-008, FR-010) |

**Relationships**: belongs to Pause Menu; EXIT delegates the actual
shutdown to `main.c` (the menu never calls SDL teardown itself).

---

## State transitions

The only stateful automaton in the feature is the pause session
(`open` + `wants_exit`), driven by discrete key events:

```text
ACTIVE ──ESC down──▶ PAUSED ──ESC down / CONTINUE──▶ ACTIVE
  ▲                    │
  │                    └──EXIT──▶ TERMINATED (process teardown)
  │
  └── window close (any state) ──▶ TERMINATED
```

- `PAUSED → ACTIVE` preserves the run bit-for-bit: position, speed,
  score, run state, and animation clocks are untouched because
  `app_update()` was the sole mutator and it was skipped (FR-003,
  SC-001).
- `ACTIVE` includes both in-flight and the game-over screen; the
  menu opens on both (edge case), and closing it returns to whichever
  screen was active — the crash state is a separate `game_over` flag
  the menu never reads or writes.
- `TERMINATED` is process exit: `wants_exit` is a one-shot flag; the
  menu module itself has no teardown responsibility.
- Volume changes are not transitions of this automaton: they mutate
  `volume` while `PAUSED` and are applied immediately; they are also
  valid to make repeatedly in one pause session (FR-006).

## Concurrency

The menu state lives on the main thread only. The one cross-thread
interaction is the audio stream gain (`SDL_SetAudioStreamGain` is
internally mutexed by SDL against the audio callback thread, per the
SDL3 header documentation); the menu never touches audio-thread state
directly. No heap allocation, no globals — the menu module's kernel
cleanup chain is empty by construction.
