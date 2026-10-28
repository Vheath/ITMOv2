#ifndef MENU_H
#define MENU_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/* Pause menu row order (data-model.md: Menu Action) */
#define MENU_ROW_CONTINUE 0
#define MENU_ROW_VOLUME 1
#define MENU_ROW_EXIT 2
#define MENU_ROW_COUNT 3

struct pause_menu {
	bool open;		/* true = session paused, overlay visible */
	int selected;		/* row index, 0..MENU_ROW_COUNT - 1 */
	float volume;		/* master level, 0.0..1.0 */
	bool wants_exit;	/* one-shot quit request from the EXIT row */
};

void menu_init(struct pause_menu *m);
bool menu_is_open(const struct pause_menu *m);
void menu_toggle(struct pause_menu *m);
void menu_handle_key(struct pause_menu *m, SDL_Keycode key);
float menu_volume(const struct pause_menu *m);
bool menu_wants_exit(const struct pause_menu *m);
void menu_render(const struct pause_menu *m, SDL_Renderer *renderer,
		 int window_w, int window_h);

#endif /* MENU_H */
