#ifndef RING_H
#define RING_H

#include "math3d.h"
#include "render3d.h"

#define MAX_RINGS 40
#define RING_SEGMENTS 12

#define RING_START_Z 80.0f
#define RING_SPACING_Z 65.0f
#define RING_DEFAULT_RADIUS 4.5f
#define RING_BASE_ALTITUDE 14.0f
#define RING_SPIN_SPEED 1.8f
#define RING_DESPAWN_DISTANCE 50.0f
#define RING_SCORE_BASE 100
#define RING_MAX_MULTIPLIER 8
#define RING_COMBO_TIMEOUT 5.0f

struct ring {
	struct vec3 pos;
	float radius;
	bool collected;
	float spin;
};

struct ring_manager {
	struct ring rings[MAX_RINGS];
	int count;
	int combo;
	float combo_timer;
};

void ring_manager_init(struct ring_manager *rm);
void ring_manager_update(struct ring_manager *rm, struct vec3 player_pos,
			 float dt);
int ring_manager_check_collision(struct ring_manager *rm,
				 struct vec3 player_pos, float radius);
int ring_generate_mesh(const struct ring_manager *rm, struct tri3d *tris,
		       int max_tris);

#endif /* RING_H */
