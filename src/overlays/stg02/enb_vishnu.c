/*
 * Vishnu's laser, Area 2: extends from a hand toward the player and hurts on reach. Its tip is a
 * lock-on target: shooting it drives the beam back and destroys the hand.
 */

#include "stg02.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "lens.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "enb_vishnu.h"
#include "en_vishnu_hand.h"
#include "../common/enemy_vars.h"

typedef enum FxOp { FX_OP_NONE } FxOp;
extern int enb_vishnu_init_arg;
extern ParamRec enb_vishnu_params;
extern VarDef enb_vishnu_vardefs[15];
extern struct TaskDesc enb_vishnu_desc;
extern float enb_vishnu_collision_r;
extern float enb_vishnu_rate_scale;

/* 244 bytes */
typedef struct EnbVishnuWork {
    int phase;                  /* index into enb_vishnu_states */
    int next;                   /* 0x04 */
    unsigned int step;          /* 0x08 */
    int next_step;              /* 0x0c */
    ModelPart body;                /* 0x10: the tip */
    Vec3 base;                  /* 0x6c: the root (the owner's muzzle) */
    int ang[3];                 /* 0x78 */
    Vec3 pos;                   /* 0x84: the tip */
    LockObj lock;                  /* 0x90 */
    float k;                    /* 0xd0: extension 0..1 */
    float scale;                /* 0xd4: beam width */
    float fade;                 /* 0xd8 */
    float len;                  /* 0xdc */
    FxOp fx_op;                 /* 0xe0 */
    int fx;                     /* 0xe4 */
    float t;                    /* 0xe8: frames */
    float t_max;                /* 0xec */
    EnVishnuHandWork *owner;         /* 0xf0 */
} EnbVishnuWork;

/* enb_vishnu_params: name, label, values */
typedef struct EnbVishnuRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    float spd;                  /* 0x0c: per second when extending */
    float ease;                 /* 0x10 */
    float hit_r;                /* 0x14 */
    float pulse;                /* 0x18 */
    float back;                 /* 0x1c: per second when retracting */
    float die;                  /* 0x20: seconds */
    float ret;                  /* 0x24: per second when returning */
    float ret_ease;             /* 0x28 */
    int edit;                   /* 0x2c: uses the colours below */
    int col[3];                 /* 0x30 */
    float uv[2];                /* 0x3c */
} EnbVishnuRec;

#define ENB_VISHNU_REC ((EnbVishnuRec *)&enb_vishnu_params)
extern LineStyle enb_vishnu_line_style;
extern float enb_vishnu_fade_color[4];
extern char enb_vishnu_lock_name[];
extern void (*enb_vishnu_states[])(EnbVishnuWork *w);

#define MTX_TRANSLATE_V3(v)                                        \
    do {                                                           \
        Vec3 *tv_ = (v);                                           \
                                                                   \
        vu0_matrix_translate(ulMatrixCur, tv_->x, tv_->y, tv_->z); \
    } while (0)

/* 0x00b158e0 */
void enb_vishnu_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = enb_vishnu_params.name;
    hdr.fmt = enb_vishnu_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = enb_vishnu_vardefs;
    val = enb_vishnu_params.v;
    for (i = 0; i < 15; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enb_vishnu_vardefs);
}

/* 0x00b15990 */
void enb_vishnu_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b15998 */
int enb_vishnu_vars_init(int *prm)
{
    enb_vishnu_vars_register();
    enb_vishnu_init_arg = *prm;
    return 0;
}

/* 0x00b159d0 */
int enb_vishnu_vars_release(void)
{
    enb_vishnu_vars_unregister();
    return 0;
}

/* 0x00b159f0 */
void enb_vishnu_hit_check(int *w)
{
    LockObj *lock = (LockObj *)&w[36];
    Vec3 spos;

    if (lock->flags & LOCK_HIT) {
        if (lock->life <= 0) {
            enemy_status_score_popup(w[57], (Vec3 *)&w[33], 20, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            ((EnbVishnuWork *)w)->fx_op = 3;
            do {
                w[1] = 1;
                w[3] = 0;
            } while (0);
        }
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&w[33], &spos.x);
        { int n = en_vishnu_hand_count_others((int *)w[60]); SoundRhythmSe_BulletPos(enb_vishnu_init_arg, 6 - n, &spos.x); }
    }
}

/* 0x00b15ab8 */
void enb_vishnu_hit_player(unsigned int *w)
{
    if (w[0] < 2 && player_sphere_test((Vec3 *)&w[33], enb_vishnu_collision_r) == 1) {
        player_status_add_damage(1.0f);
        w[56] = 5;
        do {
            w[1] = 3;
            w[3] = 0;
        } while (0);
    }
}

/* 0x00b15b28 */
void enb_vishnu_pulse(unsigned int *w)
{
    float rate, ph, t;

    if (w[0] < 2) {
        ph = beat_clock_get_phase() * 4.0f;
        rate = enb_vishnu_rate_scale;
        t = ease_sine_in_out_2x(ph - (int)ph);
        *(float *)&w[53] = (1.0f - rate) + (rate + rate) * t;
    }
}

/* 0x00b15bb0 */
void enb_vishnu_state_extend(EnbVishnuWork *w)
{
    Vec3 pl;
    Vec3 dir;
    Vec3 spos;
    float rate;

    player_get_pos(&pl);
    v3_sub(&dir, &pl, &w->base);
    switch (w->step) {
    case 0:
        v3_copy(&w->pos, &w->base);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&w->pos, (float *)&spos);
        project_point(&spos, &spos);
        SoundRhythmSe_BulletPos(enb_vishnu_init_arg, 0, (float *)&spos);
        w->k = 0.0f;
        w->t = 1.0f;
        w->t_max = (int)(ulVec3Distance(&w->pos, &pl) / (ENB_VISHNU_REC->spd * FRAME_SEC));
        w->step++;
        w->next_step++;
    case 1:
        rate = safe_powf(ease_sine_in_out(w->t / w->t_max), ENB_VISHNU_REC->ease);
        dir.x *= rate;
        dir.y *= rate;
        dir.z *= rate;
        w->k = rate;
        v3_sum(&w->pos, &w->base, &dir);
        w->t += 1.0f;
        if (w->t_max < w->t) {
            player_status_add_damage(1.0f);
            w->fx_op = 5;
            do {
                w->next = 3;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b15df8 */
void enb_vishnu_state_reflect(EnbVishnuWork *w)
{
    Vec3 pl;
    Vec3 dir;
    float rate;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
    case 1:
        player_get_pos(&pl);
        v3_sub(&dir, &pl, &w->base);
        rate = MAX_GE(w->k - ENB_VISHNU_REC->back * FRAME_SEC, 0.0f);
        w->k = rate;
        dir.x *= rate;
        dir.y *= rate;
        dir.z *= rate;
        v3_sum(&w->pos, &w->base, &dir);
        if (fabsf(w->k) < FLT_EPSILON) {
            en_vishnu_hand_destroy(w->owner);
            FINISH_IF(w);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b15f60 */
void enb_vishnu_state_fade(EnbVishnuWork *w)
{
    float sc[3][2];
    float t, end, rate, decay;
    float x, y, z;
    int *wang;
    int ax, ay;

    switch (w->step) {
    case 0:
        w->t = 1.0f;
        w->lock.flags &= ~LOCK_LOCKABLE;
        w->t_max = (int)(ENB_VISHNU_REC->die * FPS);
        w->step++;
        w->next_step++;
    case 1:
        t = w->t;
        end = w->t_max;
        rate = t / end;
        z = -w->len;
        x = 0.0f;
        do { y = 0.0f; } while (0);
        wang = w->ang;
        ay = wang[1];
        ax = wang[0];
        ROT_Z(wang[2], sc[0], x, y);
        ROT_X(ax, sc[1], y, z);
        ROT_Y(ay, sc[2], x, z);
        v3_set(&w->pos, x, y, z);
        v3_add(&w->pos, &w->base);
        decay = 1.0f - 1.0f / (end - t + 1.0f);
        w->fade = 1.0f - rate;
        w->t += 1.0f;
        w->scale *= decay;
        if (end < w->t)
            FINISH_IF(w);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b161b8 */
void enb_vishnu_state_return(EnbVishnuWork *w)
{
    Vec3 root;
    int ang[3];
    Vec3 dir;
    float t, end, rate, ease, decay;

    en_vishnu_hand_get_muzzle(w->owner, &root, ang);
    switch (w->step) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        w->t = 1.0f;
        w->t_max = (int)(ulVec3Distance(&w->pos, &root) / (ENB_VISHNU_REC->ret * FRAME_SEC));
        w->step++;
        w->next_step++;
    case 1:
        end = w->t_max;
        t = w->t;
        rate = t / end;
        ease = safe_powf(ease_sine_in_out(rate), ENB_VISHNU_REC->ret_ease);
        v3_sub(&dir, &w->pos, &root);
        dir.x *= ease;
        dir.y *= ease;
        dir.z *= ease;
        v3_sum(&w->base, &root, &dir);
        decay = 1.0f - 1.0f / (end - t + 1.0f);
        w->fade = 1.0f - rate;
        w->t += 1.0f;
        w->scale *= decay;
        if (end < w->t)
            FINISH_IF(w);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00b163d0 */
void enb_vishnu_exec(EnbVishnuWork *w)
{
    Vec3 pl_vec;
    int n = enemy_status_get_ticks();

    enb_vishnu_hit_check((int *)w);
    en_vishnu_hand_get_muzzle(w->owner, &w->base, w->ang);
    if (n > 0) {
        do {
            enb_vishnu_hit_player((unsigned int *)w);
            do {
                w->phase = w->next;
                w->step = w->next_step;
            } while (0);
            if (enb_vishnu_states[w->phase])
                enb_vishnu_states[w->phase](w);
        } while (--n);
    } else {
        do {
            w->phase = w->next;
            w->step = w->next_step;
        } while (0);
    }
    if (w->phase == 3) {
        player_get_vec(&pl_vec);
        v3_add(&w->pos, &pl_vec);
    }
    enb_vishnu_pulse((unsigned int *)w);
}

/* steer(cast) */
/* 0x00b164e0 */
void enb_vishnu_disp(EnbVishnuWork *w)
{
    LineStyle style = enb_vishnu_line_style;
    LineStyle custom;
    LineStyle *sty = &style;
    Vec3 pts[2];
    float fade;
    int dl, blend;
    unsigned int mask;
    unsigned int col0, col1, col2;

    if (ENB_VISHNU_REC->edit) {
        sty = &custom;
        custom.col[0] = ENB_VISHNU_REC->col[0];
        custom.col[1] = ENB_VISHNU_REC->col[1];
        custom.col[2] = ENB_VISHNU_REC->col[2];
        custom.width = ENB_VISHNU_REC->uv[0];
        custom.rate_in = ENB_VISHNU_REC->uv[1];
        custom.unk_0c = 0;
        custom.unk_18[0] = 0;
        custom.unk_18[1] = 0;
    }
    dl = RenderSetCurrentDisplayList(6);
    mask = 0xffffff;
    v3_copy(&pts[0], &w->base);
    fade = w->fade;
    v3_copy(&pts[1], &w->pos);
    col0 = sty->col[0];
    col1 = sty->col[1];
    col2 = sty->col[2];
    sty->col[0] = (col0 & mask) | ((int)(color_get_a(col0) * fade) + 1) >> 1 << 24;
    sty->col[1] = (col1 & mask) | ((int)(color_get_a(col1) * fade) + 1) >> 1 << 24;
    sty->col[2] = (col2 & mask) | ((int)(color_get_a(col2) * fade) + 1) >> 1 << 24;
    sty->width *= w->scale;
    draw_polyline_wide_capped(sty, pts, 1);
    switch ((unsigned int)w->phase) {
    case 0:
    case 1:
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        MTX_TRANSLATE_V3(&w->pos);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        vu0_matrix_scale(ulMatrixCur, 1.75f, 1.75f, 1.75f);
        dc_object_draw(&w->body);
        ulMatrixPop();
        break;
    case 3:
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        ulGxSetTextureList(stage_textures);
        draw_push_strip_flags();
        draw_push_override_color();
        ulMatrixPush();
        enb_vishnu_fade_color[0] = w->fade - 1.0f;
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(enb_vishnu_fade_color);
        MTX_TRANSLATE_V3(&w->pos);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        vu0_matrix_scale(ulMatrixCur, 1.75f, 1.75f, 1.75f);
        dc_object_draw(&w->body);
        ulMatrixPop();
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        break;
    case 2:
        break;
    }
    RenderSetCurrentDisplayList(dl);
}

/* steer(cast) */
/* 0x00b16848 */
void enb_vishnu_init(int *work)
{
    EnbVishnuWork *w = (EnbVishnuWork *)work;

    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 89)), *((struct DcClip **)pnb_get_entry(stage_models, 90)), 4096);
    w->fade = 1.0f;
    w->fx = enemy_status_ent_alloc(enb_vishnu_init_arg, -1, w);
    enemy_status_event(w->fx, 2, &w->pos);
    w->fx_op = 3;
    lockon_obj_init(&w->lock, w, &w->pos, 0, enb_vishnu_lock_name, w->fx, ENB_VISHNU_REC->hp, ENB_VISHNU_REC->hp, 128);
    lockon_entry(&w->lock);
}

/* 0x00b16948 */
void enb_vishnu_release(char *wp)
{
    EnbVishnuWork *w = (EnbVishnuWork *)wp;

    dc_object_release((struct MapPart *)&w->body);
    enemy_status_event(w->fx, w->fx_op, (float *)&w->pos);
    lockon_remove(&w->lock);
}

/* 0x00b16988 */
void *enb_vishnu_create(void *parent)
{
    int *w = task_create(parent, &enb_vishnu_desc, 244);

    w[60] = (int)parent;
    enb_vishnu_init(w);
    return w;
}

/* 0x00b169d8 */
void enb_vishnu_detach(EnbVishnu *w)
{
    w->len = ulVec3Distance(&w->base, &w->pos);
    do { w->next = 2; } while (0);
    w->next_step = 0;
}
