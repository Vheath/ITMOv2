#include "bird.h"
#include <math.h>

void bird_init(struct bird *b)
{
	b->pos = vec3_create(BIRD_SPAWN_X, BIRD_SPAWN_Y, BIRD_SPAWN_Z);
	b->vel = vec3_create(0.0f, 0.0f, 0.0f);
	b->yaw = 0.0f;
	b->pitch = 0.0f;
	b->roll = 0.0f;
	b->target_pitch = 0.0f;
	b->target_roll = 0.0f;
	b->speed = BIRD_BASE_SPEED;
	b->flap_phase = 0.0f;
	b->flap_speed = BIRD_BASE_FLAP_SPEED;
	b->is_gliding = false;
	b->boost_timer = 0.0f;
	b->score = 0.0f;
}

void bird_update(struct bird *b, float dt, float input_pitch, float input_roll,
		 bool boost)
{
	if (boost && b->boost_timer <= 0.0f)
		b->boost_timer = BIRD_BOOST_DURATION;

	if (b->boost_timer > 0.0f) {
		b->boost_timer -= dt;
		b->speed = BIRD_BOOST_SPEED;
		b->flap_speed = BIRD_BOOST_FLAP_SPEED;
	} else {
		/* Gliding aerodynamics: dive increases speed, climb decreases */
		float dive_accel = sinf(-b->pitch) * BIRD_DIVE_ACCELERATION;
		b->speed += dive_accel * dt;
		b->speed = clampf(b->speed, BIRD_MIN_SPEED, BIRD_MAX_GLIDE_SPEED);
		b->flap_speed = 5.0f + (b->speed - BIRD_MIN_SPEED) * 0.25f;
	}

	/* Flap animation cycle */
	b->flap_phase += b->flap_speed * dt;
	if (b->flap_phase > 2.0f * M_PI)
		b->flap_phase -= 2.0f * M_PI;

	/* Turn rates */
	b->target_roll = input_roll * deg_to_rad(BIRD_MAX_ROLL_DEG);
	b->target_pitch = input_pitch * deg_to_rad(BIRD_MAX_PITCH_DEG);

	b->roll += (b->target_roll - b->roll) * BIRD_ROLL_SPEED * dt;
	b->pitch += (b->target_pitch - b->pitch) * BIRD_PITCH_SPEED * dt;

	/* Banking causes yaw turn */
	float turn_rate = sinf(b->roll) * BIRD_BANK_TURN_RATE;
	b->yaw += turn_rate * dt;

	/* Forward direction vector */
	struct vec3 fwd =
		vec3_create(sinf(b->yaw) * cosf(b->pitch), -sinf(b->pitch),
			    cosf(b->yaw) * cosf(b->pitch));

	b->vel = vec3_scale(fwd, b->speed);
	b->pos = vec3_add(b->pos, vec3_scale(b->vel, dt));
}

static void add_tri(struct tri3d *tris, int *count, int max_tris,
		    struct vec3 p0, struct vec3 p1, struct vec3 p2,
		    struct color_rgb c)
{
	if (*count >= max_tris)
		return;
	tris[*count].v[0] = p0;
	tris[*count].v[1] = p1;
	tris[*count].v[2] = p2;
	tris[*count].color = c;
	(*count)++;
}

/*
 * Procedural low-poly eagle / falcon bird mesh generator:
 * Featuring sculpted curved beak, dark eye brows, raptor eyes,
 * multi-layered aerodynamic fuselage, multi-segmented articulated wings,
 * primary wingtip feathers, and a fanned tail with layered markings.
 */
int bird_generate_mesh(const struct bird *b, struct tri3d *tris, int max_tris)
{
	int count = 0;
	float flap_angle = sinf(b->flap_phase) * 0.45f;
	float wing_tip_lag = sinf(b->flap_phase - 0.4f) * 0.55f;

	/* Palette: Majestic Golden Eagle */
	struct color_rgb col_plumage = {0.20f, 0.14f, 0.09f, 1.0f};
	struct color_rgb col_plumage_light = {0.28f, 0.20f, 0.13f, 1.0f};
	struct color_rgb col_chest = {0.72f, 0.62f, 0.48f, 1.0f};
	struct color_rgb col_nape_gold = {0.85f, 0.65f, 0.22f, 1.0f};
	struct color_rgb col_beak_base = {0.35f, 0.32f, 0.28f, 1.0f};
	struct color_rgb col_beak_tip = {0.95f, 0.76f, 0.10f, 1.0f};
	struct color_rgb col_eye = {0.10f, 0.08f, 0.05f, 1.0f};
	struct color_rgb col_eye_ring = {0.95f, 0.85f, 0.20f, 1.0f};
	struct color_rgb col_wing_inner = {0.24f, 0.17f, 0.11f, 1.0f};
	struct color_rgb col_wing_covert = {0.33f, 0.23f, 0.14f, 1.0f};
	struct color_rgb col_primary_dark = {0.12f, 0.09f, 0.06f, 1.0f};
	struct color_rgb col_tail_base = {0.26f, 0.19f, 0.12f, 1.0f};
	struct color_rgb col_tail_band = {0.88f, 0.86f, 0.82f, 1.0f};

	/* 1. Sculpted Head, Beak & Eyes */
	struct vec3 beak_tip = vec3_create(0.0f, -0.15f, 2.5f);
	struct vec3 beak_bridge = vec3_create(0.0f, 0.15f, 2.2f);
	struct vec3 beak_chin = vec3_create(0.0f, -0.25f, 2.0f);
	struct vec3 cere_l = vec3_create(-0.20f, 0.08f, 1.85f);
	struct vec3 cere_r = vec3_create(0.20f, 0.08f, 1.85f);

	/* Beak hook */
	add_tri(tris, &count, max_tris, beak_tip, beak_bridge, cere_r,
		col_beak_tip);
	add_tri(tris, &count, max_tris, beak_tip, cere_l, beak_bridge,
		col_beak_tip);
	add_tri(tris, &count, max_tris, beak_tip, cere_r, beak_chin,
		col_beak_tip);
	add_tri(tris, &count, max_tris, beak_tip, beak_chin, cere_l,
		col_beak_tip);

	/* Brow and Crown */
	struct vec3 brow_l = vec3_create(-0.34f, 0.40f, 1.45f);
	struct vec3 brow_r = vec3_create(0.34f, 0.40f, 1.45f);
	struct vec3 crown = vec3_create(0.0f, 0.52f, 1.35f);
	struct vec3 throat = vec3_create(0.0f, -0.32f, 1.40f);

	/* Forehead */
	add_tri(tris, &count, max_tris, beak_bridge, crown, brow_r,
		col_beak_base);
	add_tri(tris, &count, max_tris, beak_bridge, brow_l, crown,
		col_beak_base);
	add_tri(tris, &count, max_tris, beak_bridge, cere_r, brow_r,
		col_beak_base);
	add_tri(tris, &count, max_tris, beak_bridge, brow_l, cere_l,
		col_beak_base);

	/* Raptor Eyes */
	struct vec3 eye_l = vec3_create(-0.35f, 0.22f, 1.55f);
	struct vec3 eye_r = vec3_create(0.35f, 0.22f, 1.55f);
	add_tri(tris, &count, max_tris, brow_r, eye_r, cere_r, col_eye_ring);
	add_tri(tris, &count, max_tris, brow_l, cere_l, eye_l, col_eye_ring);
	add_tri(tris, &count, max_tris, eye_r, throat, cere_r, col_eye);
	add_tri(tris, &count, max_tris, eye_l, cere_l, throat, col_eye);

	/* Golden Nape & Neck */
	struct vec3 nape = vec3_create(0.0f, 0.62f, 0.85f);
	struct vec3 neck_l = vec3_create(-0.48f, 0.15f, 0.85f);
	struct vec3 neck_r = vec3_create(0.48f, 0.15f, 0.85f);

	add_tri(tris, &count, max_tris, crown, nape, brow_r, col_nape_gold);
	add_tri(tris, &count, max_tris, crown, brow_l, nape, col_nape_gold);
	add_tri(tris, &count, max_tris, brow_r, nape, neck_r, col_nape_gold);
	add_tri(tris, &count, max_tris, brow_l, neck_l, nape, col_nape_gold);
	add_tri(tris, &count, max_tris, brow_r, neck_r, eye_r, col_plumage);
	add_tri(tris, &count, max_tris, brow_l, eye_l, neck_l, col_plumage);
	add_tri(tris, &count, max_tris, eye_r, neck_r, throat, col_chest);
	add_tri(tris, &count, max_tris, eye_l, throat, neck_l, col_chest);

	/* 2. Aerodynamic Fuselage (Body, Chest & Rump) */
	struct vec3 body_top = vec3_create(0.0f, 0.72f, -0.10f);
	struct vec3 body_belly = vec3_create(0.0f, -0.65f, -0.15f);
	struct vec3 flank_l = vec3_create(-0.68f, 0.05f, -0.10f);
	struct vec3 flank_r = vec3_create(0.68f, 0.05f, -0.10f);

	/* Neck to Mid Body */
	add_tri(tris, &count, max_tris, nape, body_top, neck_r,
		col_plumage_light);
	add_tri(tris, &count, max_tris, nape, neck_l, body_top,
		col_plumage_light);
	add_tri(tris, &count, max_tris, neck_r, body_top, flank_r, col_plumage);
	add_tri(tris, &count, max_tris, neck_l, flank_l, body_top, col_plumage);
	add_tri(tris, &count, max_tris, throat, neck_r, body_belly, col_chest);
	add_tri(tris, &count, max_tris, throat, body_belly, neck_l, col_chest);
	add_tri(tris, &count, max_tris, neck_r, flank_r, body_belly, col_chest);
	add_tri(tris, &count, max_tris, neck_l, body_belly, flank_l, col_chest);

	/* Torso to Rump */
	struct vec3 rump = vec3_create(0.0f, 0.20f, -1.90f);
	struct vec3 vent = vec3_create(0.0f, -0.18f, -1.80f);

	add_tri(tris, &count, max_tris, body_top, rump, flank_r, col_plumage);
	add_tri(tris, &count, max_tris, body_top, flank_l, rump, col_plumage);
	add_tri(tris, &count, max_tris, body_belly, flank_r, vent, col_chest);
	add_tri(tris, &count, max_tris, body_belly, vent, flank_l, col_chest);
	add_tri(tris, &count, max_tris, flank_r, rump, vent, col_plumage_light);
	add_tri(tris, &count, max_tris, flank_l, vent, rump, col_plumage_light);

	/* 3. Layered Fanned Tail Feathers */
	struct vec3 tail_c = vec3_create(0.0f, 0.0f, -3.50f);
	struct vec3 tail_ml = vec3_create(-0.55f, 0.04f, -3.40f);
	struct vec3 tail_mr = vec3_create(0.55f, 0.04f, -3.40f);
	struct vec3 tail_ol = vec3_create(-1.05f, 0.08f, -3.15f);
	struct vec3 tail_or = vec3_create(1.05f, 0.08f, -3.15f);

	/* Tail upper */
	add_tri(tris, &count, max_tris, rump, tail_mr, tail_c, col_tail_base);
	add_tri(tris, &count, max_tris, rump, tail_c, tail_ml, col_tail_base);
	add_tri(tris, &count, max_tris, rump, tail_or, tail_mr, col_tail_base);
	add_tri(tris, &count, max_tris, rump, tail_ml, tail_ol, col_tail_base);

	/* Tail terminal band accents */
	add_tri(tris, &count, max_tris, vent, tail_c, tail_mr, col_tail_band);
	add_tri(tris, &count, max_tris, vent, tail_ml, tail_c, col_tail_band);
	add_tri(tris, &count, max_tris, vent, tail_mr, tail_or, col_tail_band);
	add_tri(tris, &count, max_tris, vent, tail_ol, tail_ml, col_tail_band);

	/* 4. Articulated Multi-Segment Wings with Slotted Primaries */
	float flap_in = sinf(flap_angle) * 0.95f;
	float flap_out = sinf(flap_angle) * 1.85f + sinf(wing_tip_lag) * 0.65f;

	/* Left Wing Joints */
	struct vec3 wl_root = vec3_create(-0.52f, 0.28f, 0.50f);
	struct vec3 wl_root_aft = vec3_create(-0.56f, 0.22f, -0.65f);
	struct vec3 wl_elbow = vec3_create(-1.95f, 0.38f + flap_in, 0.35f);
	struct vec3 wl_elbow_aft =
		vec3_create(-1.85f, 0.26f + flap_in * 0.8f, -0.90f);
	struct vec3 wl_wrist = vec3_create(-3.65f, 0.45f + flap_out, -0.10f);
	struct vec3 wl_wrist_aft =
		vec3_create(-3.35f, 0.30f + flap_out * 0.8f, -1.25f);
	struct vec3 wl_tip1 =
		vec3_create(-4.95f, 0.55f + flap_out * 1.3f, -0.85f);
	struct vec3 wl_tip2 =
		vec3_create(-4.75f, 0.50f + flap_out * 1.25f, -1.45f);
	struct vec3 wl_tip3 =
		vec3_create(-4.40f, 0.44f + flap_out * 1.15f, -1.95f);

	/* Left Inner Wing (Double-Sided) */
	add_tri(tris, &count, max_tris, wl_root, wl_elbow, wl_elbow_aft,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wl_root, wl_elbow_aft, wl_elbow,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wl_root, wl_elbow_aft, wl_root_aft,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wl_root, wl_root_aft, wl_elbow_aft,
		col_wing_covert);

	/* Left Mid Wing */
	add_tri(tris, &count, max_tris, wl_elbow, wl_wrist, wl_wrist_aft,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wl_elbow, wl_wrist_aft, wl_wrist,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wl_elbow, wl_wrist_aft, wl_elbow_aft,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wl_elbow, wl_elbow_aft, wl_wrist_aft,
		col_wing_inner);

	/* Left Primary Feather Fingers */
	add_tri(tris, &count, max_tris, wl_wrist, wl_tip1, wl_wrist_aft,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wl_wrist, wl_wrist_aft, wl_tip1,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wl_wrist_aft, wl_tip1, wl_tip2,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wl_wrist_aft, wl_tip2, wl_tip1,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wl_wrist_aft, wl_tip2, wl_tip3,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wl_wrist_aft, wl_tip3, wl_tip2,
		col_primary_dark);

	/* Right Wing Joints */
	struct vec3 wr_root = vec3_create(0.52f, 0.28f, 0.50f);
	struct vec3 wr_root_aft = vec3_create(0.56f, 0.22f, -0.65f);
	struct vec3 wr_elbow = vec3_create(1.95f, 0.38f + flap_in, 0.35f);
	struct vec3 wr_elbow_aft =
		vec3_create(1.85f, 0.26f + flap_in * 0.8f, -0.90f);
	struct vec3 wr_wrist = vec3_create(3.65f, 0.45f + flap_out, -0.10f);
	struct vec3 wr_wrist_aft =
		vec3_create(3.35f, 0.30f + flap_out * 0.8f, -1.25f);
	struct vec3 wr_tip1 =
		vec3_create(4.95f, 0.55f + flap_out * 1.3f, -0.85f);
	struct vec3 wr_tip2 =
		vec3_create(4.75f, 0.50f + flap_out * 1.25f, -1.45f);
	struct vec3 wr_tip3 =
		vec3_create(4.40f, 0.44f + flap_out * 1.15f, -1.95f);

	/* Right Inner Wing */
	add_tri(tris, &count, max_tris, wr_root, wr_elbow_aft, wr_elbow,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wr_root, wr_elbow, wr_elbow_aft,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wr_root, wr_root_aft, wr_elbow_aft,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wr_root, wr_elbow_aft, wr_root_aft,
		col_wing_covert);

	/* Right Mid Wing */
	add_tri(tris, &count, max_tris, wr_elbow, wr_wrist_aft, wr_wrist,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wr_elbow, wr_wrist, wr_wrist_aft,
		col_wing_covert);
	add_tri(tris, &count, max_tris, wr_elbow, wr_elbow_aft, wr_wrist_aft,
		col_wing_inner);
	add_tri(tris, &count, max_tris, wr_elbow, wr_wrist_aft, wr_elbow_aft,
		col_wing_inner);

	/* Right Primary Feather Fingers */
	add_tri(tris, &count, max_tris, wr_wrist, wr_wrist_aft, wr_tip1,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wr_wrist, wr_tip1, wr_wrist_aft,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wr_wrist_aft, wr_tip2, wr_tip1,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wr_wrist_aft, wr_tip1, wr_tip2,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wr_wrist_aft, wr_tip3, wr_tip2,
		col_primary_dark);
	add_tri(tris, &count, max_tris, wr_wrist_aft, wr_tip2, wr_tip3,
		col_primary_dark);

	return count;
}
