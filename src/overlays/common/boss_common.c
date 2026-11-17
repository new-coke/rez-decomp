/*
 * Shared boss code for the five boss overlays: boss camera, geometry and drawing helpers, model
 * and wire break effects, rings, tails, sparks; part list, loading, scoring, lock-on tables; star
 * field, gauge, delayed player commands, demo motions, screen blur, light and fog, item drops.
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "types_gw.h"
#include "anim.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "console.h"
#include "enemy_status.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "lens.h"
#include "libc.h"
#include "light.h"
#include "line_batch.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_direct.h"
#include "nj_cnk_vu1.h"
#include "nj_matrix.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "screen.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "wipefade.h"
#include "boss_common.h"
#include "boss_wire.h"
#include "eff_star.h"
#include "system.h"
#include "debug.h"

static inline void v3_copy(Vec3 *d, Vec3 *s)
{
    d->x = s->x;
    d->y = s->y;
    d->z = s->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *d, Vec3 *s)
{
    d->x += s->x;
    d->y += s->y;
    d->z += s->z;
}

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        int a_ = ang; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x - b->x;
    d->y = a->y - b->y;
    d->z = a->z - b->z;
}

static inline void v3_sum(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x + b->x;
    d->y = a->y + b->y;
    d->z = a->z + b->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void v3_mul(Vec3 *d, Vec3 *a, float k)
{
    d->x = a->x * k;
    d->y = a->y * k;
    d->z = a->z * k;
}

static inline void v3_lerp(Vec3 *d, Vec3 *a, Vec3 *b, float k)
{
    d->x = a->x * k + b->x * (1.0f - k);
    d->y = a->y * k + b->y * (1.0f - k);
    d->z = a->z * k + b->z * (1.0f - k);
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

static inline void i3_copy(int *d, int *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

extern ScreenCfg screen_cfg;

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern float project_point_size(Vec3 *p, Vec3 *out, float *sx, float *sy);

#define MTX_TRANS(p) \
    do { \
        Vec3 *p_ = (p); \
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z); \
    } while (0)

static inline void i3_sum(int *d, int *a, int *b)
{
    d[0] = a[0] + b[0];
    d[1] = a[1] + b[1];
    d[2] = a[2] + b[2];
}

/* allocation tag line numbers of the original source */
#ifdef VERSION_US
#define SRC_LINE(early, us) (us)
#else
#define SRC_LINE(early, us) (early)
#endif

#define ROTS_X(ang, s, c, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &s, &c); \
        y = rot_u * c - rot_v * s; \
        z = rot_u * s + rot_v * c; \
    } while (0)

#define ROTS_Y(ang, s, c, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &s, &c); \
        x = rot_u * c + rot_v * s; \
        z = -rot_u * s + rot_v * c; \
    } while (0)

#ifdef VERSION_US
#endif

extern void matrix_get_or_set(void *m, float *dst);

extern int system_fps;

extern BossBeamDef venus_zxyb_beams[];
extern BossColPair motion_path_colors;
extern BossLockDef *lock_table;
extern BossTNode *task_tree_found;
extern Vec3 *shard_gravity;
extern Vec3 *shard_wind;
extern Vec3 motion_point_pos;
extern Vec3 vec_forward;
extern Vec3 vec_up;
extern Vec3 vec_forward2;
extern Vec3 vec_up2;
extern Vec3 boss_fx_start_vec;
extern Vec3 boss_fx_end_vec;
extern Vec3 unit_circle[];
extern WireObj motion_point_obj;
extern char duos_vardef[];
extern GwVarDef duos_vardefs[];
extern char boss_camera_hooks[];
extern struct TaskDesc boss_camera_desc;
extern struct TaskDesc boss_cam_ang_desc;
extern char str_boss_common_c[];
extern struct TaskDesc cm_cchange_desc;
extern struct TaskDesc cm_cfchange_desc;
extern struct TaskDesc model_uv_desc;
extern struct TaskDesc cm_wire_break_desc;
extern struct TaskDesc cm_wire_mdl_break_desc;
extern struct TaskDesc venus_zxyb_desc;
extern struct TaskDesc cm_circle_desc;
extern struct TaskDesc cm_circle2_desc;
extern struct TaskDesc cm_btail_desc;
extern struct TaskDesc cm_ptcl_desc;
extern struct TaskDesc cm_obj_mot_desc;
extern struct TaskDesc cm_object_blur_desc;
extern struct TaskDesc boss_star_desc;
extern struct TaskDesc boss_gage_desc;
extern struct TaskDesc boss_pl_di_desc;
extern struct TaskDesc cm_sc_blur_desc;
extern struct TaskDesc cm_sc_cross_desc;
extern struct TaskDesc cm_light_change_desc;
extern float cm_line_sx;
extern float cm_line_sy;
extern float cm_line_w;
extern float cm_line_h;
extern float object_blur_rgba[4];
extern float light_fog_near;
extern float light_fog_far;
extern float lines_width;
extern int *boss_task;
extern int motion_point_ang[3];
extern int light_fog_back;
extern int light_fog_color;
extern int light_colors[];
extern struct GwVarNode *duos_vars_group;
extern int task_tree_index;
extern int task_tree_count;
extern int boss_level;
extern int boss_arg0;
extern int boss_fx_slot;
extern int boss_parts;
extern int boss_parts_built;
extern int lock_table_count;
extern int boss_scoring;
extern int boss_fx_live;
extern int tex_anime_running;
extern void *motion_point_buf[3];
extern void *lines_batch;

#ifndef VERSION_US
#endif

#ifndef VERSION_US
#endif

extern float duos_mf;
extern float duos_kf;
extern float duos_f;
extern float duos_t;
extern float duos_d;
extern int duos_disp;
extern int boss_file0;
extern int boss_file1;
extern float shard_fade_rgba[4];

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void i3_mul(int *dst, int *s, float k)
{
    dst[0] = s[0] * k;
    dst[1] = s[1] * k;
    dst[2] = s[2] * k;
}

/* dst = src_a + src_b */
static inline void v3_sum_to(Vec3 *src_a, Vec3 *src_b, Vec3 *dst)
{
    dst->x = src_a->x + src_b->x;
    dst->y = src_a->y + src_b->y;
    dst->z = src_a->z + src_b->z;
}

/* 0x00cb2180 b_earth, 0x00cc29f8 b_eden, 0x00cb0d28 b_mars, 0x00cb5458 b_uranus, 0x00cb5dc8 b_venus */
void boss_camera_reset_get(Camera *out, BossCameraWork *w)
{
    boss_camera_reset(w);
    *out = w->cam;
}

/* 0x00cb2208 b_earth, 0x00cc2a80 b_eden, 0x00cb0db0 b_mars, 0x00cb54e0 b_uranus, 0x00cb5e50 b_venus */
void boss_camera_update_get(Camera *out, BossCameraWork *w)
{
    boss_camera_step(w);
    *out = w->cam;
}

/* 0x00cb2290 b_earth, 0x00cc2b08 b_eden, 0x00cb0e38 b_mars, 0x00cb5568 b_uranus, 0x00cb5ed8 b_venus */
void boss_camera_reset(BossCameraWork *w)
{
    Camera *cur;

    w->t = 0;
    if (w->mot)
        w->frames = w->mot[1];
    if (!w->count) {
        player_set_eye(w->pv);
        w->state = 3;
    } else
        w->state = 0;
    cur = camera_get_current();
    w->cam = *cur;
    v3_copy(&w->eye, &cur->eye);
    w->fov = cur->fov;
    player_get_eye(w->pv);
    i3_copy(w->pv0, w->pv);
    w->prev = w->cam;
}

/* 0x00cb2418 b_earth, 0x00cc2c90 b_eden, 0x00cb0fc0 b_mars, 0x00cb56f0 b_uranus, 0x00cb6060 b_venus */
void camera_move_by(Camera *cam, Vec3 *ofs)
{
    v3_sum(&cam->eye, ofs, &cam->eye);
    v3_sum(&cam->center, ofs, &cam->center);
}

/* 0x00cb2480 b_earth, 0x00cc2cf8 b_eden, 0x00cb1028 b_mars, 0x00cb5758 b_uranus, 0x00cb60c8 b_venus */
void boss_camera_eye_step(BossCameraWork *w)
{
    Camera *cur = camera_get_main_cam();

    v3_sub(&w->vel, &cur->eye, &w->eye);
    v3_copy(&w->eye, &cur->eye);
}

/* 0x00cb24f8 b_earth, 0x00cc2d70 b_eden, 0x00cb10a0 b_mars, 0x00cb57d0 b_uranus, 0x00cb6140 b_venus */
void camera_relative_point(Vec3 *v, Vec3 *out)
{
    Vec3 dir;
    int ang[2];
    float sc[4];
    Camera *cur = camera_get_main_cam();

    v3_sub(&dir, &cur->center, &cur->eye);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    v3_copy(&dir, v);
    ROT_X(ang[0], sc, dir.y, dir.z);
    ROT_Y(ang[1], (sc + 2), dir.x, dir.z);
    v3_sum(out, &cur->eye, &dir);
}

/* 0x00cb26d0 b_earth, 0x00cc2f48 b_eden, 0x00cb1278 b_mars, 0x00cb59a8 b_uranus, 0x00cb6318 b_venus */
/* steer(cast) */
void boss_camera_step(BossCameraWork *w)
{
    Camera *cur;

    if (w->flags & 1)
        boss_camera_eye_step(w);
    if (w->flags & 0x10)
        player_set_eye(w->pv);
    if (w->timer && --w->timer <= 0)
        w->state = w->next;
    switch (w->state) {
    case 6: {
        Camera *src = camera_get_main_cam();

        w->cam = *src;
        v3_copy(&w->eye, &src->eye);
        if (w->count) {
            i3_mul(w->pv, w->pv0, (float)--w->count / w->total);
        } else
            i3_set(w->pv, 0, 0, 0);
        break;
    }
    case 5: {
        Vec3 dir;
        Vec3 v;
        int ang[2];
        Vec3 view;
        float ease_sin, ease_cos, pitch_sin, pitch_cos, yaw_sin, yaw_cos, fix_pitch_sin, fix_pitch_cos, fix_yaw_sin, fix_yaw_cos;
        float rate;

        cur = camera_get_main_cam();
        w->cam = *cur;
        v3_copy(&w->eye, &cur->eye);
        if (w->hold)
            w->hold--;
        if ((w->flags & 0x200) && !w->hold) {
            if (!(w->ease_out < w->count)) {
                rate = (float)w->count / w->ease_out;
                if (w->flags & 0x40) {
                    ulSinCosf(rate * PI, &ease_sin, &ease_cos);
                    rate = ease_cos * -0.5f + 0.5f;
                }
                v3_mul(&v, &w->ofs, rate);
                camera_relative_point(&v, &w->cam.eye);
                v3_sub(&dir, w->target, &w->cam.eye);
                vec3_normalize(&dir, &dir);
                v3_sub(&view, &cur->center, &cur->eye);
                vec3_normalize(&view, &view);
                v3_lerp(&w->cam.center, &dir, &view, rate);
                do {
                    ang[1] = RAD_TO_ANG(atan2f(-w->cam.center.x, -w->cam.center.z));
                    ang[0] = RAD_TO_ANG(atan2f(w->cam.center.y, sqrtf(w->cam.center.x * w->cam.center.x + w->cam.center.z * w->cam.center.z)));
                } while (0);
                v3_add(&w->cam.center, &w->cam.eye);
                do {
                    {
                        Vec3 *up = &w->cam.up;
                        up->x = up->z = 0.0f;
                    }
                    w->cam.up.y = 1.0f;
                } while (0);
                ROTS_X(ang[0], pitch_sin, pitch_cos, w->cam.up.y, w->cam.up.z);
                ROTS_Y(ang[1], yaw_sin, yaw_cos, w->cam.up.x, w->cam.up.z);
            } else {
                v3_copy(&v, &w->ofs);
                v3_copy(&w->cam.center, w->target);
                camera_relative_point(&v, &w->cam.eye);
                v3_sub(&dir, &w->cam.center, &w->cam.eye);
                do {
                    ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
                    ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
                } while (0);
                {
                    Vec3 *up = &w->cam.up;
                    up->x = up->z = 0.0f;
                }
                w->cam.up.y = 1.0f;
                ROTS_X(ang[0], fix_pitch_sin, fix_pitch_cos, w->cam.up.y, w->cam.up.z);
                ROTS_Y(ang[1], fix_yaw_sin, fix_yaw_cos, w->cam.up.x, w->cam.up.z);
            }
        }
        if (--w->count <= 0)
            FINISH_IF(w);
        break;
    }
    case 0: {
        Camera *src = camera_get_main_cam();

        w->cam = *src;
        v3_copy(&w->eye, &src->eye);
        if (--w->count <= 0) {
            i3_set(w->pv, 0, 0, 0);
            w->total = w->ease_in;
            w->count = w->ease_in;
            if (w->mot)
                w->state = 2;
            else
                w->state = 1;
        } else {
            i3_mul(w->pv, w->pv0, (float)w->count / w->total);
        }
        break;
    }
    case 1:
        if (!w->mot) {
            Camera *src = camera_get_main_cam();

            w->cam = *src;
            v3_copy(&w->eye, &src->eye);
            break;
        }
        w->state = 2;
    case 2: {
        Camera cam;
        float ease_sin, ease_cos;
        float rate;

        if (w->count)
            rate = 1.0f - (float)--w->count / w->total;
        else {
            w->state = 3;
            rate = 1.0f;
        }
        if (w->flags & 0x40) {
            ulSinCosf(rate * PI, &ease_sin, &ease_cos);
            rate = ease_cos * -0.5f + 0.5f;
        }
        v3_mul(&w->ofs0, &w->ofs, rate);
        if (w->flags & 2)
            camera_from_path(&cam, (float *)w->camsrc, w->mot, (void *)w->path_mot, w->t, w->path_dist);
        else
            camera_eval_motion(&cam, (void *)w->camsrc, w->mot, w->t);
        camera_move_by(&cam, &w->ofs0);
        if (w->flags & 1) {
            camera_move_by(&cam, &w->eye);
            camera_move_by(&w->cam, &w->vel);
        }
        ((void (*)(Camera *, Camera *, float, float, int))camera_lerp)(&w->cam, &cam, rate, 0.0f, 3);
        break;
    }
    case 3:
        if (w->flags & 2)
            camera_from_path(&w->cam, (float *)w->camsrc, w->mot, (void *)w->path_mot, w->t, w->path_dist);
        else
            camera_eval_motion(&w->cam, (void *)w->camsrc, w->mot, w->t);
        camera_move_by(&w->cam, &w->ofs0);
        if (w->flags & 1)
            camera_move_by(&w->cam, &w->eye);
        w->t += w->speed;
        if (w->flags & 0x80) {
            if (w->frames - w->ease_out <= w->t) {
                w->total = w->ease_out;
                w->count = w->ease_out;
                w->state = 4;
            }
        } else if (w->frames <= w->t) {
            w->state = 4;
            w->total = w->ease_out;
            w->count = w->ease_out;
            w->t = w->frames - 1.0f;
        }
        break;
    case 4: {
        Vec3 dir;
        int ang[2];
        Camera cam;
        float ease_sin, ease_cos, pitch_sin, pitch_cos, yaw_sin, yaw_cos;
        float rate;

        cur = camera_get_main_cam();
        if (w->flags & 0x80) {
            if (w->flags & 2)
                camera_from_path(&w->cam, (float *)w->camsrc, w->mot, (void *)w->path_mot, w->t, w->path_dist);
            else
                camera_eval_motion(&w->cam, (void *)w->camsrc, w->mot, w->t);
            camera_move_by(&w->cam, &w->ofs0);
            w->t += w->speed;
            if (w->frames <= w->t)
                w->t = w->frames - 1.0f;
        }
        if (w->flags & 1)
            camera_move_by(&w->cam, &w->vel);
        rate = 1.0f - (float)--w->count / w->total;
        if (w->flags & 0x40) {
            ulSinCosf(rate * PI, &ease_sin, &ease_cos);
            rate = ease_cos * -0.5f + 0.5f;
        }
        w->fov = w->fov * rate + cur->fov * (1.0f - rate);
        if (w->flags & 0x100) {
            cam = w->cam;
            v3_copy(&cam.center, &w->eye);
            v3_sub(&dir, &cam.center, &cam.eye);
            do {
                ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
                ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
            } while (0);
            dir.y = 1.0f;
            dir.x = dir.z = 0.0f;
            ROTS_X(ang[0], pitch_sin, pitch_cos, dir.y, dir.z);
            ROTS_Y(ang[1], yaw_sin, yaw_cos, dir.x, dir.z);
            v3_copy(&cam.up, &dir);
            {
                float t = rate <= 0.5f ? rate + rate : 1.0f;

                ((void (*)(Camera *, Camera *, float, float, int))camera_lerp)(&w->cam, &cam, t, 0.0f, 1);
            }
            ((void (*)(Camera *, Camera *, float, float, int))camera_lerp)(&w->cam, cur, rate, 0.0f, 1);
        } else
            ((void (*)(Camera *, Camera *, float, float, int))camera_lerp)(&w->cam, cur, rate, 0.0f, 1);
        if (w->count <= 0) {
#ifdef VERSION_US
            camera_node_remove(w->node);
            w->node_on = 0;
#endif
            FINISH_IF(w);
        }
        break;
    }
    case 7:
    case 8:
        break;
    }
    if (w->flags & 0x20)
        w->cam.fov = w->fov;
    if (w->cb) {
        v3_sub(&w->mv, &w->cam.eye, &w->prev.eye);
        w->cb(w->cb_arg, &w->mv);
        w->prev = w->cam;
    }
}

/* 0x00cb3610 b_earth, 0x00cc3e88 b_eden, 0x00cb21b8 b_mars, 0x00cb68e8 b_uranus, 0x00cb7258 b_venus */
void boss_camera_release(BossCameraWork *w)
{
#ifdef VERSION_US
    if (w->node_on) {
        camera_node_remove(w->node);
        w->node_on = 0;
    }
#else
    camera_node_remove(w->node);
#endif
    if (w->flags & 8)
        game_demo_end(w);
    if (w->flags & 0x10)
        pad_trigger_enable(0);
}

/* 0x00cb3680 b_earth, 0x00cc3ef8 b_eden, 0x00cb2228 b_mars, 0x00cb6958 b_uranus, 0x00cb72c8 b_venus */
/* steer(cast) */
BossCameraWork *boss_camera_create(void *parent, float speed, unsigned int cam_id, unsigned int mot_id, Vec3 *ofs, unsigned int flags, int hold, int ease_out, int ease_in)
{
    BossCameraWork *w = task_create_inherit(parent, &boss_camera_desc, sizeof(BossCameraWork));

    if (cam_id)
        w->camsrc = *((int *)pnb_get_entry(w->blk, cam_id));
    if (mot_id)
        w->mot = (unsigned int *)*((int *)pnb_get_entry(w->blk, mot_id));
    w->speed = speed;
    w->hold = hold;
    w->ease_out = ease_out;
    w->ease_in = ease_in;
    if (ofs)
        v3_copy(&w->ofs0, ofs);
    else
        i3_set((int *)&w->ofs0, 0, 0, 0);
    w->flags = flags;
    v3_copy(&w->ofs, &w->ofs0);
    w->total = w->hold;
    w->count = w->hold;
    if (flags & 8)
        game_demo_start(w);
    if (w->flags & 0x10)
        pad_trigger_disable(0);
    w->node = camera_node_add(2, boss_camera_hooks, w);
#ifdef VERSION_US
    w->node_on = 1;
#endif
    return w;
}

/* 0x00cb3800 b_earth, 0x00cc4078 b_eden, 0x00cb23a8 b_mars, 0x00cb6ad8 b_uranus, 0x00cb7448 b_venus */
void boss_camera_set_motion(BossCameraWork *w, int camsrc, unsigned int *mot)
{
    unsigned int frames = mot[1];

    w->camsrc = camsrc;
    w->mot = mot;
    w->frames = frames;
    w->total = w->ease_in;
    w->count = w->ease_in;
}

/* 0x00cb3850 b_earth, 0x00cc40c8 b_eden, 0x00cb23f8 b_mars, 0x00cb6b28 b_uranus, 0x00cb7498 b_venus */
void boss_camera_set_fov(BossCameraWork *w, int fov)
{
    w->fov = fov;
    w->flags |= 0x20;
}

/* 0x00cb3868 b_earth, 0x00cc40e0 b_eden, 0x00cb2410 b_mars, 0x00cb6b40 b_uranus, 0x00cb74b0 b_venus */
void boss_camera_set_player_vec(BossCameraWork *w, int *v)
{
    if (v)
        i3_copy(w->pv, v);
    else
        i3_set(w->pv, 0, 0, 0);
}

/* 0x00cb38a8 b_earth, 0x00cc4120 b_eden, 0x00cb2450 b_mars, 0x00cb6b80 b_uranus, 0x00cb74f0 b_venus */
void boss_camera_set_flags(BossCameraWork *w, unsigned int keep, unsigned int set)
{
    w->flags = (w->flags & keep) | set;
}

/* 0x00cb38c0 b_earth, 0x00cc4138 b_eden, 0x00cb2468 b_mars, 0x00cb6b98 b_uranus, 0x00cb7508 b_venus */
void boss_camera_add_frame(BossCameraWork *w, float inc)
{
    w->frames += inc;
}

/* 0x00cb38d0 b_earth, 0x00cc4148 b_eden, 0x00cb2478 b_mars, 0x00cb6ba8 b_uranus, 0x00cb7518 b_venus */
void boss_camera_set_callback(BossCameraWork *w, void (*cb)(int, Vec3 *), int cb_arg)
{
    w->cb = cb;
    w->cb_arg = cb_arg;
}

/* 0x00cb38e0 b_earth, 0x00cc4158 b_eden, 0x00cb2488 b_mars, 0x00cb6bb8 b_uranus, 0x00cb7528 b_venus */
void boss_camera_set_path(BossCameraWork *w, int path_mot, float dist)
{
    w->path_dist = dist;
    w->path_mot = path_mot;
    w->flags |= 2;
}

/* 0x00cb38f8 b_earth, 0x00cc4170 b_eden, 0x00cb24a0 b_mars, 0x00cb6bd0 b_uranus, 0x00cb7540 b_venus */
void boss_camera_set_target(BossCameraWork *w, Vec3 *target)
{
    w->target = target;
    w->flags |= 0x200;
}

/* 0x00cb3910 b_earth, 0x00cc4188 b_eden, 0x00cb24b8 b_mars, 0x00cb6be8 b_uranus, 0x00cb7558 b_venus */
void boss_camera_look(BossCameraWork *w, int n)
{
    w->count = n;
    w->state = 5;
}

/* 0x00cb3920 b_earth, 0x00cc4198 b_eden, 0x00cb24c8 b_mars, 0x00cb6bf8 b_uranus, 0x00cb7568 b_venus */
void boss_camera_follow(BossCameraWork *w, int n)
{
    w->total = n;
    w->count = n;
    w->state = 6;
}

/* 0x00cb3938 b_earth, 0x00cc41b0 b_eden, 0x00cb24e0 b_mars, 0x00cb6c10 b_uranus, 0x00cb7580 b_venus */
void boss_camera_idle_after(BossCameraWork *w, int n)
{
    w->timer = n;
    w->next = 7;
}

/* 0x00cb3948 b_earth, 0x00cc41c0 b_eden, 0x00cb24f0 b_mars, 0x00cb6c20 b_uranus, 0x00cb7590 b_venus */
void boss_cam_ang_exec(BossCamAngWork *w)
{
    float rate;

    switch (w->phase) {
    case 0:
        if (--w->wait > 0)
            return;
        w->phase = 1;
        w->wait = w->dur;
        return;
    case 1:
        if (--w->wait <= 0) {
            FINISH_IF(w);
            w->cam->fov = w->to;
        } else {
            float from = w->from, to = w->to;

            rate = (float)w->wait / w->dur;
            w->cam->fov = from * rate + to * (1.0f - rate);
        }
        return;
    }
}

/* 0x00cb3a10 b_earth, 0x00cc4288 b_eden, 0x00cb25b8 b_mars, 0x00cb6ce8 b_uranus, 0x00cb7658 b_venus */
BossCamAngWork *boss_cam_ang_create(BossCameraWork *w, int from, int to, int wait, int dur)
{
    BossCamAngWork *fov_w;

    w->flags |= 0x20;
    if (!dur) {
        w->fov = to;
        return 0;
    }
    fov_w = task_create_inherit(w, &boss_cam_ang_desc, 24);
    fov_w->cam = w;
    if (!from)
        fov_w->from = w->fov;
    else
        fov_w->from = from;
    fov_w->to = to;
    fov_w->wait = wait;
    fov_w->dur = dur;
    return fov_w;
}

/* 0x00cb3ac0 b_earth, 0x00cc4338 b_eden, 0x00cb2668 b_mars, 0x00cb6d98 b_uranus, 0x00cb7708 b_venus */
/* steer(cast) */
void camera_from_path(Camera *cam, float *src, void *hdr, void *mot, float t, float dist)
{
    Vec3 pos;
    int ang[3];
    Vec3 dir;
    Vec3 off;
    CamArgs args;
    short roll_fov[2];
    int dir_ang[2];
    float sc[6];
    int roll;
    float x, y, z;

    off.z = dist;
    off.x = off.y = 0.0f;
    motion_point_pose(mot, t, &pos, ang);
    vec_rotate_zxy(&off, ang);
    v3_add(&pos, &off);
    roll_fov[0] = 0;
    args.roll = &roll_fov[0];
    args.c = cam;
    args.d = &dir;
    args.fov = &roll_fov[1];
    ((void (*)(float *, void *, CamArgs *, float))anim_eval_header)(src, hdr, &args, t);
    v3_sub(&cam->eye, &cam->eye, &pos);
    roll = roll_fov[0];
    if (roll_fov[1])
        cam->fov = roll_fov[1];
    else
        cam->fov = 10922;
    vec3_normalize(&dir, &dir);
    v3_sum(&cam->center, &cam->eye, &dir);
    do {
        dir_ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        dir_ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    ROT_Z(roll, sc, x, y);
    ROT_X(dir_ang[0], (sc + 2), y, z);
    ROT_Y(dir_ang[1], (sc + 4), x, z);
    v3_set(&cam->up, x, y, z);
}

/* 0x00cb3d78 b_earth, 0x00cc45f0 b_eden, 0x00cb2920 b_mars, 0x00cb7050 b_uranus, 0x00cb79c0 b_venus */
float circle3_2d(float *ox, float *oy, float ax, float ay, float bx, float by, float cx, float cy)
{
    float sqa, sqb, sqc, det, dx, dy, cst, rad;

    sqa = ax * ax + ay * ay;
    sqb = bx * bx + by * by;
    det = (bx * ay - ax * by) + (cx * by - bx * cy) + (ax * cy - cx * ay);
    dx = (sqa * (by - cy) + sqb * (cy - ay) + (sqc = cx * cx + cy * cy) * (ay - by)) * 0.5f;
    dy = (sqa * (cx - bx) + sqb * (ax - cx) + sqc * (bx - ax)) * 0.5f;
    cst = sqa * (bx * cy - cx * by) + sqb * (cx * ay - ax * cy) + sqc * (ax * by - bx * ay);
    if (fabsf(det) < FLT_EPSILON)
        return -1.0f;
    rad = sqrtf((dx * dx + dy * dy - det * cst) / (det * det));
    *ox = -dx / det;
    *oy = -dy / det;
    return rad;
}

/* 0x00cb3f18 b_earth, 0x00cc4790 b_eden, 0x00cb2ac0 b_mars, 0x00cb71f0 b_uranus, 0x00cb7b60 b_venus */
float circle3_2d_degenerate(float *ox, float *oy, float ax, float ay, float bx, float by, float cx, float cy)
{
    float dab, dbc, dca, mx, my, max_dsq, dsq;

    dab = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
    dbc = (bx - cx) * (bx - cx) + (by - cy) * (by - cy);
    dca = (cx - ax) * (cx - ax) + (cy - ay) * (cy - ay);
    if (fabsf(dab) < FLT_EPSILON && fabsf(dbc) < FLT_EPSILON)
        return -1.0f;
    if (fabsf(dab) < FLT_EPSILON) {
        *ox = (bx + cx) * 0.5f;
        *oy = (by + cy) * 0.5f;
        return sqrtf(dbc) * 0.5f;
    }
    if (fabsf(dbc) < FLT_EPSILON || fabsf(dca) < FLT_EPSILON) {
        *ox = (ax + bx) * 0.5f;
        *oy = (ay + by) * 0.5f;
        return sqrtf(dab) * 0.5f;
    }
    mx = (ax + bx + cx) / 3.0f;
    my = (ay + by + cy) / 3.0f;
    *ox = mx;
    *oy = my;
    max_dsq = 0.0f;
    dsq = (ax - mx) * (ax - mx) + (ay - my) * (ay - my);
    if (max_dsq < dsq)
        max_dsq = dsq;
    dsq = (bx - mx) * (bx - mx) + (by - my) * (by - my);
    if (max_dsq < dsq)
        max_dsq = dsq;
    dsq = (cx - mx) * (cx - mx) + (cy - my) * (cy - my);
    if (max_dsq < dsq)
        max_dsq = dsq;
    if (fabsf(max_dsq) < FLT_EPSILON)
        return -1.0f;
    return sqrtf(max_dsq);
}

/* 0x00cb4150 b_earth, 0x00cc49c8 b_eden, 0x00cb2cf8 b_mars, 0x00cb7428 b_uranus, 0x00cb7d98 b_venus */
float circle3_3d(Vec3 *pt_a, Vec3 *pt_b, Vec3 *pt_c, Vec3 *out)
{
    int ang[2];
    Vec3 e1;
    Vec3 e2;
    Vec3 nrm;
    Vec3 pa;
    Vec3 pb;
    Vec3 pc;
    float rad;

    v3_sub(&e1, pt_b, pt_a);
    v3_sub(&e2, pt_c, pt_a);
    ulVec3Cross(&nrm, &e1, &e2);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-nrm.x, -nrm.z));
        ang[0] = RAD_TO_ANG(atan2f(nrm.y, sqrtf(nrm.x * nrm.x + nrm.z * nrm.z)));
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pt_a->x, &pa.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pt_b->x, &pb.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pt_c->x, &pc.x);
    rad = circle3_2d(&out->x, &out->y, pa.x, pa.y, pb.x, pb.y, pc.x, pc.y);
    out->z = pa.z;
    matrix_unit_set(0);
    vu0_matrix_rotate_x(ulMatrixCur, -ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -ang[1]);
    ulMatrixApplyPoint((float *)ulMatrixCur, &out->x, &out->x);
    ulMatrixPop();
    return rad;
}

/* 0x00cb4378 b_earth, 0x00cc4bf0 b_eden, 0x00cb2f20 b_mars, 0x00cb7650 b_uranus, 0x00cb7fc0 b_venus */
void vec_reflect(Vec3 *v, Vec3 *nrm, Vec3 *out)
{
    Vec3 proj;
    float scl;

    if (!v3_is_zero(nrm)) {
        scl = (ulVec3Dot(v, nrm) * 2.0f) / (nrm->x * nrm->x + nrm->y * nrm->y + nrm->z * nrm->z);
        v3_mul(&proj, nrm, scl);
        v3_sub(out, v, &proj);
    }
}

/* 0x00cb4480 b_earth, 0x00cc4cf8 b_eden, 0x00cb3028 b_mars, 0x00cb7758 b_uranus, 0x00cb80c8 b_venus */
void vec_reflect_unit(Vec3 *v, Vec3 *nrm, Vec3 *out)
{
    Vec3 proj;
    float scl;

    scl = ulVec3Dot(v, nrm) * 2.0f;
    v3_mul(&proj, nrm, scl);
    v3_sub(out, v, &proj);
}

/* 0x00cb4508 b_earth, 0x00cc4d80 b_eden, 0x00cb30b0 b_mars, 0x00cb77e0 b_uranus, 0x00cb8150 b_venus */
void vec_set_length(Vec3 *v, float len)
{
    Vec3 unit;

    if (!v3_is_zero(v)) {
        vec3_normalize(&unit, v);
        v3_mul(v, &unit, len);
    }
}

/* 0x00cb45c0 b_earth, 0x00cc4e38 b_eden, 0x00cb3168 b_mars, 0x00cb7898 b_uranus, 0x00cb8208 b_venus */
void vec_reflect_normalize(Vec3 *v, Vec3 *nrm, Vec3 *out)
{
    Vec3 proj;
    float scl;

    if (v3_is_zero(v)) {
        vec3_normalize(out, nrm);
        return;
    }
    scl = (ulVec3Dot(v, nrm) * 2.0f) / (nrm->x * nrm->x + nrm->y * nrm->y + nrm->z * nrm->z);
    v3_mul(&proj, nrm, scl);
    v3_sub(out, v, &proj);
    vec3_normalize(out, out);
}

/* 0x00cb46e8 b_earth, 0x00cc4f60 b_eden, 0x00cb3290 b_mars, 0x00cb79c0 b_uranus, 0x00cb8330 b_venus */
void vec_up_for_dir(Vec3 *dir, Vec3 *up, int roll)
{
    int ang[2];
    float sc[8];

    if (v3_is_zero(dir)) {
        dir->z = -1.0f;
        dir->x = dir->y = 0.0f;
    }
    up->y = 1.0f;
    up->x = up->z = 0.0f;
    if (fabsf(ulVec3Dot(up, dir)) < FLT_EPSILON) {
        up->z = 1.0f;
        up->y = 0.0f;
        ROT_Y(roll, sc, up->x, up->z);
        return;
    }
    ROT_Z(roll, (sc + 2), up->x, up->y);
    ang[1] = RAD_TO_ANG(atan2f(-dir->x, -dir->z));
    ang[0] = RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    ROT_X(ang[0], (sc + 4), up->y, up->z);
    ROT_Y(ang[1], (sc + 6), up->x, up->z);
}

/* 0x00cb4988 b_earth, 0x00cc5200 b_eden, 0x00cb3530 b_mars, 0x00cb7c60 b_uranus, 0x00cb85d0 b_venus */
void vec_copy(Vec3 *src, Vec3 *dst)
{
    *dst = *src;
}

/* 0x00cb49a8 b_earth, 0x00cc5220 b_eden, 0x00cb3550 b_mars, 0x00cb7c80 b_uranus, 0x00cb85f0 b_venus */
int sphere_push_inside(Vec3 *pos, Vec3 *ctr, float rad)
{
    Vec3 ofs;
    float len;

    v3_sub(&ofs, pos, ctr);
    len = vec3_length(&ofs);
    if (rad < len)
        return 0;
    if (fabsf(len) < FLT_EPSILON) {
        v3_set(&ofs, rad * 0.0f, rad, rad * 0.0f);
        v3_add(pos, &ofs);
    } else {
        v3_scale(&ofs, rad / len);
        v3_add(pos, &ofs);
    }
    return 1;
}

/* 0x00cb4ad0 b_earth, 0x00cc5348 b_eden, 0x00cb3678 b_mars, 0x00cb7da8 b_uranus, 0x00cb8718 b_venus */
int segment_sphere_hit(Vec3 *beg, Vec3 *end, Vec3 *ctr, Vec3 *out, float rad)
{
    Vec3 ofs;
    Vec3 dir;
    float qa, qb, t, t_hi, t_lo;

    v3_sub(&ofs, beg, ctr);
    v3_sub(&dir, end, beg);
    qa = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    if (fabsf(qa) < FLT_EPSILON)
        return 0;
    qb = ofs.x * dir.x + ofs.y * dir.y + ofs.z * dir.z;
    t = qb * qb - qa * (ofs.x * ofs.x + ofs.y * ofs.y + ofs.z * ofs.z - rad * rad);
    if (!out) {
        if (0.0f <= t)
            return 1;
        return 0;
    }
    if (t > 0.0f) {
        t = sqrtf(t);
        t_hi = (-qb + t) / qa;
        t_lo = (-qb - t) / qa;
        if (t_hi <= 1.0f && t_hi > 0.0f) {
            if (t_lo <= 1.0f && t_lo > 0.0f) {
                if (t_lo < t)
                    t = t_lo;
                else
                    t = t_hi;
            } else
                t = t_hi;
        } else if (t_lo <= 1.0f && t_lo > 0.0f)
            t = t_lo;
        else
            return 0;
    } else if (fabsf(t) < FLT_EPSILON)
        t = -qb / qa;
    else
        return 0;
    v3_scale(&dir, t);
    v3_sum(out, beg, &dir);
    return 1;
}

/* 0x00cb4d58 b_earth, 0x00cc55d0 b_eden, 0x00cb3900 b_mars, 0x00cb8030 b_uranus, 0x00cb89a0 b_venus */
int segment_circle_xz_hit(Vec3 *beg, Vec3 *end, Vec3 *ctr, float rad)
{
    float ax, az, dx, dz, qa, qb, disc, sq, t;

    rad = rad * rad;
    dx = beg->x - ctr->x;
    dz = beg->z - ctr->z;
    if (dx * dx + dz * dz < rad)
        return 1;
    dx = end->x - ctr->x;
    dz = end->z - ctr->z;
    if (dx * dx + dz * dz < rad)
        return 1;
    ax = beg->x - ctr->x;
    az = beg->z - ctr->z;
    dx = end->x - beg->x;
    dz = end->z - beg->z;
    qa = dx * dx + dz * dz;
    qb = dx * ax + dz * az;
    disc = qb * qb - qa * (ax * ax + az * az - rad);
#ifdef VERSION_US
    /* a tangent misses, and so does a segment of (almost) no length */
    if (disc <= 0.0f)
        return 0;
    sq = sqrtf(disc);
    if (fabsf(qa) < FLT_EPSILON)
        return 0;
#else
    if (disc < 0.0f)
        return 0;
    sq = sqrtf(disc);
#endif
    t = (-qb + sq) / qa;
    if (0.0f <= t && t <= 1.0f)
        return 1;
    t = (-qb - sq) / qa;
    if (0.0f <= t && t <= 1.0f)
        return 1;
    return 0;
}

/* 0x00cb4f00 b_earth, 0x00cc5778 b_eden, 0x00cb3aa8 b_mars, 0x00cb81d8 b_uranus, 0x00cb8b48 b_venus */
int segment_point_near(Vec3 *beg, Vec3 *end, Vec3 *pt, Vec3 *out, float rad)
{
    Vec3 dir;
    float t, dsq;

    v3_sub(&dir, end, beg);
    t = (dir.x * (pt->x - beg->x) + dir.y * (pt->y - beg->y) + dir.z * (pt->z - beg->z)) / (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (t < 0.0f || 1.0f < t)
        return 0;
    if (out) {
        out->x = dir.x * t + beg->x;
        out->y = dir.y * t + beg->y;
        out->z = dir.z * t + beg->z;
        dsq = (pt->x - out->x) * (pt->x - out->x) + (pt->y - out->y) * (pt->y - out->y) + (pt->z - out->z) * (pt->z - out->z);
    } else
        {
            dsq = (pt->x - (dir.x * t + beg->x)) * (pt->x - (dir.x * t + beg->x)) + (pt->y - (dir.y * t + beg->y)) * (pt->y - (dir.y * t + beg->y)) + (pt->z - (dir.z * t + beg->z)) * (pt->z - (dir.z * t + beg->z));
        }
    if (fabsf(dsq) < FLT_EPSILON)
        return 1;
    if (!(rad < sqrtf(dsq)))
        return 1;
    return 0;
}

/* 0x00cb50b0 b_earth, 0x00cc5928 b_eden, 0x00cb3c58 b_mars, 0x00cb8388 b_uranus, 0x00cb8cf8 b_venus */
void collide_xz(Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float ma, float mb)
{
    float sc[2];
    float vax = va->x;
    float vaz = va->z;
    float vbx = vb->x;
    float vbz = vb->z;
    float na, nb, ta, tb, mom, ene, sq, sol_a, sol_b;
    Vec3 *v;

    {
        float ax = pa->x, bx = pb->x, az = pa->z, bz = pb->z;

        ulSinCos((int)RAD_TO_ANG(atan2f(bz - az, bx - ax)), &sc[0], &sc[1]);
    }
    na = vax * sc[1] + vaz * sc[0];
    nb = vbx * sc[1] + vbz * sc[0];
    mom = ma * na + mb * nb;
    ene = ma * 0.5f * na * na + mb * 0.5f * nb * nb;
    ta = vaz * sc[1] - vax * sc[0];
    tb = vbz * sc[1] - vbx * sc[0];
    sq = sqrtf(mom * mom - (ma / mb + 1.0f) * (ene * -2.0f * ma + mom * mom));
    sol_b = mom + sq;
    sol_a = (mom - sq) / (ma + mb);
    sol_b = sol_b / (ma + mb);
    if (fabsf(sol_a - nb) > fabsf(sol_b - nb)) {
        nb = sol_a;
        v = va;
    } else {
        nb = sol_b;
        v = va;
    }
    na = (mom - mb * nb) / ma;
    v->x = na * sc[1] - ta * sc[0];
    va->z = na * sc[0] + ta * sc[1];
    vb->x = nb * sc[1] - tb * sc[0];
    vb->z = nb * sc[0] + tb * sc[1];
}

/* 0x00cb52f8 b_earth, 0x00cc5b70 b_eden, 0x00cb3ea0 b_mars, 0x00cb85d0 b_uranus, 0x00cb8f40 b_venus */
int spheres_separate(Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float rad_a, float mb)
{
    Vec3 dir;
    int ang[2];
    Vec3 ofs;
    Vec3 nrm;
    float pen;
    float *trans;

    v3_sub(&dir, pb, pa);
    pen = (vec3_length(&dir) - (rad_a + mb)) * -0.5f;
    if (pen <= 0.0f)
        return 0;
    ofs.x = -pa->x;
    ofs.y = -pa->y;
    ofs.z = -pa->z;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_x(ulMatrixCur, -ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -ang[1]);
    trans = &ofs.x;
    vu0_matrix_translate(ulMatrixCur, trans[0], trans[1], trans[2]);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pa->x, &pa->x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pb->x, &pb->x);
    ulMatrixApplyVector((float *)ulMatrixCur, &va->x, &va->x);
    ulMatrixApplyVector((float *)ulMatrixCur, &vb->x, &vb->x);
    collide_xz(pa, pb, va, vb, 1.0f, 1.0f);
    matrix_unit_set(0);
    ofs.x = -ofs.x;
    ofs.y = -ofs.y;
    ofs.z = -ofs.z;
    do {
        vu0_matrix_translate(ulMatrixCur, trans[0], trans[1], trans[2]);
    } while (0);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pa->x, &pa->x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &pb->x, &pb->x);
    ulMatrixApplyVector((float *)ulMatrixCur, &va->x, &va->x);
    ulMatrixApplyVector((float *)ulMatrixCur, &vb->x, &vb->x);
    ulMatrixPop();
    vec3_normalize(&nrm, va);
    v3_scale(&nrm, pen);
    v3_add(pa, &nrm);
    vec3_normalize(&nrm, vb);
    v3_scale(&nrm, pen);
    v3_add(pb, &nrm);
    return 1;
}

/* 0x00cb5680 b_earth, 0x00cc5ef8 b_eden, 0x00cb4228 b_mars, 0x00cb8958 b_uranus, 0x00cb92c8 b_venus */
int spheres_separate_k(Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float rad_a, float rb, float elas)
{
    Vec3 dir;
    int ang[2];
    Vec3 ofs;
    Vec3 nrm;
    float sc[16];
    float pen, vz;

    v3_sub(&dir, pb, pa);
    pen = (rad_a + rb) - vec3_length(&dir);
    if (pen <= 0.0f)
        return 0;
    ofs.x = -pa->x;
    ofs.y = -pa->y;
    ofs.z = -pa->z;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ROT_Y(-ang[1], sc, va->x, va->z);
    ROT_X(-ang[0], (sc + 2), va->y, va->z);
    ROT_Y(-ang[1], (sc + 4), vb->x, vb->z);
    ROT_X(-ang[0], (sc + 6), vb->y, vb->z);
    vz = va->z;
    va->z = vb->z * elas;
    vb->z = vz * elas;
    ROT_X(ang[0], (sc + 8), va->y, va->z);
    ROT_Y(ang[1], (sc + 10), va->x, va->z);
    ROT_X(ang[0], (sc + 12), vb->y, vb->z);
    ROT_Y(ang[1], (sc + 14), vb->x, vb->z);
    vec3_normalize(&nrm, va);
    v3_scale(&nrm, pen);
    v3_add(pa, &nrm);
    vec3_normalize(&nrm, vb);
    v3_scale(&nrm, pen);
    v3_add(pb, &nrm);
    return 1;
}

/* 0x00cb5af0 b_earth, 0x00cc6368 b_eden, 0x00cb4698 b_mars, 0x00cb8dc8 b_uranus, 0x00cb9738 b_venus */
int spheres_separate_mass(Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float ma, float mb, float elas)
{
    Vec3 dir;
    int ang[2];
    Vec3 ofs;
    Vec3 nrm;
    float sc[16];
    float pen, vz, msum;

    msum = ma + mb;
    v3_sub(&dir, pb, pa);
    pen = msum - vec3_length(&dir);
    if (pen <= 0.0f)
        return 0;
    ofs.x = -pa->x;
    ofs.y = -pa->y;
    ofs.z = -pa->z;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ROT_Y(-ang[1], sc, va->x, va->z);
    ROT_X(-ang[0], (sc + 2), va->y, va->z);
    ROT_Y(-ang[1], (sc + 4), vb->x, vb->z);
    ROT_X(-ang[0], (sc + 6), vb->y, vb->z);
    vz = va->z;
    va->z = vz + mb * (elas + 1.0f) * (vb->z - vz) / msum;
    vb->z = vb->z + ma * (elas + 1.0f) * (vz - vb->z) / msum;
    ROT_X(ang[0], (sc + 8), va->y, va->z);
    ROT_Y(ang[1], (sc + 10), va->x, va->z);
    ROT_X(ang[0], (sc + 12), vb->y, vb->z);
    ROT_Y(ang[1], (sc + 14), vb->x, vb->z);
    vec3_normalize(&nrm, va);
    v3_scale(&nrm, pen);
    v3_add(pa, &nrm);
    vec3_normalize(&nrm, vb);
    v3_scale(&nrm, pen);
    v3_add(pb, &nrm);
    return 1;
}

/* 0x00cb5fb0 b_earth, 0x00cc6828 b_eden, 0x00cb4b58 b_mars, 0x00cb9288 b_uranus, 0x00cb9bf8 b_venus */
void matrix_to_angles(float *mtx, int *ang)
{
    Vec3 fwd;
    Vec3 up;

    ulMatrixApplyVector(mtx, &vec_forward.x, &fwd.x);
    ulMatrixApplyVector(mtx, &vec_up.x, &up.x);
    angles_from_dirs(&fwd, &up, ang);
}

/* 0x00cb6020 b_earth, 0x00cc6898 b_eden, 0x00cb4bc8 b_mars, 0x00cb92f8 b_uranus, 0x00cb9c68 b_venus */
void matrix_saved_pose(Vec3 *pos, int *ang)
{
    float mtx[16];

    if (pos)
        ulMatrixGetTranslation((float *)ulMatrixCur, &pos->x);
    matrix_get_or_set(0, mtx);
    matrix_to_angles(mtx, ang);
}

/* 0x00cb6070 b_earth, 0x00cc68e8 b_eden, 0x00cb4c18 b_mars, 0x00cb9348 b_uranus, 0x00cb9cb8 b_venus */
void matrix_current_pose(Vec3 *pos, int *ang)
{
    Vec3 fwd;
    Vec3 up;

    if (pos)
        ulMatrixGetTranslation((float *)ulMatrixCur, &pos->x);
    if (ang) {
        ulMatrixApplyVector((float *)ulMatrixCur, &vec_forward2.x, &fwd.x);
        ulMatrixApplyVector((float *)ulMatrixCur, &vec_up2.x, &up.x);
        angles_from_dirs(&fwd, &up, ang);
    }
}

/* 0x00cb60f8 b_earth, 0x00cc6970 b_eden, 0x00cb4ca0 b_mars, 0x00cb93d0 b_uranus, 0x00cb9d40 b_venus */
void angles_turn_y(int *ang, int yaw, int *out)
{
    ulMatrixPush();
    matrix_unit_set(0);
    boss_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, yaw);
    matrix_current_pose(0, out);
    ulMatrixPop();
}

/* 0x00cb6160 b_earth, 0x00cc69d8 b_eden, 0x00cb4d08 b_mars, 0x00cb9438 b_uranus, 0x00cb9da8 b_venus */
void angles_blend(int *ang_a, int *ang_b, int *out, float t)
{
    Vec3 fa, ua, fb, ub;
    float inv_t;
    float off_ax;
    float up_y;
    float fwd_z;

    do {
        off_ax = 0.0f;
        fwd_z = -1.0f;
    } while (0);
    fa.y = off_ax;
    fa.x = off_ax;
    fa.z = fwd_z;
    fb.y = off_ax;
    do { fb.z = fwd_z; } while (0);
    do { fb.x = off_ax; } while (0);
    ua.z = off_ax;
    up_y = 1.0f;
    ua.x = off_ax;
    ua.y = up_y;
    do { ub.z = off_ax; } while (0);
    ub.x = off_ax;
    do { ub.y = up_y; } while (0);
    vec_rotate_zxy(&fa, ang_a);
    vec_rotate_zxy(&ua, ang_a);
    vec_rotate_zxy(&fb, ang_b);
    vec_rotate_zxy(&ub, ang_b);
    inv_t = up_y - t;
    fb.x = fa.x * t + fb.x * inv_t;
    fb.y = fa.y * t + fb.y * inv_t;
    fb.z = fa.z * t + fb.z * inv_t;
    ub.x = ua.x * t + ub.x * inv_t;
    ub.y = ua.y * t + ub.y * inv_t;
    ub.z = ua.z * t + ub.z * inv_t;
    angles_from_dirs(&fb, &ub, out);
}

/* 0x00cb62d8 b_earth, 0x00cc6b50 b_eden, 0x00cb4e80 b_mars, 0x00cb95b0 b_uranus, 0x00cb9f20 b_venus */
void vec_rotate_zxy(Vec3 *v, int *ang)
{
    float sc[6];

    ROT_Z(ang[2], (sc + 0), v->x, v->y);
    ROT_X(ang[0], (sc + 2), v->y, v->z);
    ROT_Y(ang[1], (sc + 4), v->x, v->z);
}

/* 0x00cb63e0 b_earth, 0x00cc6c58 b_eden, 0x00cb4f88 b_mars, 0x00cb96b8 b_uranus, 0x00cba028 b_venus */
void angles_from_dirs(Vec3 *fwd, Vec3 *up, int *ang)
{
    float sc[2][2];
    float x, y, z;

    do {
        ang[1] = RAD_TO_ANG(atan2f(-fwd->x, -fwd->z));
        ang[0] = RAD_TO_ANG(atan2f(fwd->y, sqrtf(fwd->x * fwd->x + fwd->z * fwd->z)));
    } while (0);
    x = up->x;
    y = up->y;
    z = up->z;
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    ang[2] = RAD_TO_ANG(atan2f(-x, y));
}

/* 0x00cb6568 b_earth, 0x00cc6de0 b_eden, 0x00cb5110 b_mars, 0x00cb9840 b_uranus, 0x00cba1b0 b_venus */
void quat_from_axis_angle(int ang, float *quat, float x, float y, float z)
{
    float sc[2];

    ulSinCos(ang >> 1, &sc[0], &sc[1]);
    quat[0] = sc[1];
    quat[1] = sc[0] * x;
    quat[2] = sc[0] * y;
    quat[3] = sc[0] * z;
}

/* 0x00cb65e0 b_earth, 0x00cc6e58 b_eden, 0x00cb5188 b_mars, 0x00cb98b8 b_uranus, 0x00cba228 b_venus */
void chain_step(BossChain *node)
{
    BossChain *cur;
    Vec3 ofs;
    Vec3 npos;

    for (cur = node->next; cur; cur = node->next) {
        v3_mul(&ofs, &node->dir, node->len);
        v3_sum(&ofs, &node->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_scale(&cur->vel, cur->damp);
        if (node->cb)
            node->cb(node, 0);
        v3_sum(&npos, &cur->vel, &npos);
        v3_sub(&cur->dir, &npos, &node->pos);
        vec3_normalize(&cur->dir, &cur->dir);
        v3_scale(&cur->dir, cur->len);
        v3_copy(&npos, &cur->pos);
        v3_sum(&cur->pos, &node->pos, &cur->dir);
        node = node->next;
        v3_sub(&cur->vel, &cur->pos, &npos);
    }
}

/* 0x00cb6848 b_earth, 0x00cc70c0 b_eden, 0x00cb53f0 b_mars, 0x00cb9b20 b_uranus, 0x00cba490 b_venus */
BossChain *chain_step_fwd(BossChain *node, BossChain *src, BossChain *dst, int *count)
{
    BossChain *cur;
    Vec3 ofs;
    Vec3 npos;
    int cnt;

    cnt = 1;
    for (cur = node->next; cur; cur = node->next) {
        v3_mul(&ofs, &src->dir, node->len);
        v3_sum(&ofs, &src->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_mul(&dst->vel, &cur->vel, cur->damp);
        if (node->cb)
            node->cb(node, dst);
        v3_sum(&npos, &dst->vel, &npos);
        v3_sub(&dst->dir, &npos, &src->pos);
        vec3_normalize(&dst->dir, &dst->dir);
        cnt++;
        v3_scale(&dst->dir, cur->len);
        node = node->next;
        v3_sum(&dst->pos, &src->pos, &dst->dir);
        dst++;
        src++;
    }
    *count = cnt;
    return node;
}

/* 0x00cb6a80 b_earth, 0x00cc72f8 b_eden, 0x00cb5628 b_mars, 0x00cb9d58 b_uranus, 0x00cba6c8 b_venus */
void chain_step_back(BossChain *node, BossChain *src, BossChain *dst)
{
    BossChain *cur;
    Vec3 ofs;
    Vec3 npos;

    for (cur = node->prev; cur; cur = node->prev) {
        v3_mul(&ofs, &src->dir, -node->len);
        v3_sum(&ofs, &src->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_mul(&dst->vel, &cur->vel, cur->damp);
        if (node->cb)
            node->cb(node, dst);
        v3_sum(&npos, &dst->vel, &npos);
        v3_sub(&dst->dir, &npos, &src->pos);
        vec3_normalize(&dst->dir, &dst->dir);
        v3_scale(&dst->dir, cur->len);
        node = node->prev;
        v3_sum(&dst->pos, &src->pos, &dst->dir);
        dst->dir.x = -dst->dir.x;
        dst->dir.y = -dst->dir.y;
        dst->dir.z = -dst->dir.z;
        dst--;
        src--;
    }
}

/* 0x00cb6cb0 b_earth, 0x00cc7528 b_eden, 0x00cb5858 b_mars, 0x00cb9f88 b_uranus, 0x00cba8f8 b_venus */
void chain_step_both(BossChain *node, BossChain *buf, float t)
{
    BossChain *head;
    BossChain *fwd;
    BossChain *back;
    BossChain *tail;
    Vec3 old_pos;
    int n;
    float rate;
    float step;

    head = node;
    v3_copy(&buf->pos, &node->pos);
    v3_copy(&buf->dir, &node->dir);
    v3_copy(&buf->vel, &node->vel);
    fwd = buf + 1;
    node = chain_step_fwd(node, buf, fwd, &n);
    back = buf + n;
    tail = &back[n - 1];
    v3_copy(&tail->pos, &node->pos);
    v3_copy(&tail->dir, &node->dir);
    v3_copy(&tail->vel, &node->vel);
    chain_step_back(node, tail, &back[n - 2]);
    step = -t / n;
    rate = 1.0f + step;
    for (node = head->next; node; node = node->next) {
        v3_copy(&old_pos, &node->pos);
        v3_lerp(&node->pos, &fwd->pos, &back[1].pos, rate);
        v3_lerp(&node->dir, &fwd->dir, &back[1].dir, rate);
        rate += step;
        v3_sub(&node->vel, &node->pos, &old_pos);
        fwd++;
        back++;
    }
}

/* 0x00cb6f30 b_earth, 0x00cc77a8 b_eden, 0x00cb5ad8 b_mars, 0x00cba208 b_uranus, 0x00cbab78 b_venus */
void shuffle_ints(int *arr, int n)
{
    int i;
    int j;
    int val;

    for (i = 0; i < n; i++) {
        val = arr[i];
        j = (int)(RAND_FLOAT_A() * n);
        arr[i] = arr[j];
        arr[j] = val;
    }
}

/* 0x00cb6fe8 b_earth, 0x00cc7860 b_eden, 0x00cb5b90 b_mars, 0x00cba2c0 b_uranus, 0x00cbac30 b_venus */
/* steer(cast) */
unsigned int boss_color_blend(unsigned int col_a, unsigned int col_b, float t)
{
    float inv_t;
    int al, r, g, bl;

    inv_t = 1.0f - t;
    al = (int)(color_get_a(col_a) * t + color_get_a(col_b) * inv_t);
    r = (int)((int)((unsigned int (*)(int))color_get_r)(col_a) * t + (int)((unsigned int (*)(int))color_get_r)(col_b) * inv_t);
    g = (int)(color_get_g(col_a) * t + color_get_g(col_b) * inv_t);
    bl = (int)(color_get_b(col_a) * t + color_get_b(col_b) * inv_t);
    return (al + 1) >> 1 << 24 | ((bl + 1) >> 1 & 0xff) << 16 | ((g + 1) >> 1 & 0xff) << 8 | ((r + 1) >> 1 & 0xff);
}

/* 0x00cb7168 b_earth, 0x00cc79e0 b_eden, 0x00cb5d10 b_mars, 0x00cba440 b_uranus, 0x00cbadb0 b_venus */
unsigned int color_lerp(unsigned int col_a, unsigned int col_b, float t)
{
    float inv_t;
    int al_a, al_b;
    int al, r, g, bl;

    inv_t = 1.0f - t;
    al_a = color_get_a(col_a);
    al_b = color_get_a(col_b);
    al = (int)(al_a * t + al_b * inv_t);
    r = (int)((col_a & 0xff) * t + (col_b & 0xff) * inv_t);
    g = (int)((col_a >> 8 & 0xff) * t + (col_b >> 8 & 0xff) * inv_t);
    bl = (int)((col_a >> 16 & 0xff) * t + (col_b >> 16 & 0xff) * inv_t);
    if (al_a + al_b >= 255)
        return 0x80000000 | (bl & 0xff) << 16 | (g & 0xff) << 8 | (r & 0xff);
    return (al + 1) >> 1 << 24 | (bl & 0xff) << 16 | (g & 0xff) << 8 | (r & 0xff);
}

/* 0x00cb73c0 b_earth, 0x00cc7c38 b_eden, 0x00cb5f68 b_mars, 0x00cba698 b_uranus, 0x00cbb008 b_venus */
void fog_blend(BossFog *fog_a, BossFog *fog_b, BossFog *out, float t)
{
    out->col = boss_color_blend(fog_a->col, fog_b->col, t);
    out->back = boss_color_blend(fog_a->back, fog_b->back, t);
    out->near = fog_a->near * t + fog_b->near * (1.0f - t);
    out->far = fog_a->far * t + fog_b->far * (1.0f - t);
}

/* 0x00cb7468 b_earth, 0x00cc7ce0 b_eden, 0x00cb6010 b_mars, 0x00cba740 b_uranus, 0x00cbb0b0 b_venus */
void fog_get(BossFog *fog)
{
    fog->back = fog_get_back_color();
    fog->col = fog_get_color();
    fog->near = fog_get_near();
    fog->far = fog_get_far();
}

/* 0x00cb74a8 b_earth, 0x00cc7d20 b_eden, 0x00cb6050 b_mars, 0x00cba780 b_uranus, 0x00cbb0f0 b_venus */
/* steer(cast) */
int fog_apply(BossFog *fog, int mode)
{
    do {
        BackColReq(fog->back);
    } while (0);
    fog_set(fog->col, mode, fog->near, fog->far);
    return (int)fog_set_enable((void *)1);
}

/* 0x00cb74f8 b_earth, 0x00cc7d70 b_eden, 0x00cb60a0 b_mars, 0x00cba7d0 b_uranus, 0x00cbb140 b_venus */
void lock_cursor_world(float *out, float z)
{
    float cpos[2];

    lockon_get_cursor_pos(&cpos[0], &cpos[1]);
    out[0] = (cpos[0] - screen_cfg.cx) / screen_cfg.sx;
    out[1] = (cpos[1] - screen_cfg.cy) / screen_cfg.sy;
    out[2] = z;
    ulMatrixApplyPoint((float *)camera_get_inv_mtx(), out, out);
}

/* 0x00cb7590 b_earth, 0x00cc7e08 b_eden, 0x00cb6138 b_mars, 0x00cba868 b_uranus, 0x00cbb1d8 b_venus */
int segment_on_screen(Vec3 *beg, Vec3 *end)
{
    ScreenCfg *scr = &screen_cfg;
    float clip_z, t, inv_z, x, y;

    clip_z = -cam_near;
    if (clip_z < beg->z && clip_z < end->z)
        return 0;
    if (clip_z < beg->z) {
        t = (clip_z - beg->z) / (end->z - beg->z);
        beg->z = clip_z;
        beg->x += (end->x - beg->x) * t;
        beg->y += (end->y - beg->y) * t;
    } else if (clip_z < end->z) {
        t = (clip_z - end->z) / (beg->z - end->z);
        end->z = clip_z;
        end->x += (beg->x - end->x) * t;
        end->y += (beg->y - end->y) * t;
    }
    inv_z = -1.0f / beg->z;
    x = beg->x * inv_z * scr->sx + scr->cx;
    y = beg->y * inv_z * scr->sy + scr->cy;
    if (x > 1000.0f)
        return 0;
    if (x < -1000.0f)
        return 0;
    if (y > 1000.0f)
        return 0;
    if (y < -1000.0f)
        return 0;
    inv_z = -1.0f / end->z;
    x = end->x * inv_z * scr->sx + scr->cx;
    y = end->y * inv_z * scr->sy + scr->cy;
    if (x > 1000.0f)
        return 0;
    if (x < -1000.0f)
        return 0;
    if (y > 1000.0f)
        return 0;
    if (y < -1000.0f)
        return 0;
    return 1;
}

/* 0x00cb7780 b_earth, 0x00cc7ff8 b_eden, 0x00cb6328 b_mars, 0x00cbaa58 b_uranus, 0x00cbb3c8 b_venus */
int segment_project(Vec3 *beg, Vec3 *end, float *rad_a, float *rb)
{
    ScreenCfg *scr = &screen_cfg;
    float clip_z, t, inv_z, x, y;

    clip_z = -cam_near;
    if (clip_z < beg->z && clip_z < end->z)
        return 0;
    if (clip_z < beg->z) {
        t = (clip_z - beg->z) / (end->z - beg->z);
        beg->z = clip_z;
        beg->x += (end->x - beg->x) * t;
        beg->y += (end->y - beg->y) * t;
    } else if (clip_z < end->z) {
        t = (clip_z - end->z) / (beg->z - end->z);
        end->z = clip_z;
        end->x += (beg->x - end->x) * t;
        end->y += (beg->y - end->y) * t;
    }
    inv_z = -1.0f / beg->z;
    x = beg->x * inv_z * scr->sx + scr->cx;
    y = beg->y * inv_z * scr->sy + scr->cy;
    if (x > 1000.0f)
        return 0;
    if (x < -1000.0f)
        return 0;
    if (y > 1000.0f)
        return 0;
    if (y < -1000.0f)
        return 0;
    beg->y = y;
    beg->x = x;
    beg->z = inv_z;
    *rad_a *= fabsf(inv_z * scr->sx);
    inv_z = -1.0f / end->z;
    x = end->x * inv_z * scr->sx + scr->cx;
    y = end->y * inv_z * scr->sy + scr->cy;
    if (x > 1000.0f)
        return 0;
    if (x < -1000.0f)
        return 0;
    if (y > 1000.0f)
        return 0;
    if (y < -1000.0f)
        return 0;
    end->y = y;
    end->x = x;
    end->z = inv_z;
    *rb *= fabsf(inv_z * scr->sx);
    return 1;
}

/* 0x00cb79c0 b_earth, 0x00cc8238 b_eden, 0x00cb6568 b_mars, 0x00cbac98 b_uranus, 0x00cbb608 b_venus */
int sphere_on_screen(float *pos, float rad)
{
    ScreenCfg *scr = &screen_cfg;
    float inv_z, srad, x, y;

    if (-cam_near < pos[2])
        return 0;
    inv_z = -1.0f / pos[2];
    x = pos[0] * inv_z * scr->sx + scr->cx;
    y = pos[1] * inv_z * scr->sy + scr->cy;
    srad = fabsf(inv_z * scr->sx) * rad;
    if (x + srad > 1000.0f)
        return 0;
    if (x - srad < -1000.0f)
        return 0;
    if (y + srad > 1000.0f)
        return 0;
    if (y - srad < -1000.0f)
        return 0;
    return 1;
}

/* 0x00cb7aa0 b_earth, 0x00cc8318 b_eden, 0x00cb6648 b_mars, 0x00cbad78 b_uranus, 0x00cbb6e8 b_venus */
int line_quad(Vec3 *beg, Vec3 *end, TaVtx *v, float width)
{
    Vec3 pa, pb;
    float rad_a, rb;
    float dx, dy, len;

    ulMatrixApplyPoint((float *)ulMatrixCur, &beg->x, &pa.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &end->x, &pb.x);
    rad_a = rb = width;
    if (segment_project(&pa, &pb, &rad_a, &rb)) {
        dx = pb.x - pa.x;
        dy = pb.y - pa.y;
        len = sqrtf(dx * dx + dy * dy);
        if (len != 0.0f) {
            dx /= len;
            dy /= len;
        } else {
            dx = 0.0f;
            dy = 1.0f;
        }
        v->x = pa.x - dy * rad_a;
        v->y = pa.y + dx * rad_a;
        v->q = pa.z;
        v++;
        v->x = pa.x + dy * rad_a;
        v->y = pa.y - dx * rad_a;
        v->q = pa.z;
        v++;
        v->x = pb.x - dy * rb;
        v->y = pb.y + dx * rb;
        v->q = pb.z;
        v++;
        v->x = pb.x + dy * rb;
        v->y = pb.y - dx * rb;
        v->q = pb.z;
        return 1;
    }
    return 0;
}

/* 0x00cb7c58 b_earth, 0x00cc84d0 b_eden, 0x00cb6800 b_mars, 0x00cbaf30 b_uranus, 0x00cbb8a0 b_venus */
int strip_segment(Vec3 *beg, Vec3 *end, TaVtx *v, float width, float *keep, int mode)
{
    Vec3 pa, pb;
    float rad_a, rb;
    float dx, dy, len;
    float *dst;

    ulMatrixApplyPoint((float *)ulMatrixCur, &beg->x, &pa.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &end->x, &pb.x);
    rad_a = rb = width;
    if (segment_project(&pa, &pb, &rad_a, &rb)) {
        dx = pb.x - pa.x;
        dy = pb.y - pa.y;
        len = sqrtf(dx * dx + dy * dy);
        if (len != 0.0f) {
            dx /= len;
            dy /= len;
        } else {
            dx = 0.0f;
            dy = 1.0f;
        }
        switch (mode) {
        case 0:
            keep += 5;
            v->x = pa.x - dy * rad_a;
            v->y = pa.y + dx * rad_a;
            v->q = pa.z;
            v++;
            v->x = pa.x + dy * rad_a;
            v->y = pa.y - dx * rad_a;
            v->q = pa.z;
            v++;
            *keep-- = v->x = pb.x - dy * rb;
            *keep-- = v->y = pb.y + dx * rb;
            *keep-- = v->q = pb.z;
            v++;
            *keep-- = v->x = pb.x + dy * rb;
            *keep-- = v->y = pb.y - dx * rb;
            *keep-- = v->q = pb.z;
            break;
        case 1:
            keep += 5;
            dst = keep;
            v->x = *keep--;
            v->y = *keep--;
            v->q = *keep--;
            v++;
            v->x = *keep--;
            v->y = *keep--;
            v->q = *keep--;
            v++;
            *dst-- = v->x = pb.x - dy * rb;
            *dst-- = v->y = pb.y + dx * rb;
            *dst-- = v->q = pb.z;
            v++;
            *dst-- = v->x = pb.x + dy * rb;
            *dst-- = v->y = pb.y - dx * rb;
            *dst-- = v->q = pb.z;
            break;
        }
        return 1;
    }
    return 0;
}

/* 0x00cb7f28 b_earth, 0x00cc87a0 b_eden, 0x00cb6ad0 b_mars, 0x00cbb200 b_uranus, 0x00cbbb70 b_venus */
int strip_segment_minw(Vec3 *beg, Vec3 *end, TaVtx *v, float width, float min, float *keep, int mode)
{
    Vec3 pa, pb;
    float rad_a, rb;
    float dx, dy, len;
    float *dst;

    ulMatrixApplyPoint((float *)ulMatrixCur, &beg->x, &pa.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &end->x, &pb.x);
    rad_a = rb = width;
    if (segment_project(&pa, &pb, &rad_a, &rb)) {
        if (rad_a < min)
            rad_a = min;
        if (rb < min)
            rb = min;
        dx = pb.x - pa.x;
        dy = pb.y - pa.y;
        len = sqrtf(dx * dx + dy * dy);
        if (len != 0.0f) {
            dx /= len;
            dy /= len;
        } else {
            dx = 0.0f;
            dy = 1.0f;
        }
        switch (mode) {
        case 0:
            keep += 5;
            v->x = pa.x - dy * rad_a;
            v->y = pa.y + dx * rad_a;
            v->q = pa.z;
            v++;
            v->x = pa.x + dy * rad_a;
            v->y = pa.y - dx * rad_a;
            v->q = pa.z;
            v++;
            *keep-- = v->x = pb.x - dy * rb;
            *keep-- = v->y = pb.y + dx * rb;
            *keep-- = v->q = pb.z;
            v++;
            *keep-- = v->x = pb.x + dy * rb;
            *keep-- = v->y = pb.y - dx * rb;
            *keep-- = v->q = pb.z;
            break;
        case 1:
            keep += 5;
            dst = keep;
            v->x = *keep--;
            v->y = *keep--;
            v->q = *keep--;
            v++;
            v->x = *keep--;
            v->y = *keep--;
            v->q = *keep--;
            v++;
            *dst-- = v->x = pb.x - dy * rb;
            *dst-- = v->y = pb.y + dx * rb;
            *dst-- = v->q = pb.z;
            v++;
            *dst-- = v->x = pb.x + dy * rb;
            *dst-- = v->y = pb.y - dx * rb;
            *dst-- = v->q = pb.z;
            break;
        }
        return 1;
    }
    return 0;
}

/* 0x00cb8220 b_earth, 0x00cc8a98 b_eden, 0x00cb6dc8 b_mars, 0x00cbb4f8 b_uranus, 0x00cbbe68 b_venus */
void quad_set_uv_color(TaVtx *v, unsigned int col_top, unsigned int col_bottom, float u_min, float v_min, float u_max, float v_max)
{
    v->u = u_min;
    v->v = v_min;
    v->color = col_top;
    v++;
    v->u = u_max;
    v->v = v_min;
    v->color = col_top;
    v++;
    v->u = u_min;
    v->v = v_max;
    v->color = col_bottom;
    v++;
    v->u = u_max;
    v->v = v_max;
    v->color = col_bottom;
}

/* 0x00cb8260 b_earth, 0x00cc8ad8 b_eden, 0x00cb6e08 b_mars, 0x00cbb538 b_uranus, 0x00cbbea8 b_venus */
void strip_draw(Vec3 *pts, int start, int n, unsigned int color, int tex, float width)
{
    float keep[6];
    TaVtx quad[4];
    unsigned int ca, cb;
    int i, j, k, ok;
    float fa, fb, fn;
    int segs, back;

    fn = segs = n - 1;
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    nj_tex_strip_begin(1);
    ca = cb = color;
    i = (start + n) % n;
    back = n - 1;
    j = (i + back) % n;
    quad_set_uv_color(quad, cb, cb, 0.0f, 1.0f, 1.0f, 1.0f - 1.0f / fn);
    ok = strip_segment(&pts[i], &pts[j], quad, width, keep, 0);
    if (ok)
        nj_tex_strip_vertices(quad, 4);
    for (k = 1; k < (n - 1); k++) {
        i = (start - k + n) % n;
        fa = 1.0f - k / (float)(n - 1);
        fb = 1.0f - (k + 1) / (float)(n - 1);
        ca &= 0xffffff;
        cb &= 0xffffff;
        ca |= (int)(fa * 255.0f) << 24;
        cb |= (int)(fb * 255.0f) << 24;
        back = n - 1;
        j = (i + back) % n;
        quad_set_uv_color(quad, ca, cb, 0.0f, fa, 1.0f, fb);
        if (ok)
            ok = strip_segment(&pts[i], &pts[j], quad, width, keep, 1);
        else
            ok = strip_segment(&pts[i], &pts[j], quad, width, keep, 0);
        if (ok)
            nj_tex_strip_vertices(quad, 4);
    }
    nj_tex_strip_end();
}

/* 0x00cb8520 b_earth, 0x00cc8d98 b_eden, 0x00cb70c8 b_mars, 0x00cbb7f8 b_uranus, 0x00cbc168 b_venus */
void strip_draw_alpha(Vec3 *pts, int start, int n, unsigned int color, int tex, float width, float min)
{
    float keep[6];
    TaVtx quad[4];
    unsigned int rgb, ca, cb;
    int i, j, k, ok, old_zt;
    float fa, fb, fn;
    int al;
    int segs, back;

    fn = segs = n - 1;
    old_zt = draw_set_ztest_mode(0x60000);
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    nj_tex_strip_begin(1);
    al = color_get_a(color);
    i = (start + n) % n;
    rgb = color & 0xffffff;
    ca = rgb | ((int)(al * (1.0f - 1.0f / fn)) + 1) >> 1 << 24;
    back = n - 1;
    j = (i + back) % n;
    quad_set_uv_color(quad, ca, ca, 0.0f, 0.0f, 1.0f, 1.0f);
    ok = strip_segment(&pts[i], &pts[j], quad, width, keep, 0);
    if (ok)
        nj_tex_strip_vertices(quad, 4);
    for (k = 1; k < (n - 1); k++) {
        i = (start - k + n) % n;
        fa = 1.0f - k / (float)(n - 1);
        fb = 1.0f - (k + 1) / (float)(n - 1);
        ca = (color & 0xffffff) | ((int)(al * fa) + 1) >> 1 << 24;
        cb = (color & 0xffffff) | ((int)(al * fb) + 1) >> 1 << 24;
        back = n - 1;
        j = (i + back) % n;
        quad_set_uv_color(quad, ca, cb, 0.0f, fa, 1.0f, fb);
        if (ok)
            ok = strip_segment_minw(&pts[i], &pts[j], quad, width, min, keep, 1);
        else
            ok = strip_segment_minw(&pts[i], &pts[j], quad, width, min, keep, 0);
        if (ok)
            nj_tex_strip_vertices(quad, 4);
    }
    nj_tex_strip_end();
    draw_set_ztest_mode(old_zt);
}

/* 0x00cb8828 b_earth, 0x00cc90a0 b_eden, 0x00cb73d0 b_mars, 0x00cbbb00 b_uranus, 0x00cbc470 b_venus */
int vtx_on_screen(TaVtx *v)
{
    if (v->x < -1000.0f)
        return 0;
    if (v->x > 1000.0f)
        return 0;
    if (v->y < -1000.0f)
        return 0;
    if (v->y > 1000.0f)
        return 0;
    return 1;
}

/* 0x00cb8888 b_earth, 0x00cc9100 b_eden, 0x00cb7430 b_mars, 0x00cbbb60 b_uranus, 0x00cbc4d0 b_venus */
void poly_draw_tex(TaVtx *src, int n)
{
    TaVtx vtx[4];
    TaVtx *dst;
    ScreenCfg *scr = &screen_cfg;
    float inv_z;
    int i;

    if (n >= 5)
        return;
    dst = vtx;
    for (i = 0; i < n; i++) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &src->x, &dst->x);
        if (-1.0f < dst->q)
            return;
        inv_z = -1.0f / dst->q;
        dst->q = inv_z;
        dst->x = dst->x * inv_z * scr->sx + scr->cx;
        dst->y = dst->y * inv_z * scr->sy + scr->cy;
        if (!vtx_on_screen(dst))
            return;
        dst->u = src->u;
        dst->v = src->v;
        dst->color = src->color;
        src++;
        dst++;
    }
    nj_tex_strip_vertices(vtx, n);
}

/* 0x00cb89b0 b_earth, 0x00cc9228 b_eden, 0x00cb7558 b_mars, 0x00cbbc88 b_uranus, 0x00cbc5f8 b_venus */
void tex_strip_begin(int mode)
{
    nj_tex_strip_begin(mode);
}

/* 0x00cb89b8 b_earth, 0x00cc9230 b_eden, 0x00cb7560 b_mars, 0x00cbbc90 b_uranus, 0x00cbc600 b_venus */
void tex_strip_end(void)
{
    nj_tex_strip_end();
}

/* 0x00cb89c0 b_earth, 0x00cc9238 b_eden, 0x00cb7568 b_mars, 0x00cbbc98 b_uranus, 0x00cbc608 b_venus */
void poly_draw_flat(TrailVtx *src, int n)
{
    TrailVtx vtx[4];
    TrailVtx *dst;
    ScreenCfg *scr = &screen_cfg;
    float inv_z;
    int i;

    if (n >= 5)
        return;
    dst = vtx;
    for (i = 0; i < n; i++) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &src->x, &dst->x);
        if (-1.0f < dst->q)
            return;
        inv_z = -1.0f / dst->q;
        dst->q = inv_z;
        dst->x = dst->x * inv_z * scr->sx + scr->cx;
        dst->y = dst->y * inv_z * scr->sy + scr->cy;
        if (!vtx_on_screen((TaVtx *)dst))
            return;
        dst->c = src->c;
        src++;
        dst++;
    }
    nj_color_strip_vertices(vtx, n);
}

/* 0x00cb8ad8 b_earth, 0x00cc9350 b_eden, 0x00cb7680 b_mars, 0x00cbbdb0 b_uranus, 0x00cbc720 b_venus */
void flat_strip_begin(int mode)
{
    nj_color_strip_begin(mode);
}

/* 0x00cb8ae0 b_earth, 0x00cc9358 b_eden, 0x00cb7688 b_mars, 0x00cbbdb8 b_uranus, 0x00cbc728 b_venus */
void flat_strip_end(void)
{
    nj_color_strip_end();
}

/* 0x00cb8ae8 b_earth, 0x00cc9360 b_eden, 0x00cb7690 b_mars, 0x00cbbdc0 b_uranus, 0x00cbc730 b_venus */
/* steer(cast) */
void lines_add_pairs(Vec3 *pts, int n)
{
    Vec3 *beg;

    for (; n > 0; n -= 2) {
        beg = pts++;
        lines_batch = ((void *(*)(void *, float, Vec3 *, Vec3 *))line_add_v_w)(lines_batch, lines_width, beg, pts++);
    }
}

/* 0x00cb8b60 b_earth, 0x00cc93d8 b_eden, 0x00cb7708 b_mars, 0x00cbbe38 b_uranus, 0x00cbc7a8 b_venus */
/* steer(cast) */
void lines_add_strip(Vec3 *pts, int n)
{
    Vec3 *beg;

    if (n < 2)
        return;
    beg = pts++;
    for (n--; n; n--) {
        lines_batch = ((void *(*)(void *, float, Vec3 *, Vec3 *))line_add_v_w)(lines_batch, lines_width, beg, pts);
        beg = pts++;
    }
}

/* 0x00cb8be0 b_earth, 0x00cc9458 b_eden, 0x00cb7788 b_mars, 0x00cbbeb8 b_uranus, 0x00cbc828 b_venus */
void lines_begin(float width, int color, int mode)
{
    void **lines = &lines_batch;

    lines_width = width + width;
    *lines = line_set_color(color);
}

/* 0x00cb8c18 b_earth, 0x00cc9490 b_eden, 0x00cb77c0 b_mars, 0x00cbbef0 b_uranus, 0x00cbc860 b_venus */
void lines_end(void)
{
    line_flush(lines_batch);
}

/* 0x00cb8c28 b_earth, 0x00cc94a0 b_eden, 0x00cb77d0 b_mars, 0x00cbbf00 b_uranus, 0x00cbc870 b_venus */
/* steer(cast) */
void tree_draw_culled_a(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (vis)
                    nj_cnk_direct_draw_clip(obj->model);
                else
                    nj_cnk_direct_draw_noclip(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_culled_a(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cb8d20 b_earth, 0x00cc9598 b_eden, 0x00cb78c8 b_mars, 0x00cbbff8 b_uranus, 0x00cbc968 b_venus */
/* steer(cast) */
void tree_draw_culled_b(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (vis)
                    nj_cnk_packet_draw_culled(obj->model);
                else
                    nj_cnk_packet_draw(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_culled_b(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cb8e18 b_earth, 0x00cc9690 b_eden, 0x00cb79c0 b_mars, 0x00cbc0f0 b_uranus, 0x00cbca60 b_venus */
void boss_set_texture_num(int tex)
{
    njSetTextureNum(tex);
}

/* 0x00cb8e20 b_earth, 0x00cc9698 b_eden, 0x00cb79c8 b_mars, 0x00cbc0f8 b_uranus, 0x00cbca68 b_venus */
void boss_set_screen_scale(float x, float y)
{
    nj_set_screen_scale(x, y);
}

/* 0x00cb8e28 b_earth, 0x00cc96a0 b_eden, 0x00cb79d0 b_mars, 0x00cbc100 b_uranus, 0x00cbca70 b_venus */
void tree_draw_culled_a_w(WireObj *obj)
{
    tree_draw_culled_a(obj);
}

/* 0x00cb8e30 b_earth, 0x00cc96a8 b_eden, 0x00cb79d8 b_mars, 0x00cbc108 b_uranus, 0x00cbca78 b_venus */
void tree_draw_culled_b_w(WireObj *obj)
{
    tree_draw_culled_b(obj);
}

/* 0x00cb8e38 b_earth, 0x00cc96b0 b_eden, 0x00cb79e0 b_mars, 0x00cbc110 b_uranus, 0x00cbca80 b_venus */
void draw_nop(Earth1stDemoWork *w)
{
}

/* 0x00cb8e40 b_earth, 0x00cc96b8 b_eden, 0x00cb79e8 b_mars, 0x00cbc118 b_uranus, 0x00cbca88 b_venus */
void segment_draw(int style, Vec3 *seg, int n)
{
    Vec3 p0;
    Vec3 p1;
    Vec3 pts[32];
    Vec3 dir;
    float len;
    int i, cnt;

    ulMatrixApplyPoint((float *)ulMatrixCur, &seg[0].x, &p0.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &seg[1].x, &p1.x);
    if (-1.0f < p0.z && -1.0f < p1.z)
        return;
    v3_sub(&dir, &seg[1], &seg[0]);
    if (v3_is_zero(&dir))
        return;
    len = vec3_length(&dir);
#ifdef VERSION_US
    cnt = (int)(len / 100.0f) + 1;
#else
    cnt = (int)(len * 0.015625f) + 1;
#endif
    if (cnt < 2) {
        draw_polyline_wide_capped(style, seg, n);
        return;
    }
    if (cnt >= 31)
        cnt = 31;
    pts[0] = seg[0];
    vec_set_length(&dir, len / cnt);
    for (i = 1; i <= cnt; i++)
        v3_sum_to(&pts[i - 1], &dir, &pts[i]);
    draw_polyline_wide_capped(style, pts, cnt);
}

/* 0x00cb9060 b_earth, 0x00cc98d8 b_eden, 0x00cb7c08 b_mars, 0x00cbc338 b_uranus, 0x00cbcca8 b_venus */
void quad_draw_split(TaVtx *quad, unsigned int color, float cx, float cy, float inv_z)
{
    float mx[4];
    float my[4];
    TaVtx vtx[4];

    memset(vtx, 0, sizeof(vtx));
    vtx[3].v = vtx[2].u = vtx[1].v = 1.0f;
    {
        struct xy { float x, y; };
        float x0 = (quad[0].x + quad[1].x) * 0.5f;
        float y0 = (quad[0].y + quad[1].y) * 0.5f;
        float x1 = (quad[1].x + quad[3].x) * 0.5f;
        float y1 = (quad[1].y + quad[3].y) * 0.5f;
        float x2 = (quad[3].x + quad[2].x) * 0.5f;
        float y2 = (quad[3].y + quad[2].y) * 0.5f;
        float x3 = (quad[2].x + quad[0].x) * 0.5f;
        float y3 = (quad[2].y + quad[0].y) * 0.5f;
        float px = quad[0].x;
        float py = quad[0].y;

        *(struct xy *)&vtx[0].x = (struct xy) { px, py };
        vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = color;
        vtx[0].q = vtx[1].q = vtx[2].q = vtx[3].q = inv_z;
        mx[1] = x1;
        my[1] = y1;
        mx[2] = x2;
        my[2] = y2;
        mx[0] = x0;
        my[0] = y0;
        mx[3] = x3;
        my[3] = y3;
        vtx[1].x = x0;
        vtx[1].y = y0;
        vtx[3].u = 1.0f;
        vtx[2].x = x3;
        vtx[2].y = y3;
        vtx[3].x = cx;
        vtx[3].y = cy;
    }
    nj_tex_strip_begin(1);
    nj_tex_strip_vertices(vtx, 4);
    vtx[0].x = quad[1].x;
    vtx[0].y = quad[1].y;
    vtx[2].x = mx[1];
    vtx[2].y = my[1];
    nj_tex_strip_vertices(vtx, 4);
    vtx[0].x = quad[3].x;
    vtx[0].y = quad[3].y;
    vtx[1].x = mx[2];
    vtx[1].y = my[2];
    nj_tex_strip_vertices(vtx, 4);
    vtx[0].x = quad[2].x;
    vtx[0].y = quad[2].y;
    vtx[2].x = mx[3];
    vtx[2].y = my[3];
    nj_tex_strip_vertices(vtx, 4);
    nj_tex_strip_end();
}

/* 0x00cb9250 b_earth, 0x00cc9ac8 b_eden, 0x00cb7df8 b_mars, 0x00cbc528 b_uranus, 0x00cbce98 b_venus */
void flare_draw(int tex, unsigned int color, Vec3 *pos, Vec3 *dir, float size)
{
    TaVtx quad[4];
    Vec3 ext;
    Vec3 ofs;
    Vec3 vdir;
    Vec3 spos;
    Vec3 vpos;
    float scl, sub_yx, add_xy;
    int old_tst;

    ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &vpos.x);
    if (!sphere_on_screen(&vpos.x, size))
        return;
    if (dir) {
        int al;

        ulMatrixApplyVector((float *)ulMatrixCur, &dir->x, &vdir.x);
        if (vdir.z < 0.0f)
            return;
        al = color >> 24;
        color &= 0xffffff;
        color |= (int)(al * vdir.z) << 24;
    }
    project_point(&vpos, &spos);
    ofs.x = spos.x - screen_cfg.cx;
    ofs.y = spos.y - screen_cfg.cy;
    if (ofs.x == 0.0f && ofs.y == 0.0f)
        ofs.x += 1.0f;
    scl = size / sqrtf(ofs.x * ofs.x + ofs.y * ofs.y);
    ofs.z = spos.z;
    ofs.x *= scl;
    ofs.y *= scl;
    project_point_abs(&ofs.x, &ext.x);
    if (ofs.x < 0.0f)
        ext.x = -ext.x;
    if (ofs.y < 0.0f)
        ext.y = -ext.y;
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    add_xy = ext.x + ext.y;
    sub_yx = ext.y - ext.x;
    quad[0].x = spos.x + add_xy;
    quad[0].y = spos.y + sub_yx;
    quad[1].x = spos.x + sub_yx;
    quad[1].y = spos.y - add_xy;
    quad[2].x = spos.x - sub_yx;
    quad[2].y = spos.y + add_xy;
    quad[3].x = spos.x - add_xy;
    quad[3].y = spos.y - sub_yx;
    old_tst = draw_set_test_mode(0);
    quad_draw_split(quad, color, spos.x, spos.y, 1.0f / (spos.z - size));
    draw_set_test_mode(old_tst);
}

/* 0x00cb94e0 b_earth, 0x00cc9d58 b_eden, 0x00cb8088 b_mars, 0x00cbc7b8 b_uranus, 0x00cbd128 b_venus */
void flare_draw_here(int tex, unsigned int color, float size)
{
    TaVtx quad[4];
    Vec3 ext;
    Vec3 ofs;
    Vec3 spos;
    Vec3 vpos;
    float scl, sub_yx, add_xy;
    int old_tst;

    ulMatrixGetTranslation((float *)ulMatrixCur, &vpos.x);
    if (!sphere_on_screen(&vpos.x, size))
        return;
    project_point(&vpos, &spos);
    ofs.x = spos.x - screen_cfg.cx;
    ofs.y = spos.y - screen_cfg.cy;
    if (ofs.x == 0.0f && ofs.y == 0.0f)
        ofs.x += 1.0f;
    scl = size / sqrtf(ofs.x * ofs.x + ofs.y * ofs.y);
    ofs.z = spos.z;
    ofs.x *= scl;
    ofs.y *= scl;
    project_point_abs(&ofs.x, &ext.x);
    if (ofs.x < 0.0f)
        ext.x = -ext.x;
    if (ofs.y < 0.0f)
        ext.y = -ext.y;
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    add_xy = ext.x + ext.y;
    sub_yx = ext.y - ext.x;
    quad[0].x = spos.x + add_xy;
    quad[0].y = spos.y + sub_yx;
    quad[1].x = spos.x + sub_yx;
    quad[1].y = spos.y - add_xy;
    quad[2].x = spos.x - sub_yx;
    quad[2].y = spos.y + add_xy;
    quad[3].x = spos.x - add_xy;
    quad[3].y = spos.y - sub_yx;
    old_tst = draw_set_test_mode(0);
    quad_draw_split(quad, color, spos.x, spos.y, 1.0f / (spos.z - size));
    draw_set_test_mode(old_tst);
}

/* 0x00cb9700 b_earth, 0x00cc9f78 b_eden, 0x00cb82a8 b_mars, 0x00cbc9d8 b_uranus, 0x00cbd348 b_venus */
void flare_draw_quad(int tex, unsigned int color, Vec3 *pos, Vec3 *dir, float size)
{
    TaVtx quad[4];
    Vec3 ext;
    Vec3 ofs;
    Vec3 vdir;
    Vec3 spos;
    Vec3 vpos;
    float scl, sub_yx, add_xy, inv_z;
    int old_tst;

    ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &vpos.x);
    if (!sphere_on_screen(&vpos.x, size))
        return;
    if (dir) {
        int al;

        ulMatrixApplyVector((float *)ulMatrixCur, &dir->x, &vdir.x);
        if (vdir.z < 0.0f)
            return;
        al = color >> 24;
        color &= 0xffffff;
        color |= (int)(al * vdir.z) << 24;
    }
    project_point(&vpos, &spos);
    ofs.x = spos.x - screen_cfg.cx;
    ofs.y = spos.y - screen_cfg.cy;
    if (ofs.x == 0.0f && ofs.y == 0.0f)
        ofs.x += 1.0f;
    scl = size / sqrtf(ofs.x * ofs.x + ofs.y * ofs.y);
    ofs.z = spos.z;
    ofs.x *= scl;
    ofs.y *= scl;
    project_point_abs(&ofs.x, &ext.x);
    if (ofs.x < 0.0f)
        ext.x = -ext.x;
    if (ofs.y < 0.0f)
        ext.y = -ext.y;
    add_xy = ext.x + ext.y;
    sub_yx = ext.y - ext.x;
    inv_z = 1.0f / (spos.z - size);
    quad[0].q = quad[1].q = quad[2].q = quad[3].q = inv_z;
    quad[0].u = quad[1].u = 0.0f;
    quad[2].u = quad[3].u = 1.0f;
    quad[0].v = quad[2].v = 0.0f;
    quad[1].v = quad[3].v = 1.0f;
    quad[0].x = spos.x + add_xy;
    quad[0].y = spos.y + sub_yx;
    quad[1].x = spos.x + sub_yx;
    quad[1].y = spos.y - add_xy;
    quad[2].x = spos.x - sub_yx;
    quad[2].y = spos.y + add_xy;
    quad[3].x = spos.x - add_xy;
    quad[3].y = spos.y - sub_yx;
    quad[0].color = quad[1].color = quad[2].color = quad[3].color = color;
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    old_tst = draw_set_test_mode(0);
    nj_tex_strip_begin(1);
    nj_tex_strip_vertices(quad, 4);
    nj_tex_strip_end();
    draw_set_test_mode(old_tst);
}

/* 0x00cb99d8 b_earth, 0x00cca250 b_eden, 0x00cb8580 b_mars, 0x00cbccb0 b_uranus, 0x00cbd620 b_venus */
void axes_draw(float len)
{
    Vec3 seg[2];
    Vec3 *tip = &seg[1];
    float org;

    seg[0].x = 0.0f;
    seg[0].y = 0.0f;
    seg[0].z = 0.0f;
    org = seg[0].x;
    tip->y = org;
    tip->x = org;
    tip->z = -len;
    lines_begin(2.0f, RGBA(0xff, 0, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
    tip->y = org;
    tip->x = org;
    tip->z = len;
    lines_begin(0.5f, RGBA(0xff, 0, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
    tip->z = org;
    tip->x = org;
    tip->y = len;
    lines_begin(2.0f, RGBA(0, 0xff, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
    tip->z = org;
    tip->x = org;
    tip->y = -len;
    lines_begin(0.5f, RGBA(0, 0xff, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
    tip->y = org;
    tip->x = len;
    seg[0].y = org;
    seg[0].x = -len;
    lines_begin(0.5f, -1, 0);
    lines_add_pairs(seg, 2);
    lines_end();
}

/* 0x00cb9b50 b_earth, 0x00cca3c8 b_eden, 0x00cb86f8 b_mars, 0x00cbce28 b_uranus, 0x00cbd798 b_venus */
void tree_pair_draw(WireObj *obj_a, WireObj *obj_b, Vec3 *sa, Vec3 *sb, int *idx)
{
    if (!obj_a || !obj_b)
        return;
    do {
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)obj_a);
        if (obj_a->model) {
            ulMatrixPush();
            if (sa) {
                if (!v3_is_zero(&sa[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sa[*idx].x, sa[*idx].y, sa[*idx].z);
                    model_draw_culled(obj_a->model);
                }
            } else
                model_draw_culled(obj_a->model);
            ulMatrixPop();
        }
        if (obj_b->model) {
            ulMatrixPush();
            if (sb) {
                if (!v3_is_zero(&sb[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sb[*idx].x, sb[*idx].y, sb[*idx].z);
                    model_draw_culled(obj_b->model);
                }
            } else
                model_draw_culled(obj_b->model);
            ulMatrixPop();
        }
        (*idx)++;
        if (obj_a->child)
            tree_pair_draw(obj_a->child, obj_b->child, sa, sb, idx);
        ulMatrixPop();
        obj_a = obj_a->sibling;
        obj_b = obj_b->sibling;
        if (!obj_a)
            break;
    } while (obj_b);
}

/* 0x00cb9d88 b_earth, 0x00cca600 b_eden, 0x00cb8930 b_mars, 0x00cbd060 b_uranus, 0x00cbd9d0 b_venus */
void tree_pair_draw_posed(WireObj *obj_a, WireObj *obj_b, void *mot, Vec3 *sa, Vec3 *sb, float t)
{
    int idx = 0;

    anim_start(mot, t);
    tree_pair_draw(obj_a, obj_b, sa, sb, &idx);
}

#ifdef VERSION_US

/* 0x00cb9df0 b_earth, 0x00cca668 b_eden, 0x00cb8998 b_mars, 0x00cbd0c8 b_uranus, 0x00cbda38 b_venus */
void tree_pair_draw_culled(WireObj *obj_a, WireObj *obj_b, WireObj *obj_c, BossPktNode *pkt, Vec3 *sa, Vec3 *sb,
                            int *idx)
{
    unsigned int vis;

    if (!obj_a || !obj_b)
        return;
    do {
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)obj_a);
        if (obj_a->model)
            vis = draw_cull_box(obj_a->model->r, &obj_a->model->center.x);
        else
            vis = 0xff00;
        if (obj_a->model) {
            ulMatrixPush();
            if (sa) {
                if (!v3_is_zero(&sa[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sa[*idx].x, sa[*idx].y, sa[*idx].z);
                    {
                        unsigned int vis = draw_cull_box(obj_a->model->r, &obj_a->model->center.x);

                        if (!(vis & 0xff)) {
                            vis &= 0xff00;
                            if (vis)
                                nj_cnk_vu1pkt_draw_clip(obj_a->model);
                            else
                                nj_cnk_vu1pkt_draw_noclip(obj_a->model);
                        }
                    }
                }
            } else if (!(vis & 0xff)) {
                if (vis & 0xff00)
                    nj_cnk_vu1pkt_draw_clip(obj_a->model);
                else
                    nj_cnk_vu1pkt_draw_noclip(obj_a->model);
            }
            ulMatrixPop();
        }
        if (obj_b->model) {
            ulMatrixPush();
            if (sb) {
                if (!v3_is_zero(&sb[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sb[*idx].x, sb[*idx].y, sb[*idx].z);
                    {
                        unsigned int vis = draw_cull_box(obj_b->model->r, &obj_b->model->center.x);

                        if (!(vis & 0xff)) {
                            vis &= 0xff00;
                            if (vis)
                                nj_cnk_vu1pkt_draw_clip(obj_b->model);
                            else
                                nj_cnk_vu1pkt_draw_noclip(obj_b->model);
                        }
                    }
                }
            } else if (!(vis & 0xff)) {
                if (vis & 0xff00)
                    nj_cnk_vu1pkt_draw_clip(obj_b->model);
                else
                    nj_cnk_vu1pkt_draw_noclip(obj_b->model);
            }
            ulMatrixPop();
        }
        if (obj_c->model && !(obj_c->flags & 8) && !(vis & 0xff)) {
            if (vis & 0xff00)
                nj_cnk_vu1pkt_draw_clip(obj_c->model);
            else
                nj_cnk_vu1pkt_draw_noclip(obj_c->model);
        }
        if (!(vis & 0xff)) {
            if (vis & 0xff00)
                model_packet_draw_normal(pkt);
            else
                model_packet_draw_simple(pkt);
        }
        (*idx)++;
        if (obj_a->child)
            tree_pair_draw_culled(obj_a->child, obj_b->child, obj_c->child, pkt->child, sa, sb, idx);
        ulMatrixPop();
        obj_a = obj_a->sibling;
        obj_b = obj_b->sibling;
        obj_c = obj_c->sibling;
        pkt = pkt->sibling;
        if (!obj_a)
            break;
    } while (obj_b);
}

/* 0x00cba1a0 b_earth, 0x00ccaa18 b_eden, 0x00cb8d48 b_mars, 0x00cbd478 b_uranus, 0x00cbdde8 b_venus */
void tree_pair_draw_posed_culled(WireObj *obj_a, WireObj *obj_b, WireObj *obj_c, BossPktNode *pkt, void *mot, Vec3 *sa,
                            Vec3 *sb, float t)
{
    int idx = 0;

    anim_start(mot, t);
    tree_pair_draw_culled(obj_a, obj_b, obj_c, pkt, sa, sb, &idx);
}
#else
/* 0x00cb9df0 b_earth, 0x00cca668 b_eden, 0x00cb8998 b_mars, 0x00cbd0c8 b_uranus, 0x00cbda38 b_venus */
void tree_pair_draw_culled(WireObj *obj_a, WireObj *obj_b, Vec3 *sa, Vec3 *sb, int *idx)
{
    if (!obj_a || !obj_b)
        return;
    do {
        ulMatrixPush();
        anim_apply_node((unsigned int *)obj_a);
        if (obj_a->model) {
            ulMatrixPush();
            if (sa) {
                if (!v3_is_zero(&sa[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sa[*idx].x, sa[*idx].y, sa[*idx].z);
                    {
                        unsigned int vis = draw_cull_box(obj_a->model->r, &obj_a->model->center.x);

                        if (!(vis & 0xff)) {
                            vis &= 0xff00;
                            if (vis)
                                nj_cnk_vu1pkt_draw_clip(obj_a->model);
                            else
                                nj_cnk_vu1pkt_draw_noclip(obj_a->model);
                        }
                    }
                }
            } else
                {
                    unsigned int vis = draw_cull_box(obj_a->model->r, &obj_a->model->center.x);

                    if (!(vis & 0xff)) {
                        vis &= 0xff00;
                        if (vis)
                            nj_cnk_vu1pkt_draw_clip(obj_a->model);
                        else
                            nj_cnk_vu1pkt_draw_noclip(obj_a->model);
                    }
                }
            ulMatrixPop();
        }
        if (obj_b->model) {
            ulMatrixPush();
            if (sb) {
                if (!v3_is_zero(&sb[*idx])) {
                    vu0_matrix_scale(ulMatrixCur, sb[*idx].x, sb[*idx].y, sb[*idx].z);
                    {
                        unsigned int vis = draw_cull_box(obj_b->model->r, &obj_b->model->center.x);

                        if (!(vis & 0xff)) {
                            vis &= 0xff00;
                            if (vis)
                                nj_cnk_vu1pkt_draw_clip(obj_b->model);
                            else
                                nj_cnk_vu1pkt_draw_noclip(obj_b->model);
                        }
                    }
                }
            } else
                {
                    unsigned int vis = draw_cull_box(obj_b->model->r, &obj_b->model->center.x);

                    if (!(vis & 0xff)) {
                        vis &= 0xff00;
                        if (vis)
                            nj_cnk_vu1pkt_draw_clip(obj_b->model);
                        else
                            nj_cnk_vu1pkt_draw_noclip(obj_b->model);
                    }
                }
            ulMatrixPop();
        }
        (*idx)++;
        if (obj_a->child)
            tree_pair_draw_culled(obj_a->child, obj_b->child, sa, sb, idx);
        ulMatrixPop();
        obj_a = obj_a->sibling;
        obj_b = obj_b->sibling;
        if (!obj_a)
            break;
    } while (obj_b);
}

/* 0x00cba1a0 b_earth, 0x00ccaa18 b_eden, 0x00cb8d48 b_mars, 0x00cbd478 b_uranus, 0x00cbdde8 b_venus */
void tree_pair_draw_posed_culled(WireObj *obj_a, WireObj *obj_b, void *mot, Vec3 *sa, Vec3 *sb, float t)
{
    int idx = 0;

    anim_start(mot, t);
    tree_pair_draw_culled(obj_a, obj_b, sa, sb, &idx);
}
#endif

/* 0x00cba228 b_earth, 0x00ccaaa0 b_eden, 0x00cb8dd0 b_mars, 0x00cbd500 b_uranus, 0x00cbde70 b_venus */
void tree_draw_posed(WireObj *obj, unsigned int mask)
{
    int old_fog;

    ulMatrixPush();
    anim_apply_node((struct AnimNode *)obj);
    if (obj->model && !(obj->flags & 8)) {
        if (obj->flags & mask) {
            old_fog = (int)fog_set_enable(0);
            nj_cnk_vu1_draw(obj->model);
            fog_set_enable((void *)old_fog);
        } else
            nj_cnk_vu1_draw(obj->model);
    }
    if (obj->child)
        tree_draw_posed(obj->child, mask);
    ulMatrixPop();
    if (obj->sibling)
        tree_draw_posed(obj->sibling, mask);
}

/* 0x00cba2e8 b_earth, 0x00ccab60 b_eden, 0x00cb8e90 b_mars, 0x00cbd5c0 b_uranus, 0x00cbdf30 b_venus */
void tree_draw_posed_pa(WireObj *obj, unsigned int mask)
{
    int old_fog;
    float *pos;

    do {
        ulMatrixPush();
    } while (0);
    pos = obj->pos;
    vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
    boss_rotate_yxz(obj->ang);
    if (obj->model && !(obj->flags & 8)) {
        if (obj->flags & mask) {
            old_fog = (int)fog_set_enable(0);
            nj_cnk_vu1_draw(obj->model);
            fog_set_enable((void *)old_fog);
        } else
            nj_cnk_vu1_draw(obj->model);
    }
    if (obj->child)
        tree_draw_posed_pa(obj->child, mask);
    ulMatrixPop();
    if (obj->sibling)
        tree_draw_posed_pa(obj->sibling, mask);
}

/* 0x00cba3d0 b_earth, 0x00ccac48 b_eden, 0x00cb8f78 b_mars, 0x00cbd6a8 b_uranus, 0x00cbe018 b_venus */
void tree_draw_motion(WireObj *obj, void *mot, unsigned int mask, float t)
{
    anim_start(mot, t);
    tree_draw_posed(obj, mask);
}

/* 0x00cba410 b_earth, 0x00ccac88 b_eden, 0x00cb8fb8 b_mars, 0x00cbd6e8 b_uranus, 0x00cbe058 b_venus */
void tree_draw_posed_pa_w(WireObj *obj, unsigned int mask)
{
    tree_draw_posed_pa(obj, mask);
}

/* 0x00cba418 b_earth, 0x00ccac90 b_eden, 0x00cb8fc0 b_mars, 0x00cbd6f0 b_uranus, 0x00cbe060 b_venus */
void tree_node_matrix_walk(WireObj *obj, float *mtx, int *idx, int target)
{
    ulMatrixPush();
    if (*idx <= target)
        anim_apply_node((struct AnimNode *)obj);
    if (*idx == target)
        njGetMatrix(mtx);
    (*idx)++;
    if (obj->child)
        tree_node_matrix_walk(obj->child, mtx, idx, target);
    ulMatrixPop();
    if (obj->sibling)
        tree_node_matrix_walk(obj->sibling, mtx, idx, target);
}

/* 0x00cba4c8 b_earth, 0x00ccad40 b_eden, 0x00cb9070 b_mars, 0x00cbd7a0 b_uranus, 0x00cbe110 b_venus */
int tree_node_matrix(WireObj *obj, void *mot, float *mtx, int target, float t)
{
    int idx;

    ulMatrixPush();
    matrix_unit_set(0);
    anim_start(mot, t);
    idx = 0;
    tree_node_matrix_walk(obj, mtx, &idx, target);
    ulMatrixPop();
    return 1;
}

/* 0x00cba558 b_earth, 0x00ccadd0 b_eden, 0x00cb9100 b_mars, 0x00cbd830 b_uranus, 0x00cbe1a0 b_venus */
int tree_node_pose(WireObj *obj, void *mot, Vec3 *pos, int *ang, Vec3 *scl, int target, float t)
{
    float mtx[16];

    tree_node_matrix(obj, mot, mtx, target, t);
    if (pos)
        ulMatrixGetTranslation(mtx, &pos->x);
    if (ang)
        matrix_to_angles(mtx, ang);
    if (scl) {
        scl->x = scl->y = scl->z = 1.0f;
    }
    return 1;
}

/* 0x00cba5d8 b_earth, 0x00ccae50 b_eden, 0x00cb9180 b_mars, 0x00cbd8b0 b_uranus, 0x00cbe220 b_venus */
int tree_node_pose_here(WireObj *obj, void *mot, Vec3 *pos, int *ang, Vec3 *scl, int target, float t)
{
    float mtx[16];
    int idx;

    anim_start(mot, t);
    idx = 0;
    tree_node_matrix_walk(obj, mtx, &idx, target);
    if (pos)
        ulMatrixGetTranslation(mtx, &pos->x);
    if (ang)
        matrix_to_angles(mtx, ang);
    if (scl) {
        scl->x = scl->y = scl->z = 1.0f;
    }
    return 1;
}

/* 0x00cba688 b_earth, 0x00ccaf00 b_eden, 0x00cb9230 b_mars, 0x00cbd960 b_uranus, 0x00cbe2d0 b_venus */
void motion_point_pose(void *mot, float t, Vec3 *pos, int *ang)
{
    anim_eval_hierarchy((int)&motion_point_obj, (int)mot, motion_point_buf, t);
    if (pos)
        v3_copy(pos, &motion_point_pos);
    if (ang)
        i3_copy(ang, motion_point_ang);
}

/* 0x00cba718 b_earth, 0x00ccaf90 b_eden, 0x00cb92c0 b_mars, 0x00cbd9f0 b_uranus, 0x00cbe360 b_venus */
void motion_path_draw(unsigned int *mot)
{
    Vec3 seg[2];
    int ang[3];
    BossColPair col;
    int i;
    int alt;

    alt = 0;
    col = motion_path_colors;
    for (i = 0; i < mot[1] - 2; i++) {
        motion_point_pose(mot, i, &seg[0], ang);
        motion_point_pose(mot, i + 1, &seg[1], ang);
        lines_begin(0.5f, col.c[alt], 0);
        alt ^= 1;
        lines_add_pairs(seg, 2);
        lines_end();
    }
}

/* 0x00cba820 b_earth, 0x00ccb098 b_eden, 0x00cb93c8 b_mars, 0x00cbdaf8 b_uranus, 0x00cbe468 b_venus */
void store_if(int *dst, int val)
{
    if (dst)
        *dst = val;
}

/* 0x00cba830 b_earth, 0x00ccb0a8 b_eden, 0x00cb93d8 b_mars, 0x00cbdb08 b_uranus, 0x00cbe478 b_venus */
short *model_material_find(NjCnkModel *model, int *out, int n)
{
    short *p = model->plist;
    int kind;
    int size;

    if (!p)
        return 0;
    while ((kind = *p++) != 255) {
        kind &= 0xff;
        if (kind < 8)
            ;
        else if (kind < 16)
            p++;
        else if (kind < 56) {
            size = *p++;
            if (!n)
                goto found;
            n--;
            p += size;
        } else {
            size = *p++;
            p += size;
        }
    }
    return 0;
found:
    *out = *(int *)p;
    return p;
}

/* 0x00cba8c8 b_earth, 0x00ccb140 b_eden, 0x00cb9470 b_mars, 0x00cbdba0 b_uranus, 0x00cbe510 b_venus */
int model_material_colors(NjCnkModel *model, float *dif, float *amb, float *spe)
{
    short *p = model->plist;
    int hdr;
    unsigned int kind;
    short mat_hdr, col_hi, col_lo;

    if (!p)
        return 0;
    while ((hdr = *p++) != 255) {
        kind = hdr & 0xff;
        if (kind < 8)
            ;
        else if (kind < 16)
            p++;
        else if (kind < 56) {
            p++;
            mat_hdr = *p;
            kind = (mat_hdr & 0xff) - 16;
            if (kind & 1) {
                p++;
                col_lo = *p;
                p++;
                if (dif) {
                    *dif++ = (col_lo >> 8 & 0xff) * 0.00392157f;
                    *dif++ = (col_lo & 0xff) * 0.00392157f;
                    *dif++ = (mat_hdr >> 8 & 0xff) * 0.00392157f;
                    *dif = (mat_hdr & 0xff) * 0.00392157f;
                }
            }
            goto found;
        }
    }
    return 0;
found:
    if (kind & 2) {
        col_hi = *p;
        p++;
        col_lo = *p;
        p++;
        if (amb) {
            *amb++ = (col_lo >> 8 & 0xff) * 0.00392157f;
            *amb++ = (col_lo & 0xff) * 0.00392157f;
            *amb++ = (col_hi >> 8 & 0xff) * 0.00392157f;
            *amb = (col_hi & 0xff) * 0.00392157f;
        }
    }
    if (kind & 4) {
        col_hi = p[0];
        col_lo = p[1];
        if (spe) {
            *spe++ = (col_lo >> 8 & 0xff) * 0.00392157f;
            *spe++ = (col_lo & 0xff) * 0.00392157f;
            *spe++ = (col_hi >> 8 & 0xff) * 0.00392157f;
            *spe = (col_hi & 0xff) * 0.00392157f;
        }
    }
    return 1;
}

/* 0x00cbaaf0 b_earth, 0x00ccb368 b_eden, 0x00cb9698 b_mars, 0x00cbddc8 b_uranus, 0x00cbe738 b_venus */
short *model_strip_find(void *model, short *type)
{
    short *p = ((short **)model)[1];
    int hdr, kind;

    if (p && (hdr = *p) != 255) {
        p++;
        do {
            kind = hdr & 0xff;
            if (kind < 8) {
            } else if (kind < 16) {
                p++;
            } else if (kind < 56) {
                int n = *p++;

                p += n;
            } else {
                *type = kind;
                return p;
            }
            hdr = *p;
            p++;
        } while (hdr != 255);
    }
    *type = 255;
    return 0;
}

/* 0x00cbab70 b_earth, 0x00ccb3e8 b_eden, 0x00cb9718 b_mars, 0x00cbde48 b_uranus, 0x00cbe7b8 b_venus */
int model_uv_count(void *model, short **ptrs, unsigned short *vals, int flags)
{
    short type;
    short *p;
    int n = 0;
    int strips, len;

    p = model_strip_find(model, &type);
    if (type == 65) {
        p++;
        strips = *p++;
        while (strips) {
            len = fabsf(*p++);
            for (; len; len--, p += 3) {
                if (vals) {
                    if (flags & 1)
                        *vals++ = p[1];
                    if (flags & 2)
                        *vals++ = p[2];
                }
                if (ptrs) {
                    if (flags & 1) {
                        n++;
                        *ptrs++ = &p[1];
                    }
                    if (flags & 2) {
                        n++;
                        *ptrs++ = &p[2];
                    }
                } else {
                    n += flags & 1;
                    if (flags & 2)
                        n++;
                }
            }
            strips--;
        }
        return n;
    }
    return 0;
}

/* 0x00cbacb0 b_earth, 0x00ccb528 b_eden, 0x00cb9858 b_mars, 0x00cbdf88 b_uranus, 0x00cbe8f8 b_venus */
int uv_scroll_setup(ModelUvWork *w, void *model, int flags)
{
    w->flags = flags;
    w->n = model_uv_count(model, 0, 0, flags);
    if (!w->n)
        return 0;
    w->base = heap_block_tag(game_malloc(w->n * 2), str_boss_common_c, SRC_LINE(2325, 2358));
    w->ptrs = heap_block_tag(game_malloc(w->n * 4), str_boss_common_c, SRC_LINE(2326, 2359));
    model_uv_count(model, 0, w->base, w->flags);
    model_uv_count(model, w->ptrs, 0, w->flags);
    return w->n;
}

/* 0x00cbad80 b_earth, 0x00ccb5f8 b_eden, 0x00cb9928 b_mars, 0x00cbe058 b_uranus, 0x00cbe9c8 b_venus */
void uv_scroll_free(ModelUvWork *w)
{
    if (w->n) {
        heap_free(w->base, 0);
        heap_free(w->ptrs, 0);
        w->n = 0;
    }
}

/* 0x00cbadc8 b_earth, 0x00ccb640 b_eden, 0x00cb9970 b_mars, 0x00cbe0a0 b_uranus, 0x00cbea10 b_venus */
void uv_scroll_write(ModelUvWork *w, float t)
{
    int n = w->n;
    short **p = w->ptrs;
    unsigned short *src = w->base;
    int ofs = (short)(int)(t * 255.0f);
    int val;

    for (; n; n--) {
        val = *src++;
        **p++ = ofs + val;
    }
}

/* 0x00cbae20 b_earth, 0x00ccb698 b_eden, 0x00cb99c8 b_mars, 0x00cbe0f8 b_uranus, 0x00cbea68 b_venus */
void uv_scroll_write_uv(ModelUvWork *w, float u, float v)
{
    int n = w->n;
    short **p = w->ptrs;
    unsigned short *src = w->base;
    int du = (short)(int)(u * 255.0f);
    int dv = (short)(int)(v * 255.0f);
    int val;

    for (; n; n -= 2) {
        **p++ = du + (val = *src++);
        **p++ = dv + (val = *src++);
    }
}

/* 0x00cbaea0 b_earth, 0x00ccb718 b_eden, 0x00cb9a48 b_mars, 0x00cbe178 b_uranus, 0x00cbeae8 b_venus */
void tree_flags_depth_r(WireObj *obj, int *depth, int lo, int hi, unsigned int mask, unsigned int bits)
{
    if (!(*depth < lo) && !(hi < *depth))
        obj->flags = (obj->flags & mask) | bits;
    if (obj->child) {
        (*depth)++;
        tree_flags_depth_r(obj->child, depth, lo, hi, mask, bits);
        (*depth)--;
    }
    if (obj->sibling)
        tree_flags_depth_r(obj->sibling, depth, lo, hi, mask, bits);
}

/* 0x00cbaf70 b_earth, 0x00ccb7e8 b_eden, 0x00cb9b18 b_mars, 0x00cbe248 b_uranus, 0x00cbebb8 b_venus */
void tree_flags_depth(WireObj *obj, int lo, int hi, unsigned int mask, unsigned int bits)
{
    int depth = 0;

    tree_flags_depth_r(obj, &depth, lo, hi, mask, bits);
}

/* 0x00cbafa8 b_earth, 0x00ccb820 b_eden, 0x00cb9b50 b_mars, 0x00cbe280 b_uranus, 0x00cbebf0 b_venus */
void tree_hide_range_r(WireObj *obj, int *idx, int lo, int hi, int hide)
{
    if (!(*idx < lo) && !(hi < *idx) && obj->model) {
        if (hide)
            obj->flags |= 8;
        else
            obj->flags &= ~8;
    }
    (*idx)++;
    if (obj->child)
        tree_hide_range_r(obj->child, idx, lo, hi, hide);
    if (obj->sibling)
        tree_hide_range_r(obj->sibling, idx, lo, hi, hide);
}

/* 0x00cbb070 b_earth, 0x00ccb8e8 b_eden, 0x00cb9c18 b_mars, 0x00cbe348 b_uranus, 0x00cbecb8 b_venus */
void tree_hide_range(WireObj *obj, int lo, int hi, int hide)
{
    int idx = 0;

    tree_hide_range_r(obj, &idx, lo, hi, hide);
}

/* 0x00cbb0a0 b_earth, 0x00ccb918 b_eden, 0x00cb9c48 b_mars, 0x00cbe378 b_uranus, 0x00cbece8 b_venus */
void tree_count_nodes(WireObj *obj, int models, int *n)
{
    if (!models)
        (*n)++;
    else if (obj->model)
        (*n)++;
    if (obj->child)
        tree_count_nodes(obj->child, models, n);
    if (obj->sibling)
        tree_count_nodes(obj->sibling, models, n);
}

/* 0x00cbb118 b_earth, 0x00ccb990 b_eden, 0x00cb9cc0 b_mars, 0x00cbe3f0 b_uranus, 0x00cbed60 b_venus */
void tree_copy(WireObj *src, WireObj *arr, int *n)
{
    WireObj *dst = &arr[*n];

    *dst = *src;
    if (src->model)
        dst->flags &= ~3;
    (*n)++;
    if (src->child) {
        dst->child = &arr[*n];
        tree_copy(src->child, arr, n);
    }
    if (src->sibling) {
        dst->sibling = &arr[*n];
        tree_copy(src->sibling, arr, n);
    }
}

/* 0x00cbb248 b_earth, 0x00ccbac0 b_eden, 0x00cb9df0 b_mars, 0x00cbe520 b_uranus, 0x00cbee90 b_venus */
void tree_store_pose(WireObj *obj, Vec3 *pos, int (*ang)[3], Vec3 *scl, int *i)
{
    v3_copy((Vec3 *)obj->pos, &pos[*i]);
    i3_copy(obj->ang, ang[*i]);
    v3_copy((Vec3 *)obj->scl, &scl[(*i)++]);
    if (obj->child)
        tree_store_pose(obj->child, pos, ang, scl, i);
    if (obj->sibling)
        tree_store_pose(obj->sibling, pos, ang, scl, i);
}

/* 0x00cbb348 b_earth, 0x00ccbbc0 b_eden, 0x00cb9ef0 b_mars, 0x00cbe620 b_uranus, 0x00cbef90 b_venus */
void motion_bake(BossBake *w, WireObj *tree, int *motion)
{
    void *bufs[3];
    int n;
    char *buf;
    int i = 0;

    w->nodes = 0;
    w->frames = motion[1];
    tree_count_nodes(tree, 0, &w->nodes);
    w->tree = heap_block_tag(game_malloc(w->frames * 4), str_boss_common_c, SRC_LINE(2473, 2506));
    for (; i < w->frames; i++) {
        n = 0;
        w->tree[i] = heap_block_tag(game_malloc(w->nodes * 56), str_boss_common_c, SRC_LINE(2477, 2510));
        tree_copy(tree, w->tree[i], &n);
    }
    i = 0;
    buf = heap_block_tag(game_malloc(w->nodes * 36), str_boss_common_c, SRC_LINE(2481, 2514));
    bufs[0] = buf;
    bufs[1] = buf + w->nodes * 12;
    bufs[2] = buf + w->nodes * 24;
    for (; i < w->frames; i++) {
        anim_eval_hierarchy((int)tree, (int)motion, bufs, i);
        n = 0;
        tree_store_pose(w->tree[i], bufs[0], bufs[1], bufs[2], &n);
    }
    heap_free(buf, 0);
}

/* 0x00cbb500 b_earth, 0x00ccbd78 b_eden, 0x00cba0a8 b_mars, 0x00cbe7d8 b_uranus, 0x00cbf148 b_venus */
void motion_bake_free(BossBake *w)
{
    int i;

    for (i = 0; i < w->frames; i++)
        heap_free(w->tree[i], 0);
    heap_free(w->tree, 0);
}

/* 0x00cbb570 b_earth, 0x00ccbde8 b_eden, 0x00cba118 b_mars, 0x00cbe848 b_uranus, 0x00cbf1b8 b_venus */
/* steer(cast) */
void tree_draw_bake(WireObj *obj)
{
    unsigned int fl;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8))
            model_draw_culled(obj->model);
        if (!(fl & 0x10))
            tree_draw_bake(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cbb638 b_earth, 0x00ccbeb0 b_eden, 0x00cba1e0 b_mars, 0x00cbe910 b_uranus, 0x00cbf280 b_venus */
void motion_bake_draw(BossBake *w, float t)
{
    tree_draw_bake(w->tree[(int)t % w->frames]);
}

/* 0x00cbb668 b_earth, 0x00ccbee0 b_eden, 0x00cba210 b_mars, 0x00cbe940 b_uranus, 0x00cbf2b0 b_venus */
/* steer(cast) */
void tree_draw_raw(WireObj *obj)
{
    unsigned int fl;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8))
            nj_cnk_vu1_draw_raw(obj->model);
        if (!(fl & 0x10))
            tree_draw_raw(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

#ifdef VERSION_US
/* Culled model-tree walkers; boss_dc_object_draw draws with them in place of dc_object_draw. */
/* 0x00cbb730 b_earth, 0x00ccbfa8 b_eden, 0x00cba2d8 b_mars, 0x00cbea08 b_uranus, 0x00cbf378 b_venus */
/* steer(cast) */
void tree_draw_raw_culled(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (!vis)
                    nj_cnk_vu1_draw_raw(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_raw_culled(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cbbaa0 b_earth, 0x00ccc318 b_eden, 0x00cba648 b_mars, 0x00cbed78 b_uranus, 0x00cbf6e8 b_venus */
/* steer(cast) */
void tree_draw_culled2_top(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (!vis)
                    nj_cnk_vu1pkt_draw_noclip(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_culled2(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cbbbf8 b_earth, 0x00ccc470 b_eden, 0x00cba7a0 b_mars, 0x00cbeed0 b_uranus, 0x00cbf840 b_venus */
void boss_dc_object_draw(ModelPart *part)
{
    WireObj *obj = dc_object_get_compiled(part);

    if (obj)
        tree_draw_culled2_top(obj);
    else
        tree_draw_raw_culled((WireObj *)part->root);
}
#endif

/* 0x00cbb818 b_earth, 0x00ccc090 b_eden, 0x00cba3c0 b_mars, 0x00cbeaf0 b_uranus, 0x00cbf460 b_venus */
/* steer(cast) */
void tree_draw_clip(WireObj *obj)
{
    unsigned int fl;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8))
            nj_cnk_vu1_draw_clip(obj->model);
        if (!(fl & 0x10))
            tree_draw_clip(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cbb8e0 b_earth, 0x00ccc158 b_eden, 0x00cba488 b_mars, 0x00cbebb8 b_uranus, 0x00cbf528 b_venus */
void tree_draw_bake_w(WireObj *obj)
{
    tree_draw_bake(obj);
}

/* 0x00cbb8e8 b_earth, 0x00ccc160 b_eden, 0x00cba490 b_mars, 0x00cbebc0 b_uranus, 0x00cbf530 b_venus */
void model_draw_culled(void *model)
{
    unsigned int vis = draw_cull_box(((float *)model)[5], (float *)model + 2);

    if (!(vis & 0xff)) {
        vis &= 0xff00;
        if (vis)
            nj_cnk_vu1_draw_clip(model);
        else
            nj_cnk_vu1_draw_raw(model);
    }
}

/* 0x00cbb948 b_earth, 0x00ccc1c0 b_eden, 0x00cba4f0 b_mars, 0x00cbec20 b_uranus, 0x00cbf590 b_venus */
void model_draw_box_culled(void *model)
{
    unsigned int vis = draw_cull_box(((float *)model)[5], (float *)model + 2);

    if (!(vis & 0xff)) {
        vis &= 0xff00;
        if (vis)
            nj_cnk_vu1pkt_draw_clip(model);
        else
            nj_cnk_vu1pkt_draw_noclip(model);
    }
}

/* 0x00cbb9a8 b_earth, 0x00ccc220 b_eden, 0x00cba550 b_mars, 0x00cbec80 b_uranus, 0x00cbf5f0 b_venus */
/* steer(cast) */
void tree_draw_culled2(WireObj *obj)
{
    unsigned int fl;
    unsigned int vis;
    float *pos;

    do {
        fl = obj->flags;
        ulMatrixPush();
        pos = obj->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        ((void (*)(int *, int))nj_matrix_rotate_cur_ang3)(obj->ang, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (!(fl & 8)) {
            vis = draw_cull_box(obj->model->r, &obj->model->center.x);
            if (!(vis & 0xff)) {
                vis &= 0xff00;
                if (vis)
                    nj_cnk_vu1pkt_draw_clip(obj->model);
                else
                    nj_cnk_vu1pkt_draw_noclip(obj->model);
            }
        }
        if (!(fl & 0x10))
            tree_draw_culled2(obj->child);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cbbb88 b_earth, 0x00ccc400 b_eden, 0x00cba730 b_mars, 0x00cbee60 b_uranus, 0x00cbf7d0 b_venus */
void part_draw_culled(ModelPart *part)
{
    float ctr[4];
    unsigned int vis;

    memset(ctr, 0, 16);
    vis = draw_cull_box(part->radius, ctr);
    if (!(vis & 0xff)) {
        vis &= 0xff00;
        if (vis)
            tree_draw_bake_w((WireObj *)part->root);
        else
            tree_draw_raw((WireObj *)part->root);
    }
}

/* 0x00cbbc38 b_earth, 0x00ccc4b0 b_eden, 0x00cba7e0 b_mars, 0x00cbef10 b_uranus, 0x00cbf880 b_venus */
void wobble_step(Vec3 *out, Vec3 *amp, float *phase, float step)
{
    float sin_x, cos_x, sin_y, cos_y, sin_z, cos_z;
    float ph = *phase;

    ph += step;
    while (ph > TWO_PI)
        ph -= TWO_PI;
    while (ph < -TWO_PI)
        ph += TWO_PI;
    *phase = ph;
    ulSinCosf(ph * 3.0f, &sin_x, &cos_x);
    out->x = cos_x * amp->x;
    ulSinCosf(ph * 5.0f, &sin_y, &cos_y);
    out->y = sin_y * amp->y;
    ulSinCosf(ph * 4.0f, &sin_z, &cos_z);
    out->z = sin_z * amp->z;
}

/* 0x00cbbdb8 b_earth, 0x00ccc630 b_eden, 0x00cba960 b_mars, 0x00cbf090 b_uranus, 0x00cbfa00 b_venus */
void wobble_step_slow(Vec3 *out, float amp, float *phase)
{
    float sin_x, cos_x, sin_y, cos_y, sin_z, cos_z;
    float ph = *phase;

    if (ph > 0.0f)
        ph += 0.005f;
    else
        ph -= 0.005f;
    while (ph > TWO_PI)
        ph -= TWO_PI;
    while (ph < -TWO_PI)
        ph += TWO_PI;
    *phase = ph;
    ulSinCosf(ph * 3.0f, &sin_x, &cos_x);
    out->x = cos_x * amp;
    ulSinCosf(ph * 5.0f, &sin_y, &cos_y);
    out->y = sin_y * amp;
    ulSinCosf(ph * 4.0f, &sin_z, &cos_z);
    out->z = sin_z * amp;
}

/* 0x00cbbf58 b_earth, 0x00ccc7d0 b_eden, 0x00cbab00 b_mars, 0x00cbf230 b_uranus, 0x00cbfba0 b_venus */
void spring_step(float *v, float *x, float damp, float stiff)
{
    float pos = *x;
    float vel = *v * (1.0f - damp) - stiff * pos;

    *v = vel;
    *x = pos + vel;
}

/* 0x00cbbf88 b_earth, 0x00ccc800 b_eden, 0x00cbab30 b_mars, 0x00cbf260 b_uranus, 0x00cbfbd0 b_venus */
void spring_step_v(Vec3 *v, Vec3 *x, float damp, float stiff)
{
    spring_step(&v->x, &x->x, damp, stiff);
    spring_step(&v->y, &x->y, damp, stiff);
    spring_step(&v->z, &x->z, damp, stiff);
}

/* 0x00cbbff8 b_earth, 0x00ccc870 b_eden, 0x00cbaba0 b_mars, 0x00cbf2d0 b_uranus, 0x00cbfc40 b_venus */
void spring_to(float *v, float *x, float target, float damp, float stiff)
{
    float pos = *x;
    float vel = *v * (1.0f - damp) - stiff * (pos - target);

    *v = vel;
    *x = pos + vel;
}

/* 0x00cbc030 b_earth, 0x00ccc8a8 b_eden, 0x00cbabd8 b_mars, 0x00cbf308 b_uranus, 0x00cbfc78 b_venus */
void spring_to_v(Vec3 *v, Vec3 *x, Vec3 *target, float damp, float stiff)
{
    spring_to(&v->x, &x->x, target->x, damp, stiff);
    spring_to(&v->y, &x->y, target->y, damp, stiff);
    spring_to(&v->z, &x->z, target->z, damp, stiff);
}

/* 0x00cbc0c0 b_earth, 0x00ccc938 b_eden, 0x00cbac68 b_mars, 0x00cbf398 b_uranus, 0x00cbfd08 b_venus */
void duos_vars_add(void)
{
    duos_vars_group = gw_varedit_add(&gw_varedit_root, 0, duos_vardef);
    gw_varedit_add_table(&gw_varedit_root, duos_vars_group, duos_vardefs);
}

/* 0x00cbc110 b_earth, 0x00ccc988 b_eden, 0x00cbacb8 b_mars, 0x00cbf3e8 b_uranus, 0x00cbfd58 b_venus */
void duos_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)duos_vardef);
}

/* 0x00cbc128 b_earth, 0x00ccc9a0 b_eden, 0x00cbacd0 b_mars, 0x00cbf400 b_uranus, 0x00cbfd70 b_venus */
void duos_plot_debug(void)
{
    if (duos_disp)
        duos_plot(duos_t, duos_f, duos_d,
                               duos_mf, duos_kf);
}

/* 0x00cbc168 b_earth, 0x00ccc9e0 b_eden, 0x00cbad10 b_mars, 0x00cbf440 b_uranus, 0x00cbfdb0 b_venus */
/* steer(cast) */
void duos_plot(float secs, float init_vel, float target, float damp, float stiff)
{
    float pts[4];
    float vel = init_vel;
    float x;
    float t;
    int i;

    x = 0.0f;
    ((void (*)(unsigned int, int, float))nj_flat_strip_begin)(RGBA(0xff, 0xff, 0xff, 0xff), 1, 0.5f);
    pts[0] = cm_line_sx;
    pts[1] = cm_line_sy;
    for (i = 0; i < secs * FPS; ) {
        t = i;
        spring_to(&vel, &x, target, damp, stiff);
        i++;
        pts[2] = cm_line_sx + cm_line_w * t / (secs * FPS);
        pts[3] = cm_line_sy - cm_line_h * x;
        nj_flat_strip_vertices(pts, 2, 1.0f);
        pts[0] = pts[2];
        pts[1] = pts[3];
    }
    pts[0] = cm_line_sx;
    pts[1] = cm_line_sy;
    pts[2] = cm_line_sx + cm_line_w;
    pts[3] = cm_line_sy;
    nj_flat_strip_vertices(pts, 2, 1.0f);
    pts[0] = cm_line_sx;
    pts[1] = cm_line_sy - cm_line_h;
    pts[3] = cm_line_sy + cm_line_h;
    pts[2] = cm_line_sx;
    nj_flat_strip_vertices(pts, 2, 1.0f);
    nj_flat_strip_end();
}

/* 0x00cbc380 b_earth, 0x00cccbf8 b_eden, 0x00cbaf28 b_mars, 0x00cbf658 b_uranus, 0x00cbffc8 b_venus */
void point_approach(Vec3 *from, Vec3 *to, Vec3 *pos, Vec3 *ofs, float t)
{
    Vec3 step;
    float x, y, z, cx, cy, cz;

    x = from->x + (to->x - from->x) * t;
    y = from->y + (to->y - from->y) * t;
    z = from->z + (to->z - from->z) * t;
    cx = pos->x;
    cy = pos->y;
    cz = pos->z;
    pos->x = cx + (x - cx) * t;
    pos->y = cy + (y - cy) * t;
    pos->z = cz + (z - cz) * t;
    v3_mul(&step, ofs, 1.0f - t);
    v3_add(pos, &step);
}

/* 0x00cbc450 b_earth, 0x00ccccc8 b_eden, 0x00cbaff8 b_mars, 0x00cbf728 b_uranus, 0x00cc0098 b_venus */
void point_approach_s(Vec3 *from, Vec3 *to, Vec3 *pos, Vec3 *ofs, float t, float scl)
{
    Vec3 step;
    float x, y, z, cx, cy, cz;

    x = from->x + (to->x - from->x) * t;
    y = from->y + (to->y - from->y) * t;
    z = from->z + (to->z - from->z) * t;
    cx = pos->x;
    cy = pos->y;
    cz = pos->z;
    pos->x = cx + (x - cx) * t;
    pos->y = cy + (y - cy) * t;
    pos->z = cz + (z - cz) * t;
    v3_mul(&step, ofs, scl * (1.0f - t));
    v3_add(pos, &step);
}

/* 0x00cbc520 b_earth, 0x00cccd98 b_eden, 0x00cbb0c8 b_mars, 0x00cbf7f8 b_uranus, 0x00cc0168 b_venus */
void cm_cchange_exec(CmCchangeWork *w)
{
    *w->dst = w->keys[w->idx];
    w->t += w->speed / system_fps;
    if (w->t >= w->n)
        w->t -= w->n;
    w->idx = w->t;
    if (w->count && --w->count <= 0) {
        FINISH(w);
        do {
            *w->dst = w->keys[w->n];
        } while (0);
    }
}

/* 0x00cbc5d0 b_earth, 0x00ccce48 b_eden, 0x00cbb178 b_mars, 0x00cbf8a8 b_uranus, 0x00cc0218 b_venus */
CmCchangeWork *cm_cchange_create(void *parent, int *dst, int *keys, int n, float speed, float secs)
{
    CmCchangeWork *w = task_create(parent, &cm_cchange_desc, 32);

    w->keys = keys;
    w->dst = dst;
    w->n = n;
    w->speed = speed;
    w->count = secs * FPS;
    w->idx = 0;
    w->t = 0.0f;
    return w;
}

/* 0x00cbc660 b_earth, 0x00ccced8 b_eden, 0x00cbb208 b_mars, 0x00cbf938 b_uranus, 0x00cc02a8 b_venus */
void cm_cfchange_exec(BossCfKeyWork *w)
{
    int n;

    *w->dst = w->keys[w->idx];
    n = w->n;
    w->t = w->t + w->speed / system_fps;
    if (w->t >= n)
        w->t -= n;
    w->idx = (int)w->t % n;
    if (w->count && --w->count <= 0) {
        do {
            FINISH(w);
        } while (0);
        *w->dst = w->keys[n];
    }
}

/* 0x00cbc758 b_earth, 0x00cccfd0 b_eden, 0x00cbb300 b_mars, 0x00cbfa30 b_uranus, 0x00cc03a0 b_venus */
BossCfKeyWork *cm_cfchange_create(void *parent, BossCfKey *dst, BossCfKey *keys, int n, float speed, float secs)
{
    BossCfKeyWork *w = task_create(parent, &cm_cfchange_desc, 28);

    w->keys = keys;
    w->dst = dst;
    w->n = n;
    w->speed = speed;
    w->count = secs * FPS;
    w->idx = 0;
    w->t = 0.0f;
    return w;
}

/* 0x00cbc7e8 b_earth, 0x00ccd060 b_eden, 0x00cbb390 b_mars, 0x00cbfac0 b_uranus, 0x00cc0430 b_venus */
void model_uv_exec(ModelUvWork *w)
{
    if ((w->flags & 3) == 3) {
        w->u += w->du;
        if (w->u >= 1.0f)
            w->u -= 1.0f;
        w->v += w->dv;
        if (w->v >= 1.0f)
            w->v -= 1.0f;
        uv_scroll_write_uv(w, w->u, w->v);
        return;
    }
    if (w->flags & 1) {
        w->u += w->du;
        if (w->u >= 1.0f)
            w->u -= 1.0f;
        uv_scroll_write(w, w->u);
    }
    if (w->flags & 2) {
        w->v += w->dv;
        if (w->v >= 1.0f)
            w->v -= 1.0f;
        uv_scroll_write(w, w->v);
    }
}

/* 0x00cbc910 b_earth, 0x00ccd188 b_eden, 0x00cbb4b8 b_mars, 0x00cbfbe8 b_uranus, 0x00cc0558 b_venus */
void model_uv_release(ModelUvWork *w)
{
    uv_scroll_free(w);
}

/* 0x00cbc918 b_earth, 0x00ccd190 b_eden, 0x00cbb4c0 b_mars, 0x00cbfbf0 b_uranus, 0x00cc0560 b_venus */
ModelUvWork *model_uv_create(void *parent, void *model, int flags, float du, float dv)
{
    ModelUvWork *w = task_create(parent, &model_uv_desc, 32);

    if (!uv_scroll_setup(w, model, flags)) {
        FINISH_IF(w);
        return 0;
    }
    if (flags & 1)
        w->du = du;
    if (flags & 2)
        w->dv = dv;
    return w;
}

/* 0x00cbc9c8 b_earth, 0x00ccd240 b_eden, 0x00cbb570 b_mars, 0x00cbfca0 b_uranus, 0x00cc0610 b_venus */
void cm_wire_break_exec(CmWireBreakWork *w)
{
    Vec3 drift;
    Vec3 grav;
    BossShard *shard = w->s;
    int n = w->n;
    int alive = 0;
    Vec3 *vel;
    Vec3 *pos;

    if (w->flags & 8) {
        player_get_frame_vel(&drift);
    } else {
        w->decay *= 0.97f;
        if (w->flags & 0x20) {
            v3_copy(&drift, shard_wind);
            v3_copy(&grav, shard_gravity);
        } else {
            player_get_frame_vel_old(&drift);
            player_get_frame_accel(&grav);
        }
        grav.x *= w->decay;
        grav.y *= w->decay;
        grav.z *= w->decay;
        drift.x += grav.x;
        drift.y += grav.y;
        drift.z += grav.z;
    }
    for (; n; shard++, n--) {
        if (!shard->count)
            continue;
        pos = &shard->pos;
        vel = &shard->vel;
        shard->k = (float)shard->count / shard->total;
        v3_add(pos, vel);
        v3_add(pos, &drift);
        if (w->cb)
            w->cb(w->parent, pos, vel);
        v3_scale(vel, 0.99f);
        i3_sum(shard->ang, shard->ang, shard->spin);
        if (--shard->count > 0)
            alive++;
    }
    if (!alive)
        FINISH_IF(w);
}

/* 0x00cbcc40 b_earth, 0x00ccd4b8 b_eden, 0x00cbb7e8 b_mars, 0x00cbff18 b_uranus, 0x00cc0888 b_venus */
/* steer(cast) */
void cm_wire_break_disp_model(CmWireBreakWork *w)
{
    BossShard *shard = w->s;
    int n = w->n;
    int old_blend;
    Vec3 *pos;

    ulGxSetTextureList(w->texlist);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    for (; n; n--, shard++) {
        if (!shard->count)
            continue;
        if (!view_sphere_visible(&shard->pos, shard->radius))
            continue;
        if (shard->k < 0.8f)
            shard_fade_rgba[0] = shard->k / 0.8f + -1.0f;
        else
            shard_fade_rgba[0] = 0.0f;
        draw_set_override_color(shard_fade_rgba);
        ulMatrixPush();
        pos = &shard->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        } while (0);
        boss_rotate_yxz(shard->ang);
        if (w->flags & 0x10)
            vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
        model_draw_culled(shard->model);
        ulMatrixPop();
    }
    draw_set_override_flags(old_blend);
    draw_pop_strip_flags();
    draw_pop_override_color();
}

/* 0x00cbcdc0 b_earth, 0x00ccd638 b_eden, 0x00cbb968 b_mars, 0x00cc0098 b_uranus, 0x00cc0a08 b_venus */
/* steer(cast) */
void cm_wire_break_disp(CmWireBreakWork *w)
{
    BossShard *shard = w->s;
    int n = w->n;
    int old_blend;
    unsigned int vis;
    Vec3 *pos;
    float rate;

    ulGxSetTextureList(w->texlist);
    draw_set_blend(4, 1);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    for (; n; n--, shard++) {
        if (!shard->count)
            continue;
        if (!view_sphere_visible(&shard->pos, shard->radius))
            continue;
        if (shard->k < 0.8f) {
            rate = shard->k / 0.8f;
            draw_set_color((w->color & 0xffffff) | (((int)(color_get_a(w->color) * rate) + 1) >> 1 << 24));
        } else {
            draw_set_color(w->color);
        }
        ulMatrixPush();
        pos = &shard->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        } while (0);
        boss_rotate_yxz(shard->ang);
        if (w->flags & 0x10)
            vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
        vis = draw_cull_box(((float *)shard->model)[5], (float *)shard->model + 2);
#ifdef VERSION_US
        if (!(vis & 0xff)) {
            vis &= 0xff00;
            if (!vis) {
                if (shard->wire)
                    model_packet_draw_simple(shard->wire);
                else if (shard->tex)
                    model_obj_draw_direct(shard->model, shard->tex);
                else
                    DEBUG_PRINTF("**** cmWireData Err ****");
            }
        }
#else
        if (!(vis & 0xff)) {
            vis &= 0xff00;
            if (vis) {
                if (shard->wire)
                    model_packet_draw_normal(shard->wire);
                else
                    model_obj_draw_normal(shard->model, shard->tex);
            } else {
                if (shard->wire)
                    model_packet_draw_simple(shard->wire);
                else
                    model_obj_draw_direct(shard->model, shard->tex);
            }
        }
#endif
        ulMatrixPop();
    }
    draw_set_override_flags(old_blend);
    draw_pop_strip_flags();
}

/* 0x00cbcfc0 b_earth, 0x00ccd838 b_eden, 0x00cbbb68 b_mars, 0x00cc0298 b_uranus, 0x00cc0c08 b_venus */
void cm_wire_break_release(int *w)
{
    heap_free((void *)w[2], 0);
}

/* 0x00cbcfd0 b_earth, 0x00ccd848 b_eden, 0x00cbbb78 b_mars, 0x00cc02a8 b_uranus, 0x00cc0c18 b_venus */
void shard_start(BossShard *shard, void *model, void *tex, void *wire, Vec3 *pos, Vec3 *vel, int *ang, float speed, float life)
{
    float sc[2][2];
    int n;

    i3_copy(shard->ang, ang);
    v3_copy(&shard->pos, pos);
    do {
        shard->spin[0] = DEG_TO_ANG(RAND_FLOAT_A() * 8.0f + -4.0f);
        shard->spin[1] = DEG_TO_ANG(RAND_FLOAT_A() * 8.0f + -4.0f);
    } while (0);
    shard->tex = tex;
    shard->wire = wire;
    shard->model = model;
    n = (life + RAND_FLOAT_B() * 2.0f * 0.2f - 0.2f) * FPS;
    shard->vel.x = shard->vel.y = 0.0f;
    shard->count = shard->total = n;
    shard->vel.z = speed / n;
    ROT_X((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f - 180.0f), sc[0], shard->vel.y, shard->vel.z);
    ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f - 180.0f), sc[1], shard->vel.x, shard->vel.z);
    if (vel)
        v3_sum(&shard->vel, vel, &shard->vel);
    shard->radius = ((float *)model)[5];
}

/* 0x00cbd2b0 b_earth, 0x00ccdb28 b_eden, 0x00cbbe58 b_mars, 0x00cc0588 b_uranus, 0x00cc0ef8 b_venus */
void task_tree_walk(BossTNode *node)
{
    do {
        if (task_tree_index == task_tree_count)
            task_tree_found = node;
        task_tree_count++;
        if (node->child)
            task_tree_walk(node->child);
        node = node->sibling;
    } while (node);
}

/* 0x00cbd330 b_earth, 0x00ccdba8 b_eden, 0x00cbbed8 b_mars, 0x00cc0608 b_uranus, 0x00cc0f78 b_venus */
BossTNode *task_tree_nth(BossTNode *node, int idx)
{
    task_tree_found = 0;
    task_tree_index = idx;
    task_tree_count = 0;
    task_tree_walk(node);
    return task_tree_found;
}

/* 0x00cbd370 b_earth, 0x00ccdbe8 b_eden, 0x00cbbf18 b_mars, 0x00cc0648 b_uranus, 0x00cc0fb8 b_venus */
CmWireBreakWork *cm_wire_break_create(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, Vec3 *scale, unsigned int color, float life, float speed, float frac, void (*cb)(void *, Vec3 *, Vec3 *))
{
    CmWireBreakWork *w;
    Vec3 org;
    Vec3 pl_vel;
    Vec3 opos;
    WireObj *obj;
    BossShard *shard;
    BossTNode *wire;
    int i;

    if (flags & 1)
        w = task_create_inherit(parent, &cm_wire_break_desc, 48);
    else
        w = task_create_sibling_inherit(parent, &cm_wire_break_desc, 48);
    w->parent = parent;
    w->flags = flags;
    if (scale) {
        v3_copy(&w->scale, scale);
        w->flags = flags | 0x10;
    }
    if (w->flags & 4) {
        player_get_frame_vel(&pl_vel);
        org.x = pos->x - pl_vel.x;
        org.y = pos->y - pl_vel.y;
        org.z = pos->z - pl_vel.z;
    } else {
        org.x = pos->x;
        org.y = pos->y;
        org.z = pos->z;
    }
    w->decay = 1.0f;
    obj = ((WireObj *)src->root)->child;
    w->n = 0;
    if (obj) {
    count:
        if (obj->model)
            w->n++;
        if ((obj = obj->sibling) != 0)
            goto count;
    }
    w->n = w->n * frac;
    if (w->n <= 0)
        w->n = 1;
    w->color = color;
    w->cb = cb;
    if (color_get_a(color) >= 255)
        w->color = (color & 0xffffff) | 0x7f000000;
    shard = w->s = heap_block_tag(game_calloc(w->n * 84), str_boss_common_c, SRC_LINE(3223, 3322));
    obj = ((WireObj *)src->root)->child;
    for (i = 0; i < w->n; obj = obj->sibling) {
        wire = dc_object_get_wire(src);
        if (obj->model) {
            v3_copy(&opos, (Vec3 *)obj->pos);
            if (scale) {
                opos.x = opos.x * scale->x;
                opos.y = opos.y * scale->y;
                opos.z = opos.z * scale->z;
            }
            vec_rotate_zxy(&opos, ang);
            opos.x = org.x + opos.x;
            opos.y = org.y + opos.y;
            opos.z = org.z + opos.z;
            if (w->flags & 0x40) {
                shard_start(shard, obj->model, 0, 0, &opos, vel, ang, speed, life);
                i++;
            } else if (wire)
                shard_start(shard, obj->model, 0, task_tree_nth(wire, ++i), &opos, vel, ang, speed, life);
            else
                shard_start(shard, obj->model, model_list_nth((void *)src->wire_list, ++i), 0, &opos, vel, ang, speed, life);
            shard++;
        } else {
            i++;
        }
    }
    if (w->flags & 0x40)
        TASK_OF(w)->disp = (void (*)(void *))cm_wire_break_disp_model;
    return w;
}

/* 0x00cbd768 b_earth, 0x00ccdfe0 b_eden, 0x00cbc310 b_mars, 0x00cc0a40 b_uranus, 0x00cc13b0 b_venus */
CmWireBreakWork *cm_wire_break_simple(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color)
{
    return cm_wire_break_create(parent, flags, src, pos, vel, ang, 0, color, 1.25f, 64.0f, 1.0f, 0);
}

/* 0x00cbd7a8 b_earth, 0x00cce020 b_eden, 0x00cbc350 b_mars, 0x00cc0a80 b_uranus, 0x00cc13f0 b_venus */
CmWireBreakWork *cm_wire_break_scaled(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, Vec3 *scale, unsigned int color)
{
    return cm_wire_break_create(parent, flags, src, pos, vel, ang, scale, color, 1.25f, 64.0f, 1.0f, 0);
}

/* 0x00cbd7e0 b_earth, 0x00cce058 b_eden, 0x00cbc388 b_mars, 0x00cc0ab8 b_uranus, 0x00cc1428 b_venus */
void cm_wire_break_set_cb(CmWireBreakWork *w, void (*cb)(void *, Vec3 *, Vec3 *))
{
    w->cb = cb;
}

/* 0x00cbd7e8 b_earth, 0x00cce060 b_eden, 0x00cbc390 b_mars, 0x00cc0ac0 b_uranus, 0x00cc1430 b_venus */
void cm_wire_break_set_drift(Vec3 *gravity, Vec3 *wind)
{
    shard_gravity = gravity;
    shard_wind = wind;
}

/* 0x00cbd800 b_earth, 0x00cce078 b_eden, 0x00cbc3a8 b_mars, 0x00cc0ad8 b_uranus, 0x00cc1448 b_venus */
CmWireBreakWork *cm_wire_mdl_break_create(void *parent, unsigned int flags, void *model, void *tex, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color, void (*cb)(void *, Vec3 *, Vec3 *))
{
    CmWireBreakWork *w;
    Vec3 org;
    Vec3 pl_vel;

    if (flags == 1)
        w = task_create_inherit(parent, &cm_wire_mdl_break_desc, 48);
    else
        w = task_create_sibling_inherit(parent, &cm_wire_mdl_break_desc, 48);
    w->parent = parent;
    w->flags = flags;
    if (flags & 4) {
        player_get_frame_vel(&pl_vel);
        org.x = pos->x - pl_vel.x;
        org.y = pos->y - pl_vel.y;
        org.z = pos->z - pl_vel.z;
    } else {
        org.x = pos->x;
        org.y = pos->y;
        org.z = pos->z;
    }
    w->color = color;
    w->cb = cb;
    if (color_get_a(color) >= 255)
        w->color = (color & 0xffffff) | 0x7f000000;
    w->n = 1;
    w->s = heap_block_tag(game_calloc(84), str_boss_common_c, SRC_LINE(3317, 3416));
    shard_start(w->s, model, tex, 0, &org, vel, ang, 64.0f, 1.25f);
    return w;
}

/* 0x00cbd9b0 b_earth, 0x00cce228 b_eden, 0x00cbc558 b_mars, 0x00cc0c88 b_uranus, 0x00cc15f8 b_venus */
void cm_wire_mdl_break_tree(void *parent, unsigned int flags, WireObj *obj, void *texlist, Vec3 *pts, Vec3 *vel, unsigned int color, int *n, int *i)
{
    Vec3 pos;
    Vec3 dvel;
    int ang[3];
    Vec3 *prev;

    ulMatrixPush();
    anim_apply_node((struct AnimNode *)obj);
    if (obj->model) {
        prev = &pts[*n];
        matrix_current_pose(&pos, ang);
        v3_sub(&dvel, &pos, prev);
        if (vel)
            v3_add(&dvel, vel);
        cm_wire_mdl_break_create(parent, flags, obj->model, model_list_nth(texlist, *i), &pos, &dvel, ang, color, 0);
        (*n)++;
    }
    (*i)++;
    if (obj->child)
        cm_wire_mdl_break_tree(parent, flags, obj->child, texlist, pts, vel, color, n, i);
    ulMatrixPop();
    if (obj->sibling)
        cm_wire_mdl_break_tree(parent, flags, obj->sibling, texlist, pts, vel, color, n, i);
}

/* 0x00cbdb78 b_earth, 0x00cce3f0 b_eden, 0x00cbc720 b_mars, 0x00cc0e50 b_uranus, 0x00cc17c0 b_venus */
void tree_collect_positions(WireObj *obj, Vec3 *out, int *n)
{
    ulMatrixPush();
    anim_apply_node((struct AnimNode *)obj);
    if (obj->model) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &out[*n].x);
        (*n)++;
    }
    if (obj->child)
        tree_collect_positions(obj->child, out, n);
    ulMatrixPop();
    if (obj->sibling)
        tree_collect_positions(obj->sibling, out, n);
}

/* 0x00cbdc10 b_earth, 0x00cce488 b_eden, 0x00cbc7b8 b_mars, 0x00cc0ee8 b_uranus, 0x00cc1858 b_venus */
void tree_count_models(WireObj *obj, int *n)
{
    if (obj->model)
        (*n)++;
    if (obj->child)
        tree_count_models(obj->child, n);
    if (obj->sibling)
        tree_count_models(obj->sibling, n);
}

/* 0x00cbdc78 b_earth, 0x00cce4f0 b_eden, 0x00cbc820 b_mars, 0x00cc0f50 b_uranus, 0x00cc18c0 b_venus */
void cm_wire_mdl_break_part(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color)
{
    WireObj *root = (WireObj *)src->root;
    Vec3 *pts;
    int n = 0;
    int i;
    float t;

    tree_count_models(root, &n);
    pts = heap_block_tag(game_calloc(n * 12), str_boss_common_c, SRC_LINE(3393, 3492));
    ulMatrixPush();
    do {
        matrix_unit_set(0);
    } while (0);
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    boss_rotate_yxz(ang);
    t = src->frame - src->speed;
    if (t <= 0.0f)
        anim_start((void *)src->clip, 0.0f);
    else
        anim_start((void *)src->clip, t);
    n = 0;
    tree_collect_positions(root, pts, &n);
    anim_start((void *)src->clip, src->frame);
    n = i = 0;
    cm_wire_mdl_break_tree(parent, flags, root, (void *)src->wire_list, pts, vel, color, &n, &i);
    ulMatrixPop();
    heap_free(pts, 0);
}

/* 0x00cbde00 b_earth, 0x00cce678 b_eden, 0x00cbc9a8 b_mars, 0x00cc10d8 b_uranus, 0x00cc1a48 b_venus */
void venus_zxyb_beam_start(VenusZxybWork *w, BossBeam *beam, Vec3 *dir, unsigned int color, float width, float len, float k, float spread)
{
    Vec3 kick;

    v3_copy(&beam->pos, &w->pos);
    v3_copy(&beam->vel, &w->vel);
    v3_copy(&beam->dir, dir);
    vec_rotate_zxy(&beam->dir, w->ang);
    v3_add(&beam->pos, &beam->dir);
    v3_mul(&kick, &beam->dir, k);
    v3_sum(&beam->vel, &kick, &beam->vel);
    do {
        beam->rot[0] = DEG_TO_ANG(RAND_FLOAT_A() * (spread + spread) - spread);
    } while (0);
    beam->rot[1] = DEG_TO_ANG(RAND_FLOAT_A() * (spread + spread) - spread);
    beam->rot[2] = DEG_TO_ANG(RAND_FLOAT_A() * (spread + spread) - spread);
    do {
        beam->color = color;
    } while (0);
    beam->len = len;
    beam->width = width;
}

/* 0x00cbe008 b_earth, 0x00cce880 b_eden, 0x00cbcbb0 b_mars, 0x00cc12e0 b_uranus, 0x00cc1c50 b_venus */
void venus_zxyb_exec(VenusZxybWork *w)
{
    Vec3 pl_vel;
    BossBeam *beam;
    int i;

    player_get_frame_vel(&pl_vel);
    for (i = 5, beam = w->beam; i >= 0; i--, beam++) {
        Vec3 *pos = &beam->pos;
        float x, y, z;

        x = pos->x;
        y = pos->y;
        z = pos->z;
        pos->x = beam->vel.x + x + pl_vel.x;
        pos->y = beam->vel.y + y + pl_vel.y;
        pos->z = beam->vel.z + z + pl_vel.z;
        vec_rotate_zxy(&beam->dir, beam->rot);
    }
    if (--w->count <= 0 && w)
        FINISH(w);
}

/* 0x00cbe0e0 b_earth, 0x00cce958 b_eden, 0x00cbcc88 b_mars, 0x00cc13b8 b_uranus, 0x00cc1d28 b_venus */
void venus_zxyb_disp(VenusZxybWork *w)
{
    Vec3 seg[2];
    float rate = (float)w->count / w->total;
    BossBeam *beam;
    unsigned int col;
    int i;

    for (i = 5, beam = w->beam; i >= 0; i--, beam++) {
        Vec3 *pos = &beam->pos;

        col = beam->color;
        lines_begin(beam->width, (col & 0xffffff) | (((int)(color_get_a(col) * rate) + 1) >> 1 << 24), 1);
        {
            float x, y, z, nx, ny, nz;

            x = beam->dir.x;
            y = beam->dir.y;
            z = beam->dir.z;
            nx = -x;
            ny = -y;
            nz = -z;
            seg[0].x = x + pos->x;
            seg[0].y = y + pos->y;
            seg[0].z = z + pos->z;
            seg[1].x = pos->x + nx;
            seg[1].y = pos->y + ny;
            seg[1].z = pos->z + nz;
        }
        lines_add_pairs(seg, 2);
        lines_end();
    }
}

/* 0x00cbe208 b_earth, 0x00ccea80 b_eden, 0x00cbcdb0 b_mars, 0x00cc14e0 b_uranus, 0x00cc1e50 b_venus */
VenusZxybWork *venus_zxyb_create(void *parent, Vec3 *pos, Vec3 *vel, int *ang, float size, float secs, float k, float spread)
{
    VenusZxybWork *w = task_create_sibling(parent, &venus_zxyb_desc, 408);
    Vec3 pl_vel;
    Vec3 v;
    BossBeamDef *def = venus_zxyb_beams;
    BossBeam *beam;
    float rad;
    int i;

    player_get_frame_vel(&pl_vel);
    v3_sub(&w->pos, pos, &pl_vel);
    i3_copy(w->ang, ang);
    w->count = w->total = secs * FPS;
    v3_copy(&w->vel, vel);
    rad = size * 0.5f;
    for (i = 0; i < 6; i++) {
        def = &venus_zxyb_beams[i];
        beam = &w->beam[i];
        v.x = def->dir.x * rad;
        v.y = def->dir.y * rad;
        v.z = def->dir.z * rad;
        venus_zxyb_beam_start(w, beam, &v, def->color, def->width, rad, k, spread);
    }
    return w;
}

/* 0x00cbe3b8 b_earth, 0x00ccec30 b_eden, 0x00cbcf60 b_mars, 0x00cc1690 b_uranus, 0x00cc2000 b_venus */
void scroll_relative(Vec3 *in, Vec3 *out)
{
    player_get_frame_vel(out);
    v3_sub(out, in, out);
}

/* 0x00cbe420 b_earth, 0x00ccec98 b_eden, 0x00cbcfc8 b_mars, 0x00cc16f8 b_uranus, 0x00cc2068 b_venus */
void tree_scale_vertices(WireObj *obj, Vec3 *scale)
{
    BossVtxBlk *blk;
    int n;
    Vec3 *v;
    WireObj *nxt;

    do {
        if (obj->model) {
            blk = (BossVtxBlk *)obj->model->vlist;
            v = &blk->v[0].pos;
            for (n = blk->n; n; n--) {
                v->x *= scale->x;
                v->y *= scale->y;
                v->z *= scale->z;
                v = (Vec3 *)((char *)v + 24);
            }
        }
        nxt = obj->child;
        if (nxt)
            tree_scale_vertices(nxt, scale);
        nxt = obj->sibling;
        obj = nxt;
    } while (nxt);
}

/* 0x00cbe4d0 b_earth, 0x00cced48 b_eden, 0x00cbd078 b_mars, 0x00cc17a8 b_uranus, 0x00cc2118 b_venus */
void unit_circle_build(void)
{
    int i;
    int step = 2002;

    for (i = 0; i < 32; i++) {
        ulSinCos(i * step, &unit_circle[i].x, &unit_circle[i].y);
        unit_circle[i].z = 0.0f;
    }
    {
        Vec3 *src = &unit_circle[0];
        Vec3 *dst = &unit_circle[32];

        dst->x = src->x;
        dst->y = src->y;
        dst->z = src->z;
    }
}

/* 0x00cbe590 b_earth, 0x00ccee08 b_eden, 0x00cbd138 b_mars, 0x00cc1868 b_uranus, 0x00cc21d8 b_venus */
void circle_draw_flat(Vec3 *pos, unsigned int color, float size)
{
    unit_circle_build();
    ulMatrixPush();
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    vu0_matrix_rotate_x(ulMatrixCur, ANG_90);
    vu0_matrix_scale(ulMatrixCur, size, size, size);
    lines_begin(0.5f, color, 1);
    lines_add_strip(unit_circle, 33);
    lines_end();
    ulMatrixPop();
}

/* 0x00cbe640 b_earth, 0x00cceeb8 b_eden, 0x00cbd1e8 b_mars, 0x00cc1918 b_uranus, 0x00cc2288 b_venus */
void cm_circle_exec(CmCircleWork *w)
{
    Vec3 pl_vel;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    w->k = 1.0f - (float)w->count / w->total;
    if (--w->count <= 0)
        FINISH_IF(w);
}

/* 0x00cbe6f0 b_earth, 0x00ccef68 b_eden, 0x00cbd298 b_mars, 0x00cc19c8 b_uranus, 0x00cc2338 b_venus */
void cm_circle_disp(CmCircleWork *w)
{
    unsigned int col;
    int old_list;
    float rad;
    Vec3 *pos;

    if (w->k > w->fade)
        col = (w->color & 0xffffff) | (((int)(color_get_a(w->color) * (1.0f - (w->k - w->fade) / (1.0f - w->fade))) + 1) >> 1 << 24);
    else
        col = w->color;
    old_list = RenderSetCurrentDisplayList(6);
    draw_set_blend(4, 1);
    ulMatrixPush();
    pos = &w->pos;
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    rad = w->size * w->k;
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    vu0_matrix_scale(ulMatrixCur, rad, rad, rad);
    lines_begin(0.5f, col, 1);
    lines_add_strip(unit_circle, 33);
    lines_end();
    ulMatrixPop();
    RenderSetCurrentDisplayList(old_list);
}

/* 0x00cbe848 b_earth, 0x00ccf0c0 b_eden, 0x00cbd3f0 b_mars, 0x00cc1b20 b_uranus, 0x00cc2490 b_venus */
CmCircleWork *cm_circle_create(void *parent, Vec3 *pos, unsigned int color, float size, float fade, float secs)
{
    CmCircleWork *w = task_create(parent, &cm_circle_desc, 44);

    v3_copy(&w->pos, pos);
    w->color = color;
    w->size = size;
    w->fade = fade;
    w->total = w->count = secs * FPS;
    return w;
}

/* 0x00cbe8f0 b_earth, 0x00ccf168 b_eden, 0x00cbd498 b_mars, 0x00cc1bc8 b_uranus, 0x00cc2538 b_venus */
void cm_circle2_exec(CmCircle2Work *w)
{
    Vec3 pl_vel;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    w->count--;
    w->k += (1.0f - w->k) * w->rate;
    if (w->count <= 0)
        FINISH_IF(w);
}

/* 0x00cbe990 b_earth, 0x00ccf208 b_eden, 0x00cbd538 b_mars, 0x00cc1c68 b_uranus, 0x00cc25d8 b_venus */
void cm_circle2_draw_ring(unsigned int col, int n, float x, float y, float z, float width, float rad)
{
    float sc[2];
    int i;
    int step = 0xffff / n;
    float prev_x = x + rad;
    float prev_y = y;
    int ang = step;
    void *lines = line_set_color(col);

    for (i = n; i; i--) {
        ulSinCos(ang, &sc[0], &sc[1]);
        ang += step;
        sc[1] = x + sc[1] * rad;
        sc[0] = y + sc[0] * rad;
        lines = line_add_seg(lines, prev_x, prev_y, z, sc[1], sc[0], z);
        prev_x = sc[1];
        prev_y = sc[0];
    }
    line_flush(lines);
}

/* 0x00cbeab0 b_earth, 0x00ccf328 b_eden, 0x00cbd658 b_mars, 0x00cc1d88 b_uranus, 0x00cc26f8 b_venus */
void cm_circle2_disp(CmCircle2Work *w)
{
    float vpos[4];
    float spos[4];
    float rad;
    float depth;
    unsigned int col;
    int elap, old_fog;

    elap = w->total - w->count;
    if (elap > 0)
        col = (w->color & 0xffffff) | (((int)(color_get_a(w->color) * (1.0f - (float)elap / w->total)) + 1) >> 1 << 24);
    else
        col = w->color;
    draw_set_blend(4, 1);
    old_fog = (int)fog_set_enable(0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &w->pos.x, vpos);
    if (w->shrink)
        rad = w->size * (1.0f - w->k);
    else
        rad = w->size * w->k;
    depth = project_point_size((Vec3 *)vpos, (Vec3 *)spos, &rad, 0);
    if (depth <= 1.0f)
        cm_circle2_draw_ring(col, w->segs, spos[0], spos[1], depth, w->width, rad);
    fog_set_enable((void *)old_fog);
}

/* 0x00cbec10 b_earth, 0x00ccf488 b_eden, 0x00cbd7b8 b_mars, 0x00cc1ee8 b_uranus, 0x00cc2858 b_venus */
CmCircle2Work *cm_circle2_create(void *parent, Vec3 *pos, unsigned int color, int segs, int frames, float size, float width)
{
    CmCircle2Work *w = task_create(parent, &cm_circle2_desc, 72);

    v3_copy(&w->pos, pos);
    w->color = color;
    w->count = w->total2 = w->total = frames;
    w->rate = 0.1f;
    w->size = size;
    w->width = width;
    w->segs = segs;
    if (segs < 3)
        w->segs = 3;
    return w;
}

/* 0x00cbece0 b_earth, 0x00ccf558 b_eden, 0x00cbd888 b_mars, 0x00cc1fb8 b_uranus, 0x00cc2928 b_venus */
void cm_circle2_disp_3d(CmCircle2Work *w)
{
    unsigned int col;
    int elap, old_fog;
    float rad;
    Vec3 *pos;

    if (!w->shrink) {
        elap = w->total - w->count;
        col = (w->color & 0xffffff) | (((int)(color_get_a(w->color) * ((float)elap / w->total)) + 1) >> 1 << 24);
    } else {
        elap = w->total - w->count;
        col = (w->color & 0xffffff) | (((int)(color_get_a(w->color) * (1.0f - (float)elap / w->total)) + 1) >> 1 << 24);
    }
    rad = w->size * (1.0f - w->k);
    draw_set_blend(4, 1);
    old_fog = (int)fog_set_enable(0);
    ulMatrixPush();
    pos = &w->pos;
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    boss_rotate_yxz(w->ang);
    vu0_matrix_rotate_x(ulMatrixCur, ANG_90);
    vu0_matrix_scale(ulMatrixCur, rad, rad, rad);
    lines_begin(w->width, col, 1);
    lines_add_strip(unit_circle, 33);
    lines_end();
    ulMatrixPop();
    fog_set_enable((void *)old_fog);
}

/* 0x00cbeea8 b_earth, 0x00ccf720 b_eden, 0x00cbda50 b_mars, 0x00cc2180 b_uranus, 0x00cc2af0 b_venus */
void cm_circle2_set_3d(CmCircle2Work *w, int *ang, int shrink, float rate)
{
    w->shrink = shrink;
    TASK_OF(w)->disp = (void (*)(void *))cm_circle2_disp_3d;
    i3_copy(w->ang, ang);
    w->rate = rate;
}

/* 0x00cbeee0 b_earth, 0x00ccf758 b_eden, 0x00cbda88 b_mars, 0x00cc21b8 b_uranus, 0x00cc2b28 b_venus */
void cm_btail_exec(CmBtailWork *w)
{
    Vec3 pl_vel;
    int i;

    player_get_frame_vel(&pl_vel);
    for (i = 0; i < w->n; i++) {
        w->pts[i].x += pl_vel.x;
        w->pts[i].y += pl_vel.y;
        w->pts[i].z += pl_vel.z;
    }
    if (!(w->flags & 1)) {
        if (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END)) {
            w->flags |= 3;
        } else {
            w->acc += w->rate;
            if (w->acc >= 1.0f) {
                w->acc -= 1.0f;
                w->head = (w->head + 1) % w->n;
            }
            w->pts[w->head].x = w->src->x;
            w->pts[w->head].y = w->src->y;
            w->pts[w->head].z = w->src->z;
        }
    }
    if (w->flags & 2) {
        w->alpha = (float)w->count / w->total;
        w->color = (w->color0 & 0xffffff) | (((int)(color_get_a(w->color0) * w->alpha) + 1) >> 1 << 24);
        if (--w->count <= 0)
            FINISH_IF(w);
    }
}

/* 0x00cbf0b8 b_earth, 0x00ccf930 b_eden, 0x00cbdc60 b_mars, 0x00cc2390 b_uranus, 0x00cc2d00 b_venus */
void cm_btail_disp(CmBtailWork *w)
{
    ulGxSetTextureList(w->texlist);
    draw_set_blend(4, 1);
    strip_draw_alpha(w->pts, w->head, w->n, w->color, w->tex, w->width_a * w->alpha, w->width_b * w->alpha);
}

/* 0x00cbf118 b_earth, 0x00ccf990 b_eden, 0x00cbdcc0 b_mars, 0x00cc23f0 b_uranus, 0x00cc2d60 b_venus */
void cm_btail_release(CmBtailWork *w)
{
    heap_free(w->pts, 0);
}

/* 0x00cbf128 b_earth, 0x00ccf9a0 b_eden, 0x00cbdcd0 b_mars, 0x00cc2400 b_uranus, 0x00cc2d70 b_venus */
void cm_btail_stop(CmBtailWork *w)
{
    w->flags |= 3;
}

/* 0x00cbf138 b_earth, 0x00ccf9b0 b_eden, 0x00cbdce0 b_mars, 0x00cc2410 b_uranus, 0x00cc2d80 b_venus */
void cm_btail_set_running(CmBtailWork *w, int on)
{
    if (on)
        TASK_OF(w)->exec = (void (*)(void *))cm_btail_exec;
    else
        TASK_OF(w)->exec = 0;
}

/* 0x00cbf158 b_earth, 0x00ccf9d0 b_eden, 0x00cbdd00 b_mars, 0x00cc2430 b_uranus, 0x00cc2da0 b_venus */
CmBtailWork *cm_btail_create(void *parent, void *owner, Vec3 *src, BossRibbonDef *def)
{
    CmBtailWork *w = task_create_sibling_inherit(parent, &cm_btail_desc, 72);
    int i;

    w->owner = owner;
    w->src = src;
    w->n = def->n;
    w->pts = heap_block_tag(game_malloc(w->n * 12), str_boss_common_c, SRC_LINE(3861, 3960));
    for (i = 0; i < w->n; i++) {
        w->pts[i].x = w->src->x;
        w->pts[i].y = w->src->y;
        w->pts[i].z = w->src->z;
    }
    w->tex = def->tex;
    w->color0 = w->color = def->color;
    w->count = w->total = def->secs * FPS;
    w->rate = def->rate;
    w->width_a = def->width_a;
    w->width_b = def->width_b;
    w->alpha = 1.0f;
    return w;
}

/* 0x00cbf290 b_earth, 0x00ccfb08 b_eden, 0x00cbde38 b_mars, 0x00cc2568 b_uranus, 0x00cc2ed8 b_venus */
void cm_ptcl_spark_start(CmPtclWork *w, BossSpark *spark)
{
    spark->size = 0.0f;
    spark->lift = w->psize * RAND_FLOAT_B();
    v3_mul(&spark->vel, &w->dir, RAND_FLOAT_B());
    spark->count = spark->total = RAND_FLOAT_B() * (w->life_max - w->life_min) + w->life_min;
    spark->ang = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    spark->spin = RAND_FLOAT_B() * w->spread;
}

/* Sparks fly out from origin over their life; expired ones restart, or die when last is set. */
/* 0x00cbf400 b_earth, 0x00ccfc78 b_eden, 0x00cbdfa8 b_mars, 0x00cc26d8 b_uranus, 0x00cc3048 b_venus */
void cm_ptcl_sparks_move(CmPtclWork *w, BossSpark *spark, int last, Vec3 *origin, float size_to, float ease)
{
    Vec3 ofs;
    float sc[6];
    float rate, x, y;
    int i;

    for (i = w->n; i; spark++, i--) {
        if (spark->dead)
            continue;
        if (--spark->count <= 0) {
            if (!last)
                cm_ptcl_spark_start(w, spark);
            else
                spark->dead = 1;
        }
        rate = (float)spark->count / spark->total;
        spark->size = spark->size * ease + size_to * (1.0f - ease);
        spark->ang += spark->spin;
        v3_mul(&ofs, &spark->vel, rate);
        v3_sum(&spark->pos, origin, &ofs);
        x = 0.0f;
        y = spark->lift * rate;
        ofs.y = y;
        ofs.z = x;
        ofs.x = x;
        ROT_Z(spark->ang, (sc + 0), ofs.x, ofs.y);
        ROT_X(w->ang_y, (sc + 2), ofs.y, ofs.z);
        ROT_Y(w->ang_x, (sc + 4), ofs.x, ofs.z);
        v3_sum(&spark->pos, &ofs, &spark->pos);
    }
}

/*
 * Follows its target and ends with its life. Emits a spark per interval up to its maximum until
 * the count reaches the stop point, then lets them die; while hidden it only shrinks them.
 */
/* 0x00cbf670 b_earth, 0x00ccfee8 b_eden, 0x00cbe218 b_mars, 0x00cc2948 b_uranus, 0x00cc32b8 b_venus */
void cm_ptcl_exec(CmPtclWork *w)
{
    BossSpark *spark;
    int i;

    if (w->track) {
        vec3_normalize(&w->dir, w->dirsrc);
        v3_mul(&w->vel, &w->dir, w->speed);
        v3_scale(&w->dir, w->dscale);
    }
    v3_sum(&w->pos, w->follow, &w->vel);
    if (--w->count <= 0) {
        FINISH_IF(w);
        return;
    }
    if (w->hide) {
        for (i = w->n, spark = w->p; i; i--, spark++) {
            if (!spark->dead)
                spark->size *= 0.9f;
        }
        return;
    }
    switch (w->state) {
    case 0:
        cm_ptcl_sparks_move(w, w->p, 0, &w->pos, w->k1, w->k2);
        if (w->n < w->max && --w->timer != 0) {
            w->n++;
            w->timer = w->interval;
            if (w->n == w->max)
                w->state++;
        }
        break;
    case 1:
        cm_ptcl_sparks_move(w, w->p, 0, &w->pos, w->k1, w->k2);
        if (w->count <= w->stop)
            w->state++;
        break;
    case 2:
        cm_ptcl_sparks_move(w, w->p, 1, &w->pos, w->k1, w->k2);
        break;
    }
}

/* 0x00cbf8a0 b_earth, 0x00cd0118 b_eden, 0x00cbe448 b_mars, 0x00cc2b78 b_uranus, 0x00cc34e8 b_venus */
void cm_ptcl_disp(CmPtclWork *w)
{
    TaVtx vtx[4];
    float vpos[4];
    float core_vpos[4];
    BossSpark *spark;
    ScreenCfg *scr = &screen_cfg;
    int i;

    ulGxSetTextureList(w->texlist);
    draw_set_blend(4, 1);
    ulGxSelectTexture(ulGxGetTextureList(), w->tex);
    vtx[0].u = vtx[1].u = 0.0f;
    vtx[2].u = vtx[3].u = 1.0f;
    vtx[0].v = vtx[2].v = 0.0f;
    vtx[1].v = vtx[3].v = 1.0f;
    vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = w->color;
    nj_tex_strip_begin(1);
    for (i = w->n, spark = w->p; i; i--, spark++) {
        float inv_z, x, y, rad;

        if (spark->dead)
            continue;
        ulMatrixApplyPoint((float *)ulMatrixCur, &spark->pos.x, vpos);
        if (!sphere_on_screen(vpos, spark->size))
            continue;
        inv_z = -1.0f / vpos[2];
        x = vpos[0] * inv_z * scr->sx + scr->cx;
        y = vpos[1] * inv_z * scr->sy + scr->cy;
        rad = fabsf(inv_z * scr->sx) * spark->size;
        vtx[0].q = vtx[1].q = vtx[2].q = vtx[3].q = inv_z;
        vtx[0].x = vtx[1].x = x - rad;
        vtx[2].x = vtx[3].x = x + rad;
        vtx[0].y = vtx[2].y = y - rad;
        vtx[1].y = vtx[3].y = y + rad;
        nj_tex_strip_vertices(vtx, 4);
    }
    if (!w->hide) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &w->pos.x, core_vpos);
        if (sphere_on_screen(core_vpos, w->size)) {
            float inv_z, x, y, rad;

            inv_z = -1.0f / core_vpos[2];
            x = core_vpos[0] * inv_z * scr->sx + scr->cx;
            y = core_vpos[1] * inv_z * scr->sy + scr->cy;
            rad = fabsf(inv_z * scr->sx) * w->size * (1.0f - (float)w->count / w->total);
            vtx[0].q = vtx[1].q = vtx[2].q = vtx[3].q = inv_z;
            vtx[0].x = vtx[1].x = x - rad;
            vtx[2].x = vtx[3].x = x + rad;
            vtx[0].y = vtx[2].y = y - rad;
            vtx[1].y = vtx[3].y = y + rad;
            nj_tex_strip_vertices(vtx, 4);
        }
    }
    nj_tex_strip_end();
}

/* 0x00cbfb68 b_earth, 0x00cd03e0 b_eden, 0x00cbe710 b_mars, 0x00cc2e40 b_uranus, 0x00cc37b0 b_venus */
void cm_ptcl_release(CmPtclWork *w)
{
    heap_free(w->p, 0);
}

/* 0x00cbfb78 b_earth, 0x00cd03f0 b_eden, 0x00cbe720 b_mars, 0x00cc2e50 b_uranus, 0x00cc37c0 b_venus */
CmPtclWork *cm_ptcl_create(void *parent, Vec3 *follow, Vec3 *dirsrc, BossSparkDef *def)
{
    CmPtclWork *w = task_create_inherit(parent, &cm_ptcl_desc, 152);

    w->follow = follow;
    v3_copy(&w->dir, dirsrc);
    w->speed = def->speed;
    w->dscale = def->dscale;
    vec3_normalize(&w->dir, &w->dir);
    v3_mul(&w->vel, &w->dir, w->speed);
    v3_scale(&w->dir, w->dscale);
    do {
        w->ang_x = RAD_TO_ANG(atan2f(-w->dir.x, -w->dir.z));
        w->ang_y = RAD_TO_ANG(atan2f(w->dir.y, sqrtf(w->dir.x * w->dir.x + w->dir.z * w->dir.z)));
    } while (0);
    w->tex = def->tex;
    w->count = w->total = def->secs * FPS;
    w->stop = (float)w->total * def->fade;
    w->psize = def->psize;
    w->k1 = def->k1;
    w->size = def->size;
    w->k2 = def->k2;
    w->max = def->max;
    w->color = COL_WHITE;
    w->life_min = def->life_min * FPS;
    w->spread = DEG_TO_ANG(def->spread);
    w->life_max = w->timer = w->interval = def->life_max * FPS;
    w->p = heap_block_tag(game_calloc(w->max * 64), str_boss_common_c, SRC_LINE(4142, 4241));
    return w;
}

/* 0x00cbfdf0 b_earth, 0x00cd0668 b_eden, 0x00cbe998 b_mars, 0x00cc30c8 b_uranus, 0x00cc3a38 b_venus */
void cm_ptcl_hide(int *w)
{
    w[12] = 1;
    if (w[7] < w[5])
        w[5] = w[7];
}

/* 0x00cbfe18 b_earth, 0x00cd0690 b_eden, 0x00cbe9c0 b_mars, 0x00cc30f0 b_uranus, 0x00cc3a60 b_venus */
void cm_ptcl_track(int *w, int dirsrc)
{
    w[23] = dirsrc;
    w[4] = 1;
}

/* 0x00cbfe28 b_earth, 0x00cd06a0 b_eden, 0x00cbe9d0 b_mars, 0x00cc3100 b_uranus, 0x00cc3a70 b_venus */
void cm_obj_mot_exec(CmObjMotWork *w)
{
    Vec3 pl_vel;

    switch (w->state) {
    case 0:
        if (!w->follow) {
            player_get_frame_vel(&pl_vel);
            v3_add(&w->pos, &pl_vel);
        }
        w->m.anim.t += w->m.anim.speed;
        if (w->m.anim.len - w->count < w->m.anim.t) {
            w->count = w->total;
            w->state++;
        }
        break;
    case 1:
        w->m.anim.t += w->m.anim.speed;
        if (w->m.anim.len < w->m.anim.t)
            w->m.anim.t = w->m.anim.len;
        if (--w->count <= 0)
            FINISH_IF(w);
        break;
    }
}

/* 0x00cbff38 b_earth, 0x00cd07b0 b_eden, 0x00cbeae0 b_mars, 0x00cc3210 b_uranus, 0x00cc3b80 b_venus */
/* steer(cast) */
void cm_obj_mot_disp(CmObjMotWork *w)
{
    float col[4];
    int old_blend;

    draw_push_strip_flags();
    draw_push_override_color();
    old_blend = draw_set_override_flags(0);
    ulGxSetTextureList(w->texlist);
    ulMatrixPush();
    switch (w->state) {
    case 0:
        MTX_TRANS(w->ppos);
        boss_rotate_yxz(w->pang);
        vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
        dc_object_draw(&w->m.part);
        break;
    case 1:
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        col[1] = col[2] = col[3] = 0.0f;
        col[0] = (float)w->count / w->total + -1.0f;
        draw_set_override_color(col);
        MTX_TRANS(w->ppos);
        boss_rotate_yxz(w->pang);
        vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
        dc_object_draw(&w->m.part);
        break;
    }
    ulMatrixPop();
    draw_set_override_flags(old_blend);
    draw_pop_strip_flags();
    draw_pop_override_color();
}

/* 0x00cc00b8 b_earth, 0x00cd0930 b_eden, 0x00cbec60 b_mars, 0x00cc3390 b_uranus, 0x00cc3d00 b_venus */
CmObjMotWork *cm_obj_mot_create(void *parent, Vec3 *pos, int *ang, ModelPart *src, int follow, float secs, float fade)
{
    Vec3 pl_vel;
    CmObjMotWork *w = task_create_inherit(parent, &cm_obj_mot_desc, 164);

    w->follow = follow;
    if (follow) {
        w->ppos = pos;
        w->pang = ang;
    } else {
        player_get_frame_vel(&pl_vel);
        v3_copy(&w->pos, pos);
        v3_sub(&w->pos, &w->pos, &pl_vel);
        if (ang)
            i3_copy(w->ang, ang);
        w->ppos = &w->pos;
        w->pang = w->ang;
    }
    w->scale.x = w->scale.y = w->scale.z = 1.0f;
    w->count = secs * FPS;
    w->total = fade * FPS;
    w->m.part = *src;
    return w;
}

/* 0x00cc02e0 b_earth, 0x00cd0b58 b_eden, 0x00cbee88 b_mars, 0x00cc35b8 b_uranus, 0x00cc3f28 b_venus */
void cm_obj_mot_set_time(CmObjMotWork *w, float t)
{
    w->m.anim.t = t;
}

/* 0x00cc02e8 b_earth, 0x00cd0b60 b_eden, 0x00cbee90 b_mars, 0x00cc35c0 b_uranus, 0x00cc3f30 b_venus */
void cm_obj_mot_set_scale(CmObjMotWork *w, Vec3 *scale)
{
    v3_copy(&w->scale, scale);
}

/* 0x00cc0308 b_earth, 0x00cd0b80 b_eden, 0x00cbeeb0 b_mars, 0x00cc35e0 b_uranus, 0x00cc3f50 b_venus */
void cm_object_blur_exec(CmObjectBlurWork *w)
{
    Vec3 pl_vel;
    BossTrailEnt *ent = w->ring;
    int i;

    player_get_frame_vel(&pl_vel);
    for (i = 0; i < w->n; i++, ent++) {
        ent->pos.x += pl_vel.x;
        ent->pos.y += pl_vel.y;
        ent->pos.z += pl_vel.z;
    }
    if (--w->step <= 0) {
        w->step = w->interval;
        w->head = (w->head + 1) % w->n;
    }
    v3_copy(&w->ring[w->head].pos, w->pos);
    i3_copy(w->ring[w->head].ang, w->ang);
    if (w->count) {
        if (--w->count <= 0 && w)
            FINISH(w);
        if (w->count <= w->total)
            w->alpha = (float)w->count / w->total;
        else
            w->alpha = 1.0f;
    } else {
        w->alpha = 1.0f;
    }
    if (w->speed != 0.0f) {
        w->t += w->speed;
        if (w->t >= w->end)
            w->t = w->end;
    }
}

/* 0x00cc04c8 b_earth, 0x00cd0d40 b_eden, 0x00cbf070 b_mars, 0x00cc37a0 b_uranus, 0x00cc4110 b_venus */
/* steer(cast) */
void cm_object_blur_disp(CmObjectBlurWork *w)
{
    BossTrailEnt *ent;
    int i = 0;
    int old_blend, old_fog;

    ulGxSetTextureList(w->texlist);
    draw_push_strip_flags();
    draw_push_override_color();
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    old_fog = (int)fog_set_enable((void *)(w->fog));
    for (; i < w->n; i++) {
        object_blur_rgba[0] = (float)(w->n - i) / w->n * w->alpha + -1.0f;
        draw_set_override_color(object_blur_rgba);
        ent = &w->ring[(w->head - i + w->n) % w->n];
        ulMatrixPush();
        do {
        vu0_matrix_translate(ulMatrixCur, ent->pos.x, ent->pos.y, ent->pos.z);
        } while (0);
        boss_rotate_yxz(ent->ang);
        if (w->clip) {
            model_object_draw(w->model, w->t);
        } else {
            dc_object_draw(w->model);
        }
        ulMatrixPop();
    }
    fog_set_enable((void *)old_fog);
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old_blend);
}

/* 0x00cc0660 b_earth, 0x00cd0ed8 b_eden, 0x00cbf208 b_mars, 0x00cc3938 b_uranus, 0x00cc42a8 b_venus */
void cm_object_blur_release(CmObjectBlurWork *w)
{
    heap_free(w->ring, 0);
}

/* 0x00cc0670 b_earth, 0x00cd0ee8 b_eden, 0x00cbf218 b_mars, 0x00cc3948 b_uranus, 0x00cc42b8 b_venus */
CmObjectBlurWork *cm_object_blur_create(void *parent, Vec3 *pos, int *ang, ModelPart *model, int n, int interval, float secs, float fade)
{
    CmObjectBlurWork *w = task_create_inherit(parent, &cm_object_blur_desc, 76);
    BossTrailEnt *ent;
    int i;

    w->n = n;
    ent = w->ring = heap_block_tag(game_calloc(n * 24), str_boss_common_c, SRC_LINE(4369, 4468));
    w->model = model;
    w->pos = pos;
    w->ang = ang;
    for (i = 0; i < w->n; i++, ent++) {
        v3_copy(&ent->pos, w->pos);
        i3_copy(ent->ang, w->ang);
    }
    w->step = w->interval = interval;
    w->fog = 1;
    w->count = secs * FPS;
    w->total = fade * FPS;
    return w;
}

/* 0x00cc07c0 b_earth, 0x00cd1038 b_eden, 0x00cbf368 b_mars, 0x00cc3a98 b_uranus, 0x00cc4408 b_venus */
void cm_object_blur_set_fade(CmObjectBlurWork *w, float secs)
{
    w->count = w->total = secs * FPS;
}

/* 0x00cc07e0 b_earth, 0x00cd1058 b_eden, 0x00cbf388 b_mars, 0x00cc3ab8 b_uranus, 0x00cc4428 b_venus */
void cm_object_blur_set_anim(CmObjectBlurWork *w, unsigned int *clip, float t, float speed)
{
    w->t = t;
    w->speed = speed;
    w->clip = clip;
    w->end = clip[1] - 1;
}

/* 0x00cc0828 b_earth, 0x00cd10a0 b_eden, 0x00cbf3d0 b_mars, 0x00cc3b00 b_uranus, 0x00cc4470 b_venus */
void cm_object_blur_set_fog(int *w, int fog)
{
    w[12] = fog;
}

/* 0x00cc0830 b_earth, 0x00cd10a8 b_eden, 0x00cbf3d8 b_mars, 0x00cc3b08 b_uranus, 0x00cc4478 b_venus */
void parts_set_radii(ModelPart **list, float *radii)
{
    while (*radii >= 0.0f)
        (*list++)->radius = *radii++;
}

/* 0x00cc0870 b_earth, 0x00cd10e8 b_eden, 0x00cbf418 b_mars, 0x00cc3b48 b_uranus, 0x00cc44b8 b_venus */
/* steer(cast) */
void boss_parts_build(void **bank, BossModelDef *def, ModelPart **out, ModelPart *pool)
{
    int n = 0;
    int i;

    boss_parts_built_set(1);
    for (; def->idx >= 0; def++, n++) {
        i = def->idx;
        out[i] = &pool[i];
        switch (def->kind) {
        case 4:
        dc_object_init(out[def->idx], *((struct DcNode **)pnb_get_entry(*bank, def->node)), def->clip ? *((int *)pnb_get_entry(*bank, def->clip)) : 0, def->flags);
        dc_object_rebuild(out[def->idx]);
        break;
        case 0:
        dc_object_init(out[def->idx], *((struct DcNode **)pnb_get_entry(*bank, def->node)), def->clip ? *((int *)pnb_get_entry(*bank, def->clip)) : 0, def->flags);
        break;
        case 1:
        dc_object_init(out[def->idx], *((struct DcNode **)pnb_get_entry(*bank, def->node)), def->clip ? *((int *)pnb_get_entry(*bank, def->clip)) : 0, def->flags);
        *(void **)&out[def->idx]->wire_list = model_load_wire_data((void *)*((int *)pnb_get_entry(*bank, def->node)));
        out[def->idx]->flags2 &= 0x7fffffff;
        dc_object_set_wire(out[def->idx], out[def->idx]->wire_list);
        break;
        case 2:
        dc_object_init(out[def->idx], *((struct DcNode **)pnb_get_entry(*bank, def->node)), def->clip ? *((int *)pnb_get_entry(*bank, def->clip)) : 0, def->flags);
        dc_object_set_wire(out[def->idx], *((int *)pnb_get_entry(*bank, def->node2)));
        out[def->idx]->flags2 |= 0x80000000;
        break;
        case 3:
        {
            unsigned int *clips = (unsigned int *)def->clip;
            dc_object_init_cursor(out[def->idx], *((struct DcNode **)pnb_get_entry(*bank, def->node)), *((struct DcClip **)pnb_get_entry(*bank, clips[0])),
                            *((int *)pnb_get_entry(*bank, clips[1])), *((int *)pnb_get_entry(*bank, clips[2])), def->flags);
        }
        break;
        }
    }
    out[n] = 0;
}

/* 0x00cc0b98 b_earth, 0x00cd1410 b_eden, 0x00cbf740 b_mars, 0x00cc3e70 b_uranus, 0x00cc47e0 b_venus */
int boss_parts_release(ModelPart **list)
{
    ModelPart *part;

    if (boss_parts_built_get()) {
        for (; *list; list++) {
            part = *list;
            if (part->wire_list && !(part->flags2 & 0x80000000))
                model_free_block((void *)part->wire_list);
            dc_object_release(*list);
        }
        return 1;
    }
    boss_files_cancel();
    return 0;
}

/* 0x00cc0c30 b_earth, 0x00cd14a8 b_eden, 0x00cbf7d8 b_mars, 0x00cc3f08 b_uranus, 0x00cc4878 b_venus */
void boss_parts_reset_time(ModelPart **list)
{
    for (; *list; list++)
        (*list)->frame = 0;
}

/* 0x00cc0c68 b_earth, 0x00cd14e0 b_eden, 0x00cbf810 b_mars, 0x00cc3f40 b_uranus, 0x00cc48b0 b_venus */
void part_radius_default(ModelPart *part)
{
    part->radius = 500.0f;
}

/* 0x00cc0c78 b_earth, 0x00cd14f0 b_eden, 0x00cbf820 b_mars, 0x00cc3f50 b_uranus, 0x00cc48c0 b_venus */
void boss_files_request(int file0, int file1)
{
    boss_files_set(file0, file1);
    file_request_simple((void *)file0, 0);
    file_request_simple((void *)file1, 0);
}

/* 0x00cc0cc0 b_earth, 0x00cd1538 b_eden, 0x00cbf868 b_mars, 0x00cc3f98 b_uranus, 0x00cc4908 b_venus */
void boss_files_cancel(void)
{
    file_request_cancel((void *)boss_file_get(0));
    file_request_cancel((void *)boss_file_get(1));
}

/* 0x00cc0cf0 b_earth, 0x00cd1568 b_eden, 0x00cbf898 b_mars, 0x00cc3fc8 b_uranus, 0x00cc4938 b_venus */
int boss_files_ready(void *file0, void *file1)
{
    int done = 0;
    int ready = 1;

    if (file_request_status(file0) == 1)
        done |= 1;
    if (file_request_status(file1) == 1)
        done |= 2;
    if (done <= 2)
        ready = 0;
    return ready;
}

/* 0x00cc0d58 b_earth, 0x00cd15d0 b_eden, 0x00cbf900 b_mars, 0x00cc4030 b_uranus, 0x00cc49a0 b_venus */
void task_end_with_exit(void *w)
{
    Task *task = TASK_OF(w);

    if (task->release)
        ((void (*)(void *))task->release)(w);
    FINISH_IF(w);
    do { task->disp = 0; } while (0);
    task->release = 0;
    task->exec = 0;
}

/* 0x00cc0dc0 b_earth, 0x00cd1638 b_eden, 0x00cbf968 b_mars, 0x00cc4098 b_uranus, 0x00cc4a08 b_venus */
void boss_start(int *task, int parts, int start_arg, int level)
{
    boss_parts = parts;
    boss_arg0 = start_arg;
    boss_task = task;
    boss_level = level;
    boss_fx_slot = enemy_status_ent_alloc(2, -1, task);
    enemy_status_event(boss_fx_slot, 2, &boss_fx_start_vec);
    boss_fx_live = 1;
    tex_anime_running = 0;
    boss_parts_built = 0;
    wire_key_register();
}

/* 0x00cc0e40 b_earth, 0x00cd16b8 b_eden, 0x00cbf9e8 b_mars, 0x00cc4118 b_uranus, 0x00cc4a88 b_venus */
void boss_fx_release(void)
{
    if (boss_fx_live) {
        if (boss_scoring)
            enemy_status_event(boss_fx_slot, 3, &boss_fx_end_vec);
        else
            enemy_status_event(boss_fx_slot, 4, &boss_fx_end_vec);
        boss_fx_live = 0;
    }
}

/* 0x00cc0eb0 b_earth, 0x00cd1728 b_eden, 0x00cbfa58 b_mars, 0x00cc4188 b_uranus, 0x00cc4af8 b_venus */
int boss_get_scoring(void)
{
    return boss_scoring;
}

/* 0x00cc0ec0 b_earth, 0x00cd1738 b_eden, 0x00cbfa68 b_mars, 0x00cc4198 b_uranus, 0x00cc4b08 b_venus */
void boss_end(void *w)
{
    boss_fx_release();
    wire_key_unregister();
}

/* 0x00cc0ee0 b_earth, 0x00cd1758 b_eden, 0x00cbfa88 b_mars, 0x00cc41b8 b_uranus, 0x00cc4b28 b_venus */
int boss_get_level(void)
{
    return boss_level;
}

/* 0x00cc0ef0 b_earth, 0x00cd1768 b_eden, 0x00cbfa98 b_mars, 0x00cc41c8 b_uranus, 0x00cc4b38 b_venus */
int *boss_get_task(void)
{
    return boss_task;
}

/* 0x00cc0f00 b_earth, 0x00cd1778 b_eden, 0x00cbfaa8 b_mars, 0x00cc41d8 b_uranus, 0x00cc4b48 b_venus */
int boss_get_parts(void)
{
    return boss_parts;
}

/* 0x00cc0f10 b_earth, 0x00cd1788 b_eden, 0x00cbfab8 b_mars, 0x00cc41e8 b_uranus, 0x00cc4b58 b_venus */
void boss_files_set(int file0, int file1)
{
    boss_file0 = file0;
    boss_file1 = file1;
}

/* 0x00cc0f28 b_earth, 0x00cd17a0 b_eden, 0x00cbfad0 b_mars, 0x00cc4200 b_uranus, 0x00cc4b70 b_venus */
int boss_file_get(int which)
{
    if (!which)
        return boss_file0;
    return boss_file1;
}

/* 0x00cc0f48 b_earth, 0x00cd17c0 b_eden, 0x00cbfaf0 b_mars, 0x00cc4220 b_uranus, 0x00cc4b90 b_venus */
int boss_get_arg0(void)
{
    return boss_arg0;
}

/* 0x00cc0f58 b_earth, 0x00cd17d0 b_eden, 0x00cbfb00 b_mars, 0x00cc4230 b_uranus, 0x00cc4ba0 b_venus */
void tex_anime_running_set(int on)
{
    tex_anime_running = on;
}

/* 0x00cc0f68 b_earth, 0x00cd17e0 b_eden, 0x00cbfb10 b_mars, 0x00cc4240 b_uranus, 0x00cc4bb0 b_venus */
int tex_anime_running_get(void)
{
    return tex_anime_running;
}

/* 0x00cc0f78 b_earth, 0x00cd17f0 b_eden, 0x00cbfb20 b_mars, 0x00cc4250 b_uranus, 0x00cc4bc0 b_venus */
void boss_parts_built_set(int on)
{
    boss_parts_built = on;
}

/* 0x00cc0f88 b_earth, 0x00cd1800 b_eden, 0x00cbfb30 b_mars, 0x00cc4260 b_uranus, 0x00cc4bd0 b_venus */
int boss_parts_built_get(void)
{
    return boss_parts_built;
}

/* 0x00cc0f98 b_earth, 0x00cd1810 b_eden, 0x00cbfb40 b_mars, 0x00cc4270 b_uranus, 0x00cc4be0 b_venus */
void lock_table_set(BossLockDef *tab, int n)
{
    lock_table = tab;
    lock_table_count = n;
    boss_scoring = 1;
}

/* 0x00cc0fb8 b_earth, 0x00cd1830 b_eden, 0x00cbfb60 b_mars, 0x00cc4290 b_uranus, 0x00cc4c00 b_venus */
void lock_init(int idx, LockObj *lock, void *task, Vec3 *pos, int lock_arg, int hp, int flags)
{
    BossLockDef *def = &lock_table[idx];

    lockon_obj_init(lock, task, pos, lock_arg, def->type, -1, hp, def->lock_max, flags);
}

/* 0x00cc1008 b_earth, 0x00cd1880 b_eden, 0x00cbfbb0 b_mars, 0x00cc42e0 b_uranus, 0x00cc4c50 b_venus */
void lock_remove_entry(int idx, LockObj *lock)
{
    lockon_remove(lock);
}

/* 0x00cc1010 b_earth, 0x00cd1888 b_eden, 0x00cbfbb8 b_mars, 0x00cc42e8 b_uranus, 0x00cc4c58 b_venus */
void lock_score(int idx, LockObj *lock)
{
    Vec3 pos;
    BossLockDef *def = &lock_table[idx];

    lockon_obj_get_pos(lock, &pos);
    enemy_status_score_popup(-1, &pos, def->score, lockon_hits_max_shot_cnt(lock));
}

/* 0x00cc1078 b_earth, 0x00cd18f0 b_eden, 0x00cbfc20 b_mars, 0x00cc4350 b_uranus, 0x00cc4cc0 b_venus */
void lock_score_at(int idx, Vec3 *pos, int n)
{
    BossLockDef *def = &lock_table[idx];

    enemy_status_score_popup(-1, pos, def->score, n);
}

/* 0x00cc10a0 b_earth, 0x00cd1918 b_eden, 0x00cbfc48 b_mars, 0x00cc4378 b_uranus, 0x00cc4ce8 b_venus */
void score_at(Vec3 *pos, int score)
{
    enemy_status_score_popup(-1, pos, score, 1);
}

/* 0x00cc10b8 b_earth, 0x00cd1930 b_eden, 0x00cbfc60 b_mars, 0x00cc4390 b_uranus, 0x00cc4d00 b_venus */
void lock_score_level(int idx, Vec3 *pos)
{
    BossLockDef *def = &lock_table[idx];
    int mul;

    if (boss_scoring) {
        mul = boss_get_level();
        if (mul)
            mul *= 2;
        else
            mul = 1;
        enemy_status_score_popup(-1, pos, def->score * mul, 1);
    }
}

/* 0x00cc1140 b_earth, 0x00cd19b8 b_eden, 0x00cbfce8 b_mars, 0x00cc4418 b_uranus, 0x00cc4d88 b_venus */
void boss_scoring_on(int on)
{
    boss_scoring = on;
}

/* 0x00cc1150 b_earth, 0x00cd19c8 b_eden, 0x00cbfcf8 b_mars, 0x00cc4428 b_uranus, 0x00cc4d98 b_venus */
void lock_set_type(int idx, LockObj *lock)
{
    BossLockDef *def = &lock_table[idx];

    lockon_obj_set_name(lock, def->type);
}

/* 0x00cc1170 b_earth, 0x00cd19e8 b_eden, 0x00cbfd18 b_mars, 0x00cc4448 b_uranus, 0x00cc4db8 b_venus */
void *lock_get_type(int idx)
{
    BossLockDef *def = &lock_table[idx];

    return def->type;
}

/* 0x00cc1190 b_earth, 0x00cd1a08 b_eden, 0x00cbfd38 b_mars, 0x00cc4468 b_uranus, 0x00cc4dd8 b_venus */
void boss_texlist_bind(int *w)
{
    int old_list = RenderSetCurrentDisplayList(4);

    ulGxSetTextureList((void *)w[1]);
    RenderSetCurrentDisplayList(5);
    ulGxSetTextureList((void *)w[1]);
    RenderSetCurrentDisplayList(6);
    ulGxSetTextureList((void *)w[1]);
    RenderSetCurrentDisplayList(old_list);
}

/* 0x00cc11f0 b_earth, 0x00cd1a68 b_eden, 0x00cbfd98 b_mars, 0x00cc44c8 b_uranus, 0x00cc4e38 b_venus */
void boss_star_exec(BossStarCtl *w)
{
    Vec3 step;
    Vec3 eye;

    if (w->limit > 0.0f) {
        camera_get_eye(&eye);
        v3_sub(&step, &eye, &w->eye);
        v3_copy(&w->eye, &eye);
        if (v3_is_zero(&step))
            w->speed[w->idx] = 0.0f;
        else
            w->speed[w->idx] = vec3_length(&step);
        if (w->speed[(w->idx + 1) % 4] > w->limit || w->speed[(w->idx + 2) % 4] > w->limit
            || w->speed[(w->idx + 3) % 4] > w->limit)
            eff_star_set_color(w->star, 0);
        else
            eff_star_set_color(w->star, w->color);
    } else {
        eff_star_set_color(w->star, w->color);
    }
    if (w->count) {
        if (--w->count == 0)
            w->color = w->to;
        else
            w->color = color_lerp(w->from, w->to, (float)w->count / w->total);
    }
    w->idx = (w->idx + 1) % 4;
}

/* 0x00cc1448 b_earth, 0x00cd1cc0 b_eden, 0x00cbfff0 b_mars, 0x00cc4720 b_uranus, 0x00cc5090 b_venus */
BossStarCtl *boss_star_create(void *parent, FogArg *arg, float limit)
{
    BossStarCtl *w = task_create(parent, &boss_star_desc, 60);

    w->star = eff_star_create(w, arg);
    w->limit = limit;
    camera_get_eye(&w->eye);
    w->color = eff_star_get_color(w->star);
    return w;
}

/* 0x00cc14c0 b_earth, 0x00cd1d38 b_eden, 0x00cc0068 b_mars, 0x00cc4798 b_uranus, 0x00cc5108 b_venus */
void boss_star_set_color(BossStarCtl *w, unsigned int color)
{
    w->color = color;
    eff_star_set_color(w->star, color);
}

/* 0x00cc14d8 b_earth, 0x00cd1d50 b_eden, 0x00cc0080 b_mars, 0x00cc47b0 b_uranus, 0x00cc5120 b_venus */
void boss_star_fade(BossStarCtl *w, unsigned int color, int frames)
{
    w->from = w->color;
    w->to = color;
    w->count = w->total = frames;
}

/* 0x00cc14f0 b_earth, 0x00cd1d68 b_eden, 0x00cc0098 b_mars, 0x00cc47c8 b_uranus, 0x00cc5138 b_venus */
unsigned int boss_star_get_color(BossStarCtl *w)
{
    return w->color;
}

/* 0x00cc14f8 b_earth, 0x00cd1d70 b_eden, 0x00cc00a0 b_mars, 0x00cc47d0 b_uranus, 0x00cc5140 b_venus */
void boss_gage_exec(BossGageWork *w)
{
    w->count--;
    console2_set_boss_width(w->target * (1.0f - (float)w->count / w->total));
    if (w->count <= 0)
        do {
            FINISH(w);
        } while (0);
}

/* 0x00cc1578 b_earth, 0x00cd1df0 b_eden, 0x00cc0120 b_mars, 0x00cc4850 b_uranus, 0x00cc51c0 b_venus */
BossGageWork *boss_gage_create(int target)
{
    BossGageWork *w = task_create(boss_get_task(), &boss_gage_desc, 12);

    w->count = w->total = 240;
    if (target)
        w->target = target;
    else
        w->target = 255;
    console2_swap_boss_on(1);
    return w;
}

/* 0x00cc15e8 b_earth, 0x00cd1e60 b_eden, 0x00cc0190 b_mars, 0x00cc48c0 b_uranus, 0x00cc5230 b_venus */
void boss_pl_di_exec(BossPlDiWork *w)
{
    if (--w->delay > 0)
        return;
    if (!w->states) {
        player_command_start(w->cmd);
    } else {
        int level = player_status_get_level();

        if ((unsigned int)(level - 1) < 3 || level == 7) {
            if (w->states[0])
                player_demo_motion_start(w->states[0]);
        } else if (level == 4) {
            if (w->states[1])
                player_demo_motion_start(w->states[1]);
        }
    }
    FINISH(w);
}

/* 0x00cc16a8 b_earth, 0x00cd1f20 b_eden, 0x00cc0250 b_mars, 0x00cc4980 b_uranus, 0x00cc52f0 b_venus */
BossPlDiWork *boss_pl_di_create(void *parent, int delay, int cmd)
{
    BossPlDiWork *w = task_create(parent, &boss_pl_di_desc, 12);

    w->delay = delay;
    w->cmd = cmd;
    return w;
}

/* 0x00cc16f0 b_earth, 0x00cd1f68 b_eden, 0x00cc0298 b_mars, 0x00cc49c8 b_uranus, 0x00cc5338 b_venus */
void boss_pl_di_set(BossPlDiWork *w, int *states)
{
    w->states = states;
}

/* 0x00cc16f8 b_earth, 0x00cd1f70 b_eden, 0x00cc02a0 b_mars, 0x00cc49d0 b_uranus, 0x00cc5340 b_venus */
void cm_sc_blur_exec(CmScBlurWork *w)
{
    float rate;

    switch (w->state) {
    case 0:
        if (--w->delay > 0)
            break;
        w->state = 1;
    case 1:
        if (--w->count <= 0) {
            FINISH_IF(w);
            break;
        }
        rate = 0.0f;
        switch (w->curve) {
        case 0:
            rate = 1.0f - (float)w->count / w->total;
            if (rate < 0.25f) {
                rate = rate * 4.0f;
            } else {
                rate = 1.0f - (rate - 0.25f) / 0.75f;
                rate = rate * rate;
            }
            break;
        case 1:
            rate = (float)w->count / w->total;
            break;
        }
        screen_set_margin(rate * w->margin);
        screen_set_fade_alpha(rate * w->alpha);
        break;
    }
}

/* 0x00cc1868 b_earth, 0x00cd20e0 b_eden, 0x00cc0410 b_mars, 0x00cc4b40 b_uranus, 0x00cc54b0 b_venus */
void cm_sc_blur_release(void)
{
    screen_set_margin(0);
    screen_set_fade_alpha(0);
}

/* 0x00cc1888 b_earth, 0x00cd2100 b_eden, 0x00cc0430 b_mars, 0x00cc4b60 b_uranus, 0x00cc54d0 b_venus */
CmScBlurWork *cm_sc_blur_create(void *parent, float margin, float alpha, float delay, float secs)
{
    CmScBlurWork *w = task_create(parent, &cm_sc_blur_desc, 28);

    w->margin = margin;
    w->alpha = alpha;
    w->count = w->total = secs * FPS;
    w->delay = delay * FPS;
    return w;
}

/* 0x00cc1910 b_earth, 0x00cd2188 b_eden, 0x00cc04b8 b_mars, 0x00cc4be8 b_uranus, 0x00cc5558 b_venus */
void cm_sc_blur_set_curve(CmScBlurWork *w, int curve)
{
    w->curve = curve;
}

/* 0x00cc1918 b_earth, 0x00cd2190 b_eden, 0x00cc04c0 b_mars, 0x00cc4bf0 b_uranus, 0x00cc5560 b_venus */
/* steer(cast) */
void cm_sc_cross_exec(CmScCrossWork *w)
{
    switch (w->state) {
    case 0:
        if (--w->delay > 0)
            break;
        w->state = 1;
        w->wipe = wipe_start(0, 0, 0);
    case 1:
        if (wipe_is_covered(w->wipe)) {
            ((void (*)(void *, int, int))wipe_finish)(w->wipe, w->n, w->arg);
            w->state++;
        }
        break;
    case 2:
        break;
    }
    if (w->wipe && (TASK_OF(w->wipe)->flags & TASK_F_END))
        w->wipe = 0;
}

/* 0x00cc19c8 b_earth, 0x00cd2240 b_eden, 0x00cc0570 b_mars, 0x00cc4ca0 b_uranus, 0x00cc5610 b_venus */
void cm_sc_cross_release(CmScCrossWork *w)
{
    if (w->wipe)
        wipe_is_done(w->wipe);
}

/* 0x00cc19e8 b_earth, 0x00cd2260 b_eden, 0x00cc0590 b_mars, 0x00cc4cc0 b_uranus, 0x00cc5630 b_venus */
CmScCrossWork *cm_sc_cross_create(void *parent, int arg, float delay, float secs)
{
    CmScCrossWork *w = task_create(parent, &cm_sc_cross_desc, 20);

    w->arg = arg;
    w->delay = delay * FPS;
    w->n = secs * FPS;
    return w;
}

/* 0x00cc1a58 b_earth, 0x00cd22d0 b_eden, 0x00cc0600 b_mars, 0x00cc4d30 b_uranus, 0x00cc56a0 b_venus */
void cm_light_change_exec(CmLightChangeWork *w)
{
    unsigned int col;

    switch (w->state) {
    case 0:
        if (--w->delay > 0)
            break;
    case 1:
        if (--w->count <= 0) {
            w->state++;
            col = w->to;
            light_set_color((col & 0xff) / 255.0f, (col >> 8 & 0xff) / 255.0f, (col >> 16 & 0xff) / 255.0f);
            FINISH_IF(w);
        } else {
            col = color_lerp(w->from, w->to, (float)w->count / w->total);
            light_set_color((col & 0xff) / 255.0f, (col >> 8 & 0xff) / 255.0f, (col >> 16 & 0xff) / 255.0f);
        }
        break;
    }
}

/* 0x00cc1cb8 b_earth, 0x00cd2530 b_eden, 0x00cc0860 b_mars, 0x00cc4f90 b_uranus, 0x00cc5900 b_venus */
void cm_light_change_release(Earth1stDemoWork *w)
{
}

/* 0x00cc1cc0 b_earth, 0x00cd2538 b_eden, 0x00cc0868 b_mars, 0x00cc4f98 b_uranus, 0x00cc5908 b_venus */
CmLightChangeWork *cm_light_change_create(float delay, float secs)
{
    CmLightChangeWork *w;

    if (game_type_get() != 5)
        return 0;
    if (ConfigGetExtraType() == 3)
        return 0;
    w = task_create(boss_get_task(), &cm_light_change_desc, 28);
    w->idx = (unsigned int)ConfigGetSelect(0) % 6;
    light_get_color_packed(&w->from);
    w->to = light_colors[w->idx];
    w->delay = delay * FPS;
    w->count = w->total = secs * FPS;
    return w;
}

/* 0x00cc1da8 b_earth, 0x00cd2620 b_eden, 0x00cc0950 b_mars, 0x00cc5080 b_uranus, 0x00cc59f0 b_venus */
void cm_light_change_set_target(CmLightChangeWork *w, int *tab)
{
    w->to = tab[ConfigGetSelect(0)];
}

/* 0x00cc1df0 b_earth, 0x00cd2668 b_eden, 0x00cc0998 b_mars, 0x00cc50c8 b_uranus, 0x00cc5a38 b_venus */
int light_mode_index(void)
{
    if (game_type_get() != 5)
        return 0;
    if (ConfigGetExtraType() == 3)
        return 0;
    return ConfigGetSelect(0);
}

/* 0x00cc1e38 b_earth, 0x00cd26b0 b_eden, 0x00cc09e0 b_mars, 0x00cc5110 b_uranus, 0x00cc5a80 b_venus */
int light_mode_is_fog(void)
{
    if (game_type_get() != 5)
        return 0;
    return ConfigGetExtraType() == 3;
}

/* 0x00cc1e70 b_earth, 0x00cd26e8 b_eden, 0x00cc0a18 b_mars, 0x00cc5148 b_uranus, 0x00cc5ab8 b_venus */
void light_reset(void)
{
    light_set_color(1.0f, 1.0f, 1.0f);
}

/* 0x00cc1e88 b_earth, 0x00cd2700 b_eden, 0x00cc0a30 b_mars, 0x00cc5160 b_uranus, 0x00cc5ad0 b_venus */
/* steer(cast) */
void light_fog_set(void)
{
    if (game_type_get() == 5 && ConfigGetExtraType() == 3) {
        fog_set(light_fog_color, 4, light_fog_near, light_fog_far);
        do { fog_set_enable((void *)1); } while (0);
        BackColReq(light_fog_back);
    }
}

/* 0x00cc1f00 b_earth, 0x00cd2778 b_eden, 0x00cc0aa8 b_mars, 0x00cc51d8 b_uranus, 0x00cc5b48 b_venus */
float beat_curve(int rising)
{
    float t;

    if (rising) {
        t = 1.0f - beat_clock_get_frames_to_beat() / beat_clock_get_interval();
        if (t < 0.75f)
            t = 1.0f - t / 0.75f;
        else
            t = (t - 0.75f) * 4.0f;
    } else {
        t = beat_clock_get_frames_to_beat() / beat_clock_get_interval();
    }
    return t * t;
}

/* 0x00cc1fb8 b_earth, 0x00cd2830 b_eden, 0x00cc0b60 b_mars, 0x00cc5290 b_uranus, 0x00cc5c00 b_venus */
void items_spawn3(Vec3 *pos)
{
    Spawn rec;
    int i;

    for (i = 2; i >= 0; i--) {
        rec.x = pos->x;
        rec.y = pos->y;
        rec.z = pos->z;
        player_get_frame_accel((Vec3 *)&rec.vel_x);
        rec.scale = 1.0f;
        rec.kind = RAND_FLOAT_A() * 4.0f;
        item_create(&rec);
    }
}

/* 0x00cc2088 b_earth, 0x00cd2900 b_eden, 0x00cc0c30 b_mars, 0x00cc5360 b_uranus, 0x00cc5cd0 b_venus */
void boss_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
