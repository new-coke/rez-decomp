/*
 * Brahma, Area 2 enemy: slides along its rail (en_brahma_rail) in eased hops, throwing sparks;
 * between hops fires from both guns, head turned to the player. Runs off when its time is up.
 */

#include "stg02.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "sound_beat.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_brahma.h"
#include "e_shot.h"
#include "en_brahma_rail.h"
#include "en_brahma_spark.h"
#include "en_shiva_break.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"
#include "task.h"

extern void (*en_brahma_exec_table[])(void *o, void *w);
extern void (*en_brahma_disp_table[])(void *o, void *w);
extern ParamRec en_brahma_params;
extern VarDef en_brahma_vardefs[9];
extern int *en_brahma_gun_nodes[2];

/* five model parts and the lock-on record */
typedef struct EnBrahmaWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_brahma_states */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    Vec3 head;                  /* 0x20: the head part's position */
    int head_ang[3];            /* 0x2c: turned toward the player */
    int spin;                   /* 0x38: z turn per frame */
    ModelPart parts[5];                /* 0x3c */
    int clip;                   /* 0x208 */
    LockObj lock;                  /* 0x20c */
    void *wire;                 /* 0x24c: the spark task (en_brahma_spark) */
    Vec3 wire_pos;              /* 0x250 */
    float spark_rate;                 /* 0x25c: 0..1 from the speed along the rail */
    float rail;                 /* 0x260: position on the rail */
    float drift;                /* 0x264 */
    float dist;                 /* 0x268: of the current move */
    float n;                    /* 0x26c: frames into the move */
    float len;                  /* 0x270: frames of the move */
    int stop;                   /* 0x274 */
    float speed;                /* 0x278 */
    float t;                    /* 0x27c: frames */
    float t_max;                /* 0x280 */
    int flags;                  /* 0x284: 1 drawn, 2 moving */
} EnBrahmaWork;

extern int en_brahma_head_ofs;

/* en_brahma_params: name, label, values */
typedef struct EnBrahmaRec {
    char *name;
    char *label;                /* 0x04 */
    int he;                     /* 0x08 */
    int score;                  /* 0x0c */
    float anim;                 /* 0x10: seconds of the open motion */
    float anim2;                /* 0x14: seconds of the close motion */
    float spin;                 /* 0x18: degrees per frame when hit */
    float spd;                  /* 0x1c: rail units per second of a move */
    float fade_lo;              /* 0x20 */
    float fade_hi;              /* 0x24 */
    float run;                  /* 0x28: rail units per second when leaving */
} EnBrahmaRec;

#define EN_BRAHMA_REC ((EnBrahmaRec *)&en_brahma_params)
extern void (*en_brahma_states[])(void *o, EnBrahmaWork *w);

typedef struct EnBrahmaPlace {
    char pad_00[4];
    float x, y;                 /* 0x04: passed to en_brahma_rail_create */
    float rail;                 /* 0x0c: the start position on the rail (negated) */
    char pad_10[8];
    int rotz;                   /* 0x18 */
    char pad_1c[12];
    float kind;                    /* 0x28: its fraction (x1000, rounded) is the rail's kind */
} EnBrahmaPlace;

extern float en_brahma_rgba[4];
extern float en_brahma_spark_ofs[];
extern char en_brahma_lock_name[];
extern char en_brahma_name[];

/* 0x00afbfb8 */
void en_brahma_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_brahma_params.name;
    def.fmt = en_brahma_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_brahma_vardefs;
    fld = en_brahma_params.v;
    for (i = 0; i < 9; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_brahma_vardefs);
    en_brahma_rail_vars_register();
    en_brahma_spark_vars_register();
}

/* 0x00afc078 */
void en_brahma_vars_unregister(void)
{
    en_brahma_spark_vars_unregister();
    en_brahma_rail_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00afc0a0 */
int en_brahma_marker_draw(int obj, void *w)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 74);
    return obj_draw_at(obj, w, (void *)*ent);
}

/* 0x00afc0f8 */
int en_brahma_vars_init(void)
{
    en_brahma_vars_register();
    return 0;
}

/* 0x00afc118 */
int en_brahma_vars_release(void)
{
    en_brahma_vars_unregister();
    return 0;
}

/* 0x00afc138 */
void en_brahma_aim_head(void *obj, EnBrahmaWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 dir;
    Vec3 *head;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_brahma_rotate_yxz(ang);
    head = &w->head;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&en_brahma_head_ofs, (float *)head);
    ulMatrixApplyPoint((float *)ulMatrixCur, en_brahma_spark_ofs, (float *)&w->wire_pos);
    ulMatrixPop();
    player_get_pos(&pos);
    v3_sub(&dir, &pos, head);
    do {
        w->head_ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        w->head_ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
}

/* 0x00afc2c8 */
void en_brahma_hit_check(void *obj, int *wp)
{
    EnBrahmaWork *w = (EnBrahmaWork *)wp;
    LockObj *lock = &w->lock;
    Vec3 opos, pl;
    int ang[3];
    Vec3 dir, v;
    float sc[3][2];
    float x, y, z;
    int spr;
    Vec3 *pos;

    if (lock->flags & LOCK_HIT) {
        if (lock->life <= 0) {
            spr = enemy_task_get_status_slot(obj);
            pos = enemy_task_pos_ptr(obj);
            enemy_status_score_popup(spr, pos, EN_BRAHMA_REC->score, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            enemy_task_set_state(obj, 3);
        } else {
            en_shiva_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 9);
            enemy_task_get_pos(obj, &opos);
            player_get_pos(&pl);
            v3_sub(&dir, &opos, &pl);
            enemy_task_get_rot(obj, ang);
            x = 0.0f;
            y = 0.0f;
            z = 1.0f;
            ROT_Z(ang[2], sc[0], x, y);
            ROT_X(ang[0], sc[1], y, z);
            ROT_Y(ang[1] - ANG_90, sc[2], x, z);
            v.x = x;
            v.y = y;
            v.z = z;
            if (ulVec3Dot(&dir, &v) > 0.0f)
                w->spin = DEG_TO_ANG(EN_BRAHMA_REC->spin);
            else
                w->spin = -(int)DEG_TO_ANG(EN_BRAHMA_REC->spin);
        }
    }
}

/* 0x00afc518 */
void en_brahma_follow_rail(void *obj, EnBrahmaWork *w)
{
    Vec3 pos;
    int ang[3];
    Task *parent = TASK_OF(obj)->parent;

    if (!en_brahma_rail_sample(parent ? parent + 1 : 0, w->rail, &pos, ang)) {
        w->flags &= ~2;
        if (w->wire) {
            do {
                FINISH(w->wire);
            } while (0);
            w->wire = 0;
        }
    }
    enemy_task_set_pos(obj, &pos);
    enemy_task_set_rot(obj, ang);
}

/* 0x00afc5b8 */
void en_brahma_start_move(void *obj, EnBrahmaWork *w)
{
    float dis, n;

    if (enemy_task_get_param(obj, 1) < w->rail) {
        dis = enemy_task_get_param(obj, 1);
        dis += (RAND_FLOAT_A() - 1.0f) * enemy_task_get_param(obj, 5);
        dis -= w->rail;
    } else {
        dis = enemy_task_get_param(obj, 1);
        dis += RAND_FLOAT_A() * enemy_task_get_param(obj, 5);
        dis -= w->rail;
    }
    w->speed = 0.0f;
    w->dist = dis;
    w->n = 1.0f;
    n = (int)(fabsf(dis) / (EN_BRAHMA_REC->spd * FRAME_SEC));
    if (!(n >= 1.0f))
        n = 1.0f;
    w->len = n;
}

/* 0x00afc720 */
void en_brahma_state_move_in(void *obj, EnBrahmaWork *w)
{
    float frm, dst;

    switch (w->step) {
    case 0:
        frm = enemy_task_get_param(obj, 2);
        dst = enemy_task_get_param(obj, 1);
        w->t = 1.0f;
        w->speed = (dst - w->rail) / frm;
        w->t_max = frm;
        w->step++;
        w->next_step++;
    case 1:
        w->rail += w->speed;
        en_brahma_follow_rail(obj, w);
        w->t += 1.0f;
        if (w->t_max < w->t) {
            w->drift = w->speed;
            do {
                w->next = 2;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        w->speed = 0.0f;
        en_brahma_start_move(obj, w);
        w->flags |= 2;
        break;
    }
}

/* 0x00afc878 */
void en_brahma_state_wait(void *obj, EnBrahmaWork *w)
{
    Task *parent;
    float frm;

    switch (w->step) {
    case 0:
        w->t = 1.0f;
        w->t_max = enemy_task_get_param(obj, 3);
        w->step++;
        w->next_step++;
    case 1:
        en_brahma_follow_rail(obj, w);
        frm = enemy_task_get_state_time(obj);
        if ((float)(int)enemy_task_get_param(obj, 4) < frm) {
            if (!w->stop) {
                parent = TASK_OF(obj)->parent;
                en_brahma_rail_leave(parent ? (int *)(parent + 1) : 0);
                enemy_task_set_end_event(obj, 4);
            } else {
                do {
                    w->next_step = 0;
                    w->next = 5;
                } while (0);
            }
        }
        w->t += 1.0f;
        if (w->t_max < w->t) {
            do {
                w->next_step = 0;
                w->next = 2;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afc9d0 */
void en_brahma_state_attack_begin(void *obj, EnBrahmaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    switch (w->step) {
    case 0:
        w->flags |= 1;
        dc_object_set_clip(gfx, w->clip);
        gfx->speed = gfx->frames / (EN_BRAHMA_REC->anim * FPS);
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        en_brahma_follow_rail(obj, w);
        if (gfx->flags2 & 1) {
            do {
                w->next_step = 0;
                w->next = 3;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afcad8 */
void en_brahma_state_fire(void *obj, EnBrahmaWork *w)
{
    Vec3 pts[2];
    int angs[2][3];
    Vec3 dir;
    Vec3 out;
    ModelPart *gfx;
    Vec3 *pos;
    int *ang;
    int level;
    float fire_t;

    switch (w->step) {
    case 0:
        w->step++;
        w->t = 1.0f;
        w->next_step++;
    case 1:
        en_brahma_follow_rail(obj, w);
        fire_t = 2.0f;
        if (w->t == fire_t) {
            enemy_task_spawn(obj, 0x4000000, 2, pts, 0, 0);
            level = 8;
            gfx = enemy_task_gfx_ptr(obj);
            pos = enemy_task_pos_ptr(obj);
            ang = enemy_task_rot_ptr(obj);
            node_tree_collect_xforms_at_buf(gfx->root, gfx->clip, gfx->frame, 0x4000000, 2, pos, ang, enemy_task_param_vec_ptr(obj), angs[0]);
            v3_set(&dir, 0.0f, 0.0f, -1.0f);
            ulMatrixPush();
            if (game_type_get() == 3)
                level = 9;
            matrix_unit_set(0);
            en_brahma_rotate_yxz(angs[0]);
            ulMatrixApplyVector((float *)ulMatrixCur, (float *)&dir, (float *)&out);
            e_shot_fire(obj, &pts[0], &out, level);
            matrix_unit_set(0);
            en_brahma_rotate_yxz(angs[1]);
            ulMatrixApplyVector((float *)ulMatrixCur, (float *)&dir, (float *)&out);
            e_shot_fire(obj, &pts[1], &out, level);
            ulMatrixPop();
            w->flags &= ~1;
        }
        w->t += 1.0f;
        if (fire_t < w->t) {
            do {
                w->next_step = 0;
                w->next = 4;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afcd20 */
void en_brahma_state_attack_end(void *obj, EnBrahmaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    switch (w->step) {
    case 0:
        gfx->speed = -gfx->frames / (EN_BRAHMA_REC->anim2 * FPS);
        w->step++;
        w->next_step++;
    case 1:
        dc_object_advance_time(gfx);
        en_brahma_follow_rail(obj, w);
        if (gfx->flags2 & 1) {
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        dc_object_set_clip(gfx, 0);
        break;
    }
}

/* 0x00afce40 */
void en_brahma_state_escape(void *obj, EnBrahmaWork *w)
{
    Vec3 pos;
    int ang[3];
    Task *parent;

    switch (w->step) {
    case 0:
        w->step++;
        w->next_step++;
    case 1:
        w->rail += EN_BRAHMA_REC->run * FRAME_SEC;
        parent = TASK_OF(obj)->parent;
        if (en_brahma_rail_sample(parent ? parent + 1 : 0, w->rail, &pos, ang) == 1) {
            enemy_task_set_state(obj, 2);
            parent = TASK_OF(obj)->parent;
            en_brahma_rail_leave(parent ? (int *)(parent + 1) : 0);
        }
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afcf50 */
void en_brahma_exec_normal(void *obj, EnBrahmaWork *w)
{
    int ang[3];
    float prev, rate;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        prev = w->rail;
        w->drift *= 0.92f;
        w->rail = prev + w->drift;
        if (w->flags & 2) {
            w->rail -= w->speed;
            w->speed = w->dist * ease_sine_in_out(w->n / w->len);
            w->n += 1.0f;
            w->rail += w->speed;
            if (w->len < w->n)
                en_brahma_start_move(obj, w);
        }
        if (en_brahma_states[w->phase])
            en_brahma_states[w->phase](obj, w);
        w->spin = w->spin * 0.99f;
        enemy_task_get_rot(obj, ang);
        ang[2] += w->spin;
        enemy_task_set_rot(obj, ang);
        rate = w->rail - prev;
        rate = (rate - EN_BRAHMA_REC->fade_lo) / (EN_BRAHMA_REC->fade_hi - EN_BRAHMA_REC->fade_lo);
        w->spark_rate = MIN(MAX_GE(rate, 0.0f), 1.0f);
    }
}

/* 0x00afd120 */
void en_brahma_exec_appear(void *obj, EnBrahmaWork *w)
{
    float t;

    switch ((unsigned int)enemy_task_get_phase(obj)) {
    case 0:
        w->t = 1.0f;
        break;
    case 1:
        t = w->t + 1.0f;
        w->t = t;
        if ((float)(int)enemy_task_get_param(obj, 0) < t)
            enemy_task_next_phase(obj);
        break;
    case 2:
        w->wire = en_brahma_spark_create(obj, &w->wire_pos, enemy_task_rot_ptr(obj), (unsigned int)&w->spark_rate);
        break;
    }
}

/* 0x00afd218 */
void en_brahma_exec_destroyed(void *obj, void *w)
{
    SpawnDesc desc;
    Task *parent;

    switch ((unsigned int)enemy_task_get_phase(obj)) {
    case 0:
        parent = TASK_OF(obj)->parent;
        en_brahma_rail_leave(parent ? (int *)(parent + 1) : 0);
        break;
    case 1:
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 84;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_hit_imp(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
        break;
    case 2:
        break;
    }
}

/* 0x00afd2e0 */
void en_brahma_pre_exec(void *obj, int *w)
{
    if (w[147] && (TASK_OF(w[147])->flags & TASK_F_END))
        w[147] = 0;
    en_brahma_hit_check(obj, w);
}

/* 0x00afd308 */
void en_brahma_exec(void *obj, void *w)
{
    void (**tbl)(void *obj, void *w) = en_brahma_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tbl[i])
        tbl[i](obj, w);
}

/* 0x00afd368 */
void en_brahma_post_exec(void *obj, EnBrahmaWork *w)
{
    ModelPart *gfx;

    dc_object_advance_time(&w->parts[3]);
    w->parts[1].frame = beat_clock_get_bar() * w->parts[1].frames;
    gfx = enemy_task_gfx_ptr(obj);
    w->parts[0].frame = gfx->frame;
    w->parts[4].frame = w->parts[3].frame;
    w->parts[2].frame = w->parts[1].frame;
    en_brahma_aim_head(obj, w);
}

/* 0x00afd3d8 */
void en_brahma_toggle_guns(void *obj, int *w)
{
    if (w[161] & 1)
        return;
    util_node_toggle(en_brahma_gun_nodes[0]);
    util_node_toggle(en_brahma_gun_nodes[1]);
}

/* steer(cast) */
/* 0x00afd428 */
void en_brahma_disp_body(void *obj, EnBrahmaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 *pos = enemy_task_pos_ptr(obj);
    int *ang;
    int blend;
    Vec3 *head;

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    ang = enemy_task_rot_ptr(obj);
    ulGxSetTextureList(stage_textures);
    ulMatrixPush();
    do {
        Vec3 *q = pos;
        vu0_matrix_translate(ulMatrixCur, q->x, q->y, q->z);
    } while (0);
    en_brahma_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_brahma_toggle_guns(obj, (int *)w);
    dc_object_draw(gfx);
    en_brahma_toggle_guns(obj, (int *)w);
    dc_object_draw(&w->parts[3]);
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    en_brahma_rgba[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_brahma_rgba);
    dc_object_draw(&w->parts[0]);
    dc_object_draw(&w->parts[4]);
    ulMatrixPop();
    ulMatrixPush();
    do {
        head = &w->head;
        vu0_matrix_translate(ulMatrixCur, head->x, head->y, head->z);
    } while (0);
    en_brahma_rotate_yxz(w->head_ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(&w->parts[2]);
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
    dc_object_draw(&w->parts[1]);
    ulMatrixPop();
}

/* 0x00afd5e8 */
void en_brahma_disp(void *obj, void *w)
{
    void (**tbl)(void *obj, void *w) = en_brahma_disp_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tbl[i])
        tbl[i](obj, w);
}

/* steer(cast) */
/* 0x00afd648 */
void en_brahma_init(void *obj, EnBrahmaWork *w, EnBrahmaPlace *prm)
{
    LockObj *lock;
    int n;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    lock = &w->lock;
    dc_object_init(&w->parts[0], *((struct DcNode **)pnb_get_entry(stage_models, 80)), *((struct DcClip **)pnb_get_entry(stage_models, 81)), 0);
    dc_object_init(&w->parts[1], *((struct DcNode **)pnb_get_entry(stage_models, 76)), *((struct DcClip **)pnb_get_entry(stage_models, 77)), 4096);
    dc_object_init(&w->parts[2], *((struct DcNode **)pnb_get_entry(stage_models, 72)), *((struct DcClip **)pnb_get_entry(stage_models, 73)), 4096);
    dc_object_init(&w->parts[3], *((struct DcNode **)pnb_get_entry(stage_models, 78)), *((struct DcClip **)pnb_get_entry(stage_models, 79)), 4096);
    dc_object_init(&w->parts[4], *((struct DcNode **)pnb_get_entry(stage_models, 82)), *((struct DcClip **)pnb_get_entry(stage_models, 83)), 4096);
    dc_object_rebuild(&w->parts[0]);
    dc_object_rebuild(&w->parts[1]);
    dc_object_rebuild(&w->parts[2]);
    dc_object_rebuild(&w->parts[3]);
    dc_object_rebuild(&w->parts[4]);
    w->clip = *((int *)pnb_get_entry(stage_models, 75));
    lockon_obj_init(lock, obj, &w->head, 0, en_brahma_lock_name, enemy_task_get_status_slot(obj), EN_BRAHMA_REC->he, EN_BRAHMA_REC->he, 0);
    lockon_entry(lock);
    w->rail = -prm->rail;
    w->parts[3].speed = 0.6666667f;
    n = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 4)) * 1000.0f + 0.5f);
    n = MAX_GE(n, 0);
    w->stop = n > 1 ? 1 : n;
    do {
        w->next = 0;
        w->next_step = 0;
    } while (0);
    enemy_task_set_vanish(obj, 2, 12);
}

/* 0x00afd8f8 */
void en_brahma_release(void *obj, EnBrahmaWork *w)
{
    dc_object_release(&w->parts[0]);
    dc_object_release(&w->parts[1]);
    dc_object_release(&w->parts[2]);
    dc_object_release(&w->parts[3]);
    dc_object_release(&w->parts[4]);
    lockon_remove(&w->lock);
}

/* steer(cast) */
/* 0x00afd948 */
int en_brahma_create(void *parent, EnBrahmaPlace *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_brahma_init;
    funcs.fn[1] = en_brahma_release;
    funcs.fn[2] = en_brahma_exec;
    funcs.fn[3] = en_brahma_disp;
    funcs.fn[4] = en_brahma_pre_exec;
    funcs.fn[5] = en_brahma_post_exec;
    desc.name = en_brahma_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 74;
    desc.model_no2 = -1;
    desc.hp = EN_BRAHMA_REC->he;
    desc.score = EN_BRAHMA_REC->score;
    desc.size = 648;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(en_brahma_rail_create(parent, place->rotz, (int)(((float (*)(float))float_frac)(place->kind) * 1000.0f + 0.5f), place->x, place->y), &desc);
}

/* steer(cast) */
/* 0x00afdab0 */
void en_brahma_models_init(void)
{
    int model = *((int *)pnb_get_entry(stage_models, 74));

    node_tree_collect_to((void *)model, 0x4000000, (struct NodeTreeNode * *)en_brahma_gun_nodes);
    model_pnb_get_wire(stage_models, 84);
    ulMatrixPush();
    matrix_unit_set(0);
    util_node_collect_pos_at((void *)model, 0, 0.0f, 0x20000000, 1, (int)&en_brahma_head_ofs);
    ulMatrixPop();
}

/* 0x00afdb38 */
void en_brahma_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
