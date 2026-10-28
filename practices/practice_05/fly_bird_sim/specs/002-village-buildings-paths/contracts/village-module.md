# Contract: village module (internal C API)

**Feature**: specs/002-village-buildings-paths/spec.md
**Date**: 2026-10-28

The application exposes no external interfaces (no API, CLI, or file
formats); the contract below is the internal module boundary between
`src/village.h`/`src/village.c` and its single consumer `src/main.c`,
following the same protocol as `terrain_generate_mesh()`,
`river_generate_mesh()`, and `scenery_generate_mesh()`.

## Public interface

```c
int village_generate_mesh(struct vec3 center_pos,
			  struct tri3d *tris, int max_tris);
```

| Parameter | Meaning |
|-----------|---------|
| `center_pos` | Bird world position; defines which 96 m lattice cells are in range |
| `tris` | Caller-owned triangle buffer (same `static_tris` array the other generators use) |
| `max_tris` | Capacity of `tris` |

## Behavior contract

- **Returns** the number of triangles written; `0 <= count <= max_tris`.
  A short count is normal (no clusters in range) and is not an error.
- **Never writes past `max_tris`.** Buildings and path segments are
  emitted whole: if the remaining budget cannot fit a complete building,
  the building is skipped entirely — a partial building must never
  appear (spec FR-011).
- **Purity**: output is a deterministic function of `center_pos` and
  compile-time constants (spec FR-007). No dependence on frame time,
  call order, or prior calls. No heap allocation, no global mutable
  state, no SDL calls — the module is renderer-agnostic and draws
  nothing itself.
- **Semantics**: emitted triangles represent clusters of 1–2 story
  wooden buildings and connecting dirt paths placed only in the valley
  meadow band, outside the canyon/river corridor margin (spec FR-001,
  FR-002, FR-004, FR-006). All triangles opaque.
- **Gameplay isolation**: the module reads the world only through
  `terrain_get_height()` and mutates nothing — no collision, scoring, or
  audio side effects (spec FR-008).

## Preconditions

- `tris` is writable and `max_tris > 0`.
- The terrain module is usable (`terrain_get_height()` callable), same
  lifetime as the other scenery generators.

## Integration (main.c, per frame)

```c
int village_count = village_generate_mesh(app->bird.pos,
					  app->static_tris,
					  STATIC_MESH_TRIS);
render_draw_triangles(&app->r_ctx, app->static_tris, village_count,
		      NULL);
```

Submitted as its own step in `app_render()` alongside the river and
scenery batches; depth ordering is handled downstream by the render
context's painter's-algorithm sort before `render_present()`.

## Failure modes

None beyond a short count: the module allocates nothing and calls only
pure math plus `terrain_get_height()`, so there is no failure path to
signal. (This keeps the kernel-style `goto` cleanup chain empty by
construction.)
