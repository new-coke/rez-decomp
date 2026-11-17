/* Uranus boss course. */

#ifndef REZ_B_URANUS_URANUS_MAP_H
#define REZ_B_URANUS_URANUS_MAP_H

#include "types_common.h"
#include "types_overlays.h"

struct UranusMapWork;
struct UranusMapSeg;
struct UranusMapSegSrc;
struct UranusMapItemWork;
struct UranusMapObjectWork;

int uranus_map_motion_eval(int mot, int flags, Vec3 *pos_out, int *ang_out, float t);
void uranus_map_path_pose(struct UranusMapWork *w, struct UranusMapSeg *seg);
float uranus_map_time_for(struct UranusMapWork *w, float x);
void uranus_map_pos_at(struct UranusMapWork *w, Vec3 *out, float t);
void uranus_map_clock_step(struct UranusMapWork *w);
void uranus_map_player_set(Vec3 *pos, int *ang, int roll);
void uranus_map_exec(struct UranusMapWork *w);
void uranus_map_disp(struct UranusMapWork *w);
int uranus_map_seg_init(struct UranusMapWork *w, struct UranusMapSeg *seg, struct UranusMapSegSrc *src, int i);
void uranus_map_seg_free(struct UranusMapSeg *seg);
void uranus_map_camera_set(void);
void uranus_map_camera_restore(void);
void uranus_map_release(struct UranusMapWork *w);
struct UranusMapWork *uranus_map_create(void *parent, ModelPart **parts, int cnt, Vec3 *boss_pos);
int uranus_map_pick_seg(struct UranusMapWork *w, int course);
int uranus_map_pick_seg_rnd(struct UranusMapWork *w, int course, float rate);
void uranus_map_set_next(struct UranusMapWork *w, int course);
void uranus_map_build_course(struct UranusMapWork *w, int course);
void uranus_map_reset(struct UranusMapWork *w, int course);
void uranus_map_start_end(struct UranusMapWork *w);
void uranus_map_set_spd(struct UranusMapWork *w, float spd);
float uranus_map_get_spd(struct UranusMapWork *w);
void uranus_map_blend_spd(struct UranusMapWork *w, float spd, float rate);
void uranus_map_ring_set(struct UranusMapWork *w, int i, int seg);
void uranus_map_ring_fill(struct UranusMapWork *w, int seg);
struct UranusMapSeg *uranus_map_ring_get(struct UranusMapWork *w, int i);
void uranus_map_advance(struct UranusMapWork *w);
int uranus_map_col_records(int *rec, int mask, int n, int pt, float rad);
int uranus_map_col_check(struct UranusMapWork *w, Vec3 *pt, float *hit, float *nrm, float rad, int mask);
void uranus_map_get_vel_z(Vec3 *v);
void uranus_map_get_pos_half(Vec3 *v);
void uranus_map_get_dir(Vec3 *v);
void uranus_map_get_drift(Vec3 *v);
void uranus_map_get_scroll(Vec3 *v);
void uranus_map_get_drift_b(Vec3 *v);
int uranus_map_is_seg_end(struct UranusMapWork *w);
void uranus_map_hold(struct UranusMapWork *w);
void uranus_map_get_seg_ofs(struct UranusMapWork *w, Vec3 *out, int n);
int uranus_map_check_event(struct UranusMapWork *w, int event);
int uranus_map_get_seg_id(struct UranusMapWork *w, int i);
void uranus_map_item_spawn(Vec3 *pos);
void uranus_map_item_exec(struct UranusMapItemWork *w);
struct UranusMapItemWork *uranus_map_item_create(void *parent, struct UranusMapWork *map, Vec3 *pos, ModelPart **parts);
void uranus_map_object_exec(struct UranusMapObjectWork *w);
void uranus_map_object_disp(struct UranusMapObjectWork *w);
void uranus_map_object_release(struct UranusMapObjectWork *w);
struct UranusMapObjectWork *uranus_map_object_create(struct UranusMapWork *parent, ModelPart **parts, ModelPart *model, ModelPart *wire, int kind);
void uranus_map_rotate_zyx(int *ang);

#endif /* REZ_B_URANUS_URANUS_MAP_H */
