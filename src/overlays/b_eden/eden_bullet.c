/*
 * Eden boss bullets, eight kinds: EB, UB, KB from the pieces (UB also from the snake), NB from the
 * trees, FB from the core, BB from the belts, XB from the MB drones, RB from the mirror wall. All
 * lockable; eden_bullet_area_check bounces shots off the map's floor and ceiling.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_bullet.h"
#include "eden.h"
#include "eden_core.h"
#include "eden_map.h"
#include "eden_snake.h"
#include "../common/boss_common.h"

/* EB, from the pieces: homes on the player, 300 bytes */
typedef struct EdenBulletEbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    ModelPart body;
    ModelPart glow;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    LockObj lock;
    float spd;
} EdenBulletEbWork;

/* UB, from the pieces and the snake: curves to the player, 180 bytes */
typedef struct EdenBulletUbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    void *parent;
    CmBtailWork *tail;
    ModelPart **parts;
    Vec3 from;
    Vec3 pos;
    Vec3 to;
    Vec3 axis;
    Vec3 dir;
    int ang[3];
    int roll_spd;
    float phase;
    char pad_64[8];
    int time;                   /* 0x6c */
    int len;
    LockObj lock;
} EdenBulletUbWork;

/* KB, from the pieces: a cube that tumbles onto the player, 168 bytes */
typedef struct EdenBulletKbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    Vec3 from;
    Vec3 pos;
    Vec3 ofs;
    Vec3 axis;
    Vec3 unk_3c;                /* 0x3c: debris velocity, never written */
    int ang[3];
    LockObj lock;
    int time;
    int len;
    float spin;
    float frm;
    float frm_end;
} EdenBulletKbWork;

/* NB, from the trees: drifts, turns to the player, then springs at it, 192 bytes */
typedef struct EdenBulletNbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    char pad_0c[4];
    Vec3 base;                  /* 0x10 */
    Vec3 pos;
    Vec3 vel;
    Vec3 dir;
    Vec3 move;
    LockObj lock;
    int ang[3];
    int dang[3];
    int state;
    int time;
    int len;
    float spring_v;
    float spring_x;
    float phase;
    void *parent;
} EdenBulletNbWork;

/* FB, from the core: curves to the player trailing a ribbon, 292 bytes */
typedef struct EdenBulletFbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    void *parent;
    CmBtailWork *tail;
    ModelPart **parts;
    ModelPart glow;
    Vec3 from;
    Vec3 pos;
    Vec3 to;
    Vec3 glow_pos;
    Vec3 axis;
    Vec3 vel;
    int ang[3];
    int roll_spd;
    int roll;
    float phase;
    float swing;
    char pad_d4[8];
    int time;                   /* 0xdc */
    int len;
    LockObj lock;
} EdenBulletFbWork;

/* BB, from the belts: flies straight, 136 bytes */
typedef struct EdenBulletBbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    void *parent;
    ModelPart **parts;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    char pad_34[4];
    int time;                   /* 0x38 */
    int len;
    LockObj lock;
    float frm;
    float frm_end;
} EdenBulletBbWork;

/* XB, from the MB drones: flies straight trailing a ribbon, 244 bytes */
typedef struct EdenBulletXbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    void *parent;
    CmBtailWork *tail;
    ModelPart **parts;
    ModelPart glow;
    Vec3 pos;
    Vec3 tail_pos;
    Vec3 vel;
    int ang[3];
    char pad_a0[4];
    int time;                   /* 0xa4 */
    int len;
    LockObj lock;
    float frm;
    float frm_end;
} EdenBulletXbWork;

/* RB, from the mirror wall: circles the player while closing in, 216 bytes */
typedef struct EdenBulletRbWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    Vec3 center;
    Vec3 ofs;
    Vec3 pos;
    Vec3 prev;
    Vec3 vel;
    Vec3 drift;
    char pad_54[12];
    int ang[3];                 /* 0x60 */
    int unk_6c[3];              /* 0x6c: random spin, never read */
    void *parent;
    CmBtailWork *tail;
    float rot;
    float rot_spd;
    float rad;
    int time;
    int len;
    char pad_94[4];
    LockObj lock;               /* 0x98 */
} EdenBulletRbWork;

extern char eden_bullet_vardef[];
extern GwVarDef eden_bullet_vardefs[];
extern struct GwVarNode *eden_bullet_vars_group;
extern float cmd_xb_frm_v;
extern float cmd_nb_scl;
extern float cmd_fb_offz;
extern struct TaskDesc eden_bullet_bb_desc;
extern float cmd_bb_v, cmd_bb_t;
extern float cmd_rb_r;
extern unsigned int eden_bullet_rb_col;
extern int eden_bullet_fb_counts[];
extern float cmd_fba_psy;
extern float cmd_fba_msy;
extern float cmd_map_col_r;
extern float cmd_map_col_ty;
extern float cmd_map_col_by;
extern float cmd_map_col_h;
extern struct TaskDesc eden_bullet_nb_desc;
extern float cmd_nb_rz;
extern float cmd_nb_t;
extern float cmd_nb_td;
extern float cmd_xb_blen;
extern float cmd_bb_ft;
extern float cmd_bb_frm_v;
extern unsigned int cm_bb_col;
extern float cmd_xb_ft;
extern struct TaskDesc eden_bullet_ub_desc;
extern float cmd_ub_t;
extern float cmd_ub_dt;
extern float cmd_tf_t;
extern float cmd_tf_v;
extern int cmd_tf_n;
extern float cmd_tf_w;
extern float cmd_tf_wl;
extern unsigned int cm_eb_col;
extern struct TaskDesc eden_bullet_xb_desc;
extern float cmd_xb_v, cmd_xb_t;
extern int eden_bullet_tail_num;
extern struct TaskDesc eden_bullet_eb_desc;
extern float cmd_eb_t;
extern struct TaskDesc eden_bullet_kb_desc;
extern float cmd_kb_t, cmd_kb_dt, cmd_kb_vr, cmd_kb_rv;
extern struct TaskDesc eden_bullet_rb_desc;
extern float cmd_rb_dvy, cmd_rb_svy, cmd_rb_brz;
extern const float cmd_rb_t;
extern float cmd_ub_pr, cmd_ub_sr, cmd_ub_drz, cmd_ub_rzm;
extern unsigned int cm_ub_col;
extern float cmd_ub_sang_s, cmd_ub_sang_l, cmd_ub_sv, cmd_ub_sdv;
extern float cmd_kb_ry, cmd_kb_ryr, cmd_kb_mt;
extern const float cmd_kb_rxr;
extern unsigned int eden_bullet_cube_wire_col;
extern float cmd_fb_pr, cmd_fb_drz, cmd_fb_blen;
extern const float cmd_fb_rzm;
extern unsigned int cm_fb_col;
extern float cmd_rb_rad_rate, cmd_rb_rad, cmd_rb_s_rate, cmd_rb_vy, cmd_rb_l_rate, eden_bullet_rb_dy_min, eden_bullet_rb_dy_max;
extern float cmd_fb_t, cmd_fb_dt, cmd_fb_sang_s, cmd_fb_sang_l;
extern float cmd_fb_sv, cmd_fb_sdv, cmd_fb_sr;
extern struct TaskDesc eden_bullet_fb_desc;
extern float cmd_nb_t1, cmd_nb_t2, cmd_nb_t3, cmd_nb_rzr, cmd_nb_r;
extern float cmd_nb_fr;
extern unsigned int cm_nb_col;
extern float eden_bullet_nb_swings[];

/* 0x00ca6758 */
void eden_bullet_vars_add(void)
{
    eden_bullet_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_bullet_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_bullet_vars_group, eden_bullet_vardefs);
}

/* 0x00ca67a8 */
void eden_bullet_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_bullet_vardef);
}

/* 0x00ca67c0 */
void eden_bullet_eb_exec(EdenBulletEbWork *w)
{
    Vec3 pl_vel;
    int n;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    for (n = enemy_status_get_ticks(); n; n--) {
        player_get_pos(&pl_vel);
        v3_sub(&w->vel, &pl_vel, &w->pos);
        vec_set_length(&w->vel, w->spd);
        v3_add(&w->pos, &w->vel);
        dc_object_advance_time(&w->body);
        dc_object_advance_time(&w->glow);
    }
    do {
        float tau, full;

        full = ANG_TURN;
        tau = TWO_PI;
        w->ang[1] = (int)(atan2f(-w->vel.x, -w->vel.z) * full / tau);
        w->ang[0] = (int)(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)) * full / tau);
    } while (0);
    if (player_sphere_test(&w->pos, 1.0f) == 1)
        eden_player_damage();
    else if (!eden_get_lock_off()) {
        if (!(w->lock.flags & LOCK_HIT))
            return;
        if (w->lock.life > 0)
            return;
        lock_score(1, &w->lock);
    }
    boss_se_play(12, (int)&w->lock, 0);
    cm_wire_break_simple(w, 2, w->parts[9], &w->pos, &w->vel, w->ang, cm_eb_col);
    FINISH_IF(w);
}

/* 0x00ca6a50 */
void eden_bullet_eb_disp(EdenBulletEbWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    eden_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->body);
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    dc_object_draw(&w->glow);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca6af0 */
void eden_bullet_eb_release(EdenBulletEbWork *w)
{
    lock_remove_entry(1, &w->lock);
}

/* 0x00ca6b00 */
void *eden_bullet_eb_create(void *parent, void *pos, int arg, void *cell_data)
{
    EdenBulletEbWork *w = task_create_sibling_inherit(parent, &eden_bullet_eb_desc, 300);
    Vec3 pl_vel;

    w->parts = (ModelPart **)arg;
    player_get_frame_vel(&pl_vel);
    v3_sub(&w->pos, pos, &pl_vel);
    player_get_pos(&pl_vel);
    v3_sub(&pl_vel, &pl_vel, &w->pos);
    v3_scale(&pl_vel, 1.0f / (cmd_eb_t * FPS));
    w->spd = vec3_length(&pl_vel);
    w->body = *w->parts[7];
    w->glow = *w->parts[8];
    lock_init(1, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    return w;
}

/* 0x00ca6e18 */
void eden_bullet_ub_disp(EdenBulletUbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    old_blend = draw_set_override_flags(0);
    if (view_sphere_visible(pos, 10.0f)) {
        MTX_TRANS(pos);
        eden_bullet_rotate_yxz(w->ang);
        dc_object_draw(w->parts[10]);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00ca6eb0 */
void eden_bullet_curve_move(Vec3 *from, Vec3 *to, Vec3 *pos, Vec3 *axis, float t)
{
    Vec3 step;
    float sc[2];

    step.x = (to->x * t + from->x * (1.0f - t) - pos->x) * t;
    step.y = (to->y * t + from->y * (1.0f - t) - pos->y) * t;
    step.z = (to->z * t + from->z * (1.0f - t) - pos->z) * t;
    v3_add(pos, &step);
    ulSinCosf(t * PI, &sc[0], &sc[1]);
    v3_mul(&step, axis, sc[0]);
    v3_add(pos, &step);
}

/* steer(cast) */
/* 0x00ca6fd8 */
void eden_bullet_ub_exec(char *w)
{
    EdenBulletUbWork *ub = (EdenBulletUbWork *)w;
    Vec3 pl_vel;
    Vec3 wob;
    Vec3 z;
    int n, lim;
    float t;

    player_get_frame_vel(&pl_vel);
    v3_add(&ub->to, &pl_vel);
    v3_add(&ub->from, &pl_vel);
    v3_add(&ub->pos, &pl_vel);
    v3_copy(&ub->dir, &ub->pos);
    player_get_pos(&pl_vel);
    v3_lerp(&ub->to, &ub->to, &pl_vel, cmd_ub_pr);
    for (n = enemy_status_get_ticks(); n; n--) {
        lim = (int)DEG_TO_ANG(cmd_ub_rzm);
        ub->roll_spd += (int)DEG_TO_ANG(cmd_ub_drz);
        t = 1.0f - (float)ub->time / (float)ub->len;
        if (lim < ub->roll_spd)
            ub->roll_spd = lim;
        ub->ang[2] += ub->roll_spd;
        eden_bullet_curve_move(&ub->from, &ub->to, &ub->pos, &ub->axis, t);
        wobble_step_slow(&wob, cmd_ub_sr * (1.0f - t), &ub->phase);
        v3_add(&ub->pos, &wob);
        if (--ub->time <= 0)
            goto hit;
    }
    v3_sub(&ub->dir, &ub->dir, &ub->pos);
    do {
        ub->ang[1] = (int)RAD_TO_ANG(atan2f(-ub->dir.x, -ub->dir.z));
        ub->ang[0] = (int)RAD_TO_ANG(atan2f(ub->dir.y, sqrtf(ub->dir.x * ub->dir.x + ub->dir.z * ub->dir.z)));
    } while (0);
    if (player_sphere_test(&ub->pos, 1.0f) == 1)
        eden_player_damage();
    else if (!eden_get_lock_off()) {
        if (!(ub->lock.flags & LOCK_HIT))
            return;
        if (ub->lock.life > 0)
            return;
        lock_score(2, &ub->lock);
    }
hit:
    boss_se_play(16, (int)&ub->lock, 0);
    z.x = 0;
    z.y = 0;
    z.z = 0;
    cm_wire_break_simple(w, 2, ub->parts[11], &ub->pos, &z, ub->ang, cm_ub_col);
    FINISH_IF(w);
}

/* 0x00ca7428 */
void eden_bullet_ub_release(EdenBulletUbWork *w)
{
    lock_remove_entry(2, &w->lock);
}

/* 0x00ca7438 */
EdenBulletUbWork *eden_bullet_ub_create(void *parent, Vec3 *pos, Vec3 *dir, int kind)
{
    EdenBulletUbWork *w = task_create_inherit(parent, &eden_bullet_ub_desc, 180);
    BossRibbonDef def;

    v3_copy(&w->pos, pos);
    w->parts = (ModelPart **)kind;
    w->parent = parent;
    v3_copy(&w->from, &w->pos);
    v3_copy(&w->axis, dir);
    player_get_pos(&w->to);
    w->phase = RAND_FLOAT_A() * 2.0f * PI;
    w->time = w->len = (int)((cmd_ub_t + RAND_FLOAT_A() * cmd_ub_dt) * FPS);
    lock_init(2, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    def.color = COL_WHITE;
    def.rate = cmd_tf_v;
    def.secs = cmd_tf_t;
    def.tex = 4;
    def.n = cmd_tf_n;
    def.width_a = cmd_tf_w;
    def.width_b = cmd_tf_wl;
    w->tail = cm_btail_create(parent, w, &w->pos, &def);
    return w;
}

/* 0x00ca7630 */
void eden_bullet_ub_fire(void *parent, void *vp, int kind, int n)
{
    Vec3 *pos = vp;
    Vec3 pl;
    Vec3 spawn_pos;
    Vec3 pl_vel;
    int ang[3];
    Vec3 v;
    float sc[8];
    float k, z;
    int i;

    player_get_frame_vel(&pl_vel);
    v3_sum(&spawn_pos, &pl_vel, pos);
    player_get_pos(&pl);
    do {
        float px = spawn_pos.x;
        pl_vel.x = pl.x - px;
        pl_vel.y = pl.y - spawn_pos.y;
        pl_vel.z = pl.z - spawn_pos.z;
    } while (0);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-pl_vel.x, -pl_vel.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(pl_vel.y, sqrtf(pl_vel.x * pl_vel.x + pl_vel.z * pl_vel.z)));
    } while (0);
    ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    for (i = 0; i < n; i++) {
        z = 0.0f;
        v.y = z;
        v.z = -1.0f;
        v.x = z;
        ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_ub_sang_l - cmd_ub_sang_s) + cmd_ub_sang_s), sc, v.y, v.z);
        do {
            float rot_u = v.x, rot_v = v.y;
            ulSinCos((int)DEG_TO_ANG((float)(i * 360 / n)) + ang[2], &sc[2], &sc[3]);
            v.x = rot_u * sc[3] - rot_v * sc[2];
            v.y = rot_u * sc[2] + rot_v * sc[3];
        } while (0);
        vec3_normalize(&v, &v);
        k = RAND_FLOAT_A() * ((cmd_ub_sv + cmd_ub_sdv) - cmd_ub_sv) + cmd_ub_sv;
        v.x *= k;
        v.y *= k;
        v.z *= k;
        ROT_X(ang[0], (sc + 4), v.y, v.z);
        ROT_Y(ang[1], (sc + 6), v.x, v.z);
        eden_bullet_ub_create(parent, &spawn_pos, &v, kind);
    }
    boss_se_play(15, 0, eden_core_shine_get_pos(0));
}

/* 0x00ca7a80 */
void eden_bullet_kb_exec(EdenBulletKbWork *w)
{
    Vec3 pl_vel;
    Vec3 pl;
    Vec3 ofs;
    float sc[4];
    int n;
    float t, k;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->from, &pl_vel);
    v3_add(&w->pos, &pl_vel);
    player_get_pos(&pl);
    w->spin = w->spin * cmd_kb_ryr + cmd_kb_ry * (1.0f - cmd_kb_ryr);
    w->ang[0] = (int)((float)w->ang[0] * cmd_kb_rxr);
    w->ang[2] = (int)((float)w->ang[2] * cmd_kb_rxr);
    w->ang[1] += (int)DEG_TO_ANG(w->spin);
    for (n = enemy_status_get_ticks(); n; n--) {
        t = (float)w->time / (float)w->len;
        if (0.5f <= t)
            k = 1.0f;
        else {
            ulSinCosf((t + t) * PI, &sc[0], &sc[1]);
            k = sc[1] * -0.5f + 0.5f;
        }
        v3_mul(&ofs, &w->ofs, k);
        v3_lerp(&w->pos, &w->from, &pl, t);
        v3_add(&w->pos, &ofs);
        ulSinCosf(t * PI, &sc[2], &sc[3]);
        v3_mul(&ofs, &w->axis, sc[2]);
        v3_add(&w->pos, &ofs);
        if (w->time)
            w->time--;
        if ((float)w->time < cmd_kb_mt * FPS) {
            w->frm += 1.0f;
            if (w->frm_end <= w->frm)
                w->frm = w->frm_end;
        }
    }
    if (!eden_get_lock_off()) {
        if (w->time <= 0)
            eden_player_damage();
        else {
            if (!(w->lock.flags & LOCK_HIT))
                return;
            if (w->lock.life > 0)
                return;
            lock_score(6, &w->lock);
        }
    }
    boss_se_play(14, (int)&w->lock, 0);
    eden_shatter_spawn(w, 2, w->parts[2], &w->pos, &w->unk_3c, w->ang, eden_bullet_cube_wire_col);
    if ((float)w->time < cmd_kb_mt * FPS)
        cm_wire_break_simple(w, 2, w->parts[6], &w->pos, &w->unk_3c, w->ang, eden_bullet_cube_wire_col);
    FINISH_IF(w);
}

/* 0x00ca7f08 */
void eden_bullet_kb_disp(EdenBulletKbWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    boss_set_texture_num(eden_tex_offset());
    boss_set_screen_scale(1.0f, 1.0f);
    ((void (*)(float, float))draw_nop)(0.0f, 0.0f);
    old_blend = draw_set_override_flags(0);
    draw_set_color(RGBA(0, 0xc8, 0, 0x80));
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    eden_bullet_rotate_yxz(w->ang);
    tree_draw_culled_a_w((WireObj *)w->parts[0]->root);
    dc_object_draw(w->parts[1]);
    model_object_draw(w->parts[5], w->frm);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca7fe0 */
void eden_bullet_kb_release(EdenBulletKbWork *w)
{
    lock_remove_entry(6, &w->lock);
}

/* 0x00ca7ff0 */
void *eden_bullet_kb_create(void *parent, void *pos, void *vel, void *ang, int src)
{
    EdenBulletKbWork *w = task_create_sibling_inherit(parent, &eden_bullet_kb_desc, 168);
    Vec3 pl_vel;

    w->parts = (ModelPart **)src;
    w->time = w->len = (int)((cmd_kb_t + RAND_FLOAT_A() * cmd_kb_dt) * FPS);
    player_get_frame_vel(&pl_vel);
    v3_sub(&pl_vel, pos, &pl_vel);
    v3_copy(&w->from, &pl_vel);
    v3_copy(&w->pos, &pl_vel);
    v3_mul(&w->ofs, vel, cmd_kb_vr);
    player_get_pos(&pl_vel);
    v3_sub(&pl_vel, &pl_vel, &w->pos);
    vec_up_for_dir(&pl_vel, &w->axis, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
    vec_set_length(&w->axis, cmd_kb_rv);
    i3_copy(w->ang, ang);
    do {
        w->ang[0] = ANG_WRAP(w->ang[0]);
        w->ang[1] = ANG_WRAP(w->ang[1]);
        w->ang[2] = ANG_WRAP(w->ang[2]);
    } while (0);
    lock_init(6, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    w->frm_end = w->parts[5]->frames;
    return w;
}

/* steer(cast) */
/* 0x00ca82b0 */
void eden_bullet_nb_exec(char *w)
{
    EdenBulletNbWork *nb = (EdenBulletNbWork *)w;
    Vec3 pl_vel;
    Vec3 pl;
    Vec3 pl_pos;
    Vec3 step;
    float one;
    float ratio;
    int n;

    player_get_frame_vel(&pl_vel);
    n = enemy_status_get_ticks();
    v3_sum(&nb->base, &pl_vel, &nb->base);
    switch (nb->state) {
    case 0:
        for (; n; n--) {
            v3_sum(&nb->base, &nb->vel, &nb->base);
            v3_scale(&nb->vel, cmd_nb_fr);
            nb->dang[2] = (float)nb->dang[2] * cmd_nb_rzr;
            i3_sum(nb->ang, nb->ang, nb->dang);
            if (nb->time)
                nb->time -= 1;
        }
        do {
            nb->ang[1] = RAD_TO_ANG(atan2f(-nb->vel.x, -nb->vel.z));
            nb->ang[0] = RAD_TO_ANG(atan2f(nb->vel.y, sqrtf(nb->vel.x * nb->vel.x + nb->vel.z * nb->vel.z)));
        } while (0);
        if (nb->time <= 0) {
            nb->time = nb->len = (int)(cmd_nb_t1 * FPS);
            nb->state = 1;
        }
        break;
    case 1:
        for (; n; n--) {
            v3_sum(&nb->base, &nb->vel, &nb->base);
            v3_scale(&nb->vel, cmd_nb_fr);
            i3_sum(nb->ang, nb->ang, nb->dang);
            if (nb->time)
                nb->time -= 1;
        }
        one = 1.0f;
        player_get_pos(&pl);
        v3_sub(&pl, &pl, &nb->base);
        vec3_normalize(&pl, &pl);
        ratio = (float)nb->time / (float)nb->len;
        v3_lerp(&pl, &nb->dir, &pl, ratio);
        do {
            nb->ang[1] = RAD_TO_ANG(atan2f(-pl.x, -pl.z));
            nb->ang[0] = RAD_TO_ANG(atan2f(pl.y, sqrtf(pl.x * pl.x + pl.z * pl.z)));
        } while (0);
        if (nb->time <= 0) {
            nb->time = nb->len = (int)(cmd_nb_t3 * FPS);
            nb->state = 2;
            player_get_pos(&pl_pos);
            v3_sub(&nb->move, &pl_pos, &nb->base);
            v3_scale(&nb->move, one / (cmd_nb_t2 * FPS));
            nb->spring_v = 0.208f;
        }
        break;
    case 2:
        for (; n; n--) {
            v3_sum(&nb->base, &nb->vel, &nb->base);
            v3_scale(&nb->vel, cmd_nb_fr);
            spring_to(&nb->spring_v, &nb->spring_x, 1.0f, 0.12f, 0.014f);
            v3_mul(&step, &nb->move, nb->spring_x);
            v3_sum(&nb->base, &step, &nb->base);
            i3_sum(nb->ang, nb->ang, nb->dang);
            if (nb->time)
                nb->time -= 1;
        }
        do {
            nb->ang[1] = RAD_TO_ANG(atan2f(-nb->move.x, -nb->move.z));
            nb->ang[0] = RAD_TO_ANG(atan2f(nb->move.y, sqrtf(nb->move.x * nb->move.x + nb->move.z * nb->move.z)));
        } while (0);
        break;
    }
    wobble_step_slow(&pl, eden_bullet_nb_swings[boss_get_level()], &nb->phase);
    v3_sum(&nb->pos, &nb->base, &pl);
    if (eden_get_lock_off())
        goto kill;
    if (player_sphere_test(&nb->pos, cmd_nb_r) & 1) {
        eden_player_damage();
        goto kill;
    }
    if ((nb->lock.flags & LOCK_HIT) && nb->lock.life <= 0) {
        lock_score(3, &nb->lock);
    kill:
        boss_se_play(18, (int)&nb->lock, 0);
        cm_wire_break_simple(w, 2, nb->parts[13], &nb->pos, 0, nb->ang, cm_nb_col);
        FINISH_IF(w);
    }
}

/* 0x00ca8b90 */
void eden_bullet_nb_disp(EdenBulletNbWork *w)
{
    int old_blend;
    float k;

    ulGxSetTextureList(w->tex_list);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    eden_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    k = cmd_nb_scl;
    vu0_matrix_scale(ulMatrixCur, k, k, k);
    dc_object_draw(w->parts[12]);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca8c38 */
void eden_bullet_nb_release(EdenBulletNbWork *w)
{
    lock_remove_entry(3, &w->lock);
}

/* 0x00ca8c48 */
void *eden_bullet_nb_create(void *parent, Vec3 *pos, Vec3 *dir, int arg)
{
    EdenBulletNbWork *w = task_create_inherit(parent, &eden_bullet_nb_desc, 192);

    v3_copy(&w->base, pos);
    w->parts = (ModelPart **)arg;
    w->parent = parent;
    v3_copy(&w->vel, dir);
    vec3_normalize(&w->dir, dir);
    w->dang[2] = (int)DEG_TO_ANG(cmd_nb_rz);
    w->time = w->len = (int)((cmd_nb_t + RAND_FLOAT_A() * cmd_nb_td) * FPS);
    lock_init(3, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    w->phase = RAND_FLOAT_A() * TWO_PI;
    return w;
}

/* 0x00ca8dd8 */
void eden_bullet_fb_disp(EdenBulletFbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    old_blend = draw_set_override_flags(0);
    if (view_sphere_visible(pos, 10.0f)) {
        ulMatrixPush();
        MTX_TRANS(pos);
        eden_bullet_rotate_yxz(w->ang);
        vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, cmd_fb_offz);
        dc_object_draw(w->parts[14]);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&w->glow_pos);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        dc_object_draw(&w->glow);
        ulMatrixPop();
    }
    draw_set_override_flags(old_blend);
}

/* steer(cast) */
/* 0x00ca8ed0 */
void eden_bullet_fb_exec(char *w)
{
    EdenBulletFbWork *fb = (EdenBulletFbWork *)w;
    Vec3 pl_vel;
    Vec3 wob;
    float s, c;
    int n, lim;
    float t;

    player_get_frame_vel(&pl_vel);
    v3_add(&fb->to, &pl_vel);
    v3_add(&fb->from, &pl_vel);
    v3_add(&fb->pos, &pl_vel);
    v3_copy(&fb->vel, &fb->pos);
    player_get_pos(&pl_vel);
    v3_lerp(&fb->to, &fb->to, &pl_vel, cmd_fb_pr);
    for (n = enemy_status_get_ticks(); n; n--) {
        if (fb->time)
            fb->time--;
        t = 1.0f - (float)fb->time / (float)fb->len;
        fb->roll_spd += (int)DEG_TO_ANG(cmd_fb_drz);
        lim = (int)DEG_TO_ANG(cmd_fb_rzm);
        if (lim < fb->roll_spd)
            fb->roll_spd = lim;
        fb->roll += fb->roll_spd;
        eden_bullet_curve_move(&fb->from, &fb->to, &fb->pos, &fb->axis, t);
        if (fb->time) {
            Vec3 *v = &wob;
            float amp;

            ulSinCosf(t * PI, &s, &c);
            amp = s;
            wobble_step_slow(v, fb->swing * amp, &fb->phase);
            v3_add(&fb->pos, &wob);
        }
        dc_object_advance_time(&fb->glow);
    }
    v3_sub(&fb->vel, &fb->vel, &fb->pos);
    do {
        fb->ang[1] = (int)RAD_TO_ANG(atan2f(-fb->vel.x, -fb->vel.z));
        fb->ang[0] = (int)RAD_TO_ANG(atan2f(fb->vel.y, sqrtf(fb->vel.x * fb->vel.x + fb->vel.z * fb->vel.z)));
    } while (0);
    v3_neg(&fb->vel, &fb->vel);
    {
        float k = cmd_fb_blen;

        fb->ang[2] = fb->roll;
        fb->glow_pos.z = k;
    }
    fb->glow_pos.x = fb->glow_pos.y = 0.0f;
    vec_rotate_zxy(&fb->glow_pos, fb->ang);
    v3_add(&fb->glow_pos, &fb->pos);
    if (fb->time <= 0)
        eden_player_damage();
    else if (player_sphere_test(&fb->pos, 1.0f) == 1)
        eden_player_damage();
    else if (!eden_get_lock_off()) {
        if (eden_bullet_area_check(&fb->pos, &fb->vel, 0))
            fb->vel.y = -fb->vel.y;
        else {
            if (!(fb->lock.flags & LOCK_HIT))
                return;
            if (fb->lock.life > 0)
                return;
            lock_score(4, &fb->lock);
        }
    }
    boss_se_play(20, (int)&fb->lock, 0);
    cm_wire_break_simple(w, 2, fb->parts[15], &fb->pos, &fb->vel, fb->ang, cm_fb_col);
    FINISH_IF(w);
}

/* 0x00ca9418 */
void eden_bullet_fb_release(EdenBulletFbWork *w)
{
    cm_btail_set_running(w->tail, 1);
    lock_remove_entry(4, &w->lock);
}

/* 0x00ca9450 */
void *eden_bullet_fb_create(void *w, Vec3 *pos, Vec3 *dir, void *src)
{
    EdenBulletFbWork *task = task_create_inherit(w, &eden_bullet_fb_desc, 292);
    int ang[2];
    BossRibbonDef def;
    float sc[4];
    float z;

    task->parent = w;
    task->parts = src;
    task->glow = *((ModelPart **)src)[63];
    v3_copy(&task->pos, pos);
    v3_copy(&task->from, &task->pos);
    player_get_pos(&task->to);
    v3_copy(&task->axis, dir);
    do { ang[1] = (int)RAD_TO_ANG(atan2f(-task->axis.x, -task->axis.z));
    ang[0] = (int)RAD_TO_ANG(atan2f(task->axis.y, sqrtf(task->axis.x * task->axis.x + task->axis.z * task->axis.z))); } while (0);
    z = -(RAND_FLOAT_A() * (cmd_fb_sv + cmd_fb_sdv - cmd_fb_sv) + cmd_fb_sv);
    task->axis.x = task->axis.y = 0.0f;
    task->axis.z = z;
    ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fb_sang_l - cmd_fb_sang_s) + cmd_fb_sang_s), sc, task->axis.y, task->axis.z);
    ROT_Z((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), (sc + 2), task->axis.x, task->axis.y);
    vec_rotate_zxy(&task->axis, ang);
    task->swing = RAND_FLOAT_A() * (cmd_fb_sr - cmd_fb_sr * 0.8f) + cmd_fb_sr * 0.8f;
    task->phase = RAND_FLOAT_A() * 2.0f * PI;
    task->time = task->len = (int)((cmd_fb_t + RAND_FLOAT_A() * cmd_fb_dt) * FPS);
    lock_init(4, &task->lock, task, &task->pos, 0, 1, 0);
    lockon_entry(&task->lock);
    def.color = COL_WHITE;
    def.rate = cmd_tf_v;
    def.secs = cmd_tf_t;
    def.tex = 5;
    def.n = eden_bullet_tail_num;
    def.width_a = cmd_tf_w * 0.5f;
    def.width_b = cmd_tf_wl * 0.5f;
    task->tail = cm_btail_create(w, task, &task->pos, &def);
    return task;
}

/* 0x00ca99f8 */
void eden_bullet_fb_spawn(void *w, void *src, unsigned int mask)
{
    int n = eden_bullet_fb_counts[boss_get_level()];
    int i;
    Vec3 pos, dir;

    if (mask & 1) {
        pos.x = pos.z = 0.0f;
        pos.y = cmd_fba_psy;
        dir.x = -pos.x;
        dir.y = -pos.y;
        dir.z = -pos.z;
        for (i = 0; i < n; i++)
            eden_bullet_fb_create(w, &pos, &dir, src);
    }
    if (mask & 2) {
        pos.x = pos.z = 0.0f;
        pos.y = cmd_fba_msy;
        dir.x = -pos.x;
        dir.y = -pos.y;
        dir.z = -pos.z;
        for (i = 0; i < n; i++)
            eden_bullet_fb_create(w, &pos, &dir, src);
    }
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
    boss_se_play(19, 0, (int)&pos);
}

/* 0x00ca9b58 */
void eden_bullet_bb_disp(EdenBulletBbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    old_blend = draw_set_override_flags(0);
    if (view_sphere_visible(pos, 10.0f)) {
        MTX_TRANS(pos);
        eden_bullet_rotate_yxz(w->ang);
        model_object_draw(w->parts[48], w->frm);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00ca9bf8 */
void eden_bullet_bb_disp_fade(EdenBulletBbWork *w)
{
    float rgba[4];
    int old_blend;

    memset(rgba, 0, 16);
    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    draw_push_strip_flags();
    draw_push_override_color();
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    rgba[0] = (float)w->time / (float)w->len + -1.0f;
    draw_set_override_color(rgba);
    if (view_sphere_visible(&w->pos, 10.0f)) {
        MTX_TRANS(&w->pos);
        eden_bullet_rotate_yxz(w->ang);
        model_object_draw(w->parts[48], w->frm);
    }
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00ca9d10 */
void eden_bullet_bb_step(EdenBulletBbWork *w)
{
    w->time--;
    v3_add(&w->pos, &w->vel);
}

/* 0x00ca9d58 */
void eden_bullet_bb_exec_fade(EdenBulletBbWork *w)
{
    eden_bullet_bb_step(w);
    if (--w->time <= 0)
        FINISH_IF(w);
}

/* 0x00ca9da0 */
void eden_bullet_bb_exec(EdenBulletBbWork *w)
{
    int n;
    Vec3 z;

    for (n = enemy_status_get_ticks(); n; n--) {
        eden_bullet_bb_step(w);
        w->frm += cmd_bb_frm_v;
        if (w->frm_end <= w->frm)
            w->frm = w->frm_end;
    }
    if (w->time <= 0) {
        TASK_OF(w)->exec = (void (*)(void *))eden_bullet_bb_exec_fade;
        TASK_OF(w)->disp = (void (*)(void *))eden_bullet_bb_disp_fade;
        w->time = w->len = (int)(cmd_bb_ft * FPS);
    }
    if (player_sphere_test(&w->pos, 1.0f) == 1)
        eden_player_damage();
    else if (!eden_get_lock_off() && !eden_bullet_area_check(&w->pos, &w->vel, 0)) {
        if (!(w->lock.flags & LOCK_HIT))
            return;
        if (w->lock.life > 0)
            return;
        lock_score(8, &w->lock);
    }
    boss_se_play(26, (int)&w->lock, 0);
    z.x = 0;
    z.y = 0;
    z.z = 0;
    cm_wire_break_simple(w, 2, w->parts[49], &w->pos, &z, w->ang, cm_bb_col);
    FINISH_IF(w);
}

/* 0x00ca9f50 */
void eden_bullet_bb_release(EdenBulletBbWork *w)
{
    lock_remove_entry(8, &w->lock);
}

/* 0x00ca9f60 */
EdenBulletBbWork *eden_bullet_bb_create(void *parent, Vec3 *pos, int *ang, int src)
{
    EdenBulletBbWork *w = task_create_inherit(parent, &eden_bullet_bb_desc, 136);

    v3_copy(&w->pos, pos);
    i3_copy(w->ang, ang);
    w->parent = parent;
    w->parts = (ModelPart **)src;
    w->vel.z = cmd_bb_v;
    vec_rotate_zxy(&w->vel, ang);
    w->time = w->len = cmd_bb_t * FPS;
    lock_init(8, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    w->frm_end = w->parts[48]->frames;
    return w;
}

/* 0x00caa090 */
void eden_bullet_xb_disp(EdenBulletXbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    old_blend = draw_set_override_flags(0);
    if (view_sphere_visible(pos, 10.0f)) {
        ulMatrixPush();
        MTX_TRANS(pos);
        eden_bullet_rotate_yxz(w->ang);
        model_object_draw(w->parts[54], w->frm);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&w->tail_pos);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        dc_object_draw(&w->glow);
        ulMatrixPop();
    }
    draw_set_override_flags(old_blend);
}

/* steer(cast) */
/* 0x00caa178 */
void eden_bullet_xb_disp_fade(EdenBulletXbWork *w)
{
    float rgba[4];
    Vec3 *pos = &w->pos;
    int old;

    memset(rgba, 0, 16);
    ulGxSetTextureList(w->tex_list);
    draw_push_strip_flags();
    draw_push_override_color();
    old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    rgba[0] = (float)w->time / (float)w->len + -1.0f;
    draw_set_override_color(rgba);
    if (view_sphere_visible(pos, 10.0f)) {
        ulMatrixPush();
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        eden_bullet_rotate_yxz(w->ang);
        model_object_draw(w->parts[54], w->frm);
        ulMatrixPop();
        ulMatrixPush();
        MTX_TRANS(&w->tail_pos);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        dc_object_draw(&w->glow);
        ulMatrixPop();
    }
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old);
}

/* 0x00caa2d8 */
void eden_bullet_xb_step(EdenBulletXbWork *w)
{
    v3_add(&w->pos, &w->vel);
    do {
        w->ang[1] = (int)RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = (int)RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
    angles_turn_y(w->ang, ANG_180, w->ang);
    w->tail_pos.x = w->tail_pos.y = 0.0f;
    w->tail_pos.z = cmd_xb_blen;
    vec_rotate_zxy(&w->tail_pos, w->ang);
    w->time--;
    v3_add(&w->tail_pos, &w->pos);
}

/* 0x00caa470 */
void eden_bullet_xb_exec_fade(EdenBulletXbWork *w)
{
    eden_bullet_xb_step(w);
    w->frm += cmd_xb_frm_v;
    if (w->frm >= w->frm_end)
        w->frm -= w->frm_end;
    if (--w->time <= 0)
        FINISH_IF(w);
}

/* 0x00caa4e8 */
void eden_bullet_xb_exec(EdenBulletXbWork *w)
{
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        eden_bullet_xb_step(w);
        dc_object_advance_time(&w->glow);
        w->frm += cmd_xb_frm_v;
        if (w->frm_end <= w->frm)
            w->frm -= w->frm_end;
    }
    if (w->time <= 0) {
        TASK_OF(w)->exec = (void (*)(void *))eden_bullet_xb_exec_fade;
        TASK_OF(w)->disp = (void (*)(void *))eden_bullet_xb_disp_fade;
        w->time = w->len = (int)(cmd_xb_ft * FPS);
    }
    if (player_sphere_test(&w->pos, 1.0f) == 1)
        eden_player_damage();
    else if (!eden_get_lock_off()) {
        if (eden_bullet_area_check(&w->pos, &w->vel, 0))
            w->vel.y = -w->vel.y;
        else {
            if (!(w->lock.flags & LOCK_HIT))
                return;
            if (w->lock.life > 0)
                return;
            lock_score(11, &w->lock);
        }
    }
    boss_se_play(38, (int)&w->lock, 0);
    cm_wire_break_simple(w, 2, w->parts[55], &w->pos, &w->vel, w->ang, cm_bb_col);
    FINISH_IF(w);
}

/* 0x00caa6c8 */
void eden_bullet_xb_release(EdenBulletXbWork *w)
{
    lock_remove_entry(11, &w->lock);
}

/* 0x00caa6d8 */
EdenBulletXbWork *eden_bullet_xb_create(void *parent, Vec3 *pos, Vec3 *dir, char *src)
{
    EdenBulletXbWork *w = task_create_sibling_inherit(parent, &eden_bullet_xb_desc, 244);
    BossRibbonDef def;

    w->parent = parent;
    w->parts = (ModelPart **)src;
    w->glow = *w->parts[63];
    v3_copy(&w->pos, pos);
    v3_copy(&w->vel, dir);
    vec_set_length(&w->vel, cmd_xb_v);
    w->time = w->len = (int)(cmd_xb_t * FPS);
    lock_init(11, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    w->frm_end = w->parts[54]->frames;
    def.color = COL_WHITE;
    def.rate = cmd_tf_v;
    def.secs = cmd_tf_t;
    def.tex = 5;
    def.n = eden_bullet_tail_num;
    def.width_a = cmd_tf_w * 0.5f;
    def.width_b = cmd_tf_wl * 0.5f;
    w->tail = cm_btail_create(parent, w, &w->pos, &def);
    return w;
}

/* 0x00caa988 */
void eden_bullet_rb_step(EdenBulletRbWork *w)
{
    int ang[2];
    Vec3 pl;
    float sc[2];
    float len;
    int dy;

    v3_copy(&w->prev, &w->pos);
    w->rot_spd = w->rot_spd * cmd_rb_rad_rate + TWO_PI / (cmd_rb_rad * FPS) * (1.0f - cmd_rb_rad_rate);
    w->rot += w->rot_spd;
    while (TWO_PI < w->rot)
        w->rot -= TWO_PI;
    while (w->rot < -TWO_PI)
        w->rot += TWO_PI;
    w->ofs.x = 0.0f;
    w->ofs.z = w->rad;
    ROT_Y((int)(w->rot * ANG_PER_RAD), sc, w->ofs.x, w->ofs.z);
    v3_sum(&w->pos, &w->ofs, &w->center);
    player_get_pos(&pl);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-pl.x, -pl.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(pl.y, sqrtf(pl.x * pl.x + pl.z * pl.z)));
    } while (0);
    v3_sub(&pl, &pl, &w->center);
    len = sqrtf(pl.x * pl.x + pl.z * pl.z);
    w->rad = w->rad * cmd_rb_l_rate + len * (1.0f - cmd_rb_l_rate);
    v3_sub(&pl, &pl, &w->pos);
    dy = (int)(w->rot * ANG_PER_RAD);
    dy -= ang[1];
    do {
        dy = ANG_WRAP(dy);
    } while (0);
    if ((int)DEG_TO_ANG(eden_bullet_rb_dy_min) < dy && dy < (int)DEG_TO_ANG(eden_bullet_rb_dy_max) && cmd_rb_vy < fabsf(pl.y)) {
        if (pl.y < 0.0f)
            w->ofs.y -= cmd_rb_vy;
        else
            w->ofs.y += cmd_rb_vy;
    }
    w->drift.y *= cmd_rb_s_rate;
    v3_add(&w->pos, &w->drift);
    v3_sub(&w->vel, &w->pos, &w->prev);
    do {
        w->ang[1] = (int)RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = (int)RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
}

/* 0x00caaee8 */
void eden_bullet_rb_exec(EdenBulletRbWork *w)
{
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        eden_bullet_rb_step(w);
        if (--w->time <= 0)
            goto die;
    }
    if (player_sphere_test(&w->pos, cmd_rb_r) & 1)
        eden_player_damage();
    else if (!eden_get_lock_off()) {
        if (!(w->lock.flags & LOCK_HIT))
            return;
        if (w->lock.life > 0)
            return;
        lock_score(5, &w->lock);
    }
die:
    boss_se_play(22, (int)&w->lock, 0);
    cm_wire_break_simple(w, 2, w->parts[57], &w->pos, &w->vel, w->ang, eden_bullet_rb_col);
    FINISH_IF(w);
}

/* 0x00cab018 */
void eden_bullet_rb_disp(EdenBulletRbWork *w)
{
    float amb, dif;
    int old_blend;

    ulGxSetTextureList(w->tex_list);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    light_get_intensity(&amb, &dif);
    light_set_intensity(amb, 0.0f);
    MTX_TRANS(&w->pos);
    eden_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(w->parts[56]);
    light_set_intensity(amb, dif);
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00cab0d0 */
void eden_bullet_rb_release(EdenBulletRbWork *w)
{
    lock_remove_entry(5, &w->lock);
}

/* 0x00cab0e0 */
void *eden_bullet_rb_create(void *parent, int arg, Vec3 *pos, Vec3 *start, int ang)
{
    EdenBulletRbWork *w = task_create_inherit(parent, &eden_bullet_rb_desc, 216);
    Vec3 dir;
    int aim[3];
    BossRibbonDef def;

    v3_copy(&w->pos, pos);
    w->parts = (ModelPart **)arg;
    w->parent = parent;
    v3_copy(&w->center, start);
    {
        float x, z;

        v3_sub(&dir, &w->pos, &w->center);
        x = dir.x;
        z = dir.z;
        w->rad = sqrtf(x * x + z * z);
        aim[1] = (int)RAD_TO_ANG(atan2f(-x, -z));
    }
    w->rot_spd = ang * RAD_PER_ANG;
    w->rot = (aim[1] + ANG_180) * RAD_PER_ANG;
    w->drift.y = cmd_rb_svy + RAND_FLOAT_A() * cmd_rb_dvy;
    w->time = w->len = (int)(cmd_rb_t * FPS);
    lock_init(5, &w->lock, w, &w->pos, 0, 1, 0);
    do {
        lockon_entry(&w->lock);
    } while (0);
    w->unk_6c[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_rb_brz + cmd_rb_brz) - cmd_rb_brz);
    w->unk_6c[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_rb_brz + cmd_rb_brz) - cmd_rb_brz);
    do {
        w->unk_6c[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_rb_brz + cmd_rb_brz) - cmd_rb_brz);
    } while (0);
    def.color = COL_WHITE;
    def.rate = cmd_tf_v;
    def.secs = cmd_tf_t;
    def.tex = 5;
    def.n = eden_bullet_tail_num;
    def.width_a = cmd_tf_w * 0.5f;
    def.width_b = cmd_tf_wl * 0.5f;
    w->tail = cm_btail_create(parent, w, &w->pos, &def);
    return w;
}

/* 0x00cab430 */
int eden_bullet_area_check(Vec3 *pos, Vec3 *vel, Vec3 *out)
{
    int hi;

    if (pos->x * pos->x + pos->z * pos->z <= cmd_map_col_r * cmd_map_col_r)
        return 0;
    hi = !(eden_map_get_pass(0) < 4);
    if (!hi && cmd_map_col_ty <= pos->y && pos->y <= cmd_map_col_ty + cmd_map_col_h) {
        if (out) {
            out->z = vel->z;
            out->x = vel->x;
            out->y = -vel->y;
        }
        return 1;
    }
    if (pos->y <= cmd_map_col_by && cmd_map_col_by - cmd_map_col_h <= pos->y) {
        if (out) {
            out->z = vel->z;
            out->x = vel->x;
            out->y = -vel->y;
        }
        return 1;
    }
    if (0.0f <= vel->y) {
        if (cmd_map_col_by <= pos->y && pos->y - vel->y < cmd_map_col_by) {
            if (out) {
            out->z = vel->z;
            out->x = vel->x;
            out->y = -vel->y;
        }
        return 1;
        }
    } else if (!hi && pos->y <= cmd_map_col_ty && cmd_map_col_ty < pos->y - vel->y) {
        if (out) {
            out->z = vel->z;
            out->x = vel->x;
            out->y = -vel->y;
        }
        return 1;
    }
    return 0;
}

/* 0x00cab5a8 */
void eden_bullet_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
