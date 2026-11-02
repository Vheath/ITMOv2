#include "menu.h"
#include <stdio.h>

void menu_init(struct pause_menu *m)
{
	m->open = false;
	m->selected = MENU_ROW_CONTINUE;
	m->volume = 1.0f;
	m->wants_exit = false;
}

bool menu_is_open(const struct pause_menu *m)
{
	return m->open;
}

void menu_toggle(struct pause_menu *m)
{
	m->open = !m->open;
}

void menu_handle_key(struct pause_menu *m, SDL_Keycode key)
{
	switch (key) {
	case SDLK_UP:
		m->selected =
			(m->selected + MENU_ROW_COUNT - 1) % MENU_ROW_COUNT;
		break;
	case SDLK_DOWN:
		m->selected = (m->selected + 1) % MENU_ROW_COUNT;
		break;
	case SDLK_LEFT:
		if (m->selected == MENU_ROW_VOLUME) {
			m->volume -= 0.1f;
			if (m->volume < 0.0f)
				m->volume = 0.0f;
		}
		break;
	case SDLK_RIGHT:
		if (m->selected == MENU_ROW_VOLUME) {
			m->volume += 0.1f;
			if (m->volume > 1.0f)
				m->volume = 1.0f;
		}
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (m->selected == MENU_ROW_CONTINUE)
			m->open = false;
		else if (m->selected == MENU_ROW_EXIT)
			m->wants_exit = true; /* one-shot; teardown is the
					       * caller's job (FR-008)
					       */
		break;
	case SDLK_ESCAPE:
		m->open = !m->open;
		break;
	default:
		break;
	}
}

float menu_volume(const struct pause_menu *m)
{
	return m->volume;
}

bool menu_wants_exit(const struct pause_menu *m)
{
	return m->wants_exit;
}

void menu_render(const struct pause_menu *m, SDL_Renderer *renderer,
		 int window_w, int window_h)
{
	if (!m->open)
		return;

	int percent = (int)(m->volume * 100.0f + 0.5f);
	char volume_row[24];

	snprintf(volume_row, sizeof(volume_row), "VOLUME: %d%%", percent);

	const char *rows[MENU_ROW_COUNT] = {"CONTINUE", volume_row, "EXIT"};

	/* Centered panel, same blend mode as the game-over banner */
	float panel_w = 280.0f;
	float panel_h = 130.0f;
	float panel_x = ((float)window_w - panel_w) * 0.5f;
	float panel_y = ((float)window_h - panel_h) * 0.5f;
	SDL_FRect panel = {panel_x, panel_y, panel_w, panel_h};

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 15, 15, 25, 210);
	SDL_RenderFillRect(renderer, &panel);

	for (int i = 0; i < MENU_ROW_COUNT; i++) {
		if (i == m->selected)
			SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
		else
			SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);

		SDL_RenderDebugText(renderer, panel_x + 40.0f,
				    panel_y + 40.0f + (float)i * 30.0f,
				    rows[i]);
	}
}
