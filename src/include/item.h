/* Item manager, item tasks, floor billboards. */

#ifndef REZ_ITEM_H
#define REZ_ITEM_H

#include "types_game.h"

struct Spawn;
struct ItemManageWork;
struct FormMsg;

void line_plane_intersect(float *out, float *plane_pos, float *nrm, float *org, float *dir);
void billboard_corner(float *obj, float *plane_pos, float *nrm, float *org, float *dir, float scale);
void draw_billboard_quad(float height, int tex, float sc, int col, int fog, int mode);
void item_params_register(void);
void item_count_listener(struct FormMsg *msg);
void item_spawn_listener(struct FormMsg *msg);
void item_got_listener(void);
void item_scatter_pos(float *out);
void item_task_init(struct ItemManageWork *w);
void item_task_release(struct ItemManageWork *w);
void item_task_exec(struct ItemManageWork *w);
void item_task_disp(struct ItemManageWork *w);
int item_task_create(void *parent);
void item_random_offset(float *out);
void item_count(void);
void item_count_got(void);
void item_pickup_notify(ItemWork *w);
void item_draw_model(ItemWork *w);
void item_update_scale(ItemWork *w);
void item_update_speed(ItemWork *w);
void item_push_away(float *center, float *pos, float r);
void item_push_chain(ItemWork *w);
void item_spawn_effect(ItemWork *w);
void item_set_origin(ItemOrigin *w, float *src);
void item_approach_exec(ItemOrigin *w, float speed);
void item_resolve_pos(ItemOrigin *obj, float *out, float *add);
void item_set_state(ItemOrigin *obj, int state);
void item_release(ItemWork *w);
void item_fade_exec(ItemWork *w);
void item_fall_exec(ItemWork *w);
void item_pickup_exec(ItemWork *w);
void item_main_exec(ItemWork *w);
void item_disp(ItemWork *w);
void *item_create(struct Spawn *spawn);
void item_state_if_alive(ItemWork *w, int state);
void item_set_level(ItemWork *w, int level);
void item_set_camera(int *rot);

#endif /* REZ_ITEM_H */
