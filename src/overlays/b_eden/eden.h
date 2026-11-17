/* Eden boss main task. */

#ifndef REZ_B_EDEN_EDEN_H
#define REZ_B_EDEN_EDEN_H

#include "types_overlays.h"
#include "types_common.h"

struct EdenWireEditManWork;
struct EdenWork;

void *module_entry(void *parent, void *arg);
void star_color_apply(void);
int boss_main_demo_cb(BossMainWork *w);
void eden_exec_start(struct EdenWork *w);
void eden_exec_load(struct EdenWork *w);
void eden_init(struct EdenWork *w, int *arg);
void eden_release(struct EdenWork *w);
void eden_children_finish(struct EdenWork *w);
void eden_phase_change(struct EdenWork *w, int kind);
int eden_is_piece_clear(struct EdenWork *w);
void eden_bgm_flag_clear(void);
int eden_ending_type(void);
void eden_stage_start_cb(struct EdenWork *w);
void eden_stage_start_request(struct EdenWork *w, int req);
void eden_stage_clear_cb(struct EdenWork *w);
void eden_stage_clear_request(struct EdenWork *w);
void eden_bar_countdown(struct EdenWork *w);
void eden_exec(struct EdenWork *w);
void eden_disp(void **w);
void *eden_create(void *parent, void *arg);
void task_finish_if(void *w);
void eden_circle_spawn(void *w, Vec3 *pos, int flag);
void eden_player_damage(void);
int eden_get_arg0(void);
int eden_tex_offset(void);
struct EdenFogWork;
int eden_fog_exec(struct EdenFogWork *w);
int *eden_fog_create(void *parent, BossFog *to, float secs);
void eden_set_dalpha(float x);
float eden_get_dalpha(void);
int eden_get_lock_off(void);
int eden_get_break_all(void);
void eden_set_lock_off(int on);
void eden_set_break_all(int on);
int eden_test_flags(int mask);
void eden_set_flags(int flags);
int eden_get_flags(void);
void *eden_get_piece(struct EdenWork *w);
int eden_get_core(struct EdenWork *w);
int eden_get_parts(struct EdenWork *w);
void eden_wire_colors_set(void);
CmWireBreakWork *eden_shatter_spawn(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color);
CmWireBreakWork *eden_shatter_model(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color);
void eden_end_request(void);
int eden_is_end(void);
void eden_item_point_random(Vec3 *out);
int eden_item_kind(void);
void eden_item_vel_adjust(float *pos, Vec3 *vel);
void eden_items_spawn(Vec3 *pos, int n);
void eden_items_drop(Vec3 *pos, int n);
void eden_wire_edit_man_exec(struct EdenWireEditManWork *w);
void eden_wire_edit_man_disp(struct EdenWireEditManWork *w);
void eden_wire_edit_man_release(struct EdenWireEditManWork *w);
void *eden_wire_edit_man_create(void *parent);

#endif /* REZ_B_EDEN_EDEN_H */
