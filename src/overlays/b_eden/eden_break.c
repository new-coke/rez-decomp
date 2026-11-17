/*
 * Eden boss break-up effects: a model's nodes drift or fall away piece by piece, then fade.
 * eden_break_d_create is the end-of-fight version, with its own camera and a fog fade.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_break.h"
#include "eden.h"
#include "../common/boss_common.h"

extern char eden_break_vardef[];
extern GwVarDef eden_break_vardefs[];
extern struct GwVarNode *eden_break_vars_group;
extern struct TaskDesc eden_break_desc;
extern char str_eden_break_c[];


extern float eden_break_fade[4];

/* broken model piece, 64 bytes: waits, then drifts or falls */
typedef struct EdenDrop {
    Vec3 pos;
    Vec3 vel;                   /* 0x0c: damped by cmd_st_vr, added to pos */
    int ang[3];                 /* 0x18 */
    int dang[3];                /* 0x24 */
    int model;
    int timer;                  /* 0x34: frames before falling */
    int fall_frm;
    int state;                  /* 0x3c: 0 waiting, 1 falling, 2 gone */
} EdenDrop;

/* a form breaking into falling pieces, 112 bytes */
typedef struct EdenBreakWork {
    struct PnbHead *pack;
    void *tex_list;
    void *parent;
    BossCameraWork *cam;
    int kind;
    EdenDrop *drops;
    char pad_18[12];
    Vec3 cam_target;            /* 0x24 */
    char pad_30[24];
    int state;                  /* 0x48 */
    int lvl;
    int ndrop;
    int time;
    int len;
    WireObj *models[5];
} EdenBreakWork;

extern const float eden_break_fall_g;
extern const float cmd_fall_vlim;
extern const float cmd_fall_vlim_r;
extern const float cmd_fall_bottom;
extern float eden_break_d_fade[4];
extern float cmd_fall_t;
extern float cmd_fall_dt;
extern float cmd_fall_dtl;
extern float cmd_fall_dr;
extern struct TaskDesc eden_break_d_desc;
extern unsigned int eden_break_d_models[5];
extern unsigned int eden_break_d_parts[5];
extern float cmd_fall_fade_t;
extern float eden_break_fog_t;
extern unsigned int cm_break_fog_col;
extern float cmd_fall_t2, cmd_fall_dt2, cmd_st_v, cmd_st_dv;
extern const float cmd_fall_top, cmd_fall_g2, cmd_st_vr, cmd_fall_fst2, cmd_fall_ft2;

/* 0x00ca57c8 */
void eden_break_vars_add(void)
{
    eden_break_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_break_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_break_vars_group, eden_break_vardefs);
}

/* 0x00ca5818 */
void eden_break_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_break_vardef);
}

/* 0x00ca5830 */
int eden_break_count_alive(WireObj *w)
{
    WireObj *node = w->child;
    int n = 0;

    while (node) {
        if (node->model)
            n++;
        node = node->sibling;
    }
    return n;
}

/* 0x00ca5868 */
EdenDrop *eden_break_fall_start(EdenDrop *drop, WireObj *src, int lvl)
{
    Vec3 base;
    int n;

    v3_copy(&base, (Vec3 *)src->pos);
    n = eden_break_count_alive(src);
    for (src = src->child; n; n--) {
        if (src->model) {
            v3_sum(&drop->pos, &base, (Vec3 *)src->pos);
            drop->model = (int)src->model;
            drop->timer = (int)((cmd_fall_t + drop->pos.y * cmd_fall_dt + cmd_fall_dtl * (float)lvl) * FPS);
            drop->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            drop->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            drop->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            drop++;
            do {
                src = src->sibling;
            } while (0);
        }
    }
    return drop;
}

/* 0x00ca5a58 */
void eden_break_fall_step(EdenBreakWork *w)
{
    int i;
    int live = 0;
    EdenDrop *drop = w->drops;

    for (i = 0; i < w->ndrop; i++, drop++) {
        switch (drop->state) {
        case 0:
            if (--drop->timer <= 0)
                drop->state = 1;
            live++;
            break;
        case 1:
            drop->vel.y += eden_break_fall_g;
            if (drop->vel.y < -cmd_fall_vlim)
                drop->vel.y = drop->vel.y * cmd_fall_vlim_r - cmd_fall_vlim * (1.0f - cmd_fall_vlim_r);
            drop->pos.y += drop->vel.y;
            i3_add(drop->ang, drop->dang);
            if (drop->pos.y < cmd_fall_bottom)
                drop->state++;
            live++;
            break;
        case 2:
            break;
        }
    }
    if (!live)
        FINISH_IF(w);
}

/* 0x00ca5ba0 */
void eden_break_exec(EdenBreakWork *w)
{
    switch (w->state) {
    case 0:
        if (--w->time <= 0)
            w->state = 1;
        break;
    case 1:
        eden_break_fall_step(w);
        break;
    }
}

/* 0x00ca5be0 */
void eden_break_pieces_draw(EdenBreakWork *w)
{
    EdenDrop *drop = w->drops;
    int i;

    for (i = 0; i < w->ndrop; i++, drop++) {
        if (drop->state < 2) {
            ulMatrixPush();
            MTX_TRANS(&drop->pos);
            eden_break_rotate_yxz(drop->ang);
            model_draw_culled((void *)drop->model);
            ulMatrixPop();
        }
    }
}

/* 0x00ca5c80 */
void eden_break_disp(EdenBreakWork *w)
{
    int old;
    float k;

    ulGxSetTextureList(w->tex_list);
    switch (w->state) {
    case 0:
        k = -1.0f;
        old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        eden_break_fade[0] = (float)w->time / (float)w->len + k;
        draw_set_override_color(eden_break_fade);
        tree_draw_bake_w(w->models[w->lvl]);
        eden_break_fade[0] = k - eden_break_fade[0];
        draw_set_override_color(eden_break_fade);
        eden_break_pieces_draw(w);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(old);
        break;
    case 1:
        old = draw_set_override_flags(0);
        eden_break_pieces_draw(w);
        draw_set_override_flags(old);
        break;
    }
}

/* 0x00ca5dc0 */
void eden_break_release(EdenBreakWork *w)
{
    heap_free(w->drops, 0);
}

/* steer(cast) */
/* 0x00ca5dd0 */
void *eden_break_d_create(void *parent, int kind, int lvl)
{
    char *w;
    EdenBreakWork *brk;
    EdenDrop *drop;
    unsigned int i;
    BossFog fog;

    if (lvl <= 0)
        return 0;
    w = task_create_inherit(parent, &eden_break_d_desc, 112);
    brk = (EdenBreakWork *)w;
    brk->parent = parent;
    brk->kind = kind;
    brk->lvl = lvl - 1;
    for (i = 0; i < 5; i++)
        brk->models[i] = pnb_get_entry(brk->pack, eden_break_d_parts[i % 5])->src;
    brk->ndrop = eden_break_count_alive(pnb_get_entry(brk->pack, eden_break_d_models[(unsigned int)brk->lvl % 5])->src);
    drop = heap_block_tag(game_calloc(brk->ndrop << 6), str_eden_break_c, 303);
    brk->drops = drop;
    eden_break_fall_start(drop, pnb_get_entry(brk->pack, eden_break_d_models[(unsigned int)brk->lvl % 5])->src, brk->lvl);
    brk->time = brk->len = (int)(cmd_fall_fade_t * FPS);
    brk->cam = boss_camera_create(brk, 1.0f, 0, 0, &brk->cam_target, 88, 3, 120, 1);
    boss_camera_look(brk->cam, 1200);
    boss_camera_set_target(brk->cam, &brk->cam_target);
    fog.near = fog_get_near();
    fog.far = fog_get_far();
    fog.back = fog.col = cm_break_fog_col;
    eden_fog_create(w, &fog, eden_break_fog_t);
    return w;
}

/* 0x00ca5ff0 */
void *eden_break_d_fall_start(EdenDrop *drop, void *node)
{
    WireObj *obj = node;
    Vec3 base;
    int n;

    v3_copy(&base, (Vec3 *)obj->pos);
    n = eden_break_count_alive(obj);
    for (obj = obj->child; n; n--) {
        if (obj->model) {
            Vec3 *v = &drop->vel;
            float k;

            v3_sum(&drop->pos, &base, (Vec3 *)obj->pos);
            drop->model = (int)obj->model;
            drop->timer = (int)((cmd_fall_t2 + drop->pos.y * cmd_fall_dt2) * FPS);
            do {
                drop->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            } while (0);
            drop->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            do {
                drop->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_fall_dr + cmd_fall_dr) - cmd_fall_dr);
            } while (0);
            v->x = drop->pos.x;
            v->y = 0.0f;
            v->z = drop->pos.z;
            vec3_normalize(v, v);
            k = RAND_FLOAT_A() * ((cmd_st_v + cmd_st_dv) - cmd_st_v) + cmd_st_v;
            v->x *= k;
            v->y *= k;
            v->z *= k;
            drop++;
            obj = obj->sibling;
        }
    }
    return drop;
}

/* 0x00ca6258 */
void eden_break_d_exec(EdenBreakWork *w)
{
    int i;
    int live = 0;
    EdenDrop *drop = w->drops;

    for (i = 0; i < w->ndrop; i++, drop++) {
        switch (drop->state) {
        case 0:
            if (--drop->timer <= 0) {
                drop->state = 1;
                drop->timer = (int)(cmd_fall_fst2 * FPS);
            }
            live++;
            break;
        case 1:
            drop->vel.x *= cmd_st_vr;
            drop->vel.y += cmd_fall_g2;
            drop->vel.z *= cmd_st_vr;
            if (cmd_fall_vlim < drop->vel.y)
                drop->vel.y = drop->vel.y * cmd_fall_vlim_r + cmd_fall_vlim * (1.0f - cmd_fall_vlim_r);
            v3_add(&drop->pos, &drop->vel);
            i3_add(drop->ang, drop->dang);
            drop->timer--;
            if (cmd_fall_top < drop->pos.y || drop->timer <= 0) {
                drop->state++;
                drop->timer = drop->fall_frm = (int)(cmd_fall_ft2 * FPS);
            }
            live++;
            break;
        case 2:
            drop->vel.y -= eden_break_fall_g;
            drop->vel.x *= cmd_st_vr;
            drop->vel.z *= cmd_st_vr;
            if (cmd_fall_vlim < drop->vel.y)
                drop->vel.y = drop->vel.y * cmd_fall_vlim_r + cmd_fall_vlim * (1.0f - cmd_fall_vlim_r);
            v3_add(&drop->pos, &drop->vel);
            i3_add(drop->ang, drop->dang);
            if (--drop->timer <= 0)
                drop->state++;
            live++;
            break;
        }
    }
    if (!live)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca6508 */
void eden_break_d_disp(EdenBreakWork *w)
{
    int old;
    int i;
    EdenDrop *drop;

    ulGxSetTextureList(w->tex_list);
    old = draw_set_override_flags(0);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    for (i = 0, drop = w->drops; i < w->ndrop; i++, drop++) {
        switch (drop->state) {
        case 0:
        case 1:
            draw_set_override_flags(0);
            ulMatrixPush();
            do {
                vu0_matrix_translate(ulMatrixCur, drop->pos.x, drop->pos.y, drop->pos.z);
            } while (0);
            eden_break_rotate_yxz(drop->ang);
            model_draw_culled((void *)drop->model);
            ulMatrixPop();
            break;
        case 2:
            draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            eden_break_d_fade[0] = (float)drop->timer / (float)drop->fall_frm + -1.0f;
            draw_set_override_color(eden_break_d_fade);
            ulMatrixPush();
            do {
                vu0_matrix_translate(ulMatrixCur, drop->pos.x, drop->pos.y, drop->pos.z);
            } while (0);
            eden_break_rotate_yxz(drop->ang);
            model_draw_culled((void *)drop->model);
            ulMatrixPop();
            break;
        }
    }
    draw_set_override_flags(old);
    draw_pop_override_color();
    draw_pop_strip_flags();
}

/* 0x00ca6680 */
int *eden_break_create(void *parent, int *src)
{
    EdenBreakWork *w = task_create_inherit(parent, &eden_break_desc, 112);
    void *debris;

    w->parent = parent;
    w->ndrop = eden_break_count_alive(((WireObj **)src)[1]);
    debris = heap_block_tag(game_calloc(w->ndrop << 6), str_eden_break_c, 479);
    eden_break_d_fall_start(w->drops = debris, ((WireObj **)src)[1]);
    return (int *)w;
}

/* 0x00ca6708 */
void eden_break_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
