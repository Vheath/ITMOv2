#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "bird.h"
#include "camera.h"
#include "math3d.h"
#include "render3d.h"
#include "ring.h"
#include "terrain.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define STATIC_MESH_TRIS 65536

struct app_context {
	SDL_Window *window;
	SDL_Renderer *renderer;
	bool running;

	struct audio_engine audio;
	struct render_context r_ctx;
	struct camera cam;
	struct bird bird;
	struct terrain terrain;
	struct ring_manager rings;

	struct tri3d *static_tris;
	int static_tri_count;

	struct tri3d *bird_tris;
	int bird_tri_count;

	/* Input state */
	float input_pitch;
	float input_roll;
	bool input_boost;

	uint64_t last_ticks;
	float elapsed_time;
	float last_flap_phase;
	bool game_over;
};

static void app_handle_events(struct app_context *app)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			app->running = false;
		} else if (event.type == SDL_EVENT_KEY_DOWN) {
			if (event.key.key == SDLK_ESCAPE)
				app->running = false;
			else if (event.key.key == SDLK_R && app->game_over) {
				bird_init(&app->bird);
				ring_manager_init(&app->rings);
				camera_init(&app->cam);
				app->game_over = false;
			}
		}
	}

	const bool *keys = SDL_GetKeyboardState(NULL);

	app->input_pitch = 0.0f;
	app->input_roll = 0.0f;
	app->input_boost = false;

	if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
		app->input_pitch -= 1.0f; /* Pitch down (dive) */
	if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
		app->input_pitch += 1.0f; /* Pitch up (climb) */
	if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
		app->input_roll += 1.0f; /* Bank left */
	if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
		app->input_roll -= 1.0f; /* Bank right */
	if (keys[SDL_SCANCODE_SPACE])
		app->input_boost = true;
}

static void app_update(struct app_context *app, float dt)
{
	if (app->game_over)
		return;

	bird_update(&app->bird, dt, app->input_pitch, app->input_roll,
		    app->input_boost);

	/* Audio: stream speed and detect wing downstroke */
	audio_engine_set_speed(&app->audio, app->bird.speed,
			       app->bird.boost_timer > 0.0f);

	if (app->bird.flap_phase > M_PI && app->last_flap_phase <= M_PI) {
		float strength = 0.5f + (app->bird.speed / 36.0f) * 0.5f;
		audio_engine_trigger_flap(&app->audio, strength);
	}
	app->last_flap_phase = app->bird.flap_phase;

	/* Check terrain collision */
	float ground_h = terrain_get_height(app->bird.pos.x, app->bird.pos.z);
	if (app->bird.pos.y <= ground_h + 0.8f) {
		app->bird.pos.y = ground_h + 0.8f;
		app->game_over = true;
		audio_engine_trigger_crash(&app->audio);
	}

	/* Rings update and collision */
	ring_manager_update(&app->rings, app->bird.pos, dt);
	int points =
		ring_manager_check_collision(&app->rings, app->bird.pos, 1.8f);
	if (points > 0) {
		app->bird.score += (float)points;
		app->bird.boost_timer = 2.0f; /* Reward ring with boost */
		audio_engine_trigger_ring(&app->audio, app->rings.combo);
	}

	app->bird.score += dt * 10.0f;
	app->elapsed_time += dt;

	/* Camera update with dynamic airspeed FOV & bank tilt */
	camera_update_chase(&app->cam, app->bird.pos, app->bird.yaw,
			    app->bird.pitch, app->bird.roll, app->bird.speed,
			    dt);
}

static void app_render(struct app_context *app)
{
	int w, h;
	SDL_GetWindowSize(app->window, &w, &h);

	render_context_clear(&app->r_ctx, w, h);
	render_context_set_camera(&app->r_ctx, app->cam.eye, app->cam.target,
				  app->cam.up, app->cam.fov, 1.0f, 3500.0f);

	/* 1. Generate & submit terrain mesh centered around bird */
	int terrain_count =
		terrain_generate_mesh(&app->terrain, app->bird.pos,
				      app->static_tris, STATIC_MESH_TRIS);
	render_draw_triangles(&app->r_ctx, app->static_tris, terrain_count,
			      NULL);

	/* 2. Generate & submit animated river mesh */
	int river_count =
		river_generate_mesh(app->bird.pos, app->elapsed_time,
				    app->static_tris, STATIC_MESH_TRIS);
	render_draw_triangles(&app->r_ctx, app->static_tris, river_count, NULL);

	/* 3. Generate & submit scenery (pine trees & clouds) */
	int scenery_count =
		scenery_generate_mesh(app->bird.pos, app->elapsed_time,
				      app->static_tris, STATIC_MESH_TRIS);
	render_draw_triangles(&app->r_ctx, app->static_tris, scenery_count,
			      NULL);

	/* 4. Generate & submit golden rings mesh */
	int ring_count = ring_generate_mesh(&app->rings, app->static_tris,
					    STATIC_MESH_TRIS);
	render_draw_triangles(&app->r_ctx, app->static_tris, ring_count, NULL);

	/* 5. Generate & submit animated bird mesh */
	app->bird_tri_count =
		bird_generate_mesh(&app->bird, app->bird_tris, BIRD_MAX_TRIS);

	struct mat4 t = mat4_translation(app->bird.pos.x, app->bird.pos.y,
					 app->bird.pos.z);
	struct mat4 r = mat4_rotation_yaw_pitch_roll(
		app->bird.yaw, app->bird.pitch, -app->bird.roll);
	struct mat4 bird_model = mat4_multiply(&t, &r);

	render_draw_triangles(&app->r_ctx, app->bird_tris, app->bird_tri_count,
			      &bird_model);

	/* Clear screen with atmospheric sky gradient */
	SDL_SetRenderDrawColor(app->renderer, 150, 185, 225, 255);
	SDL_RenderClear(app->renderer);
	render_draw_sky_gradient(app->renderer, w, h);

	/* Draw 3D scene */
	render_present(&app->r_ctx, app->renderer);

	/* Draw Minimalistic Retro HUD */
	float ground_h = terrain_get_height(app->bird.pos.x, app->bird.pos.z);
	float altitude = app->bird.pos.y - ground_h;

	SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
	SDL_RenderDebugTextFormat(app->renderer, 20.0f, 20.0f, "SCORE: %06d",
				  (int)app->bird.score);
	SDL_RenderDebugTextFormat(app->renderer, 20.0f, 38.0f,
				  "SPEED: %3.1f KTS", app->bird.speed * 2.5f);
	SDL_RenderDebugTextFormat(app->renderer, 20.0f, 56.0f,
				  "ALT:   %3.0f M (AGL)", altitude);

	if (app->rings.combo > 1) {
		SDL_SetRenderDrawColor(app->renderer, 255, 215, 0, 255);
		SDL_RenderDebugTextFormat(
			app->renderer, 20.0f, 74.0f, "COMBO: x%d (%.1fs)",
			app->rings.combo, app->rings.combo_timer);
	}

	if (app->bird.boost_timer > 0.0f) {
		SDL_SetRenderDrawColor(app->renderer, 255, 220, 40, 255);
		SDL_RenderDebugText(app->renderer, 20.0f, 92.0f,
				    "BOOST ACTIVE!");
	} else {
		SDL_SetRenderDrawColor(app->renderer, 180, 200, 220, 255);
		SDL_RenderDebugText(app->renderer, 20.0f, 92.0f,
				    "[SPACE] BOOST");
	}

	SDL_SetRenderDrawColor(app->renderer, 220, 230, 240, 200);
	SDL_RenderDebugText(
		app->renderer, 20.0f, (float)h - 30.0f,
		"CONTROLS: W/S Pitch | A/D Bank & Turn | SPACE Boost");

	if (app->game_over) {
		/* Semi-transparent dark overlay banner */
		SDL_FRect banner = {0.0f, (float)h * 0.4f, (float)w, 110.0f};
		SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(app->renderer, 15, 15, 25, 210);
		SDL_RenderFillRect(app->renderer, &banner);

		SDL_SetRenderDrawColor(app->renderer, 255, 80, 80, 255);
		SDL_RenderDebugText(app->renderer, (float)w * 0.5f - 80.0f,
				    (float)h * 0.4f + 25.0f,
				    "CRASHED INTO TERRAIN!");
		SDL_SetRenderDrawColor(app->renderer, 255, 255, 255, 255);
		SDL_RenderDebugText(app->renderer, (float)w * 0.5f - 85.0f,
				    (float)h * 0.4f + 55.0f,
				    "PRESS [R] TO SOAR AGAIN");
	}

	SDL_RenderPresent(app->renderer);
}

int main(int argc, char *argv[])
{
	struct app_context app;
	int ret = 0;

	(void)argc;
	(void)argv;

	memset(&app, 0, sizeof(app));
	app.running = true;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	if (!SDL_CreateWindowAndRenderer(
		    "Alpine Bird Flight Simulator", WINDOW_WIDTH, WINDOW_HEIGHT,
		    SDL_WINDOW_RESIZABLE, &app.window, &app.renderer)) {
		SDL_Log("Unable to create window/renderer: %s", SDL_GetError());
		ret = 1;
		goto err_quit_sdl;
	}

	if (audio_engine_init(&app.audio) < 0) {
		SDL_Log("Warning: Audio device initialization failed: %s",
			SDL_GetError());
	}

	if (render_context_init(&app.r_ctx, 131072) < 0) {
		SDL_Log("Failed to allocate render context memory");
		ret = 1;
		goto err_destroy_audio;
	}

	app.static_tris = malloc(sizeof(struct tri3d) * STATIC_MESH_TRIS);
	app.bird_tris = malloc(sizeof(struct tri3d) * BIRD_MAX_TRIS);
	if (!app.static_tris || !app.bird_tris) {
		SDL_Log("Failed to allocate triangle meshes memory");
		ret = 1;
		goto err_destroy_render;
	}

	camera_init(&app.cam);
	bird_init(&app.bird);
	terrain_init(&app.terrain);
	ring_manager_init(&app.rings);

	app.last_ticks = SDL_GetTicksNS();

	while (app.running) {
		uint64_t current_ticks = SDL_GetTicksNS();
		float dt =
			(float)(current_ticks - app.last_ticks) / 1000000000.0f;
		app.last_ticks = current_ticks;

		if (dt > 0.05f)
			dt = 0.05f;

		app_handle_events(&app);
		app_update(&app, dt);
		app_render(&app);

		SDL_Delay(1);
	}

	free(app.bird_tris);
	free(app.static_tris);
err_destroy_render:
	render_context_destroy(&app.r_ctx);
err_destroy_audio:
	audio_engine_destroy(&app.audio);
err_destroy_window:
	SDL_DestroyRenderer(app.renderer);
	SDL_DestroyWindow(app.window);
err_quit_sdl:
	SDL_Quit();
	return ret;
}
