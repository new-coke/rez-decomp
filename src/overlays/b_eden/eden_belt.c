/*
 * Eden boss belts: two spinning bars with a lockable part at both ends; they swing around the
 * boss, fire BB bullets on a timer and send a ring on the beat. As ends are shot off, the manager
 * restores the hits of the ends left and moves the shot timing to the next level.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "debug.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_belt.h"
#include "eden.h"
#include "eden_bullet.h"
#include "eden_effect.h"
#include "eden_snake.h"
#include "../common/boss_common.h"

extern char eden_belt_vardef[];
extern GwVarDef eden_belt_vardefs[];
extern struct GwVarNode *eden_belt_vars_group;
extern float cmd_belt_break_t;
extern int cm_belt_at_time[];
extern unsigned int cm_belt_po_col;
extern float cm_belt_y_rot_time[];
extern float cmd_test_y1;
extern float cmd_test_y2;
extern float cmd_test_r;
extern struct TaskDesc eden_belt_man_desc;
extern char str_eden_belt_c[];
extern int cmd_belt_num;
extern char str_eden_belt_man_hp_fmt[];
extern char str_eden_belt_man_mode_fmt[];
extern int cmd_belt_disp;
extern int cmd_belt_hp;
extern float eden_belt_fade[4];
extern unsigned int cm_belt_wire_col[2];
extern float cmd_belt_sh_r, cmd_belt_sh_t;
extern float eden_belt_shot_secs[];
extern struct TaskDesc eden_belt_desc;
extern float cmd_belt_r1, cmd_belt_r2, cmd_belt_ax, cmd_belt_bank_tz;
extern const float cmd_belt_bank_tx;
extern float cmd_belt_az;
extern float cmd_belt_ay;
extern float cmd_belt_frr, cmd_belt_bank_rr, cmd_belt_act, cmd_ring_rrate, cmd_ring_lw;
extern const float cmd_ring_rfrm;
extern unsigned int cm_ring_col;

/* one of the two belts: a swinging rod with a lockable end at each side, 332 bytes */
typedef struct EdenBeltWork {
    struct PnbHead *pack;
    void *tex_list;
    int parts;
    Vec3 pos;
    float swing[3];             /* radians: bank, turn, roll */
    float swing_amp[3];         /* degrees */
    float swing_spd[3];         /* radians per frame */
    int ang[3];
    float r;
    float spd;
    float bank;
    float bank_to;
    char pad_58[8];
    int idx;                    /* 0x60 */
    int ring_made;              /* a ring was made on this beat */
    int shot_state;
    int shot_time;
    int shot_wait;
    int shot_cnt;
    int shot_num;
    int active;
    int act_time;
    int act_len;
    Vec3 ends[2];
    LockObj lock[2];
    ModelPart *model[8];        /* end 0/1 break, shine, wire, body */
    int on[2];
    struct EdenBeltManWork *man;
} EdenBeltWork;

/* eden_belt_create copies the part pointers as ints */
typedef struct EdenBeltWorkModelView {
    char pad_00[288];
    int model[8];               /* 0x120 */
} EdenBeltWorkModelView;

/* 56 bytes */
typedef struct EdenBeltManWork {
    struct PnbHead *pack;
    void *tex_list;
    int parts;
    EdenBeltWork **belts;
    int nbelt;
    int on[4];
    int hp;
    int mode;
    int non;
    int time;
    int wait;
} EdenBeltManWork;

/* 0x00ca4330 */
void eden_belt_vars_add(void)
{
    eden_belt_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_belt_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_belt_vars_group, eden_belt_vardefs);
}

/* 0x00ca4380 */
void eden_belt_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_belt_vardef);
}

/* 0x00ca4398 */
void eden_belt_swing_update(EdenBeltWork *w, float dt)
{
    float sc[4];

    w->swing[0] += w->swing_spd[0] * dt;
    if (w->swing[0] >= TWO_PI)
        w->swing[0] -= TWO_PI;
    w->swing[1] += w->swing_spd[1] * dt;
    if (w->swing[1] >= TWO_PI)
        w->swing[1] -= TWO_PI;
    w->swing[2] += w->swing_spd[2] * dt;
    if (w->swing[2] >= TWO_PI)
        w->swing[2] -= TWO_PI;
    ulSinCosf(w->swing[0], &sc[0], &sc[1]);
    w->ang[1] = 0;
    w->ang[0] = DEG_TO_ANG(w->swing_amp[0] * w->bank * sc[0]);
    ulSinCosf(w->swing[2], &sc[2], &sc[3]);
    w->ang[2] = DEG_TO_ANG(w->swing_amp[2] * w->bank * sc[2]);
    angles_turn_y(w->ang, w->swing[1] * ANG_PER_RAD, w->ang);
}

/* 0x00ca44e0 */
void eden_belt_rod_ends(EdenBeltWork *w)
{
    Vec3 v;

    v.x = v.y = 0.0f;
    v.z = -w->r;
    vec_rotate_zxy(&v, w->ang);
    v3_sum(&w->ends[0], &v, &w->pos);
    v.x = v.y = 0.0f;
    v.z = w->r;
    vec_rotate_zxy(&v, w->ang);
    v3_sum(&w->ends[1], &v, &w->pos);
}

/* 0x00ca45c8 */
void eden_belt_shot_step(EdenBeltWork *w)
{
    Vec3 v;
    int ang[3];
    int z[3];
    int k;

    switch (w->shot_state) {
    case 0:
        if (--w->shot_time > 0)
            return;
        w->shot_state = 1;
        w->shot_cnt = w->shot_num;
        w->shot_time = 0;
        break;
    case 1:
        if (--w->shot_time > 0)
            return;
        if (w->on[1]) {
            v.z = w->r;
            v.x = v.y = 0.0f;
            vec_rotate_zxy(&v, w->ang);
            v3_sum(&v, &w->pos, &v);
            eden_bullet_bb_create(w, &v, w->ang, w->parts);
            eden_effect_dshine_ex_create(w, (int)&w->ends[1], (int)(cmd_belt_sh_t * FPS), cmd_belt_sh_r);
        }
        if (w->on[0]) {
            v.z = w->r;
            v.x = v.y = 0.0f;
            angles_turn_y(w->ang, ANG_180, ang);
            vec_rotate_zxy(&v, ang);
            v3_sum(&v, &w->pos, &v);
            eden_bullet_bb_create(w, &v, ang, w->parts);
            eden_effect_dshine_ex_create(w, (int)&w->ends[0], (int)(cmd_belt_sh_t * FPS), cmd_belt_sh_r);
        }
        if (w->on[0] || w->on[1]) {
            z[0] = 0;
            z[1] = 0;
            z[2] = 0;
            boss_se_play(25, 0, (int)z);
        }
        if (--w->shot_cnt <= 0) {
            w->shot_state = 0;
            w->shot_time = w->shot_wait;
        } else {
            k = boss_get_level();
            w->shot_time = (int)(eden_belt_shot_secs[k] * FPS);
        }
        break;
    }
}

/* 0x00ca4820 */
void eden_belt_set_level(EdenBeltWork *w, int i)
{
    w->shot_time = w->shot_wait = cm_belt_at_time[i * 2];
    w->shot_cnt = w->shot_num = cm_belt_at_time[i * 2 + 1];
}

/* steer(cast): (LockObj *)&w->lock indexes base before index */
/* 0x00ca4858 */
void eden_belt_exec(EdenBeltWork *w)
{
    int i, n;
    Vec3 z;

    if (cmd_belt_disp) {
        if (!w->idx)
            w->r = cmd_belt_r1;
        else
            w->r = cmd_belt_r2;
    }
    w->bank = w->bank * cmd_belt_bank_rr + w->bank_to * (1.0f - cmd_belt_bank_rr);
    eden_belt_swing_update(w, w->spd);
    eden_belt_rod_ends(w);
    if (eden_test_flags(4)) {
        if (w->on[0] || w->on[1]) {
            if (beat_grid_hit(0)) {
                if (!w->ring_made) {
                    n = (int)(beat_clock_get_haku() * cmd_ring_rfrm);
                    w->ring_made = 1;
                    cm_circle2_set_3d(cm_circle2_create(w, &w->pos, cm_ring_col, 24, n <= 17 ? 18 : n, w->r, cmd_ring_lw), w->ang, 1, cmd_ring_rrate);
                }
            } else
                w->ring_made = 0;
        }
        if (!w->active) {
            w->act_time = w->act_len = (int)(cmd_belt_act * FPS);
            w->active = 1;
        } else if (w->act_time)
            w->act_time--;
        w->spd = w->spd * cmd_belt_frr + (1.0f - cmd_belt_frr);
        eden_belt_shot_step(w);
        if (w->on[0])
            w->lock[0].flags |= LOCK_LOCKABLE;
        if (w->on[1])
            w->lock[1].flags |= LOCK_LOCKABLE;
    } else {
        w->spd *= cmd_belt_frr;
        w->active = 0;
        if (w->on[0])
            w->lock[0].flags &= ~LOCK_LOCKABLE;
        if (w->on[1])
            w->lock[1].flags &= ~LOCK_LOCKABLE;
    }
    for (i = 0; i < 2; i++) {
        if (!(((LockObj *)&w->lock)[i].flags & LOCK_HIT))
            continue;
        boss_se_play(23, (int)&((LockObj *)&w->lock)[i], 0);
        if (((LockObj *)&w->lock)[i].life > 0)
            continue;
        if (eden_belt_man_get_mode(w->man)) {
            boss_se_play(24, (int)&((LockObj *)&w->lock)[i], 0);
            lock_score(7, &((LockObj *)&w->lock)[i]);
            ((void (*)(EdenBeltManWork *, int, int, int))eden_belt_man_hits_apply)(w->man, w->idx, i, -1); /* steer(cast) */
            w->on[i] = 0;
            z.x = 0;
            z.y = 0;
            z.z = 0;
            cm_wire_mdl_break_part(w, 2, w->model[i], &w->pos, &z, w->ang, cm_belt_po_col);
        } else
            ((void (*)(EdenBeltManWork *, int, int, int))eden_belt_man_hits_apply)(w->man, w->idx, i, -1); /* steer(cast) */
    }
}

/* steer(cast) */
/* 0x00ca4bc8 */
void eden_belt_debug_draw(EdenBeltWork *belt)
{
    Vec3 z;
    Vec3 v;

    ulMatrixPush();
    MTX_TRANS(&belt->pos);
    eden_belt_rotate_yxz(belt->ang);
    z.x = 0;
    z.y = 0;
    z.z = 0;
    circle_draw_flat(&z, 0x80800000, belt->r);
    ulMatrixPop();
    v.x = v.z = 0.0f;
    v.y = cmd_test_y1;
    circle_draw_flat(&v, 0x80800000, cmd_test_r);
    v.y = cmd_test_y2;
    circle_draw_flat(&v, 0x80000080, cmd_test_r);
}

/* steer(cast) */
/* 0x00ca4c90 */
void eden_belt_disp(EdenBeltWork *w)
{
    float t;
    int old, list;

    ulGxSetTextureList(w->tex_list);
    t = eden_get_dalpha();
    if (fabsf(t) < FLT_EPSILON)
        return;
    old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    eden_belt_fade[0] = t + -1.0f;
    draw_set_override_color(eden_belt_fade);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    eden_belt_rotate_yxz(w->ang);
    if (w->active) {
        list = RenderSetCurrentDisplayList(4);
        if (w->on[0])
            dc_object_draw(w->model[2]);
        if (w->on[1])
            dc_object_draw(w->model[3]);
        RenderSetCurrentDisplayList(list);
        draw_set_color(color_lerp(cm_belt_wire_col[0], cm_belt_wire_col[1], beat_curve(1)));
        draw_set_blend(4, 1);
        if (w->on[0])
            dc_object_draw(w->model[4]);
        if (w->on[1])
            dc_object_draw(w->model[5]);
    } else {
        if (w->on[0])
            dc_object_draw(w->model[6]);
        if (w->on[1])
            dc_object_draw(w->model[7]);
    }
    draw_push_override_color();
    draw_push_strip_flags();
    draw_set_override_flags(old);
    ulMatrixPop();
    if (cmd_belt_disp)
        eden_belt_debug_draw((char *)w);
}

/* 0x00ca4e78 */
void eden_belt_release(EdenBeltWork *w)
{
    lock_remove_entry(7, w->lock);
    lock_remove_entry(7, &w->lock[1]);
}

/* 0x00ca4eb0 */
EdenBeltWork *eden_belt_create(void *parent, int arg, int i)
{
    EdenBeltWork *w = task_create_inherit(parent, &eden_belt_desc, 332);
    int *tbl = (int *)arg;
    EdenBeltWorkModelView *view = (EdenBeltWorkModelView *)w; /* steer(alias) */

    w->swing_amp[0] = cmd_belt_ax;
    w->bank = w->bank_to = 1.0f;
    w->swing_amp[1] = cmd_belt_ay;
    w->swing_amp[2] = cmd_belt_az;
    w->swing_spd[0] = TWO_PI / (cmd_belt_bank_tx * FPS);
    w->swing_spd[1] = TWO_PI / (cm_belt_y_rot_time[0] * FPS);
    w->swing_spd[2] = TWO_PI / (cmd_belt_bank_tz * FPS);
    w->man = parent;
    w->parts = arg;
    w->idx = i;
    w->swing[1] = w->swing[0] = (float)((i + 1) % 4) * 1.5707964f;
    w->swing[2] = (float)((i + 2) % 4) * 1.5707964f;
    if (!i) {
        w->r = cmd_belt_r1;
        view->model[0] = tbl[35];
        view->model[1] = tbl[31];
        view->model[2] = tbl[30];
        view->model[3] = tbl[34];
        view->model[4] = tbl[32];
        view->model[5] = tbl[36];
        view->model[6] = tbl[33];
        view->model[7] = tbl[37];
    } else {
        w->r = cmd_belt_r2;
        view->model[0] = tbl[43];
        view->model[1] = tbl[39];
        view->model[2] = tbl[38];
        view->model[3] = tbl[42];
        view->model[4] = tbl[40];
        view->model[5] = tbl[44];
        view->model[6] = tbl[41];
        view->model[7] = tbl[45];
    }
    w->on[0] = w->on[1] = 1;
    lock_init(7, &w->lock[0], w, &w->ends[0], 0, cmd_belt_hp, 0);
    lockon_entry(&w->lock[0]);
    lock_init(7, &w->lock[1], w, &w->ends[1], 0, cmd_belt_hp, 0);
    lockon_entry(&w->lock[1]);
    eden_belt_swing_update(w, 1.0f);
    eden_belt_rod_ends(w);
    eden_belt_set_level(w, 0);
    unit_circle_build();
    return w;
}

/* 0x00ca5150 */
void eden_belt_set_hits(EdenBeltWork *w, int i, int hits)
{
    ((LockObj *)&w->lock)[i].life = hits; /* steer(cast) */
}

/* 0x00ca5160 */
int eden_belt_get_hits(EdenBeltWork *w, int i)
{
    return ((LockObj *)&w->lock)[i].life; /* steer(cast) */
}

/* 0x00ca5170 */
void eden_belt_set_spd(EdenBeltWork *w, float spd)
{
    w->bank_to = spd;
}

/* 0x00ca5178 */
void eden_belt_set_period(EdenBeltWork *w, float t)
{
    w->swing_spd[1] = TWO_PI / (t * FPS);
}

/* 0x00ca51a8 */
void eden_belt_part_break(EdenBeltWork *w, int i)
{
    Vec3 v;

    w->on[i] = 0;
    v.x = 0;
    v.y = 0;
    v.z = 0;
    cm_wire_mdl_break_part(w, 2, w->model[i], &w->pos, &v, w->ang, cm_belt_po_col);
    ((LockObj *)&w->lock)[i].life = 0; /* steer(cast) */
}

/* 0x00ca5220 */
void eden_belt_man_exec(EdenBeltManWork *w)
{
    int *hit = w->on;
    int n = 0;
    int i, k;

    for (i = 0; i < 4; i++)
        if (hit[i])
            n++;
    k = 4;
    if (n < 2) {
        eden_belt_set_spd(w->belts[0], 0.0f);
        eden_belt_set_spd(w->belts[1], 0.0f);
    }
    k -= n;
    if (n != w->non) {
        eden_belt_set_level(w->belts[0], k);
        eden_belt_set_level(w->belts[1], k);
        w->non = n;
    }
    w->hp = cmd_belt_hp;
    if (eden_test_flags(4) && w->wait && w->mode) {
        if (--w->time <= 0) {
            for (i = 0; i < 4; i++) {
                if (hit[i]) {
                    hit[i] = 0;
                    eden_belt_part_break(w->belts[i / 2], i % 2);
                    w->wait = 0;
                    w->mode = 0;
                    break;
                }
            }
        }
    }
}

/* 0x00ca5388 */
void eden_belt_man_release(EdenBeltManWork *w)
{
    heap_free(w->belts, 0);
}

/* 0x00ca5398 */
void eden_belt_man_disp(EdenBeltManWork *w)
{
    int i, j;

    if (!debug_flags_test(4))
        return;
    if (!cmd_belt_disp)
        return;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            int hp = eden_belt_get_hits(w->belts[i], j);

            debug_set_origin(10, i * 2 + 10 + j);
            debug_printf(str_eden_belt_man_hp_fmt, hp);
        }
    }
    debug_set_origin(10, i * 2 + 10 + j);
    debug_printf(str_eden_belt_man_mode_fmt, w->mode);
}

/* 0x00ca54b8 */
int *eden_belt_man_create(void *parent, int arg)
{
    EdenBeltManWork *w = task_create_inherit(parent, &eden_belt_man_desc, 56);
    int i;
    int *p;

    w->parts = arg;
    w->nbelt = cmd_belt_num;
    w->belts = heap_block_tag(game_calloc(w->nbelt * 4), str_eden_belt_c, 568);
    for (i = 0; i < w->nbelt; i++)
        w->belts[i] = eden_belt_create(w, w->parts, i);
    w->non = 4;
    for (i = 0, p = w->on; i < w->non; i++)
        *p++ = 1;
    w->mode = 1;
    return (int *)w;
}

/* 0x00ca55b8 */
void eden_belt_man_hits_apply(EdenBeltManWork *w, int i, int j)
{
    int row, side;

    if (w->mode)
        w->on[i * 2 + j] = 0;
    for (row = 0; row < w->nbelt; row++)
        for (side = 0; side < 2; side++)
            if (w->on[row * 2 + side])
                eden_belt_set_hits(w->belts[row], side, w->hp);
    w->mode = 0;
}

/* 0x00ca5690 */
int eden_belt_man_get_mode(EdenBeltManWork *w)
{
    return w->mode;
}

/* 0x00ca5698 */
void eden_belt_man_set_mode(EdenBeltManWork *w, int mode)
{
    int k;
    int i;

    w->mode = mode;
    k = (w->nbelt * 2 - w->non + 4) % 4;
    for (i = 0; i < w->nbelt; i++)
        eden_belt_set_period(w->belts[i], cm_belt_y_rot_time[k]);
}

/* 0x00ca5750 */
void eden_belt_man_set_wait(EdenBeltManWork *w, int wait)
{
    w->time = cmd_belt_break_t * FPS;
    w->wait = wait;
}

/* 0x00ca5778 */
void eden_belt_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
