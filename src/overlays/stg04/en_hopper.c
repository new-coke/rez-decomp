/*
 * Hopper, Area 4 enemy: jumps between points near the player (first from its placement or a
 * marimo), leaving a landing ring; fires a homing shot (enb_hopper) between jumps.
 */

#include "stg04.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "player.h"
#include "pnb.h"
#include "sound_midise.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_hopper.h"
#include "en_flash.h"
#include "en_hopper_circle.h"
#include "enb_hopper.h"
#include "../common/en_break.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

/* as en_hopper_release sees it */
typedef struct EnHopperReleaseWork {
    char pad_00[32];
    ModelPart body;                   /* 0x20 */
    ModelPart jump;                   /* 0x7c */
    char pad_d8[40];
    ModelPart gun;                   /* 0x100 */
    char pad_15c[52];
    LockObj lock;                  /* 0x190 */
} EnHopperReleaseWork;

extern ParamRec en_hopper_params;
extern VarDef en_hopper_vardefs[16];
extern int en_hopper_init_arg;

/* 492 bytes beyond the object */
typedef struct EnHopperWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_hopper_states */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    ModelPart body;                   /* 0x20: body */
    ModelPart jump;                   /* 0x7c: the jump's timing */
    int wire;                   /* 0xd8 */
    Vec3 mov;                   /* 0xdc: sideways move of the jump */
    Vec3 jmp;                   /* 0xe8: the jump's offset */
    float height;               /* 0xf4 */
    float aim;                  /* 0xf8: the shot's aim distance */
    float start;                /* 0xfc */
    ModelPart gun;                   /* 0x100: drawn at base */
    Vec3 base;                  /* 0x15c */
    int base_ang[3];            /* 0x168 */
    Vec3 off;                   /* 0x174: wobble offset */
    float t;                    /* 0x180: wobble time */
    Vec3 knock;                 /* 0x184 */
    LockObj lock;                  /* 0x190 */
    Vec3 muzzle;                /* 0x1d0 */
    int skip;                   /* 0x1dc: bit 0 skips en_hopper_calc_base, bit 1 skips en_hopper_calc_muzzle; 4, 8, 0x10, 0x20 done */
    float cnt;                  /* 0x1e0 */
    float cnt_end;              /* 0x1e4 */
    int n;                      /* 0x1e8: jumps */
} EnHopperWork;

extern ObjStateFn en_hopper_exec_table[];

/* en_hopper_params, 16 words */
typedef struct EnHopperRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_swing;            /* 0x10: wobble time step */
    float r_swing;              /* 0x14: its amplitude */
    float frm_attack_motion;    /* 0x18: the turn animation (seconds) */
    int dst_away;               /* 0x1c */
    float sca_shot;             /* 0x20: the shot's aim height (per 0.03) */
    float sca_laser;            /* 0x24 */
    float spd_jump;             /* 0x28: jump time per 1000 units of height */
    float dst_jump1;            /* 0x2c: jump height with parameter 0 zero */
    float dst_jump2;            /* 0x30: and otherwise */
    float ang_error_min;        /* 0x34: the shot's turn range (degrees) */
    float ang_error_max;        /* 0x38 */
    float acc_gravity;          /* 0x3c */
    float dst_ground;           /* 0x40: height above the player */
    int test;                   /* 0x44: prints the object's state */
} EnHopperRec;

#define EN_HOPPER_REC ((EnHopperRec *)&en_hopper_params)
extern void (*en_hopper_states[])(void *o, EnHopperWork *w);
extern void (*en_hopper_disp_table[])(void *o, EnHopperWork *w);
extern char en_hopper_name[];
extern char en_hopper_lock_name[];
extern char str_en_hopper_pos[];
extern char str_en_hopper_ang[];
extern char str_en_hopper_frm[];
extern char str_en_hopper_cnt[];
extern char str_en_hopper_mod[];
extern char str_en_hopper_nul[];
extern char str_en_hopper_mov[];
extern char str_en_hopper_jmp[];

/* 0x00b03758 */
void en_hopper_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_hopper_params.name;
    hdr.fmt = en_hopper_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_hopper_vardefs;
    val = en_hopper_params.v;
    for (i = 0; i < 16; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_hopper_vardefs);
    en_hopper_circle_vars_register();
}

/* 0x00b03810 */
void en_hopper_vars_unregister(void)
{
    en_hopper_circle_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00b03830 */
int en_hopper_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 119);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b03888 */
int en_hopper_vars_init(int *se)
{
    en_hopper_vars_register();
    en_hopper_init_arg = *se;
    return 0;
}

/* 0x00b038c0 */
int en_hopper_vars_release(void)
{
    en_hopper_vars_unregister();
    return 0;
}

/* 0x00b038e0 */
void en_hopper_calc_base(void *obj, EnHopperWork *w)
{
    Vec3 v;
    int ang[3];

    enemy_task_get_rot(obj, ang);
    enemy_task_get_pos(obj, &w->base);
    player_get_pos(&v);
    v3_sub(&v, &v, &w->base);
    do {
        w->base_ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        w->base_ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    w->base_ang[2] = ang[2];
    ang[1] = w->base_ang[1];
    enemy_task_set_rot(obj, ang);
    w->skip |= 1;
}

/* 0x00b03a40 */
void en_hopper_calc_muzzle(void *obj, EnHopperWork *w)
{
    if (!(w->skip & 1))
        en_hopper_calc_base(obj, w);
    util_rotate_xyz(w->base_ang, 0.0f, 0.0f, -4.0f, &w->muzzle);
    w->skip |= 2;
    v3_add(&w->muzzle, &w->base);
}

/* 0x00b03ae0 */
void en_hopper_calc_points(void *obj, EnHopperWork *w)
{
    int skip = w->skip;

    if (!(skip & 1))
        en_hopper_calc_base(obj, w);
    if (!(skip & 2))
        en_hopper_calc_muzzle(obj, w);
}

/* 0x00b03b50 */
void en_hopper_hit_check(void *obj, EnHopperWork *w)
{
    LockObj *lock = &w->lock;
    EnHopperRec *rec;
    int hits;
    float xy;

    if (lock->flags & LOCK_HIT) {
        rec = EN_HOPPER_REC;
        hits = lock->life;
        xy = 0.0f;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xy, xy, rec->sca_laser, &w->knock);
        if (hits <= 0) {
            enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), rec->score, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_task_set_state(obj, 3);
        } else
            en_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 0);
    }
}

/* 0x00b03c98 */
void en_hopper_swing(void *obj, EnHopperWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->t += EN_HOPPER_REC->spd_swing;
    r = EN_HOPPER_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->off);
    v3_set(&w->off, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, &w->off);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b03dc8 */
void en_hopper_fire_flash(void *obj, EnHopperWork *w)
{
    EnFlashArg prm;

    if (fabsf(enemy_task_get_param(obj, 0)) < FLT_EPSILON)
        return;
    if (!(fabsf((float)w->n) < FLT_EPSILON))
        return;
    prm.v = (int)&w->muzzle;
    prm.frm = 24.0f;
    prm.scl = 1.7f;
    en_hopper_calc_muzzle(obj, w);
    en_flash_create(obj, &prm);
}

/* 0x00b03e70 */
void en_hopper_fire_shot(void *obj, EnHopperWork *w)
{
    float xy;

    if (fabsf(enemy_task_get_param(obj, 0)) < FLT_EPSILON)
        return;
    if (!(fabsf((float)w->n) < FLT_EPSILON))
        return;
    en_hopper_calc_base(obj, w);
    en_hopper_calc_muzzle(obj, w);
    xy = 0.0f;
    enb_hopper_create(obj, &w->muzzle, w->base_ang);
    util_rotate_xyz(w->base_ang, xy, xy, EN_HOPPER_REC->sca_shot, &w->knock);
    w->knock.y = xy;
}

/* 0x00b03f38 */
void en_hopper_pick_move(void *obj, EnHopperWork *w, Vec3 *mov)
{
    Vec3 pos;
    Vec3 tgt;
    Vec3 hop;
    float s, c;
    float x, z;
    float rnd_max;
    int ang;

    enemy_task_get_pos(obj, &pos);
    if (!w->n) {
        player_get_frame_pos(&tgt);
        w->aim = ulVec3Distance(&pos, &tgt) * 0.5f + EN_HOPPER_REC->sca_shot / 0.0299999714f;
    }
    tgt.x = 0;
    tgt.y = 0;
    tgt.z = w->aim;
    util_pos_to_player_frame(&tgt, &tgt);
    rnd_max = 32769.0f;
    v3_sub(&hop, &tgt, &pos);
    v3_scale(&hop, 1.0f / (float)(3 - w->n));
    ang = (int)(rand_lcg_next_a() / rnd_max * (float)((int)DEG_TO_ANG(EN_HOPPER_REC->ang_error_max) - (int)DEG_TO_ANG(EN_HOPPER_REC->ang_error_min)) +
              (float)(int)DEG_TO_ANG(EN_HOPPER_REC->ang_error_min));
    if (w->n == 2) {
        if (w->base_ang[1] < 0)
            ang = -ang;
    } else if (rand_lcg_next_a() / rnd_max < 0.5f)
        ang = -ang;
    x = hop.x;
    z = hop.z;
    do {
        float ox = x, oz = z;

        ulSinCos(ang, &s, &c);
        x = ox * c + oz * s;
        z = -ox * s + oz * c;
    } while (0);
    mov->x = x;
    mov->z = z;
    mov->y = 0;
}

/* 0x00b04188 */
void en_hopper_jump_curve(EnHopperWork *w, Vec3 *out, float t)
{
    float spd;

    if (t < 27.0f) {
        out->z = 0;
        out->x = 0;
        out->y = 1.0f - t * 0.0013717421f * t;
    } else {
        t -= 27.0f;
        spd = 2.0f / (w->jump.frames - 27.0f);
        out->z = 0;
        out->x = 0;
        out->y = spd * t - spd * 0.5f * spd * 0.5f * t * t;
    }
}

/* 0x00b04220 */
void en_hopper_jump_start(void *obj, EnHopperWork *w, float height)
{
    Vec3 pos;
    Vec3 v;
    ModelPart *jump = &w->jump;

    height += EN_HOPPER_REC->dst_ground;
    w->height = height;
    enemy_task_get_pos(obj, &pos);
    v3_add(&pos, &w->jmp);
    en_hopper_jump_curve(w, &v, jump->frame);
    v3_scale(&v, height);
    v3_sub(&pos, &pos, &v);
    enemy_task_set_pos(obj, &pos);
    jump->speed = jump->frames / ((height + height) / (EN_HOPPER_REC->spd_jump * FRAME_SEC));
}

/* 0x00b04358 */
void en_hopper_place_first(void *obj, EnHopperWork *w)
{
    Vec3 pos;
    Vec3 pl;
    ModelPart *jump = &w->jump;
    float height;

    enemy_task_get_pos(obj, &pos);
    player_get_pos(&pl);
    height = pos.y;
    pos.y = pl.y - EN_HOPPER_REC->dst_ground;
    height -= pos.y;
    w->height = height;
    enemy_task_set_pos(obj, &pos);
    *(int *)&jump->frame = 0; /* steer(type): stored as an int zero, as the original */
    jump->speed = jump->frames / ((height + height) / (EN_HOPPER_REC->spd_jump * FRAME_SEC));
}

/* 0x00b04420 */
void en_hopper_land(void *obj, EnHopperWork *w)
{
    Vec3 pos;

    enemy_task_get_pos(obj, &pos);
    pos.y -= 8.316f;
    en_hopper_circle_create(obj, &pos);
    SoundRhythmSe_Enemy(en_hopper_init_arg, 2);
}

/* 0x00b04480 */
void en_hopper_jump_update(void *obj, EnHopperWork *w)
{
    ModelPart *jump = &w->jump;
    int skip;
    Vec3 pos;
    float frm, rate;

    skip = w->skip;
    dc_object_advance_time(jump);
    frm = jump->frame;
    if (27.0f <= frm && !(skip & 0x10)) {
        w->skip |= 0x10;
        v3_set(&w->mov, 0.0f, 0.0f, 0.0f);
        en_hopper_land(obj, w);
    }
    if (27.0f <= frm && !(skip & 0x20)) {
        w->skip |= 0x20;
        en_hopper_pick_move(obj, w, &w->mov);
        rate = jump->speed / jump->frames;
        v3_scale(&w->mov, rate);
        if (!(fabsf(enemy_task_get_param(obj, 0)) < FLT_EPSILON))
            en_hopper_jump_start(obj, w, EN_HOPPER_REC->dst_jump2);
        else
            en_hopper_jump_start(obj, w, EN_HOPPER_REC->dst_jump1);
    }
    if (jump->flags2 & 1)
        w->skip &= ~0x30;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->jmp);
    en_hopper_jump_curve(w, &w->jmp, frm);
    v3_scale(&w->jmp, w->height);
    v3_add(&pos, &w->jmp);
    v3_add(&pos, &w->mov);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b046f0 */
void en_hopper_state_first_jump(void *obj, EnHopperWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *body = &w->body;
    ModelPart *jump;
    unsigned int step = w->step;

    switch (step) {
    case 0:
        step = ++w->step;
        w->next_step++;
    case 1:
        jump = &w->jump;
        if (jump->flags2 & 1) {
            body->frame = gfx->frame = gfx->frames;
            do {
                w->next = 2;
                w->next_step = 0;
            } while (0);
        } else {
            body->frame = gfx->frame = gfx->frames * (jump->frame / jump->frames);
        }
        if (w->phase != w->next || step != w->next_step)
            w->step = step + 1;
        break;
    case 2:
        break;
    }
}

/* steer(cast) */
/* 0x00b047c0 */
void en_hopper_state_jump(void *obj, EnHopperWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *body = &w->body;
    ModelPart *jump = &w->jump;

    switch (w->step) {
    case 0:
        dc_object_set_clip(body, *((struct DcClip **)pnb_get_entry(stage_models, 126)));
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 122)));
        gfx->frame = 0.0f;
        w->step++;
        w->start = jump->frame;
        w->next_step++;
    case 1:
        if (jump->flags2 & 1) {
            gfx->frame = gfx->frames;
            body->frame = gfx->frames;
            if (w->n < 2) {
                do {
                    w->next = 2;
                    w->next_step = 0;
                } while (0);
            } else
                enemy_task_set_state(obj, 2);
        } else {
            gfx->frame = gfx->frames *
                ((jump->frame - w->start) / (jump->frames - w->start));
            body->frame = gfx->frame;
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* steer(cast) */
/* 0x00b04930 */
void en_hopper_state_attack(void *obj, EnHopperWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *body = &w->body;
    ModelPart *gun = &w->gun;
    float frm;

    switch (w->step) {
    case 0:
        dc_object_set_clip(body, *((struct DcClip **)pnb_get_entry(stage_models, 125)));
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 121)));
        w->step++;
        w->next_step++;
        gfx->speed = gfx->frames / (EN_HOPPER_REC->frm_attack_motion * FPS);
        gfx->frame = 0.0f;
        w->cnt = 1.0f;
        w->cnt_end = (int)(EN_HOPPER_REC->frm_attack_motion * FPS);
    case 1:
        dc_object_advance_time(gfx);
        frm = gfx->frame;
        body->frame = frm;
        gun->frame = frm;
        if (!(w->skip & 4) && 0.0f <= frm) {
            w->skip |= 4;
            en_hopper_fire_flash(obj, w);
        }
        if (!(w->skip & 8) && 15.0f <= frm) {
            w->skip |= 8;
            en_hopper_fire_shot(obj, w);
        }
        if (gfx->flags2 & 1) {
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->n++;
        w->skip &= ~0xc;
        break;
    }
}

/* 0x00b04b40 */
void en_hopper_exec_normal(void *obj, EnHopperWork *w)
{
    Vec3 pos;
    int mode;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    mode = enemy_task_get_phase(obj);
    if (mode != 1) {
        if (mode == 0)
            en_hopper_place_first(obj, w);
    } else {
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->knock, 0.97f);
        v3_add(&pos, &w->knock);
        enemy_task_set_pos(obj, &pos);
        en_hopper_swing(obj, w);
        en_hopper_jump_update(obj, w);
        if (en_hopper_states[w->phase])
            en_hopper_states[w->phase](obj, w);
    }
}

/* 0x00b04c68 */
void en_hopper_exec_destroyed(void *obj, EnHopperWork *w)
{
    SpawnDesc desc;
    int mode = enemy_task_get_phase(obj);

    if (mode != 1) {
        if (mode == 0) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0x40000;
            desc.models = stage_models;
            desc.type = 119;
            switch ((unsigned int)w->phase) {
            case 0:
                desc.motion = 120;
                break;
            case 1:
                desc.motion = 122;
                break;
            case 2:
                desc.motion = 121;
                break;
            }
            desc.time = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
            desc.v.x = 0;
            desc.v.y = 0;
            desc.v.z = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            enemy_shatter_create(obj, &desc, 0, 0);
        }
    } else
        enemy_task_next_phase(obj);
}

/* 0x00b04d80 */
void en_hopper_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_hopper_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b04de0 */
void en_hopper_pre_exec(void *obj, EnHopperWork *w)
{
    w->skip &= ~3;
    en_hopper_hit_check(obj, w);
}

/* 0x00b04df8 */
void en_hopper_post_exec(void *obj, EnHopperWork *w)
{
    en_hopper_calc_points(obj, w);
}

/* 0x00b04e00 */
void en_hopper_disp_body(void *obj, EnHopperWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    ulGxSetTextureList(stage_textures);
    do {
        ulMatrixPush();
    } while (0);
    ulMatrixPush();
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    en_hopper_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    do {
        Vec3 *p_ = &w->base;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hopper_rotate_yxz(w->base_ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->gun);
    ulMatrixPop();
}

/* 0x00b04f20 */
void en_hopper_disp(void *obj, EnHopperWork *w)
{
    void (**tbl)(void *obj, EnHopperWork *w) = en_hopper_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 pl;
    int ang[3];
    Vec3 v;
    ModelPart *jump;
    ModelPart *gfx;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_HOPPER_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        player_get_pos(&pl);
        v3_sub(&pos, &pos, &pl);
        debug_printf(str_en_hopper_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_hopper_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        jump = &w->jump;
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf(str_en_hopper_frm, (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf(str_en_hopper_cnt, (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        debug_printf(str_en_hopper_mod, w->phase);
        debug_newline();
        debug_printf(str_en_hopper_nul, jump->speed, (int)jump->frame, (int)jump->frames);
        debug_newline();
        v3_copy(&v, &w->mov);
        debug_printf(str_en_hopper_mov, v.x, v.y, v.z);
        debug_newline();
        v3_copy(&v, &w->jmp);
        debug_printf(str_en_hopper_jmp, v.x, v.y, v.z);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b05200 */
void en_hopper_init(void *obj, EnHopperWork *w)
{
    ModelPart *body = &w->body;
    ModelPart *jump = &w->jump;
    ModelPart *gun = &w->gun;
    LockObj *lock = &w->lock;

    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    {
        int *mdl = ((int *)pnb_get_entry(stage_models, 123));
        dc_object_init(body, *mdl, *((struct DcClip **)pnb_get_entry(stage_models, 124)), 0);
    }
    {
        int *mdl = ((int *)pnb_get_entry(stage_models, 117));
        dc_object_init(gun, *mdl, *((struct DcClip **)pnb_get_entry(stage_models, 118)), 0);
    }
    {
        int n = system_pnb_entry_get(0);
        dc_object_init(jump, n, *((struct DcClip **)pnb_get_entry(stage_models, 128)), 0x1000);
    }
    dc_object_rebuild(body);
    dc_object_rebuild(gun);
    w->wire = *((int *)pnb_get_entry(stage_models, 127));
    lockon_obj_init(lock, obj, &w->muzzle, 0, en_hopper_lock_name, enemy_task_get_status_slot(obj), EN_HOPPER_REC->hp, EN_HOPPER_REC->hp, 0);
    lockon_entry(lock);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b053b8 */
void en_hopper_release(void *obj, EnHopperReleaseWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->gun);
    dc_object_release(&w->jump);
    lockon_remove(&w->lock);
}

/* 0x00b053f8 */
int en_hopper_create(void *parent, EnHopperPlace *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_hopper_init;
    funcs.fn[1] = en_hopper_release;
    funcs.fn[2] = en_hopper_exec;
    funcs.fn[3] = en_hopper_disp;
    funcs.fn[4] = en_hopper_pre_exec;
    funcs.fn[5] = en_hopper_post_exec;
    desc.name = en_hopper_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 119;
    desc.model_no2 = 120;
    desc.hp = EN_HOPPER_REC->hp;
    desc.score = EN_HOPPER_REC->score;
    desc.size = 492;
    place->se = *(unsigned char *)&en_hopper_init_arg;
    place->link = -1;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b05548 */
void en_hopper_models_init(void)
{
}

/* 0x00b05550 */
void en_hopper_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
