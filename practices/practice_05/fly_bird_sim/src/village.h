#ifndef VILLAGE_H
#define VILLAGE_H

#include "math3d.h"
#include "render3d.h"

/*
 * Procedural settlement scenery: loose clusters of 1-2 story wooden
 * buildings with connecting dirt paths, deterministically derived
 * from a 96 m integer lattice around the given world position.
 *
 * Pure function of center_pos and compile-time constants: no time
 * dependence, no heap allocation, no global mutable state, no SDL
 * calls (FR-007). Writes whole buildings/path segments only (FR-011)
 * and never exceeds max_tris.
 */
int village_generate_mesh(struct vec3 center_pos, struct tri3d *tris,
			  int max_tris);

#endif /* VILLAGE_H */
