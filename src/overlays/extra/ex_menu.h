/* Beyond menu pages. */

#ifndef REZ_EXTRA_EX_MENU_H
#define REZ_EXTRA_EX_MENU_H

#include "extra_shared.h"

void ex_menu_adjust_setting(int sel_id, int max);
void ex_menu_draw_value_box(ExItem *it, float z);
void ex_menu_draw_new_mark(ExItem *it, float z);
void ex_menu_draw_setting(int sel_id, ExItem *it, unsigned char **labels, float z);
void beyond_mode_count(int *available, int *seen, int *total);
void beyond_options_count(int *available, int *seen, int *total);
void beyond_mode_draw(ExMenu *mn, ExItem *it, float z);
void beyond_mode_select(ExMenu *mn);
int beyond_mode_marked(ExMenu *mn);
int beyond_mode_enabled(ExMenu *mn);
void beyond_options_draw(ExMenu *mn, ExItem *it, float z);
void beyond_options_select(ExMenu *mn);
int beyond_options_marked(ExMenu *mn);
int beyond_options_enabled(ExMenu *mn);
void return_select(ExMenu *mn);
void direct_assault_adjust(ExMenu *mn, ExItem *it);
void direct_assault_draw(ExMenu *mn, ExItem *it, float z);
void direct_assault_select(ExMenu *mn);
int direct_assault_marked(ExMenu *mn);
int direct_assault_enabled(ExMenu *mn);
void direct_assault_commit(ExMenu *mn);
void lost_area_draw(ExMenu *mn, ExItem *it, float z);
void lost_area_select(ExMenu *mn);
int lost_area_marked(ExMenu *mn);
int lost_area_enabled(ExMenu *mn);
void lost_area_commit(ExMenu *mn);
void trance_mission_draw(ExMenu *mn, ExItem *it, float z);
void trance_mission_select(ExMenu *mn);
int trance_mission_marked(ExMenu *mn);
int trance_mission_enabled(ExMenu *mn);
void trance_mission_commit(ExMenu *mn);
void boss_rush_select(ExMenu *mn);
int boss_rush_marked(ExMenu *mn);
int boss_rush_enabled(ExMenu *mn);
void boss_rush_commit(ExMenu *mn);
void player_select_adjust(ExMenu *mn, ExItem *it);
void player_select_draw(ExMenu *mn, ExItem *it, float z);
int player_select_marked(ExMenu *mn);
int player_select_enabled(ExMenu *mn);
void player_select_commit(ExMenu *mn);
void immortal_adjust(ExMenu *mn, ExItem *it);
void immortal_draw(ExMenu *mn, ExItem *it, float z);
int immortal_marked(ExMenu *mn);
int immortal_enabled(ExMenu *mn);
void immortal_commit(ExMenu *mn);
void beam_type_adjust(ExMenu *mn, ExItem *it);
void beam_type_draw(ExMenu *mn, ExItem *it, float z);
int beam_type_marked(ExMenu *mn);
int beam_type_enabled(ExMenu *mn);
void beam_type_commit(ExMenu *mn);
void overdrive_infinity_adjust(ExMenu *mn, ExItem *it);
void overdrive_infinity_draw(ExMenu *mn, ExItem *it, float z);
int overdrive_infinity_marked(ExMenu *mn);
int overdrive_infinity_enabled(ExMenu *mn);
void overdrive_infinity_commit(ExMenu *mn);
void camera_view_adjust(ExMenu *mn, ExItem *it);
void camera_view_draw(ExMenu *mn, ExItem *it, float z);
int camera_view_marked(ExMenu *mn);
int camera_view_enabled(ExMenu *mn);
void camera_view_commit(ExMenu *mn);
void default_select(ExMenu *mn);
void ex_menu_draw_sprite(int id, float x, float y, float z);
void ex_menu_draw_cursor(float x, float y, float z);
void ex_menu_init(ExMenu *mn, int id, int sel);
void ex_menu_release(ExMenu *mn);
void ex_menu_exec(ExMenu *mn);
void ex_menu_disp(ExMenu *mn);
void *ex_menu_create(void *parent, int id, int sel);
int ex_menu_get_result(ExMenu *mn, int *res);

#endif /* REZ_EXTRA_EX_MENU_H */
