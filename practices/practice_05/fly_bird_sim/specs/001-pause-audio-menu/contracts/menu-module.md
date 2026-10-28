# Contract: menu module (internal C API) + audio volume setter

**Feature**: specs/001-pause-audio-menu/spec.md
**Date**: 2026-10-28

The application exposes no external interfaces (no API, CLI, or file
formats); the contracts below are the internal module boundaries of
this feature: `src/menu.h`/`src/menu.c` (new) against its single
consumer `src/main.c`, and the one-function extension of
`src/audio.h`/`src/audio.c`. Style follows the same protocol as the
existing module headers (`audio.h`, `terrain.h`).

## Public interface — menu module

```c
void menu_init(struct pause_menu *m);
bool menu_is_open(const struct pause_menu *m);
void menu_toggle(struct pause_menu *m);
void menu_handle_key(struct pause_menu *m, SDL_Keycode key);
float menu_volume(const struct pause_menu *m);
bool menu_wants_exit(const struct pause_menu *m);
void menu_render(const struct pause_menu *m, SDL_Renderer *renderer,
		 int window_w, int window_h);
```

| Function | Meaning |
|----------|---------|
| `menu_init` | Zeroes state; volume = 1.0 (100%); row 0 selected; closed |
| `menu_is_open` | `true` = session paused, menu visible |
| `menu_toggle` | Flips `open` exactly once (ESC semantics, FR-004) |
| `menu_handle_key` | Consumes one discrete KEY_DOWN (non-repeat) while open: arrows navigate/adjust, Enter activates, ESC toggles |
| `menu_volume` | Current level, clamped 0.0..1.0 |
| `menu_wants_exit` | One-shot: set by the EXIT action; caller resets on read via `menu_init`-style teardown or ignores post-exit |
| `menu_render` | Immediate-mode 2D overlay: panel + 3 rows + percentage; no-op when closed |

## Behavior contract — menu module

- **Purity of scope**: no SDL audio calls, no event polling, no
  teardown calls, no heap allocation, no global state. The module
  mutates only its own struct; the caller applies side effects
  (volume, wind fade, quit) — it never observes the run state
  (`game_over`), the bird, or the camera.
- **Key semantics** (spec FR-004, FR-005, FR-011): ESC toggles;
  Up/Down move `selected` with wraparound across 3 rows; Left/Right on
  the VOLUME row step `volume` by ±0.1 with clamping to [0.0, 1.0] —
  never wrapping, never out of range (edge case); Enter on CONTINUE
  closes, on EXIT sets `wants_exit`, on VOLUME is a no-op.
- **Determinism**: `menu_render` output is a pure function of the
  struct plus window size; identical state draws identical pixels.
- **Volume display**: `(int)(volume * 100.0f + 0.5f)` on the VOLUME
  row whenever open (FR-007).

## Public interface — audio volume setter

```c
void audio_engine_set_volume(struct audio_engine *ae, float volume);
```

| Parameter | Meaning |
|-----------|---------|
| `ae` | Initialized audio engine (stream may be NULL if init failed — then the call is a safe no-op) |
| `volume` | Master gain; clamped to [0.0, 1.0] before use |

**Behavior contract**: clamps, then calls
`SDL_SetAudioStreamGain(ae->stream, volume)`. Effect is immediate for
already-playing and newly-triggered sounds (applied downstream of the
synthesis callback during `SDL_GetAudioStreamData`) with no restart
(FR-006); 0.0 is true digital silence, 1.0 is the pre-feature
loudness (FR-005). Thread-safe against the audio callback thread (SDL
holds the stream mutex internally). The `bool` return of the SDL call
is checked; on `false` the error is logged via `SDL_GetError()` and
the engine continues (audio volume is non-fatal). Callers may invoke
it idempotently with the same value.

## Preconditions

- `struct pause_menu` is initialized by `menu_init` before any other
  call; the struct lives inside `struct app_context` with the same
  lifetime as the loop.
- `menu_handle_key` is called only for events observed while the menu
  is open (the caller gates routing); repeat events must be filtered
  by the caller (`event.key.repeat`) before dispatch.
- `audio_engine_set_volume` requires only `audio_engine_init` to have
  run (failure-tolerant).

## Integration (main.c)

Event routing (`app_handle_events`), in order:

```c
if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
	if (event.key.key == SDLK_ESCAPE) {
		menu_toggle(&app->menu);
		if (menu_is_open(&app->menu))
			audio_engine_set_speed(&app->audio, 0.0f, false);
	} else if (menu_is_open(&app->menu)) {
		menu_handle_key(&app->menu, event.key.key);
	} else if (event.key.key == SDLK_R && app->game_over) {
		/* existing restart path, unchanged */
	}
}
```

Update guard (`app_update`, first line, beside the existing
`game_over` return):

```c
if (app->game_over || menu_is_open(&app->menu))
	return;
```

Volume application (after the event loop, idempotent):

```c
audio_engine_set_volume(&app->audio, menu_volume(&app->menu));
```

Render call (`app_render`, after the game-over banner, before
`SDL_RenderPresent()`):

```c
if (menu_is_open(&app->menu))
	menu_render(&app->menu, app->renderer, w, h);
```

Loop exit (main loop condition extension):

```c
while (app.running && !menu_wants_exit(&app->menu)) { /* ... */ }
```

`SDL_EVENT_QUIT` keeps setting `running = false` while the menu is
open (FR-010); wind is restored on resume by the first
`audio_engine_set_speed()` call from the unguarded `app_update()`.

## Failure modes

- `menu_*`: none observable — all functions are state flips and 2D
  drawing over validated indices; no allocation, no SDL calls that
  can fail (text/fill draws are fire-and-forget in SDL3).
- `audio_engine_set_volume`: only failure is `SDL_SetAudioStreamGain`
  returning `false` (e.g. NULL stream after failed audio init) —
  logged, non-fatal, run continues silently. This keeps the
  kernel-style `goto` cleanup chain in `main.c` unchanged: the menu
  adds no resources to release.
