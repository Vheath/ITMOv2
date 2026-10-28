#include "village.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* 96 m cluster lattice: 8x the flora cell, snapped like scenery */
#define VILLAGE_CELL_SIZE 96.0f
#define VILLAGE_CELL_RADIUS 3

/*
 * Integer hash of (ix, iz) in the same family as terrain's
 * hash_coord(), with the multiplies done in unsigned arithmetic.
 */
static inline uint32_t village_hash(int x, int z)
{
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;

	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

int village_generate_mesh(struct vec3 center_pos, struct tri3d *tris,
			  int max_tris)
{
	int count = 0;

	(void)tris;
	(void)max_tris;

	float step = VILLAGE_CELL_SIZE;
	int radius = VILLAGE_CELL_RADIUS;

	float snapped_x = floorf(center_pos.x / step) * step;
	float snapped_z = floorf(center_pos.z / step) * step;

	/*
	 * Same loop pattern as scenery_generate_mesh(): every 96 m
	 * lattice cell around the snapped position is hashed; cluster
	 * derivation and geometry arrive with the user stories.
	 */
	for (int gz = -radius; gz <= radius; gz++) {
		for (int gx = -radius; gx <= radius; gx++) {
			int ix = (int)(snapped_x / step) + gx;
			int iz = (int)(snapped_z / step) + gz;

			(void)village_hash(ix, iz);
		}
	}

	return count; /* empty world until US1 */
}
