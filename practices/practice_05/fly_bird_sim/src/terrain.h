#ifndef TERRAIN_H
#define TERRAIN_H

#include "math3d.h"
#include "render3d.h"

#define CHUNK_GRID_SIZE 72
#define CHUNK_STEP 10.0f
#define TERRAIN_MAX_TRIS (CHUNK_GRID_SIZE * CHUNK_GRID_SIZE * 2)

struct terrain {
	float center_x;
	float center_z;
};

void terrain_init(struct terrain *t);
float terrain_get_height(float x, float z);
int terrain_generate_mesh(struct terrain *t, struct vec3 center_pos,
			  struct tri3d *tris, int max_tris);
int scenery_generate_mesh(struct vec3 center_pos, float time_sec,
			  struct tri3d *tris, int max_tris);
int river_generate_mesh(struct vec3 center_pos, float time_sec,
			struct tri3d *tris, int max_tris);

#endif /* TERRAIN_H */
