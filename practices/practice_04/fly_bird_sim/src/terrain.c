#include "terrain.h"
#include <math.h>

void terrain_init(struct terrain *t)
{
	t->center_x = 0.0f;
	t->center_z = 0.0f;
}

/*
 * Layered multi-octave pseudo-procedural alpine mountain and canyon generation
 */
float terrain_get_height(float x, float z)
{
	/* Alpine ridges */
	float h1 = sinf(x * 0.015f) * cosf(z * 0.015f) * 28.0f;
	float h2 = sinf(x * 0.035f + 1.2f) * sinf(z * 0.030f + 0.8f) * 14.0f;
	float h3 = cosf(x * 0.08f) * cosf(z * 0.075f) * 6.0f;

	/* Deep canyon corridor running along Z with gentle curve */
	float canyon_center = sinf(z * 0.01f) * 35.0f;
	float dist_to_canyon = fabsf(x - canyon_center);
	float canyon_factor = 1.0f;

	if (dist_to_canyon < 30.0f) {
		float t = dist_to_canyon / 30.0f;
		canyon_factor = t * t;
	}

	float mountain_peaks = (h1 + h2 + h3) * 1.5f;

	/* Peak sharpening */
	if (mountain_peaks > 15.0f)
		mountain_peaks += (mountain_peaks - 15.0f) * 0.8f;

	float base_height = 8.0f;
	float total = base_height + mountain_peaks * canyon_factor;

	if (total < 0.0f)
		total = 0.0f;

	return total;
}

static struct color_rgb get_terrain_color(float x, float z, float h)
{
	/* Multi-frequency terrain variation hash for organic micro-biome
	 * mottling */
	float noise = sinf(x * 0.08f) * cosf(z * 0.08f) * 0.5f +
		      sinf(x * 0.22f + z * 0.17f) * 0.5f;

	/* Alpine biomes: valley green -> rock slate -> snow caps */
	if (h > 42.0f) {
		/* Glacial Snow cap with subtle blueish tint variation */
		float n = noise * 0.04f;
		return (struct color_rgb){0.92f + n, 0.94f + n, 0.98f, 1.0f};
	} else if (h > 31.0f) {
		/* Upper rock / transition to snow */
		float t = (h - 31.0f) / 11.0f;
		float rock_r = 0.50f + 0.42f * t;
		float rock_g = 0.52f + 0.42f * t;
		float rock_b = 0.55f + 0.43f * t;
		return (struct color_rgb){rock_r, rock_g, rock_b, 1.0f};
	} else if (h > 19.0f) {
		/* Mountain slate rock cliffs with warm stone striations */
		float stone = 0.42f + noise * 0.05f;
		return (struct color_rgb){stone + 0.03f, stone + 0.02f, stone,
					  1.0f};
	} else if (h > 12.0f) {
		/* Upper alpine pasture / subalpine meadow - rich vibrant lime
		 * green */
		float t = (h - 12.0f) / 7.0f;
		float g_var = noise * 0.05f;
		float r = (0.24f * (1.0f - t) + 0.42f * t) + g_var;
		float g = (0.58f * (1.0f - t) + 0.45f * t) + g_var;
		float b = (0.20f * (1.0f - t) + 0.38f * t);
		return (struct color_rgb){r, g, b, 1.0f};
	} else if (h > 5.0f) {
		/* Lush valley floor & emerald pine meadows - lush, deep vibrant
		 * green */
		float g_var = noise * 0.07f;
		float r = 0.15f + g_var * 0.4f;
		float g = 0.62f + g_var;
		float b = 0.22f + g_var * 0.3f;
		return (struct color_rgb){r, g, b, 1.0f};
	} else {
		/* River canyon bed - polished dark wet riverbed stones & silt
		 */
		float w_mix = (h / 5.0f);
		if (w_mix < 0.0f)
			w_mix = 0.0f;
		float r = 0.22f * w_mix + 0.12f * (1.0f - w_mix);
		float g = 0.38f * w_mix + 0.20f * (1.0f - w_mix);
		float b = 0.25f * w_mix + 0.24f * (1.0f - w_mix);
		return (struct color_rgb){r, g, b, 1.0f};
	}
}

int terrain_generate_mesh(struct terrain *t, struct vec3 center_pos,
			  struct tri3d *tris, int max_tris)
{
	(void)t;
	int count = 0;

	/*
	 * Continuous infinite ground coverage centered on the bird:
	 * Near detailed grid: 64x64 cells at 10m step = 640m span (-320m to
	 * +320m behind & ahead) Mid-distance grid: 48x48 cells at 40m step =
	 * 1920m span (-960m to +960m) Far valley floor: 40x40 cells at 120m
	 * step = 4800m span (-2400m to +2400m)
	 */
	float step1 = 10.0f;
	int half1 = 32;

	float snap1_x = floorf(center_pos.x / step1) * step1;
	float snap1_z = floorf(center_pos.z / step1) * step1;

	/* 1. Near detailed terrain grid */
	for (int gz = -half1; gz < half1; gz++) {
		for (int gx = -half1; gx < half1; gx++) {
			if (count + 2 > max_tris)
				return count;

			float x0 = snap1_x + (float)gx * step1;
			float z0 = snap1_z + (float)gz * step1;
			float x1 = x0 + step1;
			float z1 = z0 + step1;

			float y00 = terrain_get_height(x0, z0);
			float y10 = terrain_get_height(x1, z0);
			float y01 = terrain_get_height(x0, z1);
			float y11 = terrain_get_height(x1, z1);

			struct vec3 p00 = vec3_create(x0, y00, z0);
			struct vec3 p10 = vec3_create(x1, y10, z0);
			struct vec3 p01 = vec3_create(x0, y01, z1);
			struct vec3 p11 = vec3_create(x1, y11, z1);

			float avg_h1 = (y00 + y10 + y01) / 3.0f;
			tris[count].v[0] = p00;
			tris[count].v[1] = p10;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x0 + x1 + x0) / 3.0f, (z0 + z0 + z1) / 3.0f,
				avg_h1);
			count++;

			float avg_h2 = (y10 + y11 + y01) / 3.0f;
			tris[count].v[0] = p10;
			tris[count].v[1] = p11;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x1 + x1 + x0) / 3.0f, (z0 + z1 + z1) / 3.0f,
				avg_h2);
			count++;
		}
	}

	/* 2. Mid-distance outer terrain ring */
	float step2 = 40.0f;
	int half2 = 24;
	int bound2 = (int)((float)half1 * step1 / step2);

	float snap2_x = floorf(center_pos.x / step2) * step2;
	float snap2_z = floorf(center_pos.z / step2) * step2;

	for (int gz = -half2; gz < half2; gz++) {
		for (int gx = -half2; gx < half2; gx++) {
			if (gx >= -bound2 && gx < bound2 && gz >= -bound2 &&
			    gz < bound2)
				continue;

			if (count + 2 > max_tris)
				return count;

			float x0 = snap2_x + (float)gx * step2;
			float z0 = snap2_z + (float)gz * step2;
			float x1 = x0 + step2;
			float z1 = z0 + step2;

			float y00 = terrain_get_height(x0, z0);
			float y10 = terrain_get_height(x1, z0);
			float y01 = terrain_get_height(x0, z1);
			float y11 = terrain_get_height(x1, z1);

			struct vec3 p00 = vec3_create(x0, y00, z0);
			struct vec3 p10 = vec3_create(x1, y10, z0);
			struct vec3 p01 = vec3_create(x0, y01, z1);
			struct vec3 p11 = vec3_create(x1, y11, z1);

			float avg_h1 = (y00 + y10 + y01) / 3.0f;
			tris[count].v[0] = p00;
			tris[count].v[1] = p10;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x0 + x1 + x0) / 3.0f, (z0 + z0 + z1) / 3.0f,
				avg_h1);
			count++;

			float avg_h2 = (y10 + y11 + y01) / 3.0f;
			tris[count].v[0] = p10;
			tris[count].v[1] = p11;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x1 + x1 + x0) / 3.0f, (z0 + z1 + z1) / 3.0f,
				avg_h2);
			count++;
		}
	}

	/* 3. Far valley floor ring out to 2500m */
	float step3 = 120.0f;
	int half3 = 20;
	int bound3 = (int)((float)half2 * step2 / step3);

	float snap3_x = floorf(center_pos.x / step3) * step3;
	float snap3_z = floorf(center_pos.z / step3) * step3;

	for (int gz = -half3; gz < half3; gz++) {
		for (int gx = -half3; gx < half3; gx++) {
			if (gx >= -bound3 && gx < bound3 && gz >= -bound3 &&
			    gz < bound3)
				continue;

			if (count + 2 > max_tris)
				return count;

			float x0 = snap3_x + (float)gx * step3;
			float z0 = snap3_z + (float)gz * step3;
			float x1 = x0 + step3;
			float z1 = z0 + step3;

			float y00 = terrain_get_height(x0, z0);
			float y10 = terrain_get_height(x1, z0);
			float y01 = terrain_get_height(x0, z1);
			float y11 = terrain_get_height(x1, z1);

			struct vec3 p00 = vec3_create(x0, y00, z0);
			struct vec3 p10 = vec3_create(x1, y10, z0);
			struct vec3 p01 = vec3_create(x0, y01, z1);
			struct vec3 p11 = vec3_create(x1, y11, z1);

			float avg_h1 = (y00 + y10 + y01) / 3.0f;
			tris[count].v[0] = p00;
			tris[count].v[1] = p10;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x0 + x1 + x0) / 3.0f, (z0 + z0 + z1) / 3.0f,
				avg_h1);
			count++;

			float avg_h2 = (y10 + y11 + y01) / 3.0f;
			tris[count].v[0] = p10;
			tris[count].v[1] = p11;
			tris[count].v[2] = p01;
			tris[count].color = get_terrain_color(
				(x1 + x1 + x0) / 3.0f, (z0 + z1 + z1) / 3.0f,
				avg_h2);
			count++;
		}
	}

	return count;
}

static inline uint32_t hash_coord(int x, int z)
{
	uint32_t h = (uint32_t)(x * 374761393 + z * 668265263);
	h = (h ^ (h >> 13)) * 1274126177;
	return h ^ (h >> 16);
}

static void add_scene_tri(struct tri3d *tris, int *count, int max_tris,
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
 * Generates low-poly pine trees along green slopes, alpine bushes, flower
 * patches, wildlife (soaring bird flocks), and drifting high-altitude clouds.
 */
int scenery_generate_mesh(struct vec3 center_pos, float time_sec,
			  struct tri3d *tris, int max_tris)
{
	int count = 0;
	float step = 12.0f;
	int radius = 22; /* 22 * 12m = ~264m radius flora distribution */

	float snapped_x = floorf(center_pos.x / step) * step;
	float snapped_z = floorf(center_pos.z / step) * step;

	struct color_rgb trunk_col = {0.32f, 0.22f, 0.14f, 1.0f};
	struct color_rgb pine_emerald = {0.08f, 0.38f, 0.14f, 1.0f};
	struct color_rgb pine_vibrant = {0.14f, 0.48f, 0.18f, 1.0f};
	struct color_rgb pine_golden = {0.42f, 0.52f, 0.16f,
					1.0f}; /* Larch / autumn accent */
	struct color_rgb bush_lime = {0.26f, 0.65f, 0.18f, 1.0f};
	struct color_rgb flower_yellow = {0.96f, 0.88f, 0.20f, 1.0f};
	struct color_rgb flower_red = {0.95f, 0.28f, 0.32f, 1.0f};
	struct color_rgb rock_moss = {0.32f, 0.42f, 0.30f, 1.0f};
	struct color_rgb cloud_col = {0.96f, 0.97f, 1.0f, 0.90f};

	/* 1. Procedural Alpine Flora: Dense Conifers, Golden Larches, Alpine
	 * Bushes, Wildflowers */
	for (int gz = -radius; gz <= radius; gz++) {
		for (int gx = -radius; gx <= radius; gx++) {
			if (count + 48 > max_tris)
				break;

			int ix = (int)((snapped_x / step) + gx);
			int iz = (int)((snapped_z / step) + gz);
			uint32_t h = hash_coord(ix, iz);

			/* 68% probability of flora per cell */
			if ((h % 100) > 68)
				continue;

			float ox = ((float)(h % 100) / 100.0f - 0.5f) * 6.0f;
			float oz = (((float)((h >> 8) % 100) / 100.0f) - 0.5f) *
				   6.0f;
			float tx = (float)ix * step + ox;
			float tz = (float)iz * step + oz;

			float ty = terrain_get_height(tx, tz);
			/* Only plant in vegetated biome altitude (5m to 31m) */
			if (ty < 5.0f || ty > 31.0f)
				continue;

			/* Keep clear bird flight canyon corridor */
			float canyon_x = sinf(tz * 0.01f) * 35.0f;
			float dist_to_canyon = fabsf(tx - canyon_x);
			if (dist_to_canyon < 12.0f)
				continue;

			int flora_type = (h >> 12) % 10;

			if (flora_type <= 5) {
				/* Tall Conifer / Pine Tree (Emerald or Golden
				 * Larch) */
				bool is_larch = ((h >> 16) % 10) == 0;
				struct color_rgb p_dark =
					is_larch ? pine_golden : pine_emerald;
				struct color_rgb p_light =
					is_larch ? flower_yellow : pine_vibrant;

				float scale =
					0.75f +
					((float)((h >> 18) % 100) / 100.0f) *
						0.7f;
				float th = 4.2f * scale;
				float tw = 1.35f * scale;

				/* Trunk */
				struct vec3 b0 =
					vec3_create(tx - 0.28f, ty, tz - 0.28f);
				struct vec3 b1 =
					vec3_create(tx + 0.28f, ty, tz - 0.28f);
				struct vec3 b2 =
					vec3_create(tx + 0.28f, ty, tz + 0.28f);
				struct vec3 b3 =
					vec3_create(tx - 0.28f, ty, tz + 0.28f);
				struct vec3 t_mid =
					vec3_create(tx, ty + th * 0.35f, tz);

				add_scene_tri(tris, &count, max_tris, b0, b1,
					      t_mid, trunk_col);
				add_scene_tri(tris, &count, max_tris, b1, b2,
					      t_mid, trunk_col);
				add_scene_tri(tris, &count, max_tris, b2, b3,
					      t_mid, trunk_col);
				add_scene_tri(tris, &count, max_tris, b3, b0,
					      t_mid, trunk_col);

				/* Pine cone pyramid foliage layer 1 */
				struct vec3 f0 = vec3_create(
					tx - tw, ty + th * 0.25f, tz - tw);
				struct vec3 f1 = vec3_create(
					tx + tw, ty + th * 0.25f, tz - tw);
				struct vec3 f2 = vec3_create(
					tx + tw, ty + th * 0.25f, tz + tw);
				struct vec3 f3 = vec3_create(
					tx - tw, ty + th * 0.25f, tz + tw);
				struct vec3 f_tip1 =
					vec3_create(tx, ty + th * 0.85f, tz);

				add_scene_tri(tris, &count, max_tris, f0, f1,
					      f_tip1, p_dark);
				add_scene_tri(tris, &count, max_tris, f1, f2,
					      f_tip1, p_light);
				add_scene_tri(tris, &count, max_tris, f2, f3,
					      f_tip1, p_dark);
				add_scene_tri(tris, &count, max_tris, f3, f0,
					      f_tip1, p_light);

				/* Pine foliage layer 2 (top) */
				float tw2 = tw * 0.65f;
				struct vec3 g0 = vec3_create(
					tx - tw2, ty + th * 0.65f, tz - tw2);
				struct vec3 g1 = vec3_create(
					tx + tw2, ty + th * 0.65f, tz - tw2);
				struct vec3 g2 = vec3_create(
					tx + tw2, ty + th * 0.65f, tz + tw2);
				struct vec3 g3 = vec3_create(
					tx - tw2, ty + th * 0.65f, tz + tw2);
				struct vec3 f_tip2 =
					vec3_create(tx, ty + th * 1.45f, tz);

				add_scene_tri(tris, &count, max_tris, g0, g1,
					      f_tip2, p_dark);
				add_scene_tri(tris, &count, max_tris, g1, g2,
					      f_tip2, p_light);
				add_scene_tri(tris, &count, max_tris, g2, g3,
					      f_tip2, p_dark);
				add_scene_tri(tris, &count, max_tris, g3, g0,
					      f_tip2, p_light);
			} else if (flora_type <= 7) {
				/* Lush Alpine Berry Bush */
				float bw = 0.9f +
					   ((float)((h >> 16) % 100) / 100.0f) *
						   0.4f;
				float bh = 0.7f +
					   ((float)((h >> 20) % 100) / 100.0f) *
						   0.5f;

				struct vec3 u0 = vec3_create(tx - bw, ty, tz);
				struct vec3 u1 = vec3_create(tx + bw, ty, tz);
				struct vec3 u2 = vec3_create(tx, ty, tz - bw);
				struct vec3 u3 = vec3_create(tx, ty, tz + bw);
				struct vec3 utop = vec3_create(tx, ty + bh, tz);

				add_scene_tri(tris, &count, max_tris, u0, u3,
					      utop, bush_lime);
				add_scene_tri(tris, &count, max_tris, u3, u1,
					      utop, pine_vibrant);
				add_scene_tri(tris, &count, max_tris, u1, u2,
					      utop, bush_lime);
				add_scene_tri(tris, &count, max_tris, u2, u0,
					      utop, pine_vibrant);
			} else if (flora_type == 8) {
				/* Blooming Alpine Wildflower Patch (Edelweiss &
				 * Poppies) */
				struct color_rgb f_col = (((h >> 22) % 2) == 0)
								 ? flower_yellow
								 : flower_red;
				float f_rad = 0.8f;
				struct vec3 p0 = vec3_create(
					tx - f_rad, ty + 0.15f, tz - f_rad);
				struct vec3 p1 = vec3_create(
					tx + f_rad, ty + 0.15f, tz - f_rad);
				struct vec3 p2 = vec3_create(
					tx + f_rad, ty + 0.25f, tz + f_rad);
				struct vec3 p3 = vec3_create(
					tx - f_rad, ty + 0.25f, tz + f_rad);

				add_scene_tri(tris, &count, max_tris, p0, p1,
					      p2, f_col);
				add_scene_tri(tris, &count, max_tris, p0, p2,
					      p3, f_col);
			} else {
				/* Mossy Boulder / Alpine Granite Outcrop */
				float rw = 1.1f;
				float rh = 1.0f;
				struct vec3 r0 =
					vec3_create(tx - rw, ty, tz - rw);
				struct vec3 r1 =
					vec3_create(tx + rw, ty, tz - rw);
				struct vec3 r2 = vec3_create(tx + rw * 0.8f, ty,
							     tz + rw);
				struct vec3 r3 = vec3_create(tx - rw * 0.8f, ty,
							     tz + rw);
				struct vec3 rtop =
					vec3_create(tx + 0.2f, ty + rh, tz);

				add_scene_tri(tris, &count, max_tris, r0, r1,
					      rtop, rock_moss);
				add_scene_tri(tris, &count, max_tris, r1, r2,
					      rtop, rock_moss);
				add_scene_tri(tris, &count, max_tris, r2, r3,
					      rtop, rock_moss);
				add_scene_tri(tris, &count, max_tris, r3, r0,
					      rtop, rock_moss);
			}
		}
	}

	/* 2. Alpine Wildlife: Flocks of soaring wild birds / falcons */
	int flock_count = 6;
	struct color_rgb wild_bird_col = {0.18f, 0.14f, 0.10f, 1.0f};
	struct color_rgb wild_wing_tip = {0.85f, 0.78f, 0.65f, 1.0f};

	for (int f = 0; f < flock_count; f++) {
		if (count + 8 > max_tris)
			break;

		/* Flocks orbit in broad thermal thermoclines */
		float f_angle = time_sec * 0.45f + (float)f * 1.05f;
		float f_dist = 60.0f + (float)(f % 3) * 35.0f;
		float f_x = center_pos.x + sinf(f_angle) * f_dist;
		float f_z = center_pos.z + cosf(f_angle) * f_dist + 40.0f;
		float base_ground = terrain_get_height(f_x, f_z);
		float f_y = base_ground + 22.0f +
			    sinf(time_sec * 1.2f + (float)f) * 6.0f;

		/* Wild bird orientation & flap */
		float b_flap = sinf(time_sec * 8.0f + (float)f * 2.0f) * 0.8f;
		float fwd_x = cosf(f_angle);
		float fwd_z = -sinf(f_angle);
		float side_x = -fwd_z;
		float side_z = fwd_x;

		float b_len = 1.6f;
		float b_span = 2.4f;

		struct vec3 b_head = vec3_create(f_x + fwd_x * b_len, f_y,
						 f_z + fwd_z * b_len);
		struct vec3 b_tail =
			vec3_create(f_x - fwd_x * (b_len * 0.8f), f_y,
				    f_z - fwd_z * (b_len * 0.8f));
		struct vec3 w_left =
			vec3_create(f_x - side_x * b_span, f_y + b_flap,
				    f_z - side_z * b_span);
		struct vec3 w_right =
			vec3_create(f_x + side_x * b_span, f_y + b_flap,
				    f_z + side_z * b_span);

		/* Double-sided triangular silhouette wings */
		add_scene_tri(tris, &count, max_tris, b_head, w_left, b_tail,
			      wild_bird_col);
		add_scene_tri(tris, &count, max_tris, b_head, b_tail, w_left,
			      wild_bird_col);
		add_scene_tri(tris, &count, max_tris, b_head, b_tail, w_right,
			      wild_wing_tip);
		add_scene_tri(tris, &count, max_tris, b_head, w_right, b_tail,
			      wild_wing_tip);
	}

	/* 2. Drifting low-poly clouds at stratospheric altitude with vast
	 * coverage */
	float cloud_step = 160.0f;
	int c_rad = 16;
	float wind_speed = 6.0f;
	float wind_drift = time_sec * wind_speed;

	/* Anchor grid to camera position snapped to cell boundaries */
	float base_cx = floorf(center_pos.x / cloud_step) * cloud_step;
	float base_cz = floorf(center_pos.z / cloud_step) * cloud_step;

	for (int cz = -c_rad; cz <= c_rad; cz++) {
		for (int cx = -c_rad; cx <= c_rad; cx++) {
			if (count + 12 > max_tris)
				break;

			int cell_x =
				(int)(floorf(center_pos.x / cloud_step)) + cx;
			int cell_z =
				(int)(floorf(center_pos.z / cloud_step)) + cz;
			uint32_t chash = hash_coord(cell_x, cell_z);

			/* 50% cell density across the sky */
			if ((chash % 100) > 50)
				continue;

			float jitter_x =
				((float)(chash % 100) / 100.0f - 0.5f) * 60.0f;
			float jitter_z =
				(((float)((chash >> 8) % 100) / 100.0f) -
				 0.5f) *
				60.0f;

			/* Continuous smooth drift along Z axis */
			float cl_x =
				base_cx + (float)cx * cloud_step + jitter_x;
			float cl_z = base_cz + (float)cz * cloud_step +
				     jitter_z + fmodf(wind_drift, cloud_step);
			float cl_y =
				140.0f +
				((float)((chash >> 16) % 100) / 100.0f) * 50.0f;

			float scale =
				1.2f +
				((float)((chash >> 24) % 100) / 100.0f) * 1.4f;
			float cw = 35.0f * scale;
			float cl = 55.0f * scale;
			float ch = 9.0f * scale;

			struct vec3 c_top = vec3_create(cl_x, cl_y + ch, cl_z);
			struct vec3 c_bot = vec3_create(cl_x, cl_y - ch, cl_z);
			struct vec3 c_l = vec3_create(cl_x - cw, cl_y, cl_z);
			struct vec3 c_r = vec3_create(cl_x + cw, cl_y, cl_z);
			struct vec3 c_f = vec3_create(cl_x, cl_y, cl_z + cl);
			struct vec3 c_b = vec3_create(cl_x, cl_y, cl_z - cl);

			add_scene_tri(tris, &count, max_tris, c_f, c_r, c_top,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_r, c_b, c_top,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_b, c_l, c_top,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_l, c_f, c_top,
				      cloud_col);

			add_scene_tri(tris, &count, max_tris, c_r, c_f, c_bot,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_b, c_r, c_bot,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_l, c_b, c_bot,
				      cloud_col);
			add_scene_tri(tris, &count, max_tris, c_f, c_l, c_bot,
				      cloud_col);
		}
	}

	/*
	 * 3. Clean, subtle distant mountain ridge backdrop:
	 * A single, wide, natural panoramic mountain skyline.
	 * Gentle rolling alpine ridges rather than jagged spikes,
	 * blending harmoniously with the atmospheric sky.
	 */
	float r_horizon = 2200.0f;
	int horizon_segs = 20;

	struct color_rgb col_mtn_sun = {0.48f, 0.58f, 0.70f, 1.0f};
	struct color_rgb col_mtn_shade = {0.36f, 0.46f, 0.60f, 1.0f};
	struct color_rgb col_mtn_snow = {0.82f, 0.88f, 0.95f, 1.0f};

	for (int i = 0; i < horizon_segs; i++) {
		if (count + 6 > max_tris)
			break;

		float a0 = ((float)i / (float)horizon_segs) * 2.0f * M_PI;
		float a1 =
			(((float)i + 1.0f) / (float)horizon_segs) * 2.0f * M_PI;
		float a_mid = (a0 + a1) * 0.5f;

		/* Broad, gentle alpine ridge heights (wide soft silhouettes) */
		float peak_h = 160.0f + sinf(a_mid * 2.0f) * 65.0f +
			       cosf(a_mid * 4.0f) * 35.0f;
		float saddle_h0 = 70.0f + sinf(a0 * 2.0f) * 25.0f;
		float saddle_h1 = 70.0f + sinf(a1 * 2.0f) * 25.0f;

		float r_p = r_horizon + cosf(a_mid * 3.0f) * 80.0f;

		struct vec3 p_peak =
			vec3_create(center_pos.x + sinf(a_mid) * r_p, peak_h,
				    center_pos.z + cosf(a_mid) * r_p);
		struct vec3 b0 =
			vec3_create(center_pos.x + sinf(a0) * r_horizon, -20.0f,
				    center_pos.z + cosf(a0) * r_horizon);
		struct vec3 b1 =
			vec3_create(center_pos.x + sinf(a1) * r_horizon, -20.0f,
				    center_pos.z + cosf(a1) * r_horizon);
		struct vec3 s0 = vec3_create(b0.x, saddle_h0, b0.z);
		struct vec3 s1 = vec3_create(b1.x, saddle_h1, b1.z);

		add_scene_tri(tris, &count, max_tris, b0, p_peak, s0,
			      col_mtn_sun);
		add_scene_tri(tris, &count, max_tris, b0, b1, p_peak,
			      col_mtn_shade);
		add_scene_tri(tris, &count, max_tris, b1, s1, p_peak,
			      col_mtn_shade);

		/* Soft snow cap for the highest peaks */
		if (peak_h > 180.0f) {
			float sn_h = peak_h * 0.75f;
			struct vec3 sn0 = vec3_create(
				center_pos.x + sinf(a0) * (r_horizon - 30.0f),
				sn_h,
				center_pos.z + cosf(a0) * (r_horizon - 30.0f));
			struct vec3 sn1 = vec3_create(
				center_pos.x + sinf(a1) * (r_horizon - 30.0f),
				sn_h,
				center_pos.z + cosf(a1) * (r_horizon - 30.0f));
			add_scene_tri(tris, &count, max_tris, sn0, p_peak, sn1,
				      col_mtn_snow);
			add_scene_tri(tris, &count, max_tris, sn0, sn1, p_peak,
				      col_mtn_shade);
		}
	}

	/* 4. Low-poly golden sun disc in the sky */
	struct vec3 sun_disc_pos =
		vec3_create(center_pos.x + 1800.0f, center_pos.y + 1600.0f,
			    center_pos.z - 1400.0f);
	float sun_radius = 120.0f;
	int sun_segments = 14;
	struct color_rgb col_sun = {1.0f, 0.98f, 0.75f, 1.0f};

	for (int s = 0; s < sun_segments; s++) {
		if (count >= max_tris)
			break;
		float sa0 = ((float)s / (float)sun_segments) * 2.0f * M_PI;
		float sa1 =
			(((float)s + 1.0f) / (float)sun_segments) * 2.0f * M_PI;

		struct vec3 sp0 =
			vec3_create(sun_disc_pos.x + cosf(sa0) * sun_radius,
				    sun_disc_pos.y + sinf(sa0) * sun_radius,
				    sun_disc_pos.z);
		struct vec3 sp1 =
			vec3_create(sun_disc_pos.x + cosf(sa1) * sun_radius,
				    sun_disc_pos.y + sinf(sa1) * sun_radius,
				    sun_disc_pos.z);

		add_scene_tri(tris, &count, max_tris, sun_disc_pos, sp0, sp1,
			      col_sun);
	}

	return count;
}

/*
 * Generates an animated glacial river plane flowing through the canyon
 * corridor. Includes procedural undulating surface waves, white foam crests
 * along rapids, and specular sunlight highlights.
 */
int river_generate_mesh(struct vec3 center_pos, float time_sec,
			struct tri3d *tris, int max_tris)
{
	int count = 0;
	float step_z = 10.0f;
	int count_z = 120; /* 120 * 10m = 1200m river visible ahead */
	int strip_cols = 6;
	float river_width = 24.0f;
	float col_w = river_width / (float)strip_cols;

	float start_z = floorf((center_pos.z - 350.0f) / step_z) * step_z;

	struct color_rgb col_deep = {0.10f, 0.42f, 0.65f, 0.88f};
	struct color_rgb col_azure = {0.18f, 0.58f, 0.78f, 0.88f};
	struct color_rgb col_foam = {0.92f, 0.98f, 1.0f, 0.92f};
	struct color_rgb col_bank = {0.14f, 0.48f, 0.45f, 0.85f};

	for (int iz = 0; iz < count_z; iz++) {
		float z0 = start_z + (float)iz * step_z;
		float z1 = z0 + step_z;

		float canyon_x0 = sinf(z0 * 0.01f) * 35.0f;
		float canyon_x1 = sinf(z1 * 0.01f) * 35.0f;

		for (int ix = 0; ix < strip_cols; ix++) {
			if (count + 2 > max_tris)
				return count;

			float rel_x0 = -river_width * 0.5f + (float)ix * col_w;
			float rel_x1 = rel_x0 + col_w;

			float wx00 = canyon_x0 + rel_x0;
			float wx10 = canyon_x0 + rel_x1;
			float wx01 = canyon_x1 + rel_x0;
			float wx11 = canyon_x1 + rel_x1;

			/* Fast animated rapid waves flowing along -Z */
			float flow = time_sec * 8.0f;
			float wave00 =
				sinf(wx00 * 0.35f + z0 * 0.4f - flow) * 0.28f +
				cosf(wx00 * 0.6f - z0 * 0.25f - flow * 1.5f) *
					0.16f;
			float wave10 =
				sinf(wx10 * 0.35f + z0 * 0.4f - flow) * 0.28f +
				cosf(wx10 * 0.6f - z0 * 0.25f - flow * 1.5f) *
					0.16f;
			float wave01 =
				sinf(wx01 * 0.35f + z1 * 0.4f - flow) * 0.28f +
				cosf(wx01 * 0.6f - z1 * 0.25f - flow * 1.5f) *
					0.16f;
			float wave11 =
				sinf(wx11 * 0.35f + z1 * 0.4f - flow) * 0.28f +
				cosf(wx11 * 0.6f - z1 * 0.25f - flow * 1.5f) *
					0.16f;

			float base_water_h = 3.6f;

			/* Sample true terrain ground height at the water
			 * vertices */
			float g00 = terrain_get_height(wx00, z0);
			float g10 = terrain_get_height(wx10, z0);
			float g01 = terrain_get_height(wx01, z1);
			float g11 = terrain_get_height(wx11, z1);

			/*
			 * If this whole quad is higher than water level, the
			 * terrain banks rise above the stream; skip drawing
			 * water here so water never shows through the earth
			 * tiles.
			 */
			if (g00 > base_water_h + 0.35f &&
			    g10 > base_water_h + 0.35f &&
			    g01 > base_water_h + 0.35f &&
			    g11 > base_water_h + 0.35f)
				continue;

			/* Clamp water surface just slightly above riverbed to
			 * avoid Z-fighting */
			float w_y00 = base_water_h + wave00;
			float w_y10 = base_water_h + wave10;
			float w_y01 = base_water_h + wave01;
			float w_y11 = base_water_h + wave11;

			if (w_y00 < g00 + 0.15f)
				w_y00 = g00 + 0.15f;
			if (w_y10 < g10 + 0.15f)
				w_y10 = g10 + 0.15f;
			if (w_y01 < g01 + 0.15f)
				w_y01 = g01 + 0.15f;
			if (w_y11 < g11 + 0.15f)
				w_y11 = g11 + 0.15f;

			struct vec3 p00 = vec3_create(wx00, w_y00, z0);
			struct vec3 p10 = vec3_create(wx10, w_y10, z0);
			struct vec3 p01 = vec3_create(wx01, w_y01, z1);
			struct vec3 p11 = vec3_create(wx11, w_y11, z1);

			/* Rapid foam threshold on wave crests */
			float avg_wave1 = (wave00 + wave10 + wave01) / 3.0f;
			float avg_wave2 = (wave10 + wave11 + wave01) / 3.0f;

			bool is_bank = (ix == 0 || ix == strip_cols - 1);

			struct color_rgb c1 = col_deep;
			if (avg_wave1 > 0.25f)
				c1 = col_foam;
			else if (is_bank)
				c1 = col_bank;
			else if (avg_wave1 > 0.05f)
				c1 = col_azure;

			struct color_rgb c2 = col_deep;
			if (avg_wave2 > 0.25f)
				c2 = col_foam;
			else if (is_bank)
				c2 = col_bank;
			else if (avg_wave2 > 0.05f)
				c2 = col_azure;

			/* Triangle 1 */
			tris[count].v[0] = p00;
			tris[count].v[1] = p10;
			tris[count].v[2] = p01;
			tris[count].color = c1;
			count++;

			/* Triangle 2 */
			tris[count].v[0] = p10;
			tris[count].v[1] = p11;
			tris[count].v[2] = p01;
			tris[count].color = c2;
			count++;
		}
	}

	return count;
}
