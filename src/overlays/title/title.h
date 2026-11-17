/* Title screen. */

#ifndef REZ_TITLE_TITLE_H
#define REZ_TITLE_TITLE_H

#include "title_shared.h"

void title_init(TitleWork *w);
void title_release(TitleWork *w);
void title_draw_final(int color);
void title_print_line(TitleWork *w, int line, unsigned char *str, int color);
void title_step_press_start(TitleWork *w);
void title_draw_press_start(TitleWork *w, int lang);
void title_step_menu(TitleWork *w);
void title_draw_menu(TitleWork *w, int lang);
void title_step_play_unused(TitleWork *w);
void title_step_travelling(TitleWork *w);
void title_step_play(TitleWork *w);
void title_step_score_attack(TitleWork *w);
void title_step_beyond(TitleWork *w);
void title_step_replay(TitleWork *w);
void title_step_options(TitleWork *w);
void title_step_how_to_play(TitleWork *w);
void title_step_demo(TitleWork *w);
void title_step_next_sequence(TitleWork *w);
void title_exec(TitleWork *w);
void title_disp(TitleWork *w);
void *title_create(void *parent);
int title_debug_update(TitleWork *w);
TitleWork *title_get_task(void);
TitleTasks *title_get_tasks(void);
int bg_exists(void);
int bg_is_done(void);
int title_play_unused_enabled(void);
int title_item_enabled(void);
int title_score_attack_enabled(void);
int title_options_enabled(void);
int title_how_to_play_enabled(void);
int title_beyond_enabled(void);
int title_replay_enabled(void);

#endif /* REZ_TITLE_TITLE_H */
