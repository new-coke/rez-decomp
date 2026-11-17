/*
 * Venus boss bullets. Homing bullets: three kinds (own hits, score, wire colour) that sway, bank,
 * trail ribbons, are kept apart and break into fragments when shot. Fire pods: 16 lockable pods
 * circling the core that fire on a timer or in bursts.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "light.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_bullet.h"
#include "venus.h"
#include "venus_ptcl.h"
#include "venus_shell.h"
#include "../common/boss_common.h"
#include "../common/w_acos.h"

extern int modeltest_compile_obj;
extern char venus_bullet_vardef[];
extern GwVarDef venus_bullet_vardefs[];
extern struct GwVarNode *venus_bullet_vars_group;
extern unsigned short cmd_fpod_hp;
extern float cmd_fpod_at_st;

/* baked motion replay: frame count and each frame's node tree */
typedef struct VenusBake {
    char pad_00[52];
    int frames;                 /* 0x34 */
    WireObj **tree;             /* 0x38: per frame */
} VenusBake;

/* homing-bullet manager: the bullet models, three per kind */
typedef struct VenusGbManWork {
    char pad_00[8];
    int models;                 /* 0x08: the model bank */
    int pos;                    /* 0x0c */
    void *parent;               /* 0x10 */
    int count;                  /* 0x14: live bullets of kind 1 */
    int count0;                 /* 0x18: kind 0 */
    int count2;                 /* 0x1c: kind 2 */
    short *attr[4];             /* 0x20: the attribute words of four model polygon lists */
    BossBake bake;              /* 0x30 */
    ModelPart anim_parts[4];                /* 0x3c: drawn with model_object_draw */
    ModelPart parts[3];                /* 0x1ac: drawn with dc_object_draw */
} VenusGbManWork;

extern float bbullet_tail_width_head;
extern float bbullet_tail_width_end;
extern float cmd_fpod_at2_t;
extern int cmd_fpod_at2_num;
extern int cmd_col_sw;
extern float cmd_fpod_ft;
extern float cmd_fpod_fr;
extern struct TaskDesc bbullet_tail_desc;
extern float bbullet_tail_sec;
extern float bbullet_tail_step;
extern float cmd_gb_br;
extern const int cm_tail_color;

/* bullet trail, 248 bytes: the last 16 positions of its point */
typedef struct BBulletTailWork {
    char pad_00[8];
    int tex;                      /* 0x08 */
    void *owner;                /* 0x0c */
    Vec3 *src;                  /* 0x10 */
    Vec3 p[16];                 /* 0x14 */
    int head;                   /* 0xd4: the newest point of the ring p[16] */
    int flags;                  /* 0xd8: bit 0 started (bits 0 and 1 set together) */
    int frames, frames2;        /* 0xdc */
    int acc;
    float rate;                  /* 0xe8 */
    float alpha;                  /* 0xec */
    int color, color0;                  /* 0xf0 */
} BBulletTailWork;

extern float cmd_fpod_at_t;
extern struct TaskDesc venus_fb_man_desc;
extern float g_break_fade[4];
extern float cmd_fb_r;
extern unsigned int cm_fb_wire_color;
extern float cmd_fb_py;
extern float cmd_fb_pz;
extern float cmd_fpod_offs_col;
extern unsigned int cm_body_wire_color;
extern unsigned int cm_body_wire_color1;
extern unsigned int cm_body_wire_color2;
extern int cmd_test_disp;
extern struct TaskDesc venus_gb_man_desc;

/* bbullet_tail2_create argument (two-sided ribbon trail) */
typedef struct VenusTrailArg {
    void *parent;
    void *owner;
    Vec3 *off;                  /* 0x08: added after the rotation */
    int *ang;                   /* 0x0c */
    unsigned int color;         /* 0x10 */
    Vec3 *end_a, *end_b;                /* 0x14: the two ends */
    int n;                      /* 0x1c: points per side */
} VenusTrailArg;

extern struct TaskDesc bbullet_tail2_desc;
extern char str_venus_bullet_c[];
extern struct TaskDesc g_break_desc;
extern int venus_gb_aim_ofs_sw;
extern float cmd_gb_sw;
extern float cmd_gb_swd;
extern float cmd_gb_mrate;
extern float cmd_gb_bg;
extern float cmd_gb_bvr;
extern float cmd_gb_ht;
extern float cmd_bg_bank;
extern float cmd_bg_bank_r;
extern float cmd_bg_bank_m;
extern Vec3 venus_gb_up;
extern float cmd_gb_b_t;
extern float cmd_gb_frm_v;
extern float cmd_gb_nf_len;
extern float cmd_gb_brake_len;
extern float cmd_gb_brake_vm;
extern float cmd_gb_brake_vr;
extern float cmd_gb_brake_vrs;
extern float venus_gb_hit_push;
extern float venus_gb_range_xz;
extern float venus_gb_range_top;
extern float venus_gb_range_bottom;
extern int cmd_bg_num;
extern struct TaskDesc venus_gb_desc;
extern const float cmd_gb_sv;
extern float cmd_gb_p_fr;
extern float cmd_gb_t;
extern float cmd_gb_r;
extern float cmd_gb_brz;
extern float cmd_gb_brz_s;
extern float cmd_gb_brake_vrt;
extern const int cmd_gb_hp;
extern const int cmd_gb_06_s_hp;
extern const float venus_gb_tail_z;
extern const float venus_gb_tail_x;
extern const int venus_gb_tail_num;
extern const float cmd_gb_06_s_vr;
extern int cmd_gb_06_s;
extern int cmd_gb_07_b;
extern float venus_gb_spin_r;
extern int cm_tail2_color1;
extern int cm_tail2_color2;
extern int venus_gb_tail_col2;
extern int cm_gb_level_num_r[];
extern int cm_gb_level_num_b[];
extern float venus_gb_spd_table[][2];
extern float cmd_gb_he;
extern float cmd_fb_rad_rate;
extern float cmd_fb_rad;
extern float cmd_fb_s_rate;
extern float cmd_fb_vy;
extern float cmd_fb_l_rate;
extern float cmd_fb_ang1;
extern float cmd_fb_ang2;
extern float cmd_fb_ht;
extern struct TaskDesc venus_fb_desc;
extern float cmd_fb_dvy;
extern float cmd_fb_svy;
extern const float cmd_fb_t;
extern float cmd_fb_brz;

/* 224 bytes */
typedef struct VenusGbWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 vel;
    Vec3 drift;
    Vec3 aim_ofs;
    LockObj lock;
    char pad_78[4];
    int timer;                          /* 0x7c */
    int timer_len;
    char *def;
    int model;
    int ang[3];
    VenusGbManWork *man;
    BBulletTailWork *tail;
    float wob_ph;
    float spd;
    float rad;
    int breaking;
    int break_timer;
    int home_timer;
    int near;
    int brake_frm;
    int brake_len;
    int kind;
    int hits;
    float frm;
    float frm_max;
    float bank_k;
    float brake_rate;
    int roll_spd;
} VenusGbWork;

/* 92 bytes */
typedef struct VenusTail2Work {
    void *pack;
    void *textures;
    void *owner;
    Vec3 *off;
    Vec3 *pts_a;
    Vec3 *pts_b;
    int *ang;
    float acc;
    float rate;
    float alpha;
    int head;
    int n;
    int flags;
    int frames;
    int frames_len;
    int color0;
    int color;
    Vec3 end_a;
    Vec3 end_b;
} VenusTail2Work;

/* 64 bytes */
typedef struct VenusGBreakWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    ModelPart *part;
    float frm;
    float frm_end;
    float fade_frm;
    float fade_k;
} VenusGBreakWork;

/* 192 bytes */
typedef struct VenusFbWork {
    void *pack;
    void *textures;
    char *models;
    Vec3 pos;
    Vec3 prev;
    Vec3 vel;
    Vec3 drift;
    Vec3 home_vel;
    int ang[3];
    int dang[3];
    VenusFbManWork *man;
    float orbit_ang;
    float orbit_spd;
    float orbit_rad;
    int timer;
    int timer_len;
    int home_timer;
    char pad_7c[4];
    LockObj lock;                       /* 0x80 */
} VenusFbWork;

#ifdef VERSION_US
#define VENUS_S16(x) (((x) & 0x8000) ? ((x) | ~0xffff) : ((x) & 0xffff))
#define VENUS_CLAMP910(x) if ((x) < -909) (x) = -910; else if ((x) >= 910) (x) = 910
#endif

/* 0x00ca1970 */
void venus_bullet_vars_add(void)
{
    venus_bullet_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_bullet_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_bullet_vars_group, venus_bullet_vardefs);
}

/* 0x00ca19c0 */
void venus_bullet_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_bullet_vardef);
}

/* 0x00ca19d8 */
void bbullet_tail_exec(BBulletTailWork *w)
{
    if (!(w->flags & 1)) {
        if (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END)) {
            w->flags |= 3;
        } else {
            *(float *)&w->acc += w->rate;
            if (*(float *)&w->acc >= 1.0f) {
                *(float *)&w->acc -= 1.0f;
                w->head = (w->head + 1) % 16;
            }
            v3_copy(&w->p[w->head], w->src);
        }
    }
    if (w->flags & 2) {
        w->alpha = (float)w->frames / (float)w->frames2;
        w->color = (w->color0 & 0xffffff) | (((int)(color_get_a(w->color0) * w->alpha) + 1) >> 1 << 24);
        if (--w->frames <= 0)
            FINISH_IF(w);
    }
}

/* 0x00ca1b48 */
void bbullet_tail_disp(BBulletTailWork *w)
{
    draw_set_blend(4, 1);
    strip_draw_alpha(w->p, w->head, 16, w->color, w->tex,
                           bbullet_tail_width_head * w->alpha, bbullet_tail_width_end * w->alpha);
}

/* 0x00ca1ba8 */
BBulletTailWork *bbullet_tail_create(void *parent, void *owner, Vec3 *src, int n, int k)
{
    BBulletTailWork *w = task_create_sibling_inherit(parent, &bbullet_tail_desc, 248);
    Vec3 *src_pt, *pt;
    int i;

    w->owner = owner;
    w->src = src;
    pt = w->p;
    for (i = 0; i < 16; i++, pt++)
        v3_copy(pt, w->src);
    w->tex = n;
    w->color = k;
    w->color0 = k;
    w->rate = bbullet_tail_step;
    w->alpha = 1.0f;
    w->frames = w->frames2 = bbullet_tail_sec * FPS;
    return w;
}

/* 0x00ca1c88 */
void bbullet_tail_stop(BBulletTailWork *w)
{
    w->flags |= 2;
}

/* 0x00ca1c98 */
void bbullet_tail2_exec(VenusTail2Work *w)
{
    Vec3 *head, *head2;

    if (!(w->flags & 1)) {
        if (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END)) {
            w->flags |= 3;
        } else {
            w->acc += w->rate;
            if (w->acc >= 1.0f) {
                w->acc -= 1.0f;
                w->head = (w->head + 1) % w->n;
            }
            head = w->pts_a + w->head;
            head2 = w->pts_b + w->head;
            v3_copy(head, &w->end_a);
            v3_copy(head2, &w->end_b);
            vec_rotate_zxy(head, w->ang);
            vec_rotate_zxy(head2, w->ang);
            v3_add(head, w->off);
            v3_add(head2, w->off);
        }
    }
    if (w->flags & 2) {
        w->alpha = (float)w->frames / (float)w->frames_len;
        w->color = (w->color0 & 0xffffff) | (((int)(color_get_a(w->color0) * w->alpha) + 1) >> 1 << 24);
        if (--w->frames <= 0)
            FINISH_IF(w);
    }
}

/* 0x00ca1eb8 */
void bbullet_tail2_disp(VenusTail2Work *w)
{
    Vec3 *pts, *pts_b;
    int head, next, i;
    float alpha, na, step;
    unsigned int col_a, col_b;

    draw_set_blend(4, 1);
    pts = w->pts_a;
    pts_b = w->pts_b;
    head = w->head;
    alpha = color_get_a(w->color0);
    step = alpha / (w->n - 1);
    for (i = 0; i < w->n - 1; i++) {
        na = alpha - step;
        col_a = (w->color & 0xffffff) | (((int)alpha + 1) >> 1 << 24);
        {
            int back = w->n - 1;

            next = (head + back) % w->n;
        }
        col_b = (w->color & 0xffffff) | (((int)na + 1) >> 1 << 24);
        lockon_draw_line_3d(&pts[head], &pts[next], col_a, col_b);
        lockon_draw_line_3d(&pts_b[head], &pts_b[next], col_a, col_b);
        alpha = na;
        {
            int back = w->n - 1;

            head = (head + back) % w->n;
        }
    }
}

/* 0x00ca2048 */
void bbullet_tail2_release(VenusTail2Work *w)
{
    heap_free(w->pts_a, 0);
    heap_free(w->pts_b, 0);
}

/* 0x00ca2080 */
int *bbullet_tail2_create(VenusTrailArg *desc)
{
    VenusTail2Work *w = task_create_sibling_inherit(desc->parent, &bbullet_tail2_desc, 92);
    Vec3 pt[2];
    Vec3 *src_a, *src_b;
    int i;

    v3_copy(&w->end_a, desc->end_a);
    v3_copy(&w->end_b, desc->end_b);
    w->color0 = w->color = desc->color;
    w->n = desc->n;
    w->owner = desc->owner;
    w->off = desc->off;
    w->ang = desc->ang;
    w->pts_a = heap_block_tag(game_calloc(w->n * 12), str_venus_bullet_c, 458);
    *(int *)&w->pts_b = (int)heap_block_tag(game_calloc(w->n * 12), str_venus_bullet_c, 459); /* steer(type) */
    v3_copy(&pt[0], desc->end_a);
    v3_copy(&pt[1], desc->end_b);
    vec_rotate_zxy(&pt[0], w->ang);
    vec_rotate_zxy(&pt[1], w->ang);
    v3_add(&pt[0], w->off);
    v3_add(&pt[1], w->off);
    src_a = w->pts_a;
    src_b = w->pts_b;
    for (i = 0; i < w->n; i++, src_a++, src_b++) {
        v3_copy(src_a, &pt[0]);
        v3_copy(src_b, &pt[1]);
    }
    w->frames = w->frames_len = bbullet_tail_sec * FPS;
    w->rate = bbullet_tail_step;
    w->alpha = 1.0f;
    return (int *)w;
}

/* 0x00ca22e0 */
void g_break_exec(VenusGBreakWork *w)
{
    w->frm += 1.0f;
    if (w->frm_end < w->frm)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca2320 */
void g_break_disp(VenusGBreakWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_bullet_rotate_yxz(w->ang);
    if (w->frm > w->fade_frm) {
        old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        g_break_fade[0] = -(w->frm - w->fade_frm) * w->fade_k;
        draw_set_override_color(g_break_fade);
        model_object_draw(w->part, w->frm);
        draw_pop_override_color();
        draw_pop_strip_flags();
    } else {
        old_blend = draw_set_override_flags(0);
        model_object_draw(w->part, w->frm);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00ca2428 */
int *g_break_create(int parent, int src, Vec3 *pos, int vel, int *ang)
{
    VenusGBreakWork *w = task_create_sibling_inherit((void *)parent, &g_break_desc, 64);

    w->part = (ModelPart *)src;
    v3_copy(&w->pos, pos);
    if (vel)
        v3_copy(&w->vel, (Vec3 *)vel);
    if (ang) {
        i3_copy(w->ang, ang);
    } else do {
        w->ang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[2] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    w->frm_end = *(float *)(src + 64);
    w->fade_frm = 6.0f;
    w->fade_k = 1.0f / (w->frm_end - 6.0f);
    return (int *)w;
}

/* 0x00ca25f8 */
void venus_gb_steer(VenusGbWork *w)
{
    Vec3 pl, dir, tgt, ofs;
    int ang[4];
    Vec3 sway, amp;
    Vec3 *v;
    float rate;

    rate = 1.0f - (float)w->timer / (float)w->timer_len;
    v3_mul(&tgt, &w->drift, 1.0f - rate);
    if (venus_gb_aim_ofs_sw) {
        player_get_pos(&pl);
        player_get_rot(ang);
        v3_copy(&ofs, &w->aim_ofs);
        vec_rotate_zxy(&ofs, ang);
        v3_add(&pl, &ofs);
        v3_sub(&dir, &pl, &w->pos);
        vec_set_length(&dir, w->spd);
        v3_add(&tgt, &dir);
    } else {
        player_get_pos(&pl);
        v3_sub(&dir, &pl, &w->pos);
        vec_set_length(&dir, w->spd);
        v3_add(&tgt, &dir);
    }
    amp.x = amp.y = amp.z = cmd_gb_sw * (1.0f - rate);
    wobble_step(&sway, &amp, &w->wob_ph, cmd_gb_swd);
    v3_sum(&tgt, &sway, &tgt);
    if (w->breaking) {
        float blend;

        if (w->kind == 2)
            w->vel.y -= cmd_gb_bg * 0.5f;
        else
            w->vel.y -= cmd_gb_bg;
        blend = cmd_gb_bvr;
        w->vel.x *= blend;
        w->vel.z *= blend;
        w->ang[2] += w->roll_spd;
    } else if (w->home_timer) {
        float blend;

        blend = (float)w->home_timer / (cmd_gb_ht * FPS);
        blend += cmd_gb_mrate * (1.0f - blend);
        (w->home_timer)--;
        v = &w->vel;
        v->x = v->x * blend + tgt.x * (1.0f - blend);
        v->y = v->y * blend + tgt.y * (1.0f - blend);
        v->z = v->z * blend + tgt.z * (1.0f - blend);
    } else {
        float blend;

        blend = cmd_gb_mrate;
        v = &w->vel;
        v->x = v->x * blend + tgt.x * (1.0f - blend);
        v->y = v->y * blend + tgt.y * (1.0f - blend);
        v->z = v->z * blend + tgt.z * (1.0f - blend);
    }
}

/* 0x00ca2958 */
void venus_gb_move(VenusGbWork *w)
{
    Vec3 dir_old, dir_new, axis;
    Vec3 *v = &w->vel;
    float turn, bank;
    int lim;

    vec3_normalize(&dir_old, v);
    venus_gb_steer(w);
    v3_add(&w->pos, v);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
    if (w->breaking)
        return;
    vec3_normalize(&dir_new, v);
    turn = acos(ulVec3Dot(&dir_new, &dir_old));
    ulVec3Cross(&axis, &dir_old, &dir_new);
    lim = DEG_TO_ANG(cmd_bg_bank_m);
    bank = (int)(turn * cmd_bg_bank * w->bank_k * ANG_PER_RAD);
    if (fabsf(bank) > lim)
        bank = lim;
    if (ulVec3Dot(&axis, &venus_gb_up) >= 0.0f)
        w->ang[2] = w->ang[2] * cmd_bg_bank_r + bank * (1.0f - cmd_bg_bank_r);
    else
        w->ang[2] = w->ang[2] * cmd_bg_bank_r - bank * (1.0f - cmd_bg_bank_r);
}

/* 0x00ca2bc0 */
void venus_gb_break(VenusGbWork *w)
{
    g_break_create((int)w->man, ((int *)w->def)[39], &w->pos, 0, w->ang);
    switch (w->kind) {
    default:
        boss_se_play(14, (int)&w->lock, 0);
        lock_score_at(6, &w->pos, w->hits);
        cm_wire_break_simple(w->man, 2, ((ModelPart **)w->def)[37], &w->pos, &w->vel, w->ang, cm_body_wire_color2);
        break;
    case 0:
        boss_se_play(12, (int)&w->lock, 0);
        lock_score_at(5, &w->pos, w->hits);
        cm_wire_break_simple(w->man, 2, ((ModelPart **)w->def)[36], &w->pos, &w->vel, w->ang, cm_body_wire_color);
        break;
    case 2:
        boss_se_play(16, (int)&w->lock, 0);
        lock_score_at(7, &w->pos, w->hits);
        cm_wire_break_simple(w->man, 2, ((ModelPart **)w->def)[38], &w->pos, &w->vel, w->ang, cm_body_wire_color1);
        break;
    }
    venus_gb_man_break_near(w->man, &w->pos);
}

/* 0x00ca2d00 */
void venus_gb_exec(VenusGbWork *w)
{
    Vec3 v;
    int n;
    float len, spd;

    for (n = enemy_status_get_ticks(); n; n--) {
        venus_gb_move(w);
        if (w->breaking) {
            if (--w->break_timer <= 0) {
                venus_gb_break(w);
                FINISH_IF(w);
                return;
            }
        }
        if (--w->timer <= 0) {
            w->timer = cmd_gb_b_t * FPS;
            w->breaking |= 1;
            break;
        }
        if (sqrtf(w->pos.x * w->pos.x + w->pos.z * w->pos.z) > venus_gb_range_xz
            || w->pos.y > venus_gb_range_top || w->pos.y < venus_gb_range_bottom) {
            venus_gb_break(w);
            FINISH_IF(w);
            return;
        }
        if (w->brake_frm) {
            int left = w->brake_frm - 1;

            w->brake_rate = cmd_gb_brake_vrs + (cmd_gb_brake_vr - cmd_gb_brake_vrs) * (1.0f - (float)left / (float)w->brake_len);
            w->brake_frm = left;
        }
    }
    if (w->lock.flags & LOCK_HIT) {
        switch (w->kind) {
        case 0:
            boss_se_play(11, (int)&w->lock, 0);
            break;
        case 2:
            boss_se_play(15, (int)&w->lock, 0);
            break;
        default:
            boss_se_play(13, (int)&w->lock, 0);
            break;
        }
        if (w->kind == 2 && !v3_is_zero(&w->lock.imp.v)) {
            vec3_normalize(&v, &w->lock.imp.v);
            v3_scale(&v, venus_gb_hit_push);
            v3_add(&w->vel, &v);
        }
        if (w->lock.life <= 0 && !w->breaking) {
            if (!w->tail) {
                w->hits = lockon_hits_max_shot_cnt(&w->lock);
                w->tail = bbullet_tail_create(w->man, w, &w->pos, 8, cm_tail_color);
            }
            w->breaking |= 1;
        }
    }
    if ((player_sphere_test(&w->pos, w->rad) & 1) && !w->breaking) {
        venus_player_damage();
        if (!w->tail)
            w->tail = bbullet_tail_create(w->man, w, &w->pos, 8, cm_tail_color);
        w->breaking |= 1;
    }
    player_get_pos(&v);
    v3_sub(&v, &v, &w->pos);
    len = vec3_length(&v);
    if (len < cmd_gb_brake_len) {
        spd = vec3_length(&w->vel);
        if (spd > cmd_gb_brake_vm)
            vec_set_length(&w->vel, spd * w->brake_rate + cmd_gb_brake_vm * (1.0f - w->brake_rate));
    }
    if (len < cmd_gb_nf_len) {
        w->frm += cmd_gb_frm_v;
        if (w->frm >= w->frm_max)
            w->frm = w->frm_max;
    } else {
        if (w->frm == 0.0f)
            goto off;
        w->frm -= cmd_gb_frm_v;
        if (w->frm <= 0.0f)
            w->frm = 0.0f;
    }
    w->model = *(int *)(w->def + 92);
    *(void **)&w->near = (void *)1; /* steer(type) */
    return;
off:
    {
        char *target = w->def;

        w->near = 0;
        w->model = *(int *)(target + 96);
    }
}

/* 0x00ca31d0 */
void venus_gb_disp(VenusGbWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->textures);
    if (!view_sphere_visible(&w->pos, 10.0f))
        return;
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (w->kind == 2)
        vu0_matrix_scale(ulMatrixCur, 2.0f, 2.0f, 2.0f);
    old_blend = draw_set_override_flags(0);
    if (dc_object_get_compiled((struct DcObjectCompiled *)w->model) && modeltest_compile_obj)
        venus_gb_man_draw_model(w->man, w->kind, w->near, w->frm);
    else {
        venus_gb_man_set_attr((int *)w->man, w->kind + 1);
        if (w->near) {
            if (cmd_test_disp)
                venus_gb_man_draw_bake((VenusBake *)w->man, w->frm);
            else
                model_object_draw((void *)w->model, w->frm);
        } else
            dc_object_draw((struct MapPart *)w->model);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00ca3330 */
void venus_gb_release(VenusGbWork *w)
{
    if (w->tail)
        bbullet_tail_stop(w->tail);
    switch (w->kind) {
    case 0:
        lock_remove_entry(5, &w->lock);
        venus_gb_man_add_count(w->man, -1, 0);
        break;
    case 1:
        lock_remove_entry(6, &w->lock);
        venus_gb_man_add_count(w->man, -1, 1);
        break;
    case 2:
        lock_remove_entry(7, &w->lock);
        venus_gb_man_add_count(w->man, -1, 2);
        break;
    }
}

/* 0x00ca3410 */
int *venus_gb_create(int *mgr, Vec3 *pos, Vec3 *dir, char *def, float rate1, float rate2)
{
    int ang[3];
    VenusTrailArg ta;
    Vec3 tail_l, tail_r;
    float sc[4];
    VenusGbWork *w;
    float spd, roll_rnd, roll;
    int kind, color, n;
    float scale;

    if (venus_gb_man_get_count((VenusGbManWork *)mgr, 4) > cmd_bg_num)
        return 0;
    w = task_create_inherit(mgr, &venus_gb_desc, 224);
    w->timer = w->timer_len = cmd_gb_t * FPS;
    v3_copy(&w->pos, pos);
    w->man = (VenusGbManWork *)mgr;
    w->def = def;
    w->model = (int)*(void **)(def + 96);
    w->hits = 1;
    v3_copy(&w->vel, dir);
    spd = venus_gb_spd_table[boss_get_level()][0];
    w->wob_ph = RAND_FLOAT_A() * TWO_PI;
    vec_set_length(&w->vel, cmd_gb_sv);
    w->drift.z = -RAND_FLOAT_A() * cmd_gb_p_fr;
    w->drift.x = w->drift.y = 0.0f;
    do {
    ang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    ang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    ang[2] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
    vec_rotate_zxy(&w->drift, ang);
    ang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 180.0f + -90.0f);
    ang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 180.0f + -90.0f);
    } while (0);
    w->aim_ofs.z = -RAND_FLOAT_A() * venus_gb_spin_r;
    ROT_Z(ang[0], sc, w->aim_ofs.y, w->aim_ofs.z);
    ROT_Z(ang[1], (sc + 2), w->aim_ofs.y, w->aim_ofs.z);
    w->break_timer = cmd_gb_b_t * FPS;
    w->brake_frm = w->brake_len = cmd_gb_brake_vrt * FPS;
    w->brake_rate = cmd_gb_brake_vrs;
    w->frm_max = ((ModelPart **)w->def)[23]->frames;
    do { /* steer(barrier) */
        roll_rnd = RAND_FLOAT_A() * ((cmd_gb_brz_s - cmd_gb_brz) - (cmd_gb_brz - cmd_gb_brz_s)) + (cmd_gb_brz - cmd_gb_brz_s);
        if (roll_rnd > 0.0f)
            roll = roll_rnd + cmd_gb_brz_s;
        else
            roll = roll_rnd - cmd_gb_brz_s;
        w->roll_spd = DEG_TO_ANG(roll);
    } while (0);
    if ((int)(RAND_FLOAT_A() * cmd_gb_06_s * rate1))
        kind = 1;
    else {
        int cnt = venus_gb_man_get_count(w->man, 0);

        kind = 0;
        if (cnt >= cm_gb_level_num_r[boss_get_level()])
            kind = 1;
    }
    if (kind == 1 && !(int)(RAND_FLOAT_A() * cmd_gb_07_b * rate2)) {
        int cnt = venus_gb_man_get_count(w->man, 2);

        if (cm_gb_level_num_b[boss_get_level()] >= cnt)
            kind = 2;
    }
    switch (kind) {
    case 0: {
        float rad;

        rad = cmd_gb_r;
        w->spd = spd * cmd_gb_06_s_vr;
        color = cm_tail2_color2;
        w->rad = rad;
        w->kind = 0;
        w->bank_k = 2.0f;
        n = venus_gb_tail_num * 2;
        lock_init(5, &w->lock, w, &w->pos, 0, cmd_gb_06_s_hp, 0);
        lockon_entry(&w->lock);
        venus_gb_man_add_count(w->man, 1, 0);
        break;
    }
    case 2:
        color = venus_gb_tail_col2;
        w->rad = cmd_gb_r * 2.0f;
        w->roll_spd = w->roll_spd * 0.5f;
        w->spd = spd;
        w->kind = kind;
        n = venus_gb_tail_num * 2;
        scale = 0.8f;
        w->bank_k = scale;
        lock_init(7, &w->lock, w, &w->pos, 0, cmd_gb_hp * 8, 0);
        lockon_entry(&w->lock);
        venus_gb_man_add_count(w->man, 1, 2);
        break;
    default: {
        float rad;

        rad = cmd_gb_r;
        color = cm_tail2_color1;
        w->kind = 1;
        w->rad = rad;
        w->spd = spd;
        scale = 1.0f;
        w->bank_k = scale;
        n = venus_gb_tail_num;
        lock_init(6, &w->lock, w, &w->pos, 0, cmd_gb_hp, 0);
        lockon_entry(&w->lock);
        venus_gb_man_add_count(w->man, 1, 1);
        break;
    }
    }
    tail_r.z = venus_gb_tail_z;
    tail_l.z = venus_gb_tail_z;
    tail_l.y = tail_r.y = 0.0f;
    tail_l.x = venus_gb_tail_x;
    tail_r.x = -venus_gb_tail_x;
    ta.owner = w;
    ta.ang = w->ang;
    ta.off = &w->pos;
    ta.parent = w->man;
    ta.n = n;
    ta.color = color;
    ta.end_a = &tail_l;
    ta.end_b = &tail_r;
    bbullet_tail2_create(&ta);
    return (int *)w;
}

/* 0x00ca3ba0 */
void venus_gb_man_separate(void *work)
{
    int *mgr = work;
    char *ball, *other, *prev;
    float spd_max, spd_max_sq, len_sq;
    float *tab = (float *)venus_gb_spd_table;
    int idx;
    Vec3 *va, *vb;

    idx = boss_get_level() * 2 + 1;
    spd_max = tab[idx];
    mgr[5] = 0;
    spd_max_sq = spd_max * spd_max;
    for (ball = TASK_OF(mgr)->child ? (char *)((Task *)TASK_OF(mgr)->child + 1) : 0; ball; ball = TASK_OF(ball)->next ? (char *)((Task *)TASK_OF(ball)->next + 1) : 0, mgr[5]++) {
        for (prev = ball; (other = TASK_OF(prev)->next ? (char *)((Task *)TASK_OF(prev)->next + 1) : 0) != 0; prev = other) {
            if (*(float *)(ball + 168) == *(float *)(other + 168)) {
                va = (Vec3 *)(ball + 20);
                vb = (Vec3 *)(other + 20);
                if (!spheres_separate_k((Vec3 *)(ball + 8), (Vec3 *)(other + 8), va, vb,
                                           *(float *)(ball + 168), *(float *)(other + 168), cmd_gb_he))
                    continue;
            } else {
                va = (Vec3 *)(ball + 20);
                vb = (Vec3 *)(other + 20);
                if (!spheres_separate_mass((Vec3 *)(ball + 8), (Vec3 *)(other + 8), va, vb,
                                           *(float *)(ball + 168), *(float *)(other + 168), cmd_gb_he))
                    continue;
            }
            len_sq = vec3_length_squared(va);
            if (len_sq > spd_max_sq) {
                len_sq = 1.0f / sqrtf(len_sq);
                len_sq *= spd_max;
                v3_scale((Vec3 *)(ball + 20), len_sq);
            }
            len_sq = vec3_length_squared((float *)(other + 20));
            if (len_sq > spd_max_sq) {
                len_sq = 1.0f / sqrtf(len_sq);
                len_sq *= spd_max;
                v3_scale((Vec3 *)(other + 20), len_sq);
                if ((((int)other) & 1) >> 1) { /* steer(zero) */
                    mgr[60] = (int)(other + 20);
                    *(float *)&mgr[61] = cmd_gb_ht;
                }
            }
            ((VenusGbWork *)ball)->home_timer = cmd_gb_ht * FPS;
            { int z = 0; if (z) cmd_gb_ht = 0.0f; }
            ((VenusGbWork *)other)->home_timer = cmd_gb_ht * FPS;
        }
    }
}

/* 0x00ca3e20 */
void venus_gb_man_exec(void *w)
{
    if (cmd_col_sw)
        venus_gb_man_separate(w);
}

/* 0x00ca3e40 */
void venus_gb_man_release(VenusGbManWork *w)
{
    int i;

    motion_bake_free(&w->bake);
    for (i = 0; i < 3; i++) {
        dc_object_release(&w->anim_parts[i]);
        dc_object_release(&w->parts[i]);
    }
}

/* 0x00ca3e98 */
int venus_gb_man_create(char *parent, void *parts, void *pos)
{
    VenusGbManWork *w = task_create_inherit(parent, &venus_gb_man_desc, 796);
    int *root = ((int **)parts)[23];
    WireObj *obj_a = (WireObj *)root[1];
    WireObj *obj_b = (WireObj *)((int **)parts)[24][1];
    int *part;
    int i;
    int motion;
    BossBake *bake;

    bake = &w->bake;
    {
        WireObj *child_b, *child_a, *cc, *sib;
        NjCnkModel *model_b, *mcc, *msib, *model_a;
        int attr0, attr1, attr2, attr3;
        child_a = obj_a->child;
        child_b = obj_b->child;
        cc = child_a->child;
        mcc = cc->model;
        sib = cc->sibling;
        model_b = child_b->model;
        attr0 = (int)((char *)model_b->plist + 14);
        model_a = child_a->model;
        attr1 = (int)((char *)model_a->plist + 14);
        attr2 = (int)((char *)mcc->plist + 14);
        msib = sib->model;
        attr3 = (int)((char *)msib->plist + 14);
        w->parent = parent;
        w->pos = (int)pos;
        w = (VenusGbManWork *)((char *)w + (((int)w & 1) >> 1)); /* steer(zero) */
        motion = (int)((int **)root)[2];
        w->models = (int)parts;
        w->attr[0] = (short *)attr0;
        w->attr[1] = (short *)attr1;
        w->attr[2] = (short *)attr2;
        w->attr[3] = (short *)attr3;
    }
    motion_bake(bake, obj_a, (int *)motion);
    for (i = 0; i < 3; i++) {
        venus_gb_man_set_attr((int *)w, i + 1);
        part = ((int **)w->models)[23];
        dc_object_init(&w->anim_parts[i], part[1], part[2], 0);
        dc_object_rebuild(&w->anim_parts[i]);
        dc_object_init(&w->parts[i], ((int **)w->models)[24][1], 0, 0);
        dc_object_rebuild(&w->parts[i]);
    }
    return (int)w;
}

/* 0x00ca4008 */
void venus_gb_man_set_attr(int *w, short attr)
{
    int i;

    for (i = 0; i < 4; i++)
        *(short *)w[8 + i] = attr;
}

/* 0x00ca4040 */
void venus_gb_man_break_near(void *w, Vec3 *pos)
{
    float r_sq = cmd_gb_br * cmd_gb_br;
    Task *node = (Task *)TASK_OF(w)->child;
    int *ball = node ? (int *)(node + 1) : 0;

    while (ball) {
        if (vec3_distance_squared(pos, (struct Vec3 *)(ball + 2)) < r_sq) {
            if (!ball[39])
                ball[39] = (int)bbullet_tail_create((void *)ball[38], ball, (Vec3 *)(ball + 2), 8, cm_tail_color);
            ball[43] |= 1;
        }
        node = (Task *)TASK_OF(ball)->next;
        ball = node ? (int *)(node + 1) : 0;
    }
}

/* 0x00ca4110 */
int venus_gb_man_get_count(VenusGbManWork *w, int kind)
{
    switch (kind) {
    case 0:
        return w->count0;
    case 2:
        return w->count2;
    }
    return w->count;
}

/* 0x00ca4138 */
void venus_gb_man_add_count(VenusGbManWork *w, int delta, int kind)
{
    switch (kind) {
    case 0:
        w->count0 += delta;
        if (w->count0 < 0)
            w->count0 = 0;
        return;
    case 2:
        w->count2 += delta;
        if (w->count2 < 0)
            w->count2 = 0;
        return;
    }
    w->count += delta;
}

/* 0x00ca4188 */
void venus_gb_man_break_all(void *w)
{
    Task *node = (Task *)TASK_OF(w)->child;
    int *ball = node ? (int *)(node + 1) : 0;
    Task *next;

    while (ball) {
        if (!ball[39])
            ball[39] = (int)bbullet_tail_create((void *)ball[38], ball, (Vec3 *)(ball + 2), 8, cm_tail_color);
        ball[43] |= 1;
        next = (Task *)TASK_OF(ball)->next;
        ball = next ? (int *)(next + 1) : 0;
    }
}

/* 0x00ca4218 */
void venus_gb_man_draw_bake(VenusBake *w, float t)
{
    nj_object_draw_vu1(w->tree[(int)t % w->frames]);
}

/* 0x00ca4248 */
void venus_gb_man_draw_model(VenusGbManWork *w, int i, int mode, float t)
{
    if (!mode)
        dc_object_draw(&w->parts[i % 3]);
    else
        model_object_draw(&w->anim_parts[i % 3], t);
}

/* 0x00ca42a0 */
void venus_fb_move(VenusFbWork *w)
{
    int ang[2];
    Vec3 pl;
    float sc[2];
    float t, rad;
    int dang;

    if (w->home_timer)
        v3_copy(&w->home_vel, &w->vel);
    v3_copy(&w->prev, &w->pos);
    w->orbit_spd = w->orbit_spd * cmd_fb_rad_rate + TWO_PI / (cmd_fb_rad * FPS) * (1.0f - cmd_fb_rad_rate);
    w->orbit_ang += w->orbit_spd;
    while (w->orbit_ang > TWO_PI)
        w->orbit_ang -= TWO_PI;
    while (w->orbit_ang < -TWO_PI)
        w->orbit_ang += TWO_PI;
    rad = w->orbit_rad;
    w->pos.x = 0.0f;
    w->pos.z = rad;
    ROT_Y((int)(w->orbit_ang * ANG_PER_RAD), sc, w->pos.x, w->pos.z);
    player_get_pos(&pl);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-pl.x, -pl.z));
        ang[0] = RAD_TO_ANG(atan2f(pl.y, sqrtf(pl.x * pl.x + pl.z * pl.z)));
    } while (0);
    {
        float dist_xz = sqrtf(pl.x * pl.x + pl.z * pl.z);

        w->orbit_rad = w->orbit_rad * cmd_fb_l_rate + dist_xz * (1.0f - cmd_fb_l_rate);
    }
    v3_sub(&pl, &pl, &w->pos);
    dang = (int)(w->orbit_ang * ANG_PER_RAD);
    dang -= ang[1];
    if (dang < 0)
        dang |= 0xffff0000;
    else
        dang &= 0xffff;
    if ((int)DEG_TO_ANG(cmd_fb_ang1) < dang && dang < (int)DEG_TO_ANG(cmd_fb_ang2)) {
        if (fabsf(pl.y) > cmd_fb_vy) {
            /* steps toward the player's height */
            if (pl.y < 0.0f)
                w->pos.y -= cmd_fb_vy;
            else
                w->pos.y += cmd_fb_vy;
        }
    }
    w->drift.y *= cmd_fb_s_rate;
    v3_sum(&w->pos, &w->pos, &w->drift);
    v3_sub(&w->vel, &w->pos, &w->prev);
    if (w->home_timer) {
        t = (float)w->home_timer / (cmd_fb_ht * FPS);
        (w->home_timer)--;
        v3_lerp(&w->vel, &w->home_vel, &w->vel, t);
    }
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
        w->ang[0] = RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
    } while (0);
}

/* 0x00ca4848 */
void venus_fb_exec(VenusFbWork *w)
{
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        venus_fb_move(w);
        if (--w->timer <= 0)
            goto die;
    }
    if (player_sphere_test(&w->pos, cmd_fb_r) & 1)
        venus_player_damage();
    else {
        if (!(w->lock.flags & LOCK_HIT))
            return;
        if (w->lock.life > 0)
            return;
    }
die:
    boss_se_play(22, (int)&w->lock, 0);
    lock_score(8, &w->lock);
    cm_wire_break_simple(w, 2, *(ModelPart **)(w->models + 128), &w->pos, &w->vel, w->ang, cm_fb_wire_color);
    FINISH_IF(w);
}

/* 0x00ca4958 */
void venus_fb_disp(VenusFbWork *w)
{
    int old_blend;
    float near, far;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    light_get_intensity(&near, &far);
    light_set_intensity(near, 0.0f);
    MTX_TRANS(&w->pos);
    venus_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(*(ModelPart **)(w->models + 124));
    light_set_intensity(near, far);
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00ca4a10 */
void venus_fb_release(VenusFbWork *w)
{
    lock_remove_entry(8, &w->lock);
}

/* 0x00ca4a20 */
int *venus_fb_create(VenusFbManWork *w, int models, VenusFbManPod *pod, int ang, int ang_spd)
{
    VenusFbWork *shot = task_create_inherit(w, &venus_fb_desc, 192);
    Vec3 up, dir;

    shot->man = w;
    shot->models = (char *)models;
    v3_copy(&shot->pos, (Vec3 *)pod);
    shot->orbit_rad = sqrtf(shot->pos.x * shot->pos.x + shot->pos.z * shot->pos.z);
    shot->orbit_spd = ang_spd * RAD_PER_ANG;
    up.z = 1.0f;
    up.x = up.y = 0.0f;
    v3_copy(&dir, &shot->pos);
    dir.y = 0.0f;
    vec3_normalize(&dir, &dir);
    shot->orbit_ang = ang * RAD_PER_ANG;
    shot->drift.y = cmd_fb_svy + RAND_FLOAT_A() * cmd_fb_dvy;
    shot->timer = shot->timer_len = cmd_fb_t * FPS;
    bbullet_tail_create(shot, shot, &shot->pos, 8, cm_tail_color);
    lock_init(8, &shot->lock, shot, &shot->pos, 0, 1, 0);
    lockon_entry(&shot->lock);
    do {
    shot->dang[0] = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fb_brz + cmd_fb_brz) - cmd_fb_brz);
    shot->dang[1] = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fb_brz + cmd_fb_brz) - cmd_fb_brz);
    shot->dang[2] = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fb_brz + cmd_fb_brz) - cmd_fb_brz);
    } while (0);
    return (int *)shot;
}

/* 0x00ca4cd0 */
void venus_fb_man_place_pods(VenusFbManWork *w)
{
    Vec3 v;
    float sc[2];
    VenusFbManPod *pod = w->r;
    int step = 4095;
    int i;

    v.y = ((Vec3 *)w->pos)->y + cmd_fb_py;
    v.x = 0.0f;
    v.z = cmd_fb_pz;
    for (i = 0; i < 16; i++, pod++) {
        v3_copy((Vec3 *)pod, &v);
        pod->ang[1] = ((int *)w->ang)[1] + i * step;
        ROT_Y(pod->ang[1], sc, ((Vec3 *)pod)->x, ((Vec3 *)pod)->z);
    }
}

/* 0x00ca4de0 */
void venus_fb_man_fire(VenusFbManWork *w)
{
    VenusFbManPod *pod = w->r;
    int i;

    if (--w->arg > 0)
        return;
    w->arg = cmd_fpod_at_t * FPS;
    boss_se_play(21, 0, (int)pod);
    for (i = 0; i < 16; i++) {
        if (pod[i].alive) {
            venus_sp_draw_create((VenusFbManWork *)w->parent, &pod[i], 6, COL_WHITE, 1, cmd_fpod_ft, cmd_fpod_fr);
            venus_fb_create(w, w->models, &pod[i], pod[i].ang[1], w->dang);
        }
    }
}

/* 0x00ca4ec0 */
void venus_fb_man_fire_burst(VenusFbManWork *w)
{
    VenusFbManPod *pod = w->r;
    int i;

    if (w->bursts && --w->arg <= 0) {
        w->arg = cmd_fpod_at2_t * FPS;
        boss_se_play(21, 0, (int)pod);
        for (i = 0; i < 16; i++) {
            if (pod[i].alive) {
                venus_sp_draw_create((VenusFbManWork *)w->parent, &pod[i], 6, COL_WHITE, 1, cmd_fpod_ft, cmd_fpod_fr);
                venus_fb_create(w, w->models, &pod[i], pod[i].ang[1], w->dang);
            }
        }
        w->bursts--;
    }
}

/* 0x00ca4fb8 */
void venus_fb_man_exec(VenusFbManWork *w)
{
    VenusFbManPod *pod;
    int dang, n, i;

    venus_fb_man_place_pods(w);
    dang = ((int *)w->ang)[1] - w->ang_y;
    w->dang = dang;
    w->ang_y = ((int *)w->ang)[1];
#ifdef VERSION_US
    w->dang = VENUS_S16(dang);
    VENUS_CLAMP910(w->dang);
    w->dang_prev = VENUS_S16(w->dang_prev);
    VENUS_CLAMP910(w->dang_prev);
#else
    if ((unsigned int)(dang + 0x10000) > 0x20000)
        w->dang = w->dang_prev;
    else
        w->dang_prev = dang;
#endif
    for (n = enemy_status_get_ticks(); n; n--) {
        if (w->on) {
            if (!w->burst_mode)
                venus_fb_man_fire(w);
            else
                venus_fb_man_fire_burst(w);
        }
    }
    if (w->stopped)
        return;
    w->n = 0;
    pod = w->r;
    for (i = 16; i != 0; i--, pod++) {
        if ((pod->lock.flags & LOCK_HIT) && pod->lock.life <= 0) {
#ifdef VERSION_US
            pod->lock.flags &= ~LOCK_LOCKABLE;
#endif
            boss_se_play(20, (int)&pod->lock, 0);
            lock_score(9, &pod->lock);
            pod->alive = 0;
        }
        if (pod->alive)
            w->n++;
    }
}

/* steer(cast) */
/* 0x00ca5170 */
void venus_fb_man_disp(VenusFbManWork *w)
{
    float col[4];
    VenusFbManPod *pod = w->r;
    int old_blend;
    int i;

    memset(col, 0, 16);
    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    col[1] = col[2] = col[3] = (1.0f - beat_curve(1)) * cmd_fpod_offs_col;
    draw_set_override_color(col);
    for (i = 16; i != 0; i--, pod++) {
        if (pod->alive) {
            ulMatrixPush();
            MTX_TRANS((Vec3 *)pod);
            venus_bullet_rotate_yxz(&pod->ang[0]);
            dc_object_draw(*(ModelPart **)((char *)w->models + 88));
            ulMatrixPop();
        }
    }
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old_blend);
}

/* 0x00ca5298 */
void venus_fb_man_release(VenusFbManWork *w)
{
    VenusFbManPod *pod = w->r;
    int i;

    for (i = 0; i < 16; i++, pod++)
        lock_remove_entry(9, &pod->lock);
}

/* 0x00ca52e0 */
VenusFbManWork *venus_fb_man_create(void *parent, int models, int pos, int *ang)
{
    VenusFbManWork *w = task_create_inherit(parent, &venus_fb_man_desc, 1596);
    VenusFbManPod *pod = w->r;
    int ang_y = ang[1];
    int i;

    w->parent = (int)parent;
    w->models = models;
    w->pos = pos;
    w->ang_y = ang_y;
    w->ang = (int)ang;
    for (i = 0; i < 16; i++, pod++) {
        lock_init(9, &pod->lock, w, (Vec3 *)pod, 0, *(int *)&cmd_fpod_hp, 0);
        lockon_entry(&pod->lock);
        pod->lock.life = 0;
#ifdef VERSION_US
        pod->lock.flags &= ~LOCK_LOCKABLE;    /* off until venus_fb_man_set_on */
#endif
        pod->alive = 1;
    }
    w->arg = cmd_fpod_at_t * FPS;
    return w;
}

/* 0x00ca53f0 */
int venus_fb_man_get_count(VenusFbManWork *w)
{
    if (w->on)
        return w->n;
    return -1;
}

/* 0x00ca5410 */
int venus_fb_man_count_shots(void *w)
{
    Task *node;
    int *shot;
    int n = 0;

    node = (Task *)TASK_OF(w)->child;
    shot = node ? (int *)(node + 1) : 0;
    while (shot) {
        node = (Task *)TASK_OF(shot)->next;
        n++;
        shot = node ? (int *)(node + 1) : 0;
    }
    return n;
}

/* 0x00ca5448 */
void venus_fb_man_set_on(VenusFbManWork *w, int on, int delay)
{
    int i;
    VenusFbManPod *pod = w->r;

    if (on) {
        w->on = 1;
        w->arg = delay;
        for (i = 0; i < 16; i++, pod++) {
            pod->alive = 1;
            pod->lock.life = cmd_fpod_hp;
#ifdef VERSION_US
            pod->lock.flags |= LOCK_LOCKABLE;
#endif
            pod->unk_5c = 0;
        }
        w->n = 16;
        return;
    }
    w->on = 0;
}

/* 0x00ca54a8 */
void venus_fb_man_set_on_now(VenusFbManWork *w, int on)
{
    venus_fb_man_set_on(w, on, 0);
}

/* 0x00ca54b0 */
void venus_fb_man_set_on_delay(VenusFbManWork *w, int on)
{
    venus_fb_man_set_on(w, on, cmd_fpod_at_st * FPS);
}

/* 0x00ca54d8 */
void venus_fb_man_revive(VenusFbManWork *w)
{
    VenusFbManPod *pod = w->r;
    int i;

    w->on = 1;
    w->arg = 0;
    for (i = 0; i < 16; i++, pod++) {
        if (!pod->alive) {
            pod->alive = 1;
            pod->unk_5c = 0;
            pod->lock.life = cmd_fpod_hp;
#ifdef VERSION_US
            pod->lock.flags |= LOCK_LOCKABLE;
#endif
            venus_sp_draw_create(w, pod, 6, COL_WHITE, 1, cmd_fpod_ft, cmd_fpod_fr);
        }
    }
    w->n = 16;
}

/* 0x00ca55b0 */
void venus_fb_man_stop(VenusFbManWork *w)
{
    Task *node;
    int *shot;

    node = (Task *)TASK_OF(w)->child;
    shot = node ? (int *)(node + 1) : 0;
    while (shot) {
        node = (Task *)TASK_OF(shot)->next;
        shot[28] = 0;
        shot = node ? (int *)(node + 1) : 0;
    }
    w->on = 0;
    w->stopped = 1;
}

/* 0x00ca55e8 */
void venus_fb_man_start_burst(VenusFbManWork *w)
{
    w->arg = 0;
    venus_fb_man_fire_burst(w);
    w->burst_mode = 1;
    w->bursts = cmd_fpod_at2_num - 1;
    w->arg = cmd_fpod_at2_t * FPS;
}

/* 0x00ca5648 */
void venus_bullet_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
