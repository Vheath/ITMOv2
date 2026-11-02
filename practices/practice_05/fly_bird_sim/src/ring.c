#include "ring.h"
#include "terrain.h"
#include <math.h>

void ring_manager_init(struct ring_manager *rm)
{
	rm->count = MAX_RINGS;
	rm->combo = 0;
	rm->combo_timer = 0.0f;

	for (int i = 0; i < MAX_RINGS; i++) {
		float z = RING_START_Z + (float)i * RING_SPACING_Z;
		float canyon_x = sinf(z * 0.01f) * 35.0f;
		float terrain_h = terrain_get_height(canyon_x, z);

		rm->rings[i].pos =
			vec3_create(canyon_x, terrain_h + RING_BASE_ALTITUDE, z);
		rm->rings[i].radius = RING_DEFAULT_RADIUS;
		rm->rings[i].collected = false;
		rm->rings[i].spin = (float)i * 0.4f;
	}
}

void ring_manager_update(struct ring_manager *rm, struct vec3 player_pos,
			 float dt)
{
	if (rm->combo_timer > 0.0f) {
		rm->combo_timer -= dt;
		if (rm->combo_timer <= 0.0f)
			rm->combo = 0;
	}

	for (int i = 0; i < rm->count; i++) {
		rm->rings[i].spin += RING_SPIN_SPEED * dt;

		/* If far behind the player, respawn forward in the canyon */
		if (rm->rings[i].pos.z < player_pos.z - RING_DESPAWN_DISTANCE) {
			/* If player missed this ring, reset combo */
			if (!rm->rings[i].collected && rm->combo > 0)
				rm->combo = 0;

			float max_z = player_pos.z;
			for (int j = 0; j < rm->count; j++) {
				if (rm->rings[j].pos.z > max_z)
					max_z = rm->rings[j].pos.z;
			}
			float new_z = max_z + RING_SPACING_Z;
			float canyon_x = sinf(new_z * 0.01f) * 35.0f;
			float terrain_h = terrain_get_height(canyon_x, new_z);

			rm->rings[i].pos = vec3_create(
				canyon_x,
				terrain_h + 12.0f + (float)(i % 3) * 4.0f,
				new_z);
			rm->rings[i].collected = false;
		}
	}
}

int ring_manager_check_collision(struct ring_manager *rm,
				 struct vec3 player_pos, float radius)
{
	int score_gain = 0;

	for (int i = 0; i < rm->count; i++) {
		if (rm->rings[i].collected)
			continue;

		struct vec3 diff = vec3_sub(player_pos, rm->rings[i].pos);
		float dist = vec3_length(diff);

		if (dist < (rm->rings[i].radius + radius)) {
			rm->rings[i].collected = true;
			rm->combo++;
			rm->combo_timer = RING_COMBO_TIMEOUT;
			int multiplier = rm->combo;
			if (multiplier > RING_MAX_MULTIPLIER)
				multiplier = RING_MAX_MULTIPLIER;
			score_gain += RING_SCORE_BASE * multiplier;
		}
	}

	return score_gain;
}

int ring_generate_mesh(const struct ring_manager *rm, struct tri3d *tris,
		       int max_tris)
{
	int count = 0;
	struct color_rgb gold = {1.0f, 0.85f, 0.15f, 1.0f};

	for (int i = 0; i < rm->count; i++) {
		if (rm->rings[i].collected)
			continue;

		float r_inner = rm->rings[i].radius * 0.8f;
		float r_outer = rm->rings[i].radius;
		struct vec3 center = rm->rings[i].pos;

		for (int s = 0; s < RING_SEGMENTS; s++) {
			if (count + 4 > max_tris)
				return count;

			float a0 =
				((float)s / (float)RING_SEGMENTS) * 2.0f * M_PI;
			float a1 = (((float)s + 1.0f) / (float)RING_SEGMENTS) *
				   2.0f * M_PI;

			/* Ring in XY plane, facing flight path */
			struct vec3 p0_in = vec3_add(
				center, vec3_create(cosf(a0) * r_inner,
						    sinf(a0) * r_inner, 0.0f));
			struct vec3 p1_in = vec3_add(
				center, vec3_create(cosf(a1) * r_inner,
						    sinf(a1) * r_inner, 0.0f));
			struct vec3 p0_out = vec3_add(
				center, vec3_create(cosf(a0) * r_outer,
						    sinf(a0) * r_outer, 0.0f));
			struct vec3 p1_out = vec3_add(
				center, vec3_create(cosf(a1) * r_outer,
						    sinf(a1) * r_outer, 0.0f));

			/* Front face */
			tris[count].v[0] = p0_in;
			tris[count].v[1] = p1_in;
			tris[count].v[2] = p0_out;
			tris[count].color = gold;
			count++;

			tris[count].v[0] = p1_in;
			tris[count].v[1] = p1_out;
			tris[count].v[2] = p0_out;
			tris[count].color = gold;
			count++;

			/* Back face */
			tris[count].v[0] = p0_in;
			tris[count].v[1] = p0_out;
			tris[count].v[2] = p1_in;
			tris[count].color = gold;
			count++;

			tris[count].v[0] = p1_in;
			tris[count].v[1] = p0_out;
			tris[count].v[2] = p1_out;
			tris[count].color = gold;
			count++;
		}
	}

	return count;
}
