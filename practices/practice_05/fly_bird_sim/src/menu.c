#include "menu.h"

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

/*
 * Placeholder until US1: consumes one discrete menu key, no action yet.
 */
void menu_handle_key(struct pause_menu *m, SDL_Keycode key)
{
	(void)m;
	(void)key;
}

float menu_volume(const struct pause_menu *m)
{
	return m->volume;
}

bool menu_wants_exit(const struct pause_menu *m)
{
	return m->wants_exit;
}

/*
 * Placeholder until US1: the overlay draws nothing while the module
 * skeleton is inert.
 */
void menu_render(const struct pause_menu *m, SDL_Renderer *renderer,
		 int window_w, int window_h)
{
	(void)m;
	(void)renderer;
	(void)window_w;
	(void)window_h;
}
