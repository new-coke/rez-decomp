/* Eden boss map and player path. */

#ifndef REZ_B_EDEN_EDEN_MAP_H
#define REZ_B_EDEN_EDEN_MAP_H

#include "types_common.h"

struct EdenMapWork;

/* how the player camera moves (EdenMapWork.mode) */
enum {
    EDEN_MAP_CENTER,
    EDEN_MAP_CIRCLE,
    EDEN_MAP_PASS,
    EDEN_MAP_PATH,
    EDEN_MAP_START,
    EDEN_MAP_PATH_END,
    EDEN_MAP_HOLD
};

void eden_map_vars_add(void);
void eden_map_vars_remove(void);
void eden_map_cam_preset_set(void);
void eden_map_cam_preset_apply(void);
void eden_map_cam_preset_side(int side);
void eden_map_dir_to_center(struct EdenMapWork *w);
void eden_map_circle_move(struct EdenMapWork *w);
void eden_map_pass_step(struct EdenMapWork *w);
void eden_map_pass_step_v2(struct EdenMapWork *w);
void eden_map_pass_step_v3(struct EdenMapWork *w);
void eden_map_pass_step_start(struct EdenMapWork *w);
void eden_map_cam_reset(void);
void eden_map_spin_step(struct EdenMapWork *w);
void eden_map_circle_step(struct EdenMapWork *w, Vec3 *pos, Vec3 *dir);
void eden_map_circle_opposite(struct EdenMapWork *w, Vec3 *pos, Vec3 *dir);
void eden_map_turn_start(struct EdenMapWork *w, void *mot, int n);
void eden_map_turn_start_rel(struct EdenMapWork *w, void *mot, int n);
void eden_map_set_default(struct EdenMapWork *w);
void eden_map_cam_step(struct EdenMapWork *w);
void eden_map_player_move(struct EdenMapWork *map);
void eden_map_exec(struct EdenMapWork *w);
void eden_map_pack_draw(struct EdenMapWork *w);
void eden_map_body_draw(struct EdenMapWork *w);
void eden_map_disp(struct EdenMapWork *w);
int eden_map_create(void *parent, void *arg);
void eden_map_set_turn(struct EdenMapWork *w, float x, float y);
void eden_map_request(struct EdenMapWork *w);
void eden_map_state_start(struct EdenMapWork *w);
void eden_map_set_target(struct EdenMapWork *w, int tgt);
void eden_map_fog_change(struct EdenMapWork *w, int n);
int eden_map_get_mode(struct EdenMapWork *w);
int eden_map_get_pass(struct EdenMapWork *w);
void eden_map_pass_change(struct EdenMapWork *w, int mode, int pass);
int eden_map_pass_is_end(struct EdenMapWork *w);
void eden_map_level_set(struct EdenMapWork *w, int n);
void eden_map_get_fog(int *out);
void eden_map_screen_rect_set(void);

#endif /* REZ_B_EDEN_EDEN_MAP_H */
