#include "render3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct clip_vert {
	struct vec4 clip;
	struct vec3 world;
};

static int tri_depth_compare(const void *a, const void *b)
{
	const struct raster_tri *ta = (const struct raster_tri *)a;
	const struct raster_tri *tb = (const struct raster_tri *)b;

	if (tb->avg_depth > ta->avg_depth)
		return 1;
	else if (tb->avg_depth < ta->avg_depth)
		return -1;
	return 0;
}

static inline struct clip_vert clip_vert_lerp(struct clip_vert a,
					      struct clip_vert b, float t)
{
	struct clip_vert res;

	res.clip.x = a.clip.x + (b.clip.x - a.clip.x) * t;
	res.clip.y = a.clip.y + (b.clip.y - a.clip.y) * t;
	res.clip.z = a.clip.z + (b.clip.z - a.clip.z) * t;
	res.clip.w = a.clip.w + (b.clip.w - a.clip.w) * t;

	res.world.x = a.world.x + (b.world.x - a.world.x) * t;
	res.world.y = a.world.y + (b.world.y - a.world.y) * t;
	res.world.z = a.world.z + (b.world.z - a.world.z) * t;

	return res;
}

static void rasterize_clipped_tri(struct render_context *ctx,
				  const struct clip_vert v[3],
				  struct color_rgb src_color, float half_w,
				  float half_h)
{
	struct vec3 screen[3];

	if (ctx->tri_count >= ctx->tri_capacity)
		return;

	for (int j = 0; j < 3; j++) {
		float inv_w = 1.0f / v[j].clip.w;
		float ndc_x = v[j].clip.x * inv_w;
		float ndc_y = v[j].clip.y * inv_w;

		screen[j].x = (ndc_x + 1.0f) * half_w;
		screen[j].y = (1.0f - ndc_y) * half_h;
		screen[j].z = v[j].clip.w;
	}

	/* Backface culling in screen-space */
	float edge = (screen[1].x - screen[0].x) * (screen[2].y - screen[0].y) -
		     (screen[1].y - screen[0].y) * (screen[2].x - screen[0].x);
	if (edge <= 0.0f)
		return;

	/* Calculate flat face normal in world space */
	struct vec3 edge1 = vec3_sub(v[1].world, v[0].world);
	struct vec3 edge2 = vec3_sub(v[2].world, v[0].world);
	struct vec3 norm = vec3_normalize(vec3_cross(edge1, edge2));

	float ndotl = -vec3_dot(norm, ctx->sun_dir);
	if (ndotl < 0.0f)
		ndotl = 0.0f;

	float lit_r =
		src_color.r * (ctx->ambient_color.r + ctx->sun_color.r * ndotl);
	float lit_g =
		src_color.g * (ctx->ambient_color.g + ctx->sun_color.g * ndotl);
	float lit_b =
		src_color.b * (ctx->ambient_color.b + ctx->sun_color.b * ndotl);

	float avg_depth = (screen[0].z + screen[1].z + screen[2].z) / 3.0f;
	float min_depth = screen[0].z;
	if (screen[1].z < min_depth)
		min_depth = screen[1].z;
	if (screen[2].z < min_depth)
		min_depth = screen[2].z;

	/* Weighted depth sorting metric to stabilize interpenetrating terrain
	 * triangles */
	float sort_depth = min_depth * RENDER_DEPTH_MIN_WEIGHT +
			   avg_depth * RENDER_DEPTH_AVG_WEIGHT;

	float fog_factor =
		(avg_depth - ctx->fog_start) / (ctx->fog_end - ctx->fog_start);
	fog_factor = clampf(fog_factor, 0.0f, 1.0f);

	lit_r = lit_r * (1.0f - fog_factor) + ctx->fog_color.r * fog_factor;
	lit_g = lit_g * (1.0f - fog_factor) + ctx->fog_color.g * fog_factor;
	lit_b = lit_b * (1.0f - fog_factor) + ctx->fog_color.b * fog_factor;

	struct raster_tri *rt = &ctx->tri_pool[ctx->tri_count++];
	rt->p[0] = (SDL_FPoint){screen[0].x, screen[0].y};
	rt->p[1] = (SDL_FPoint){screen[1].x, screen[1].y};
	rt->p[2] = (SDL_FPoint){screen[2].x, screen[2].y};
	rt->avg_depth = sort_depth;
	rt->color = (SDL_FColor){clampf(lit_r, 0.0f, 1.0f),
				 clampf(lit_g, 0.0f, 1.0f),
				 clampf(lit_b, 0.0f, 1.0f), src_color.a};
}

int render_context_init(struct render_context *ctx, int capacity)
{
	if (capacity <= 0)
		capacity = MAX_RENDER_TRIANGLES;

	ctx->tri_pool = malloc(sizeof(struct raster_tri) * capacity);
	if (!ctx->tri_pool)
		return -1;

	ctx->tri_capacity = capacity;
	ctx->tri_count = 0;

	/* Default lighting & atmospheric fog */
	ctx->sun_dir = vec3_normalize(vec3_create(-0.5f, -0.8f, 0.4f));
	ctx->sun_color = (struct color_rgb){1.0f, 0.95f, 0.85f, 1.0f};
	ctx->ambient_color = (struct color_rgb){0.28f, 0.34f, 0.44f, 1.0f};
	ctx->fog_color = (struct color_rgb){0.70f, 0.80f, 0.92f, 1.0f};
	ctx->fog_start = RENDER_FOG_DEFAULT_START;
	ctx->fog_end = RENDER_FOG_DEFAULT_END;

	return 0;
}

void render_context_destroy(struct render_context *ctx)
{
	if (ctx->tri_pool) {
		free(ctx->tri_pool);
		ctx->tri_pool = NULL;
	}
	ctx->tri_capacity = 0;
	ctx->tri_count = 0;
}

void render_context_clear(struct render_context *ctx, int screen_w,
			  int screen_h)
{
	ctx->tri_count = 0;
	ctx->screen_w = screen_w;
	ctx->screen_h = screen_h;
}

void render_context_set_camera(struct render_context *ctx, struct vec3 eye,
			       struct vec3 target, struct vec3 up,
			       float fov_rad, float near_z, float far_z)
{
	float aspect = (float)ctx->screen_w / (float)ctx->screen_h;

	ctx->view = mat4_look_at(eye, target, up);
	ctx->proj = mat4_perspective(fov_rad, aspect, near_z, far_z);
	ctx->vp = mat4_multiply(&ctx->proj, &ctx->view);
}

void render_draw_triangles(struct render_context *ctx, const struct tri3d *tris,
			   int count, const struct mat4 *model)
{
	struct mat4 mvp;
	float half_w = ctx->screen_w * 0.5f;
	float half_h = ctx->screen_h * 0.5f;

	if (model)
		mvp = mat4_multiply(&ctx->vp, model);
	else
		mvp = ctx->vp;

	for (int i = 0; i < count; i++) {
		struct clip_vert in_v[3];
		int inside_count = 0;
		int outside_count = 0;
		int inside_idx[3];
		int outside_idx[3];

		if (ctx->tri_count >= ctx->tri_capacity)
			break;

		for (int j = 0; j < 3; j++) {
			struct vec3 p = tris[i].v[j];
			if (model)
				in_v[j].world = mat4_mul_point(model, p);
			else
				in_v[j].world = p;

			in_v[j].clip = mat4_mul_vec4(
				&mvp, (struct vec4){p.x, p.y, p.z, 1.0f});

			if (in_v[j].clip.w >= RENDER_NEAR_CLIP_W)
				inside_idx[inside_count++] = j;
			else
				outside_idx[outside_count++] = j;
		}

		/* Sutherland-Hodgman near-plane clipping */
		if (inside_count == 0) {
			/* Entire triangle behind near plane */
			continue;
		} else if (inside_count == 3) {
			/* Completely in front */
			rasterize_clipped_tri(ctx, in_v, tris[i].color, half_w,
					      half_h);
		} else if (inside_count == 1) {
			/* 1 inside, 2 outside -> form 1 new triangle */
			int in0 = inside_idx[0];
			int out0 = outside_idx[0];
			int out1 = outside_idx[1];

			float t0 = (RENDER_NEAR_CLIP_W - in_v[in0].clip.w) /
				   (in_v[out0].clip.w - in_v[in0].clip.w);
			float t1 = (RENDER_NEAR_CLIP_W - in_v[in0].clip.w) /
				   (in_v[out1].clip.w - in_v[in0].clip.w);

			struct clip_vert tri[3];
			tri[0] = in_v[in0];

			/* Preserve original winding order */
			if ((in0 + 1) % 3 == out0) {
				tri[1] = clip_vert_lerp(in_v[in0], in_v[out0],
							t0);
				tri[2] = clip_vert_lerp(in_v[in0], in_v[out1],
							t1);
			} else {
				tri[1] = clip_vert_lerp(in_v[in0], in_v[out1],
							t1);
				tri[2] = clip_vert_lerp(in_v[in0], in_v[out0],
							t0);
			}

			rasterize_clipped_tri(ctx, tri, tris[i].color, half_w,
					      half_h);
		} else if (inside_count == 2) {
			/* 2 inside, 1 outside -> split quad into 2 triangles */
			int in0 = inside_idx[0];
			int in1 = inside_idx[1];
			int out0 = outside_idx[0];

			/* Ensure in0 -> in1 matches winding direction */
			if ((in0 + 1) % 3 != in1) {
				int tmp = in0;
				in0 = in1;
				in1 = tmp;
			}

			float t0 = (RENDER_NEAR_CLIP_W - in_v[in0].clip.w) /
				   (in_v[out0].clip.w - in_v[in0].clip.w);
			float t1 = (RENDER_NEAR_CLIP_W - in_v[in1].clip.w) /
				   (in_v[out0].clip.w - in_v[in1].clip.w);

			struct clip_vert c0 =
				clip_vert_lerp(in_v[in0], in_v[out0], t0);
			struct clip_vert c1 =
				clip_vert_lerp(in_v[in1], in_v[out0], t1);

			struct clip_vert triA[3] = {in_v[in0], in_v[in1], c1};
			struct clip_vert triB[3] = {in_v[in0], c1, c0};

			rasterize_clipped_tri(ctx, triA, tris[i].color, half_w,
					      half_h);
			rasterize_clipped_tri(ctx, triB, tris[i].color, half_w,
					      half_h);
		}
	}
}

void render_draw_sky_gradient(SDL_Renderer *renderer, int width, int height)
{
	float w = (float)width;
	float h = (float)height;

	/*
	 * 4-stop vertical atmospheric sky gradient:
	 * Zenith: Deep alpine azure
	 * Upper Sky: Vivid cerulean
	 * Horizon: Soft glowing atmospheric mist
	 * Foothills: Distant mountain haze
	 */
	SDL_FColor col_zenith = {0.26f, 0.52f, 0.88f, 1.0f};
	SDL_FColor col_mid_sky = {0.42f, 0.68f, 0.94f, 1.0f};
	SDL_FColor col_horizon = {0.72f, 0.82f, 0.94f, 1.0f};
	SDL_FColor col_base = {0.60f, 0.72f, 0.85f, 1.0f};

	float y_mid = h * RENDER_SKY_MID_Y_RATIO;
	float y_hor = h * RENDER_SKY_HORIZON_Y_RATIO;

	SDL_Vertex v[18] = {
		/* Stop 1: Zenith to Mid Sky */
		{{0.0f, 0.0f}, col_zenith, {0.0f, 0.0f}},
		{{w, 0.0f}, col_zenith, {0.0f, 0.0f}},
		{{w, y_mid}, col_mid_sky, {0.0f, 0.0f}},

		{{0.0f, 0.0f}, col_zenith, {0.0f, 0.0f}},
		{{w, y_mid}, col_mid_sky, {0.0f, 0.0f}},
		{{0.0f, y_mid}, col_mid_sky, {0.0f, 0.0f}},

		/* Stop 2: Mid Sky to Horizon */
		{{0.0f, y_mid}, col_mid_sky, {0.0f, 0.0f}},
		{{w, y_mid}, col_mid_sky, {0.0f, 0.0f}},
		{{w, y_hor}, col_horizon, {0.0f, 0.0f}},

		{{0.0f, y_mid}, col_mid_sky, {0.0f, 0.0f}},
		{{w, y_hor}, col_horizon, {0.0f, 0.0f}},
		{{0.0f, y_hor}, col_horizon, {0.0f, 0.0f}},

		/* Stop 3: Horizon to Base */
		{{0.0f, y_hor}, col_horizon, {0.0f, 0.0f}},
		{{w, y_hor}, col_horizon, {0.0f, 0.0f}},
		{{w, h}, col_base, {0.0f, 0.0f}},

		{{0.0f, y_hor}, col_horizon, {0.0f, 0.0f}},
		{{w, h}, col_base, {0.0f, 0.0f}},
		{{0.0f, h}, col_base, {0.0f, 0.0f}},
	};

	SDL_RenderGeometry(renderer, NULL, v, 18, NULL, 0);
}

void render_present(struct render_context *ctx, SDL_Renderer *renderer)
{
	if (ctx->tri_count == 0)
		return;

	/* Painter's algorithm: sort from back to front */
	qsort(ctx->tri_pool, ctx->tri_count, sizeof(struct raster_tri),
	      tri_depth_compare);

	/* Batch into SDL_Vertex and draw */
	int batch_size = RENDER_BATCH_SIZE;
	SDL_Vertex *verts = malloc(sizeof(SDL_Vertex) * batch_size * 3);
	if (!verts)
		return;

	int offset = 0;
	while (offset < ctx->tri_count) {
		int count = ctx->tri_count - offset;
		if (count > batch_size)
			count = batch_size;

		for (int i = 0; i < count; i++) {
			const struct raster_tri *rt =
				&ctx->tri_pool[offset + i];
			for (int v = 0; v < 3; v++) {
				int idx = i * 3 + v;
				verts[idx].position = rt->p[v];
				verts[idx].color = rt->color;
				verts[idx].tex_coord = (SDL_FPoint){0.0f, 0.0f};
			}
		}

		SDL_RenderGeometry(renderer, NULL, verts, count * 3, NULL, 0);
		offset += count;
	}

	free(verts);
}
