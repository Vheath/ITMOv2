#ifndef BIRD_H
#define BIRD_H

#include "math3d.h"
#include "render3d.h"

#define BIRD_MAX_TRIS 192

#define BIRD_SPAWN_X 0.0f
#define BIRD_SPAWN_Y 45.0f
#define BIRD_SPAWN_Z 0.0f

#define BIRD_BASE_SPEED 22.0f
#define BIRD_BOOST_SPEED 36.0f
#define BIRD_MIN_SPEED 12.0f
#define BIRD_MAX_GLIDE_SPEED 32.0f
#define BIRD_DIVE_ACCELERATION 14.0f

#define BIRD_BASE_FLAP_SPEED 7.0f
#define BIRD_BOOST_FLAP_SPEED 13.0f
#define BIRD_BOOST_DURATION 1.8f
#define BIRD_RING_BOOST_DURATION 2.0f

#define BIRD_ROLL_SPEED 3.2f
#define BIRD_PITCH_SPEED 2.4f
#define BIRD_BANK_TURN_RATE 1.5f
#define BIRD_MAX_ROLL_DEG 45.0f
#define BIRD_MAX_PITCH_DEG 35.0f

#define BIRD_COLLISION_RADIUS 1.8f
#define BIRD_GROUND_CLEARANCE 0.8f

struct bird {
	struct vec3 pos;
	struct vec3 vel;

	float yaw;
	float pitch;
	float roll;

	float target_pitch;
	float target_roll;

	float speed;
	float flap_phase;
	float flap_speed;

	bool is_gliding;
	float boost_timer;
	float score;
};

void bird_init(struct bird *b);
void bird_update(struct bird *b, float dt, float input_pitch, float input_roll,
		 bool boost);
int bird_generate_mesh(const struct bird *b, struct tri3d *tris, int max_tris);

#endif /* BIRD_H */
