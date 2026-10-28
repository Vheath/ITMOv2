# Research: Village Buildings and Field Paths

**Feature**: specs/002-village-buildings-paths/spec.md
**Date**: 2026-10-28
**Status**: All design decisions resolved (no NEEDS CLARIFICATION remained
in the plan's Technical Context; the research tasks below resolve the six
design decisions behind the spec's FRs)

Method: direct codebase analysis of the existing procedural scenery
pipeline (`src/terrain.c` flora/river generators, `src/main.c`
`app_render()` integration, `src/terrain.h`, `CMakeLists.txt`), plus the
project constitution (`.specify/memory/constitution.md`). No external
dependencies or unknown technologies are involved, so no external
research was required.

---

## D1: Cluster placement algorithm (FR-001, FR-007)

**Decision**: Hash-seeded cluster lattice. A coarse integer grid (cell
size 96 m, i.e. 8x the 12 m flora cell) is derived from the snapped bird
position, exactly like the flora loop in `scenery_generate_mesh()`. Each
cell (ix, iz) is hashed with an integer hash (same family as the existing
`hash_coord()`); a small fraction of cells (roughly 10–15%) become
cluster seeds. A cluster's building count (2–4), per-building offsets
(±20 m from the seed), rotations, story counts, and palette variants all
derive from bits of that same hash.

**Rationale**: Guarantees FR-007 (determinism: same cell → same hash →
same farmstead, every frame and every session) with zero stored state,
and matches the proven flora pattern already in the codebase. The 96 m
lattice gives an average spacing of several hundred meters between
farmsteads — occasional habitation in wilderness, per the spec's density
assumption, while SC-001 (≥3 clusters per 60 s cruise) stays reachable
given flight speed and the visible scenery radius (~264 m dense band,
far ring beyond).

**Alternatives considered**:
- Per-frame random placement — rejected: non-deterministic, buildings
  would re-shuffle every frame (flicker), violating FR-007.
- Spawn-on-distance rings around the bird — rejected: layout would
  change as the bird moves; revisits would not reproduce the same
  hamlet (violates SC-004).
- Fixed hand-placed villages at world coordinates — rejected: the world
  scrolls indefinitely; hand placement cannot cover it.

## D2: Valid placement volume (FR-001, FR-006)

**Decision**: A candidate building is accepted only if all four footprint
corners, sampled via `terrain_get_height()`, lie in the vegetated valley
band (~5–31 m altitude, same band the flora uses), the corner-to-corner
height spread is ≤ 1.5 m (flat enough ground), and every corner keeps
|corner.x − sinf(corner.z · 0.01) · 35.0| ≥ 40 m from the canyon/river
centerline. A cluster is dropped entirely if its seed fails; individual
buildings within a cluster are dropped individually. Path vertices obey
the same corridor margin.

**Rationale**: Reuses the exact exclusion model the flora already
implements (canyon corridor via the sine centerline, altitude band), so
villages inherit proven, consistent world rules. The 40 m margin is wider
than the flora's 12 m because the river strip is 24 m wide and buildings
are large visual masses — they must never line the flight corridor
(FR-006) or visually tangle with the riverbanks. The 1.5 m spread bound
keeps both seating (D3) and the "fields" reading intact; steeper cells
read as slopes, not fields.

**Alternatives considered**:
- Allow buildings anywhere green — rejected: mid-slope buildings bury or
  float; snow-line and river exclusions from FR-006 would be violated.
- 15 m corridor margin — rejected: still inside/near the 24 m river strip
  and the tree line; scenery would crowd the gameplay corridor.
- Flattening terrain under buildings — rejected: `terrain_get_height()`
  is shared by terrain rendering, flora seating, and the crash check in
  `main.c`; changing it risks gameplay and visual regressions outside
  this feature's scope (violates "keep it simple" and modularity).

## D3: Flush seating on the terrain (FR-005, SC-003)

**Decision**: Base height = min(4 corner heights) − 0.2 m sink. The walls
start slightly below the lowest corner; the D2 spread bound (≤ 1.5 m)
caps the worst-case burial at ~1.7 m on a ≥ 3.2 m tall one-story wall —
never half-buried (SC-003), and no corner can float because the base is
at the lowest sampled ground.

**Rationale**: Min-corner anchoring plus a small sink is the cheapest
guarantee of "no floating" on mildly uneven fields; the sink hides
sub-triangle terrain interpolation gaps between sampled corners. The
guard math above is what makes the spec's "no significant burying"
measurable rather than hopeful.

**Alternatives considered**:
- Base at max corner height — rejected: floating walls on the low side.
- Base at average corner height — rejected: floats on two corners.
- Per-corner skirt walls down to terrain — rejected: extra triangles and
  code for a case the D2 spread bound already excludes.

## D4: Building geometry and palette (FR-002, FR-003, FR-009)

**Decision**: Rectangular footprint ~6 × 5 m; 3.2 m per story; story
count 1 or 2 from a hash bit. Opaque flat-shaded triangles: 4 wall faces
(2 tris each, warm wood-plank browns with a lighter gable-end variant),
a pitched roof of 2 rectangular slopes (4 tris) plus 2 triangular gable
ends, a dark door quad (2 tris) on one long face, and window quads (2
tris each) only on two-story buildings. Total ≤ 20 triangles per
building. Roof color: dark slate-brown/red distinct from terrain and
foliage greens.

**Rationale**: A box + gabled prism is the minimal vertex set that still
reads unambiguously as "man-made wooden house" from cruising altitude
(FR-002, SC-002) — a tent/prism shape would read as rock or canopy.
≤ 20 tris/building means a full 4-building cluster (~80–100 tris with
paths) costs about the same as ~10 pine trees, so SC-005 is safe by
construction. Windows only on 2-story buildings add the "inhabited"
cue at negligible cost while honoring "keep it simple".

**Alternatives considered**:
- Prism-only houses — rejected: not readable as buildings (SC-002 risk).
- Full window grids on every face — rejected: triangle and visual noise
  for no gameplay value.
- Textures or imported art — rejected: forbidden by constitution
  principle II.

## D5: Path topology and geometry (FR-004)

**Decision**: Star topology per cluster: one straight dirt path per
building, from its door position to the cluster hub (the seed point near
the cluster center). Paths are 2.2 m wide flat strips, tessellated every
~8 m along the straight line; each tessellation vertex samples
`terrain_get_height()` and lifts the strip by +0.08 m; each segment is 2
triangles of packed-earth brown.

**Rationale**: A star guarantees every cluster is fully connected (via
the hub) with the simplest deterministic rule — no nearest-neighbor
sorting or graph logic, matching "keep it simple". The +0.08 m lift is
the same anti-z-fighting trick the existing flower patches use
(+0.15/+0.25 m), and per-vertex height sampling keeps paths glued to
rolling fields instead of clipping through them.

**Alternatives considered**:
- Nearest-neighbor chains between buildings — rejected: connectivity
  logic for marginal visual gain at flight altitude.
- Inter-cluster roads — rejected: explicitly out of scope in the spec
  assumptions for v1.
- Curved/spline paths — rejected: math cost, invisible benefit at
  cruising distance.
- Flat ribbon at one sampled height — rejected: clips into rises or
  floats over dips between buildings.

## D6: Performance, budget, and integration (FR-010, FR-011, SC-005)

**Decision**: A new module `src/village.h` / `src/village.c` exposing
`int village_generate_mesh(struct vec3 center_pos, struct tri3d *tris,
int max_tris)`, called once per frame from `app_render()` in `main.c`
as a new step between the river and scenery submissions (or directly
after scenery — same painter's-algorithm batch stream). Budget guards
mirror the flora generator: before emitting each building, check
`count + building_tris > max_tris` and skip the whole building
(never a partial one, satisfying FR-011); path segments use the same
guard. No heap allocation, no SDL calls, no global mutable state — a
pure function of `center_pos` and world constants.

**Rationale**: Follows the exact call/buffer protocol of
`terrain_generate_mesh` / `river_generate_mesh` / `scenery_generate_mesh`
(each writes into the caller's `static_tris` buffer, returns a count,
main.c submits via `render_draw_triangles()`), so the render pipeline
(flat shading, back-face culling, painter's depth sort,
`SDL_RenderGeometry()`) is reused untouched — constitution principle
III. Expected load: ≤ ~10 visible clusters × ~100 tris ≈ 1000 triangles
(~1.5% of the 65536-triangle buffer) and ~20 `terrain_get_height()`
calls per cluster — negligible against the terrain mesh's 10k+ samples,
keeping SC-005 comfortably within reach. A separate module keeps
`terrain.c` from accumulating a second responsibility (constitution
principle V).

**Alternatives considered**:
- Extend `scenery_generate_mesh()` with houses — rejected: `terrain.c`
  would own flora AND settlements; violates the one-concern-per-module
  rule.
- Pre-baked static village vertex arrays — rejected: the world is
  effectively infinite and scrolls; baking cannot cover it without
  memory growth.
- Sort-safe insertion into an existing batch — rejected: the render
  context already depth-sorts collected triangles before present;
  no per-batch coordination is needed.

## D7: Rendering and transparency

**Decision**: All village triangles are opaque (alpha = 1.0). Paths rely
on the +0.08 m lift for terrain separation, not transparency; buildings
are solid geometry.

**Rationale**: Opaque triangles avoid painter's-algorithm artifacts with
the semi-transparent river strip (alpha 0.85–0.92); the lift technique
is proven by the flower patches. The downstream depth sort in `render3d`
handles building-vs-terrain and building-vs-tree ordering exactly as it
does for flora today.

**Alternatives considered**:
- Semi-transparent paths — rejected: unnecessary sorting risk with the
  animated river; no visual benefit.
- Depth-buffer tricks — rejected: none exist in this software pipeline;
  painter's algorithm + lift suffices.
