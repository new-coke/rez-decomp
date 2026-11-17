/*
 * Eden boss MB enemies: lockable pulsing drones that roam the arena in the final stage, kept
 * inside it and pushed apart. Each pulse fires two XB bullets; a shot-down drone may drop an item,
 * then rests and returns. The manager brings in the next group as the stage goes on.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_enemy.h"
#include "eden.h"
#include "eden_bullet.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

extern char eden_enemy_vardef[];
extern GwVarDef eden_enemy_vardefs[];
extern struct GwVarNode *eden_enemy_vars_group;
extern float cmd_mb_or;
extern float cm_enemy_sleep_time[];
extern float cmd_mb_irate;
extern int cm_deb_disp;
extern float cmd_col_rl;
extern float cmd_col_rs;
extern float cmd_col_rt;
extern float cmd_col_rb;
extern float cmd_col_rd;
extern struct TaskDesc eden_enemy_mb_man_desc;
extern char str_eden_enemy_c[];
extern int cmd_mb_sn;
extern float cmd_mb_sr;
extern float cmd_mb_srr;
extern int cm_deb_active;
extern float eden_enemy_mb_fade[4];
extern float cmd_mb_col_rgb;
extern int cm_enemy_wire_col[2];
extern const float cmd_mb_ch_t;
extern float cmd_mb_sh_wt;
extern float cmd_mb_sh_t;
extern float cmd_mb_sh_rt;
extern int cmd_mb_sh_rnum;
extern float cmd_mb_hir, cmd_mb_mbhv;
extern struct TaskDesc eden_enemy_mb_desc;
extern float cmd_mb_nvc_s, cmd_mb_nvc_l;
extern const float cmd_mb_st;
extern const int cmd_mb_hp;
extern float cmd_mb_swr, cmd_mb_swr2, cmd_mb_nv, cmd_mb_drz, cmd_mb_vr, cmd_mb_hv;
extern float cmd_mb_vlim, cmd_mb_colrate, cmd_mb_prate, cmd_mb_slen, cmd_mb_sh_v;
extern unsigned int cm_mb_col;

/* an MB drone, 256 bytes */
typedef struct EdenEnemyMbWork {
    struct PnbHead *pack;
    void *tex_list;
    void *owner;
    void *parent;
    ModelPart **parts;
    Vec3 pos;
    Vec3 guns[2];
    char pad_38[12];
    Vec3 vel;                   /* 0x44 */
    int ang[3];
    char pad_5c[4];
    int roll;                   /* 0x60 */
    float phase_b;
    float phase_a;
    float col;                  /* fades in while awake */
    float frm;
    float frm_end;
    int shine;                  /* 0 wait, 1 rise, 2 fire, 3 fall */
    char pad_7c[4];
    int side;                   /* 0x80 */
    int time;
    int len;
    int active;
    int move_time;
    int move_len;
    int sleep;
    int sleep_len;
    int shine_time;
    int shine_len;
    int fired;
    int shots;
    LockObj lock;
    Vec3 spin_axis;
    float spin;
} EdenEnemyMbWork;

/* 24 bytes */
typedef struct EdenEnemyMbManWork {
    struct PnbHead *pack;
    void *tex_list;
    void *parent;
    EdenEnemyMbWork **mbs;
    int n;
    int group;
} EdenEnemyMbManWork;

/* 0x00cb2338 */
void eden_enemy_vars_add(void)
{
    eden_enemy_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_enemy_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_enemy_vars_group, eden_enemy_vardefs);
}

/* 0x00cb2388 */
void eden_enemy_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_enemy_vardef);
}

/* 0x00cb23a0 */
int eden_enemy_cyl_outside(Vec3 *pos, Vec3 *vel, float r_in, float r_out, float k)
{
    Vec3 nrm;
    float limit = r_out - r_in;
    float len;

    nrm.x = -pos->x;
    nrm.y = 0.0f;
    nrm.z = -pos->z;
    len = vec3_length(&nrm);
    if (limit < len) {
        vel->y = 0;
        vec3_normalize(&nrm, &nrm);
        vec_reflect_unit(vel, &nrm, vel);
        v3_scale(vel, k);
        v3_scale(&nrm, len - limit);
        v3_add(pos, &nrm);
        return 1;
    }
    return 0;
}

/* 0x00cb24c0 */
int eden_enemy_cyl_inside(Vec3 *pos, Vec3 *vel, float r_in, float r_out, float k)
{
    Vec3 nrm;
    float limit = r_in + r_out;
    float len;

    nrm.x = pos->x;
    nrm.y = 0.0f;
    nrm.z = pos->z;
    len = vec3_length(&nrm);
    if (len < limit) {
        vel->y = 0;
        vec3_normalize(&nrm, &nrm);
        vec_reflect_unit(vel, &nrm, vel);
        v3_scale(vel, k);
        v3_scale(&nrm, limit - len);
        v3_add(pos, &nrm);
        return 1;
    }
    return 0;
}

/* 0x00cb25d8 */
int eden_enemy_ceiling_check(Vec3 *pos, Vec3 *vel, float x, float y, float z)
{
    float top = pos->y + x;
    float t;

    if (y < top) {
        t = y + z;
        if (t < top) {
            vel->y = 0;
            pos->y = t - x;
        } else if (0.0f < vel->y)
            vel->y = vel->y * (1.0f - (top - y) / z);
        return 1;
    }
    return 0;
}

/* 0x00cb2658 */
int eden_enemy_floor_check(Vec3 *pos, Vec3 *vel, float x, float y, float z)
{
    float bot = pos->y - x;
    float t;

    if (bot < y) {
        t = y - z;
        if (bot < t) {
            vel->y = 0;
            pos->y = t + x;
        } else if (vel->y < 0.0f)
            vel->y = (bot - y) / z * vel->y + vel->y;
        return 1;
    }
    return 0;
}

/* steer(cast) */
/* 0x00cb26d0 */
void eden_enemy_mb_draw_body(EdenEnemyMbWork *w, int flag, float s)
{
    float t;
    int old, list;

    t = eden_get_dalpha();
    if (!(fabsf(t) < FLT_EPSILON)) {
        ulMatrixPush();
        old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        eden_enemy_mb_fade[0] = t + -1.0f;
        eden_enemy_mb_fade[1] = eden_enemy_mb_fade[2] = eden_enemy_mb_fade[3] = cmd_mb_col_rgb * (1.0f - s);
        draw_set_override_color(eden_enemy_mb_fade);
        MTX_TRANS(&w->pos);
        eden_enemy_rotate_yxz(w->ang);
        if (flag) {
            list = RenderSetCurrentDisplayList(6);
            ulGxSetTextureList(w->tex_list);
            part_draw_beat_frame(w->parts[52], w->frm, cm_enemy_wire_col[0], cm_enemy_wire_col[1], 2.0f);
            RenderSetCurrentDisplayList(list);
            model_object_draw(w->parts[50], w->frm);
        } else
            dc_object_draw(w->parts[51]);
        draw_set_override_flags(old);
        ulMatrixPop();
    }
}

/* 0x00cb2868 */
void eden_enemy_mb_disp(EdenEnemyMbWork *w)
{
    float k;
    int old;
    ModelPart *part;

    ulGxSetTextureList(w->tex_list);
    if (view_sphere_visible(&w->pos, 10.0f))
        eden_enemy_mb_draw_body(w, w->active, w->col);
    if (w->shine == 1) {
        k = 1.0f - (float)w->shine_time / (float)w->shine_len;
        old = (int)fog_set_enable(0);
        ulMatrixPush();
        MTX_TRANS(&w->guns[0]);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        part = w->parts[4];
        model_object_draw(part, part->frames * k);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&w->guns[1]);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        part = w->parts[4];
        model_object_draw(part, part->frames * k);
        ulMatrixPop();
        fog_set_enable((void *)old);
    }
}

/* 0x00cb29b8 */
int eden_enemy_mb_pulse_step(EdenEnemyMbWork *w)
{
    float t;

    switch (w->shine) {
    case 0:
        w->frm = 0;
        if (--w->shine_time <= 0) {
            w->shine_time = w->shine_len = (int)(cmd_mb_ch_t * FPS);
            w->shine = 1;
            boss_se_play(37, (int)&w->lock, 0);
        }
        return 1;
    case 1:
        t = 1.0f - (float)w->shine_time / (float)w->shine_len;
        w->frm = w->frm_end * t;
        if (--w->shine_time <= 0) {
            w->shots = cmd_mb_sh_rnum + 1;
            w->shine = 2;
            w->shine_time = 0;
        }
        break;
    case 2:
        if (--w->shine_time > 0)
            break;
        if (--w->shots) {
            w->fired = 1;
            w->shine_time = w->shine_len = (int)(cmd_mb_sh_rt * FPS);
        } else {
            w->shine = 3;
            w->shine_time = w->shine_len = (int)(cmd_mb_sh_t * FPS);
        }
        break;
    case 3:
        t = (float)w->shine_time / (float)w->shine_len;
        w->frm = w->frm_end * t;
        if (--w->shine_time <= 0) {
            w->shine_time = w->shine_len = (int)(cmd_mb_sh_wt * FPS);
            w->shine = 0;
        }
        break;
    }
    return 0;
}

/* 0x00cb2b80 */
void eden_enemy_mb_rotate(Vec3 *axis, float *ang, int *rot)
{
    float quat[4];

    *ang *= cmd_mb_irate;
    if (v3_is_zero(axis))
        return;
    quat_from_axis_angle(DEG_TO_ANG(*ang), quat, axis->x, axis->y, axis->z);
    ulMatrixPush();
    matrix_unit_set(0);
    quat_to_matrix(quat);
    eden_enemy_rotate_yxz(rot);
    matrix_current_pose(0, rot);
    ulMatrixPop();
}

/* 0x00cb2c70 */
void eden_enemy_mb_set_dir(EdenEnemyMbWork *w, Vec3 *dir, float s)
{
    float sc[4];
    int ang;

    if (!dir) {
        w->spin_axis.x = w->spin_axis.y = 0.0f;
        w->spin_axis.z = -1.0f;
        do {
            ang = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        } while (0);
        ROT_X(ang, sc, w->spin_axis.y, w->spin_axis.z);
        ang = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        ROT_Y(ang, (sc + 2), w->spin_axis.x, w->spin_axis.z);
    } else
        vec3_normalize(&w->spin_axis, dir);
    w->spin = s;
}

/* steer(cast) */
/* 0x00cb2df8 */
void eden_enemy_mb_exec(char *w)
{
    EdenEnemyMbWork *mb = (EdenEnemyMbWork *)w;
    Vec3 pl;
    Vec3 old;
    Vec3 wob_a;
    Vec3 wob_b;
    Vec3 to_pl;
    int ang[3];
    Vec3 from_pl;
    Vec3 fwd;
    Vec3 back;
    Vec3 home;
    float approach_sin, approach_cos, wob_a_sin, wob_a_cos, wob_b_sin, wob_b_cos;
    float t, k;
    int n;

    if (mb->sleep) {
        mb->sleep--;
        goto idle;
    }
    if (!mb->active)
        goto idle;
    if (!eden_test_flags(2))
        goto idle;
    v3_copy(&old, &mb->pos);
    player_get_pos(&pl);
    for (n = enemy_status_get_ticks(); n; n--) {
        t = 1.0f - (float)mb->time / (float)mb->len;
        if (eden_enemy_mb_pulse_step(mb)) {
            if (--mb->move_time <= 0)
                mb->move_time = mb->move_len = (int)((RAND_FLOAT_A() * (cmd_mb_nvc_l - cmd_mb_nvc_s) + cmd_mb_nvc_s) * FPS);
            k = (float)mb->move_time / (float)mb->move_len;
            v3_sub(&pl, &pl, &mb->pos);
            ulSinCosf(k * PI, &approach_sin, &approach_cos);
            vec_set_length(&pl, cmd_mb_nv * approach_sin);
            v3_add(&mb->pos, &pl);
        } else
            k = (float)mb->move_time / (float)mb->move_len;
        {
            Vec3 *wob = &wob_a;
            ulSinCosf(k * PI, &wob_a_sin, &wob_a_cos);
            {
                float amp = wob_a_sin;
                wobble_step_slow(wob, cmd_mb_swr2 * amp, &mb->phase_a);
            }
        }
        v3_add(&mb->pos, &wob_a);
        v3_scale(&mb->vel, cmd_mb_vr);
        v3_add(&mb->pos, &mb->vel);
        {
            Vec3 *wob = &wob_b;
            ulSinCosf(t * PI, &wob_b_sin, &wob_b_cos);
            {
                float amp = wob_b_sin;
                wobble_step_slow(wob, cmd_mb_swr * amp, &mb->phase_b);
            }
        }
        v3_add(&mb->pos, &wob_b);
    }
    v3_sub(&mb->vel, &mb->pos, &old);
    if (cmd_mb_vlim < vec3_length(&mb->vel))
        vec_set_length(&mb->vel, cmd_mb_vlim);
    player_get_pos(&to_pl);
    v3_sub(&to_pl, &to_pl, &mb->pos);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
    } while (0);
    if (!mb->side)
        mb->roll += (int)DEG_TO_ANG(cmd_mb_drz);
    else
        mb->roll -= (int)DEG_TO_ANG(cmd_mb_drz);
    ang[2] = mb->roll;
    angles_blend(ang, mb->ang, mb->ang, cmd_mb_prate);
    goto common;
idle:
    v3_scale(&mb->vel, cmd_mb_vr);
    v3_add(&mb->pos, &mb->vel);
    if (mb->shine == 1 || mb->shine == 2)
        mb->shine = 0;
common:
    eden_enemy_mb_rotate(&mb->spin_axis, &mb->spin, mb->ang);
    if (mb->active) {
        if (!mb->sleep)
            mb->col = mb->col * cmd_mb_colrate + (1.0f - cmd_mb_colrate);
        else
            mb->col *= cmd_mb_colrate;
    } else
        mb->col = 0.0f;
    v3_copy(&to_pl, &mb->vel);
    eden_enemy_cyl_outside(&mb->pos, &to_pl, cmd_mb_or, cmd_col_rl, 1.0f);
    eden_enemy_cyl_inside(&mb->pos, &to_pl, cmd_mb_or, cmd_col_rs, 1.0f);
    eden_enemy_ceiling_check(&mb->pos, &mb->vel, cmd_mb_or, cmd_col_rt, cmd_col_rd);
    eden_enemy_floor_check(&mb->pos, &mb->vel, cmd_mb_or, cmd_col_rb, cmd_col_rd);
    player_get_pos(&from_pl);
    v3_sub(&from_pl, &mb->pos, &from_pl);
    if (vec3_length(&from_pl) < cmd_mb_or) {
        vec_set_length(&from_pl, cmd_mb_hv);
        v3_sum(&mb->vel, &from_pl, &mb->vel);
    }
    fwd.y = fwd.z = 0.0f;
    fwd.x = cmd_mb_slen;
    vec_rotate_zxy(&fwd, mb->ang);
    v3_sum(&mb->guns[0], &mb->pos, &fwd);
    v3_neg(&fwd, &fwd);
    v3_sum(&mb->guns[1], &mb->pos, &fwd);
    if (mb->fired) {
        mb->fired = 0;
        do { back.x = back.y = 0.0f; back.z = -1.0f; } while (0);
        vec_rotate_zxy(&back, mb->ang);
        eden_bullet_xb_create(mb->parent, &mb->guns[0], &back, (char *)mb->parts);
        eden_bullet_xb_create(mb->parent, &mb->guns[1], &back, (char *)mb->parts);
        vec_set_length(&back, cmd_mb_sh_v);
        v3_add(&mb->vel, &back);
    }
    if (mb->lock.flags & LOCK_HIT) {
        eden_enemy_mb_set_dir(mb, 0, cmd_mb_hir);
        boss_se_play(35, (int)&mb->lock, 0);
        v3_copy(&home, &mb->lock.imp.v);
        vec_set_length(&home, cmd_mb_hv);
        v3_sum(&mb->vel, &home, &mb->vel);
        /* a hit that leaves it alive falls through to the break-all check */
        if (mb->lock.life <= 0) {
            boss_se_play(36, (int)&mb->lock, 0);
            if ((int)(RAND_FLOAT_A() * 2.0f) && !mb->sleep)
                eden_items_drop(&mb->pos, 1);
            lock_score(10, &mb->lock);
            cm_wire_break_simple(mb->parent, 2, mb->parts[53], &mb->pos, &mb->vel, mb->ang, cm_mb_col);
            mb->lock.life = mb->lock.life_max;
            mb->sleep = mb->sleep_len;
            mb->fired = 0;
            mb->shine_time = mb->shine_len = (int)(cmd_mb_ch_t * FPS);
            if (mb->shine == 1 || mb->shine == 2)
                mb->shine = 3;
            return;
        }
    }
    if (eden_get_break_all()) {
        boss_se_play(36, (int)&mb->lock, 0);
        cm_wire_break_simple(mb->parent, 2, mb->parts[53], &mb->pos, &mb->vel, mb->ang, cm_mb_col);
        FINISH_IF(w);
    }
}

/* 0x00cb3970 */
void eden_enemy_mb_release(EdenEnemyMbWork *w)
{
    lock_remove_entry(10, &w->lock);
}

/* 0x00cb3980 */
char *eden_enemy_mb_create(void *owner, void *parent, Vec3 *pos, int arg, int side)
{
    EdenEnemyMbWork *w = task_create_inherit(owner, &eden_enemy_mb_desc, 256);
    int i, n, frm;

    v3_copy(&w->pos, pos);
    w->owner = owner;
    w->parent = owner;
    w->side = side;
    w->parts = (ModelPart **)arg;
    w->phase_b = RAND_FLOAT_A() * 2.0f * PI;
    w->phase_a = RAND_FLOAT_A() * 2.0f * PI;
    frm = (int)((RAND_FLOAT_A() * (cmd_mb_nvc_l - cmd_mb_nvc_s) + cmd_mb_nvc_s) * FPS);
    w->shine_time = w->shine_len = (int)(cmd_mb_sh_wt * FPS);
    w->sleep_len = (int)(cm_enemy_sleep_time[0] * FPS);
    w->move_time = w->move_len = frm;
    do {
        w->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    w->len = 1;
    w->time = 1;
    lock_init(10, &w->lock, w, &w->pos, 0, cmd_mb_hp, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    w->frm_end = w->parts[50]->frames;
    n = (int)(cmd_mb_st * FPS);
    if (n > 0) {
        i = n;
        do
            eden_enemy_mb_exec((char *)w);
        while (--i);
    }
    return (char *)w;
}

/* 0x00cb3c50 */
void eden_enemy_mb_push_apart(EdenEnemyMbWork *mb_a, EdenEnemyMbWork *mb_b, Vec3 *pa, Vec3 *pb, Vec3 *va, Vec3 *vb, float k)
{
    Vec3 dif;
    Vec3 dv;
    Vec3 n;
    float len;

    v3_sub(&dif, pb, pa);
    if (v3_is_zero(&dif))
        return;
    len = vec3_length(&dif);
    if (!(len < k + k))
        return;
    vec_set_length(&dif, (k + k - len) * 0.5f);
    v3_add(pb, &dif);
    v3_sub(pa, pa, &dif);
    v3_sub(&dv, va, vb);
    if (v3_is_zero(&dv))
        return;
    len = vec3_length(&dv);
    if (!(cmd_mb_mbhv < len))
        return;
    ulVec3Cross(&n, va, &dif);
    eden_enemy_mb_set_dir(mb_a, &n, cmd_mb_hir);
    dif.x = -dif.x;
    dif.y = -dif.y;
    dif.z = -dif.z;
    ulVec3Cross(&n, vb, &dif);
    eden_enemy_mb_set_dir(mb_b, &n, cmd_mb_hir);
}

/* 0x00cb3ec0 */
void eden_enemy_mb_man_collide(EdenEnemyMbManWork *w)
{
    char *mb_a, *mb_b, *cur;

    for (mb_a = TASK_OF(w)->child ? (char *)((Task *)TASK_OF(w)->child + 1) : 0; mb_a; mb_a = TASK_OF(mb_a)->next ? (char *)((Task *)TASK_OF(mb_a)->next + 1) : 0) {
        for (cur = mb_a; (mb_b = TASK_OF(cur)->next ? (char *)((Task *)TASK_OF(cur)->next + 1) : 0) != 0; cur = mb_b) {
            eden_enemy_mb_push_apart((EdenEnemyMbWork *)mb_a, (EdenEnemyMbWork *)mb_b, &((EdenEnemyMbWork *)mb_a)->pos, &((EdenEnemyMbWork *)mb_b)->pos,
                                     &((EdenEnemyMbWork *)mb_a)->vel, &((EdenEnemyMbWork *)mb_b)->vel, cmd_mb_or);
        }
    }
}

/* 0x00cb3f70 */
void eden_enemy_mb_man_exec(EdenEnemyMbManWork *w)
{
    eden_enemy_mb_man_collide(w);
}

/* steer(cast) */
/* 0x00cb3f78 */
void eden_enemy_mb_man_disp(void)
{
    Vec3 ctr;

    if (!debug_flags_test(4))
        return;
    if (!cm_deb_disp)
        return;
    ctr.x = ctr.z = 0.0f;
    ctr.y = cmd_col_rt;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rl, 0x80800000);
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rs, 0x80000080);
    ctr.y = cmd_col_rt - cmd_col_rd;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rl, 0x32800000);
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rs, 0x32000080);
    ctr.y = cmd_col_rb;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rl, 0x80800000);
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rs, 0x80000080);
    ctr.y = cmd_col_rb + cmd_col_rd;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rl, 0x32800000);
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&ctr, cmd_col_rs, 0x32000080);
}

/* 0x00cb40b0 */
void eden_enemy_mb_man_release(EdenEnemyMbManWork *w)
{
    heap_free(w->mbs, 0);
}

/* 0x00cb40c0 */
int eden_enemy_mb_man_create(void *parent, int arg)
{
    EdenEnemyMbManWork *w = task_create_inherit(parent, &eden_enemy_mb_man_desc, 24);
    int n;
    int i;
    Vec3 out, amp;
    float ph;
    EdenEnemyMbWork *mb;

    amp.x = amp.y = amp.z = cmd_mb_sr;
    n = cmd_mb_sn;
    w->parent = parent;
    w->n = n;
    w->mbs = heap_block_tag(game_malloc(n * 4), str_eden_enemy_c, 699);
    for (i = 0; i < w->n; i++) {
        ph = cmd_mb_srr * PI * (float)i / (float)w->n;
        wobble_step(&out, &amp, &ph, 0.0f);
        mb = (EdenEnemyMbWork *)eden_enemy_mb_create(w, parent, &out, arg, i % 2);
        w->mbs[i] = mb;
        if (cm_deb_active) {
            mb->active = 1;
            mb->lock.flags |= LOCK_LOCKABLE;
        }
    }
    return (int)w;
}

/* 0x00cb4250 */
void eden_enemy_mb_man_next_group(EdenEnemyMbManWork *w, int grp)
{
    int *words = (int *)w; /* steer(alias): n and group read as plain ints */
    int n = words[4] / 4;
    int i;

    for (i = 0; i < n; i++) {
        EdenEnemyMbWork *mb = w->mbs[(words[5] * n + i) % words[4]];

        mb->active = grp;
        mb->sleep_len = cm_enemy_sleep_time[words[5] % words[4]] * FPS;
        mb->lock.flags |= LOCK_LOCKABLE;
    }
    words[5]++;
}

/* 0x00cb4310 */
void eden_enemy_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
