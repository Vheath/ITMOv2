# Data Model: Village Buildings and Field Paths

**Feature**: specs/002-village-buildings-paths/spec.md
**Date**: 2026-10-28

The feature is stateless and procedural: entities below are **runtime
view objects** derived per frame from integer lattice hashes — nothing is
stored, loaded, or persisted. The world is a pure function of position
(FR-007). The concrete C representation follows kernel style: explicit
`struct` names, no typedefs for structs, fields in `snake_case`.

---

## Entity: Cluster (farmstead)

The placement unit: one small group of buildings around a field,
together with its paths. Deterministically derived from one lattice
cell.

**Derived fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| seed cell (ix, iz) | integer lattice, 96 m cells | Snapped from the bird's position each frame; same cell always yields the same cluster (FR-007) |
| cell hash h | 32-bit integer hash of (ix, iz) | Single hash supplies all cluster randomness (size, offsets, rotations, stories, palette) |
| hub | ground point (x, z) near cell center | Path convergence point; must pass the placement volume checks of D2 (FR-006) |
| buildings | array of 2–4 Building | Count from hash bits; each building individually validated |
| valid | boolean | A cluster exists in a cell only if the hash threshold passes (10–15% of cells) AND at least 2 buildings survive validation; otherwise the cell is empty wilderness |

**Relationships**: owns 2–4 Buildings; owns 1 Path per Building.

**Validation rules** (from spec FRs):
- FR-001: cluster exists only in valley-meadow cells (altitude band
  ~5–31 m at all sampled corners).
- FR-006: no cluster geometry inside the canyon/river corridor (40 m
  margin to the sine centerline), on steep slopes (corner spread
  ≤ 1.5 m), or above the snow line.
- Density bound (edge case): the hash threshold caps cluster frequency —
  wilderness with occasional farmsteads, never towns.

## Entity: Building

One small wooden structure of 1 or 2 stories. Exists only as emitted
triangles; conceptually:

**Derived fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| center (x, z) | world ground position | Hub + hash-derived offset (±20 m); must pass D2 checks |
| base_y | float | min(4 corner terrain heights) − 0.2 m sink; guarantees no floating corner and bounded burial (FR-005, SC-003) |
| footprint | ~6 m × 5 m half-extents | Fixed range with small hash-derived variation |
| rotation | yaw angle | Hash-derived; orients the door face toward the path |
| stories | 1 or 2 | Hash bit; varies within and across clusters (FR-003) |
| palette variant | wood tone pair + roof color | Hash-selected from a fixed low-poly palette (FR-002, FR-009) |
| has_windows | boolean | True only when stories == 2 (D4, triangle economy) |
| door position | ground point on one long face | Start endpoint of the building's Path |

**Relationships**: belongs to exactly one Cluster; source endpoint of
exactly one Path.

**Validation rules**:
- FR-005: base_y anchoring rule above; a Building failing any D2 check
  is skipped whole (never rendered partially — FR-011).
- FR-008: carries no gameplay semantics — no collision, no score, no
  audio hooks exist in the entity.

## Entity: Path

One visible dirt strip linking a Building's door to its Cluster hub.

**Derived fields**:

| Field | Type/Domain | Derivation & validation |
|-------|-------------|-------------------------|
| endpoints | building door point, cluster hub point | Both within the cluster's validated placement volume |
| width | 2.2 m | Fixed |
| course | straight line, tessellated every ~8 m | Each interior vertex samples terrain height + 0.08 m lift (D5) |
| triangles | 2 per tessellation segment | Packed-earth brown, opaque (D7) |

**Relationships**: belongs to exactly one Cluster; connects exactly one
Building to the Cluster hub (star topology — every building reaches every
other via the hub, so the cluster is fully connected per FR-004).

**Validation rules**:
- FR-004: every Building in a surviving cluster has a Path; no unconnected
  buildings (edge case covered).
- FR-006: no path vertex inside the corridor margin.

---

## State transitions

None. There is no mutable state: `village_generate_mesh()` is a pure
function of `center_pos` plus compile-time constants; the same inputs
always produce identical output triangles (FR-007). No heap allocation,
no globals — cleanup/error paths of the kernel style reduce to the
caller-owned triangle buffer and its count return.

## Hash-bit budget

One 32-bit cell hash feeds: cluster existence (~7 bits), building count
(~2 bits), per-building offsets/rotation/stories/palette (~4–6 bits
each, reused at different shifts, mirroring how the flora generator
extracts `flora_type`, `scale`, and color from one hash). The derivation
table lives with the implementation; the invariant that matters to the
spec is: **hash(cell) → identical cluster, forever**.
