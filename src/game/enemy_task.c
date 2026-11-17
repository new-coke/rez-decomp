/*
 * Common enemy task: the shell every placed enemy runs in. Owns transform, model and lock-on
 * target, moves with the player, steps wait/appear/normal/shot-down/escape per simulation tick
 * through the type's callbacks. Also the shared appear/vanish scaling and fading.
 */

#include "types_game.h"
#include "task.h"
#include "mathdef.h"
#include "nj_draw.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "enemy_task.h"
#include "debug.h"
#include "enemy_status.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "sound_beat.h"
#include "system.h"

typedef struct ObjPlace {
    char pad_00[4];
    Vec3 pos;               /* 0x04 */
    IVec3 rot;              /* 0x10 */
} ObjPlace;

#define MAXF(a, b) ((a) >= (b) ? (a) : (b))
#define MINF(a, b) ((a) < (b) ? (a) : (b))

typedef struct ObjModel {
    char pad_00[2];
    unsigned short link;
    Vec3 pos;               /* 0x04 */
    IVec3 rot;              /* 0x10 */
    Vec3 param_vec;               /* 0x1c */
    float param[6];           /* 0x28 */
} ObjModel;

typedef struct EnemyDesc {
    char pad_00[4];
    ActVtbl *vt;            /* 0x04 */
    ObjModel *model;        /* 0x08 */
    int init_arg;                /* 0x0c */
    int pnb;                /* 0x10 */
    int ptm;                /* 0x14 */
    unsigned char flags;      /* 0x18 */
    char pad_19[3];
    int dc_flags;                /* 0x1c */
    int model_no;                /* 0x20 */
    int model_no2;                /* 0x24 */
    int life;                /* 0x28 */
    float lock_max;              /* 0x2c */
    int score;                /* 0x30 */
    char pad_34[4];
    float out_dist;              /* 0x38 */
    char pad_3c[4];
    float time_limit;              /* 0x40 */
    float wob_amp;              /* 0x44 */
    float wob_spd;              /* 0x48 */
} EnemyDesc;

extern void (*enemy_task_state_fns[])(ActObj *o);
extern void (*enemy_task_die_fn)(ActObj *o);
extern void (*enemy_task_disp_fns[])(ActObj *o);
extern float enemy_task_fade;
extern void ulMatrixPush(void);

extern struct {
    int pad[4];
    char *name;
} enemy_task_desc;

static void enemy_task_wobble_init(ActObj *w, float amp, float spd);

static inline void VecSub(Vec3 *dst, Vec3 *src)
{
    dst->x -= src->x;
    dst->y -= src->y;
    dst->z -= src->z;
}

static inline void VecAdd(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void VecCopy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* Moves by the change in wobble offset: amp on X and Z, amp / 5 on Y. */
/* 0x00113cb8 */
static void enemy_task_wobble(ActObj *w)
{
    Vec3 ofs;
    float cs;
    float sn;
    float amp = w->amp;
    IVec3 *ang = &w->ang;
    IVec3 *ang_spd = &w->angspd;

    ang->x += ang_spd->x;
    ang->y += ang_spd->y;
    ang->z += ang_spd->z;
    ulSinCos(w->ang.x, &sn, &cs);
    ofs.x = cs * amp;
    ulSinCos(w->ang.y, &sn, &cs);
    ofs.y = sn * amp * 0.2f;
    ulSinCos(w->ang.z, &sn, &cs);
    ofs.z = cs * amp;
    VecSub(&w->pos, &w->off);
    VecAdd(&w->pos, &ofs);
    VecCopy(&w->off, &ofs);
}

/*
 * Lock flag 0x2000: takes the hit impulse; at zero life pops the score (if it has a status slot)
 * and goes to shot down.
 */
/* 0x00113e18 */
static void enemy_task_lock_update(ActObj *w)
{
    ActSub *lock = &w->lock;
    int shots;
    int slot;

    if (lock->flags & LOCK_HIT) {
        VecCopy(&w->hit_imp, &w->lock_imp);
        if (lock->life <= 0) {
            shots = lockon_hits_max_shot_cnt(lock);
            slot = w->status_slot;
            if (!(w->flags & 8))
                enemy_status_score_popup(slot, w->status_pos, w->score, shots);
            lock->flags &= ~LOCK_LOCKABLE;
            do {
                w->state = 3;
                w->count = 0;
            } while (0);
        }
    }
}

/* Player within out_dist: player takes 1 damage, enemy destroyed (flag 0x100, shot down). */
/* 0x00113ec8 */
static void enemy_task_range_check(ActObj *w)
{
    if (player_sphere_test(&w->pos, w->out_dist) == 1) {
        player_status_add_damage(1.0f);
        w->flags |= 0x100;
        do {
            w->state = 3;
            w->count = 0;
        } while (0);
    }
}

/* Callback per phase (exit when it asks for a new phase or state); collision with flag 0x40. */
/* 0x00113f30 */
static void enemy_task_state_normal(ActObj *w)
{
    switch ((unsigned int)w->phase) {
    case 0:
        w->state_time = 0;
        w->state_fn(w, enemy_task_work_ptr(w));
        w->phase++;
        w->count++;
    case 1:
        w->state_fn(w, enemy_task_work_ptr(w));
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        w->state_fn(w, enemy_task_work_ptr(w));
    }
    if (w->flags & 0x40)
        enemy_task_range_check(w);
    w->state_time += 1.0f;
}

/*
 * Entry: lockable unless flag 2; reported appeared unless flag 8 (later exit then counts as shot
 * down by default). Then normal.
 */
/* 0x00114038 */
static void enemy_task_state_appear(ActObj *w)
{
    switch ((unsigned int)w->phase) {
    case 0:
        w->state_time = 0;
        if (!(w->flags & 2))
            lockon_entry(&w->lock);
        if (!(w->flags & 8)) {
            enemy_status_event(w->status_slot, 2, w->status_pos);
            w->end_event = 3;
        }
        w->state_fn(w, enemy_task_work_ptr(w));
        w->phase++;
        w->count++;
    case 1:
        w->state_fn(w, enemy_task_work_ptr(w));
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        w->state_fn(w, enemy_task_work_ptr(w));
        do {
            w->state = 0;
            w->count = 0;
        } while (0);
    }
    w->state_time += 1.0f;
}

/* Entry: not lockable, reported escaped. Task ends after exit. */
/* 0x00114168 */
static void enemy_task_state_away(ActObj *w)
{
    switch ((unsigned int)w->phase) {
    case 0:
        w->state_time = 0;
        if (!(w->flags & 2))
            w->lock.flags &= ~LOCK_LOCKABLE;
        if (!(w->flags & 8)) {
            enemy_status_event(w->status_slot, 4, w->status_pos);
            w->end_event = 0;
        }
        w->state_fn(w, enemy_task_work_ptr(w));
        w->phase++;
        w->count++;
    case 1:
        w->state_fn(w, enemy_task_work_ptr(w));
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        w->state_fn(w, enemy_task_work_ptr(w));
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
    w->state_time += 1.0f;
}

/*
 * Entry: not lockable, reported shot down (event 5 if destroyed by hitting the player, flag 0x100,
 * else 3). Task ends after exit.
 */
/* 0x001142a8 */
static void enemy_task_state_die(ActObj *w)
{
    switch ((unsigned int)w->phase) {
    case 0:
        w->state_time = 0;
        if (!(w->flags & 2))
            w->lock.flags &= ~LOCK_LOCKABLE;
        if (!(w->flags & 8)) {
            if (w->flags & 0x100)
                enemy_status_event(w->status_slot, 5, w->status_pos);
            else
                enemy_status_event(w->status_slot, 3, w->status_pos);
            w->end_event = 0;
        }
        w->state_fn(w, enemy_task_work_ptr(w));
        w->phase++;
        w->count++;
    case 1:
        w->state_fn(w, enemy_task_work_ptr(w));
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        w->state_fn(w, enemy_task_work_ptr(w));
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
    w->state_time += 1.0f;
}

/* Appears on the next beat. */
/* 0x00114408 */
static void enemy_task_state_wait(ActObj *w)
{
    if (beat_grid_hit(0)) {
        do {
            w->state = 1;
            w->count = 0;
        } while (0);
    }
}

/*
 * Lock-on hits unless flag 2; moves with the player (flag 1: player_get_vec, else frame velocity).
 * Per tick: state, wobble (flag 4), time_limit report (normal, flag 0x80), enemy_task_die_fn.
 */
/* 0x00114440 */
static void enemy_task_exec(ActObj *w)
{
    void (*fn)(ActObj *, void *);
    Vec3 frame_vel;
    Vec3 move;
    int n;

    w->pstate = w->state;
    w->phase = w->count;
    do {
        fn = w->exec_fn;
        if (fn != 0)
            fn(w, enemy_task_work_ptr(w));
    } while (0);
    n = enemy_status_get_ticks();
    if (!(w->flags & 2))
        enemy_task_lock_update(w);
    if (w->flags & 1) {
        player_get_vec(&frame_vel);
        VecAdd(&w->pos, &frame_vel);
    } else {
        player_get_frame_vel(&move);
        VecAdd(&w->pos, &move);
    }
    while (n--) {
        void (*hook)(ActObj *);

        do { w->pstate = w->state; w->phase = w->count; } while (0);
        switch ((unsigned int)w->state) {
        case 0:
        case 1:
        case 2:
        case 4:
            hook = enemy_task_state_fns[w->state];
            if (hook != 0)
                hook(w);
            if (w->flags & 4)
                enemy_task_wobble(w);
            break;
        }
        w->time += 1.0f;
    }
    switch (w->pstate) {
    case 0:
        if (w->flags & 0x80) {
            if (!(w->flags & 8))
                enemy_status_ent_set_pos(w->status_slot, w->status_pos, (int)(w->time_limit - w->state_time) + 1);
        }
        break;
    case 3:
        if (enemy_task_die_fn != 0)
            enemy_task_die_fn(w);
        break;
    }
    fn = w->post_fn;
    if (fn != 0)
        fn(w, enemy_task_work_ptr(w));
}

/* 0x00114698 */
static void enemy_task_call_disp(ActObj *w)
{
    w->disp_fn(w, enemy_task_work_ptr(w));
}

/* State's display function, if any. */
/* 0x001146d0 */
static void enemy_task_disp(ActObj *w)
{
    void (*fn)(ActObj *) = enemy_task_disp_fns[w->pstate];

    if (fn != 0)
        fn(w);
}

/* Flag 1 grows, flag 2 fades in. */
/* 0x00114710 */
void enemy_task_set_appear(ActObj *w, int flags, int frames)
{
    w->appear_flags = flags;
    w->appear_frm = frames;
}

/* Flag 1 shrinks, flag 2 fades out. */
/* 0x00114720 */
void enemy_task_set_vanish(ActObj *w, int flags, int frames)
{
    w->vanish_flags = flags;
    w->vanish_frm = frames;
}

/* Appear flag 1: 0 to full size, quarter sine over appear_frm, then ends the phase. work unused. */
/* 0x00114730 */
void enemy_task_appear_scale(ActObj *w, void *work)
{
    float cs;
    float sn;

    switch ((unsigned int)w->phase) {
    case 0:
        if (w->appear_flags & 1) {
            Vec3 *scale = &w->scale;

            scale->x = 0.0f;
            scale->y = 0.0f;
            scale->z = 0.0f;
        }
        break;
    case 1:
        if (w->appear_flags & 1) {
            Vec3 *scale;

            ulSinCos((int)(w->state_time * ANG_QUARTER / (float)w->appear_frm), &sn, &cs);
            scale = &w->scale;
            scale->x = sn;
            scale->y = sn;
            scale->z = sn;
        }
        if ((float)w->appear_frm <= w->state_time)
            w->count = w->phase + 1;
        break;
    case 2:
        if (w->appear_flags & 1) {
            Vec3 *scale = &w->scale;

            scale->x = 1.0f;
            scale->y = 1.0f;
            scale->z = 1.0f;
        }
        break;
    }
}

/*
 * Vanish flag 1: full size to 0, quarter cosine over vanish_frm, then ends the phase. work unused.
 */
/* 0x00114840 */
void enemy_task_vanish_scale(ActObj *w, void *work)
{
    float cs;
    float sn;

    switch ((unsigned int)w->phase) {
    case 0:
        if (w->vanish_flags & 1) {
            Vec3 *scale = &w->scale;

            scale->x = 1.0f;
            scale->y = 1.0f;
            scale->z = 1.0f;
        }
        break;
    case 1:
        if (w->vanish_flags & 1) {
            Vec3 *scale;

            ulSinCos((int)((float)(ANG_90 / w->vanish_frm) * w->state_time), &sn, &cs);
            scale = &w->scale;
            scale->x = cs;
            scale->y = cs;
            scale->z = cs;
        }
        if ((float)w->vanish_frm <= w->state_time) {
            do {
                w->count = w->phase + 1;
            } while (0);
        }
        break;
    case 2:
        break;
    }
}

/* Fixed 18 frames, ignores the vanish flags. */
/* 0x00114928 */
static void enemy_task_vanish_scale_fixed(ActObj *w)
{
    float cs;
    float sn;
    Vec3 *scale;

    switch ((unsigned int)w->phase) {
    case 0:
        scale = &w->scale;
        scale->x = 1.0f;
        scale->y = 1.0f;
        scale->z = 1.0f;
        break;
    case 1:
        ulSinCos((int)(w->state_time * 910.22223f), &sn, &cs);
        scale = &w->scale;
        scale->z = scale->y = scale->x = cs;
        if (18.0f <= w->state_time) {
            do {
                w->count = w->phase + 1;
            } while (0);
        }
        break;
    case 2:
        break;
    }
}

/* Appear or escape state; enemy_task_fade as override colour. Skipped out of view. */
/* steer(cast) */
/* 0x001149e0 */
void enemy_task_draw_fade(ActObj *w)
{
    Vec3 *pos = &w->pos;
    int old_flags;

    if (!view_sphere_visible(pos, w->radius))
        return;
    old_flags = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ulGxSetTextureList(w->res[1]);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    if (w->pstate == 1)
        enemy_task_fade = w->state_time / (float)w->appear_frm + -1.0f;
    else if (w->pstate == 2)
        enemy_task_fade = -w->state_time / (float)w->vanish_frm;
    ((void (*)(unsigned int, int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(&enemy_task_fade);
    do {
        vu0_matrix_translate(ulMatrixCur, w->pos.x, pos->y, pos->z);
    } while (0);
    enemy_task_rotate_yxz(&w->rot);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (w->flags & 0x20)
        vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
    dc_object_draw(&w->gfx);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(old_flags);
}

/* Faded while appearing/vanishing with fade flag 2. Skipped out of view. */
/* 0x00114b68 */
static void enemy_task_disp_fade(ActObj *w)
{
    Vec3 *pos;

    if (w->pstate == 1 && (w->appear_flags & 2)) {
        enemy_task_draw_fade(w);
        return;
    }
    if (w->pstate == 2 && (w->vanish_flags & 2)) {
        enemy_task_draw_fade(w);
        return;
    }
    pos = &w->pos;
    if (view_sphere_visible(pos, w->radius) == 0)
        return;
    do {
        ulGxSetTextureList(w->res[1]);
    } while (0);
    ulMatrixPush();
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    enemy_task_rotate_yxz(&w->rot);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (w->flags & 0x20)
        vu0_matrix_scale(ulMatrixCur, w->scale.x, w->scale.y, w->scale.z);
    dc_object_draw(&w->gfx);
    ulMatrixPop();
}

/* Radius 10 view test. Returns 0. */
/* 0x00114c90 */
int obj_draw_at(int unused, ObjPlace *obj, void *model)
{
    Vec3 *pos = &obj->pos;

    ulMatrixPush();
    if (view_sphere_visible(pos, 10.0f)) {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        enemy_task_rotate_yxz(&obj->rot);
        nj_object_draw_shapes(model);
    }
    ulMatrixPop();
    return 0;
}

static inline void IVecCopy(IVec3 *dst, IVec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void VecSet(Vec3 *dst, float x, float y, float z)
{
    dst->x = x;
    dst->y = y;
    dst->z = z;
}

/*
 * Appear state; status slot unless flag 8; lock-on with life unless flag 2; wobble with flag 4;
 * default appear 30 and vanish 60 frames.
 */
/* 0x00114d18 */
static void enemy_task_init(ActObj *w, EnemyDesc *desc)
{
    ObjModel *model;
    ActGfx *gfx;
    int *ent;
    int *ent2;

    {
        int a = desc->pnb;
        int b = desc->ptm;

        w->res[0] = a;
        w->res[1] = b;
    }
    do { } while (0);
    w->state = 1;
    w->count = 0;
    {
        unsigned int flags = desc->flags | 0xe0;

        do { } while (0);
        w->time = 0;
        w->flags = flags;
    }
    if (!(w->flags & 8)) {
        w->status_slot = enemy_status_ent_create(desc->model, w);
    }
    model = desc->model;
    VecCopy(&w->pos, &model->pos);
    IVecCopy(&w->rot, &model->rot);
    VecCopy(&w->param_vec, &model->param_vec);
    w->end_event = 0;
    w->status_pos = (int)&w->pos;
    VecSet(&w->scale, 1.0f, 1.0f, 1.0f);
    if (!(w->flags & 2)) {
        int cnt = (int)MINF(MAXF(desc->lock_max, 1.0f), desc->life);

        lockon_obj_init(&w->lock, w, &w->pos, 0, 0, w->status_slot, desc->life, cnt, 0);
        model = desc->model;
    }
    VecSet(&w->hit_imp, 0, 0, 0);
    w->score = desc->score;
    w->link = model->link;
    w->time_limit = desc->time_limit;
    w->out_dist = desc->out_dist;
    {
        float prm0 = model->param[0];
        float prm1 = model->param[1];
        float prm2 = model->param[2];
        float prm3 = model->param[3];
        float prm4 = model->param[4];
        float prm5 = model->param[5];

        w->param[5] = prm5;
        w->param[4] = prm4;
        w->param[3] = prm3;
        w->param[2] = prm2;
        w->param[1] = prm1;
        w->param[0] = prm0;
    }
    if (w->flags & 4)
        enemy_task_wobble_init(w, desc->wob_amp, desc->wob_spd);
    gfx = &w->gfx;
    if (desc->model_no2 >= 0) {
        ent = pnb_get_entry(desc->pnb, desc->model_no);
        ent2 = pnb_get_entry(desc->pnb, desc->model_no2);
        dc_object_init(gfx, ent[0], ent2[0], desc->dc_flags);
    } else {
        ent = pnb_get_entry(desc->pnb, desc->model_no);
        dc_object_init(gfx, ent[0], 0, desc->dc_flags);
    }
    if (w->flags & 0x10)
        dc_object_rebuild(gfx);
    w->appear_frm = 30;
    w->appear_flags = 1;
    w->vanish_flags = 1;
    w->vanish_frm = 60;
    *(ActVtbl *)&w->init_fn = *desc->vt;
    if (desc->init_arg)
        w->init_fn(w, enemy_task_work_ptr(w), desc->model, desc->init_arg);
    else
        w->init_fn(w, enemy_task_work_ptr(w), desc->model);
}

/* End event unless flag 8; lock-on removal unless flag 2. */
/* 0x00115060 */
static void enemy_task_release(ActObj *w)
{
    w->release_fn(w, enemy_task_work_ptr(w));
    if (!(w->flags & 8))
        enemy_status_event(w->status_slot, w->end_event, w->status_pos);
    if (!(w->flags & 2))
        lockon_remove(&w->lock);
    dc_object_release(&w->gfx);
}

/* Shared descriptor takes the type's name; work = common data + desc->size bytes. */
/* 0x001150d0 */
int enemy_task_task_create(void *parent, ObjDesc *desc)
{
    enemy_task_desc.name = desc->name;
    return task_create_inherit_arg(parent, &enemy_task_desc, desc->size + 380, desc);
}

/* Motion time = position in the bar: animates to the music. */
/* 0x00115108 */
static void enemy_task_gfx_scale(ActObj *w)
{
    ActGfx *gfx = &w->gfx;

    gfx->time = beat_clock_get_bar() * gfx->length;
}

/* World transforms of the nodes in mask (at most max). */
/* 0x00115138 */
void enemy_task_spawn(ActObj *w, int mask, int max, int out_a, int out_b, int out_c)
{
    node_tree_collect_xforms_at(w->gfx.root, w->gfx.clip, w->gfx.time, mask, max, &w->pos, &w->rot, &w->scale, out_a, out_b, out_c);
}

/* 0x00115188 */
void enemy_task_get_pos(ActObj *w, Vec3 *out)
{
    VecCopy(out, &w->pos);
}

/* 0x001151a8 */
void enemy_task_get_rot(ActObj *w, int *out)
{
    IVecCopy((IVec3 *)out, &w->rot);
}

/* 0x001151c8 */
void enemy_task_get_param_vec(ActObj *w, Vec3 *out)
{
    VecCopy(out, &w->param_vec);
}

/* 0x001151e8 */
void enemy_task_get_scl(ActObj *w, Vec3 *out)
{
    VecCopy(out, &w->scale);
}

/* 0x00115208 */
int enemy_task_get_pstate(ActObj *w)
{
    return w->pstate;
}

/* 0 entry, 1 running, 2 exit. */
/* 0x00115210 */
int enemy_task_get_phase(ActObj *w)
{
    return w->phase;
}

/* Ticks. */
/* 0x00115218 */
float enemy_task_get_time(ActObj *w)
{
    return w->time;
}

/* Ticks. */
/* 0x00115220 */
float enemy_task_get_state_time(ActObj *w)
{
    return w->state_time;
}

/* i 0..5. */
/* 0x00115228 */
float enemy_task_get_param(ActObj *w, int i)
{
    return w->param[i];
}

/* 0x00115238 */
Vec3 * enemy_task_pos_ptr(ActObj *w)
{
    return &w->pos;
}

/* 0x00115240 */
IVec3 * enemy_task_rot_ptr(ActObj *w)
{
    return &w->rot;
}

/* 0x00115248 */
Vec3 * enemy_task_param_vec_ptr(ActObj *w)
{
    return &w->param_vec;
}

/* 0x00115250 */
Vec3 * enemy_task_scl_ptr(ActObj *w)
{
    return &w->scale;
}

/* Passed to every type callback. */
/* 0x00115258 */
void * enemy_task_work_ptr(ActObj *w)
{
    return &w->work;
}

/* 0x00115260 */
void *enemy_task_gfx_ptr(ActObj *w)
{
    return &w->gfx;
}

/* 0x00115268 */
ActSub * enemy_task_lock_ptr(ActObj *w)
{
    return &w->lock;
}

/* 0x00115270 */
int enemy_task_get_status_slot(ActObj *w)
{
    return w->status_slot;
}

/* 0x00115278 */
int enemy_task_get_link(ActObj *w)
{
    return w->link;
}

/* Last lock-on hit. */
/* 0x00115280 */
void enemy_task_get_hit_imp(ActObj *w, Vec3 *out)
{
    VecCopy(out, &w->hit_imp);
}

/* 0x001152a0 */
void enemy_task_set_pos(ActObj *w, Vec3 *pos)
{
    VecCopy(&w->pos, pos);
}

/* 0x001152c0 */
void enemy_task_set_rot(ActObj *w, int *rot)
{
    IVecCopy(&w->rot, (IVec3 *)rot);
}

/* 0x001152e0 */
void enemy_task_set_param_vec(ActObj *w, Vec3 *vec)
{
    VecCopy(&w->param_vec, vec);
}

/* 0x00115300 */
void enemy_task_set_scl(ActObj *w, Vec3 *scale)
{
    VecCopy(&w->scale, scale);
}

/* Entry phase. */
/* 0x00115320 */
void enemy_task_set_state(ActObj *w, int state)
{
    w->state = state;
    w->count = 0;
}

/* Running -> exit. */
/* 0x00115330 */
void enemy_task_next_phase(ActObj *w)
{
    w->count = w->phase + 1;
}

/* 0x00115340 */
static void enemy_task_set_param(ActObj *w, int i, float v)
{
    w->param[i] = v;
}

/* Only 0x20, 0x40, 0x80 change after creation. */
/* 0x00115350 */
static void enemy_task_set_flags(ActObj *w, int bits)
{
    w->flags |= bits & 0xe0;
}

/* Only 0x20, 0x40, 0x80 change after creation. */
/* 0x00115368 */
void enemy_task_clear_flags(ActObj *w, int bits)
{
    w->flags &= ~(bits & 0xe0);
}

/* Random start angles; random speeds up to 2 degrees per tick * spd. */
/* 0x00115380 */
static void enemy_task_wobble_init(ActObj *w, float amp, float spd)
{
    Vec3 *off = &w->off;

    off->x = 0;
    off->y = 0;
    off->z = 0;
    w->amp = amp;
    w->ang.x = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f);
    w->ang.y = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f);
    w->ang.z = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f);
    w->angspd.x = (int)((float)(int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f) * spd);
    w->angspd.y = (int)((float)(int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f) * spd);
    w->angspd.z = (int)((float)(int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f) * spd);
}

/* Reported when the task ends. */
/* 0x00115528 */
void enemy_task_set_end_event(ActObj *w, int event)
{
    w->end_event = event;
}

/* Default: own position. */
/* 0x00115530 */
void enemy_task_set_status_pos(ActObj *w, int pos)
{
    w->status_pos = pos;
}

/* Y, X, Z order. */
/* 0x00115538 */
void enemy_task_rotate_yxz(IVec3 *rot)
{
    vu0_matrix_rotate_y(ulMatrixCur, rot->y);
    vu0_matrix_rotate_x(ulMatrixCur, rot->x);
    vu0_matrix_rotate_z(ulMatrixCur, rot->z);
}
