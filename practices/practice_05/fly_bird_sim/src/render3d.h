#ifndef RENDER3D_H
#define RENDER3D_H

#include "math3d.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

#define MAX_RENDER_TRIANGLES 131072
#define RENDER_BATCH_SIZE 2048
#define RENDER_NEAR_CLIP_W 0.1f
#define RENDER_DEPTH_MIN_WEIGHT 0.7f
#define RENDER_DEPTH_AVG_WEIGHT 0.3f

#define RENDER_FOG_DEFAULT_START 1400.0f
#define RENDER_FOG_DEFAULT_END 3200.0f

#define RENDER_SKY_MID_Y_RATIO 0.38f
#define RENDER_SKY_HORIZON_Y_RATIO 0.62f

struct color_rgb {
	float r;
	float g;
	float b;
	float a;
};

struct tri3d {
	struct vec3 v[3];
	struct color_rgb color;
};

struct raster_tri {
	SDL_FPoint p[3];
	float avg_depth;
	SDL_FColor color;
};

struct render_context {
	struct raster_tri *tri_pool;
	int tri_count;
	int tri_capacity;

	struct mat4 view;
	struct mat4 proj;
	struct mat4 vp;

	struct vec3 sun_dir;
	struct color_rgb sun_color;
	struct color_rgb ambient_color;
	struct color_rgb fog_color;

	float fog_start;
	float fog_end;

	int screen_w;
	int screen_h;
};

int render_context_init(struct render_context *ctx, int capacity);
void render_context_destroy(struct render_context *ctx);
void render_context_clear(struct render_context *ctx, int screen_w,
			  int screen_h);
void render_context_set_camera(struct render_context *ctx, struct vec3 eye,
			       struct vec3 target, struct vec3 up,
			       float fov_rad, float near_z, float far_z);

void render_draw_triangles(struct render_context *ctx, const struct tri3d *tris,
			   int count, const struct mat4 *model);
void render_draw_sky_gradient(SDL_Renderer *renderer, int width, int height);
void render_present(struct render_context *ctx, SDL_Renderer *renderer);

#endif /* RENDER3D_H */
