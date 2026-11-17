/*
 * Area 2 break-up effects of Shiva, Brahma and the Garuda nest: pieces and wire flashes; some
 * shatter at once, others fade, tumble or shed parts first.
 */

#include "stg02.h"
#include "anim.h"
#include "color.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_shiva_break.h"
#include "../common/enemy_shatter.h"
#include "../common/util.h"

/* as en_shiva_break_release sees it */
typedef struct EnShivaBreak {
    char pad_00[28];
    ModelPart body;                /* 0x1c */
    ModelPart wire;                /* 0x78 */
} EnShivaBreak;

typedef struct EnShivaBreakKind {
    int model;
    int wire;                   /* 0x04: -1 = none */
    int wire2;                  /* 0x08: -1 = none */
    int shard;                  /* 0x0c: -1 = the model */
    float end;                  /* 0x10 */
} EnShivaBreakKind;

extern const EnShivaBreakKind en_shiva_break_kinds[];

/* piece, 256 bytes; kinds 0, 3, 5, 6 */
typedef enum { PIECE_KIND_0 } PieceKind;

typedef struct EnShivaBreakWork {
    void *owner;
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    ModelPart body;                /* 0x1c */
    ModelPart wire;                /* 0x78 */
    int model;                  /* 0xd4 */
    int wire_model;             /* 0xd8: 0 = none */
    float t;                    /* 0xdc: frames / 3 */
    float end;                  /* 0xe0 */
    float rgba[4];              /* 0xe4 */
    int flags;                  /* 0xf4: bit i: part i broken off */
    float cnt;                  /* 0xf8 */
    PieceKind kind;                /* 0xfc: index into en_shiva_break_kinds */
} EnShivaBreakWork;

extern float en_shiva_break_part_times[4];
extern struct TaskDesc en_shiva_break_desc;

/* 0x00b0ac60 */
void en_shiva_break_shatter(EnShivaBreakWork *w)
{
    SpawnDesc desc;
    ShatterParams prm;
    int fog[3];

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    nj_fill8(fog, 0, sizeof fog);
    desc.kind = 0;
    if (w->wire_model)
        desc.kind = 0x40000;
    desc.models = stage_models;
    if (en_shiva_break_kinds[w->kind].shard != -1)
        desc.type = en_shiva_break_kinds[w->kind].shard;
    else
        desc.type = en_shiva_break_kinds[w->kind].model;
    desc.motion = en_shiva_break_kinds[w->kind].wire;
    desc.time = w->end;
    i3_copy(desc.ang, w->ang);
    v3_set(&desc.v, 0.0f, -1.0f, 0.0f);
    v3_copy(&desc.pos, &w->pos);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    prm.Gravity = 10.0f;
    prm.PowVec = 1.5f;
    prm.PowAng = 1.0f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.force_wire = 0;
    prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x40);
    prm.b_acc = 0;
    prm.frm_acc = 0.0f;
    fog[0] = 4;
    fog[1] = 1;
    fog[2] = 0;
    enemy_shatter_create(w, &desc, (float *)&prm, fog);
}

/* 0x00b0ae38 */
Shatter *en_shiva_break_spawn_plain(void *obj, Vec3 *pos, int *ang, int kind)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    v3_copy(&desc.pos, pos);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    v3_set(&desc.v, 0.0f, 1.0f, 0.0f);
    desc.models = stage_models;
    desc.type = en_shiva_break_kinds[kind].model;
    i3_copy(desc.ang, ang);
    desc.kind = 0;
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    return enemy_shatter_create(obj, &desc, 0, 0);
}

/* 0x00b0af28 */
Shatter *en_shiva_break_spawn_tuned(void *obj, Vec3 *pos, int *ang, int kind)
{
    SpawnDesc desc;
    ShatterParams prm;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    v3_set(&desc.v, 0.0f, 1.0f, 0.0f);
    desc.models = stage_models;
    desc.type = en_shiva_break_kinds[kind].model;
    v3_copy(&desc.pos, pos);
    i3_copy(desc.ang, ang);
    desc.kind = 0;
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    prm.Gravity = 10.0f;
    prm.PowVec = 0.9f;
    prm.PowAng = 6.7f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 0.7f;
    prm.TimeAlphaStart = 1.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.force_wire = 0;
    prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x40);
    prm.b_acc = 0;
    prm.frm_acc = 0.0f;
    return enemy_shatter_create(obj, &desc, (float *)&prm, 0);
}

/* 0x00b0b0b0 */
void en_shiva_break_shatter_part(EnShivaBreakWork *w, int i)
{
    SpawnDesc desc;
    ShatterParams prm;
    int fog[3];
    float mtx[16];
    float t;
    unsigned int mask;
    Vec3 *wpos;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    nj_fill8(fog, 0, sizeof fog);
    desc.kind = 0xa0000;
    mask = 1 << (i + 24);
    desc.models = stage_models;
    desc.type = en_shiva_break_kinds[w->kind].model;
    desc.motion = en_shiva_break_kinds[w->kind].wire;
    t = en_shiva_break_part_times[i];
    desc.time = t;
    node_tree_collect_counted(w->model, mask, 1, &desc.idx);
    desc.v.y = -1.0f;
    prm.PowAng = 1.0f;
    prm.Gravity = 10.0f;
    prm.PowVec = 1.5f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    desc.v.x = 0.0f;
    desc.v.z = 0.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.force_wire = 0;
    prm.pcol_wire = RGBA(0x68, 0x1e, 0, 0x40);
    fog[0] = 4;
    prm.b_acc = 0;
    prm.frm_acc = 0.0f;
    fog[1] = 1;
    fog[2] = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    wpos = &w->pos;
    do {
        Vec3 *p_ = wpos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_shiva_break_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start((void *)w->wire_model, t);
    node_tree_draw_matching_at(w->model, mask, 1, (int)mtx, 1);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx);
    enemy_shatter_create(w, &desc, (float *)&prm, fog);
    ulMatrixPop();
}

/* 0x00b0b2d8 */
void en_shiva_break_exec(EnShivaBreakWork *w)
{
    Vec3 pl_vel;
    float *tab;
    float t;
    int i;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    t = w->t + 0.33333334f;
    w->t = t;
    switch (w->kind) {
    case 5:
        w->cnt += 1.0f;
        w->pos.y += (0.5f - w->cnt) * 0.0027777778f;
        w->ang[0] += 30;
        w->ang[1] += 15;
        w->ang[2] += 15;
        w->rgba[0] = -w->t / w->end;
        break;
    case 6:
        w->cnt += 1.0f;
        w->pos.y += (0.5f - w->cnt) * 0.0022222223f;
        w->pos.z += 0.16666667f;
        w->ang[0] += 24;
        w->ang[1] += 24;
        w->ang[2] += 15;
        w->rgba[0] = -w->t / w->end;
        break;
    case 0:
    case 3:
        w->rgba[0] = -t / w->end;
        break;
    }
    if (w->kind == 0) {
        for (i = 3; i >= 0; i--) {
            if (!((w->flags >> i) & 1) && w->t >= en_shiva_break_part_times[i]) {
                en_shiva_break_shatter_part(w, i);
                w->flags |= 1 << i;
                if (!i)
                    FINISH_IF(w);
            }
        }
    } else if (w->t >= w->end) {
        if (w->kind != 5)
            en_shiva_break_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00b0b5a0 */
void en_shiva_break_toggle_parts(char *wp)
{
    EnShivaBreakWork *w = (EnShivaBreakWork *)wp;
    int *node = (int *)((int *)w->model)[11];
    unsigned int i;

    for (i = 0; i < 4; i++) {
        if ((w->flags >> i) & 1)
            util_node_toggle(node);
        node = (int *)node[12];
    }
}

/* steer(cast) */
/* 0x00b0b610 */
void en_shiva_break_disp(EnShivaBreakWork *w)
{
    int blend;
    Vec3 *pos;

    ulGxSetTextureList(stage_textures);
    do {
        ulMatrixPush();
    } while (0);
    pos = &w->pos;
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    en_shiva_break_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    switch (w->kind) {
    case 0:
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        en_shiva_break_toggle_parts((char *)w);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(w->rgba);
        w->body.frame = w->t;
        dc_object_draw(&w->body);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        draw_set_blend(4, 1);
        draw_set_color(((int)(color_get_a(RGBA(0x68, 0x1e, 0, 0x40)) * (1.0f - w->rgba[0])) + 1) >> 1 << 24 | 0x1e68);
        model_object_draw(&w->wire, w->t);
        en_shiva_break_toggle_parts((char *)w);
        break;
    case 5:
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(w->rgba);
        dc_object_draw(&w->body);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        break;
    case 3:
    case 6:
        draw_set_blend(4, 1);
        draw_set_color(((int)(color_get_a(RGBA(0x68, 0x1e, 0, 0x40)) * (1.0f - w->rgba[0])) + 1) >> 1 << 24 | 0x1e68);
        model_object_draw(&w->wire, w->t);
        break;
    default:
        draw_set_blend(4, 1);
        draw_set_color(RGBA(0x68, 0x1e, 0, 0x40));
        model_object_draw(&w->wire, w->t);
        break;
    }
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b0b868 */
void en_shiva_break_init(EnShivaBreakWork *w)
{
    int model, wire, wire2;
#ifndef VERSION_US
    Vec3 v;
#endif

    model = en_shiva_break_kinds[w->kind].model;
    wire = en_shiva_break_kinds[w->kind].wire;
    wire2 = en_shiva_break_kinds[w->kind].wire2;
    w->end = en_shiva_break_kinds[w->kind].end;
    w->model = *((int *)pnb_get_entry(stage_models, model));
    w->wire_model = wire != -1 ? *((int *)pnb_get_entry(stage_models, wire)) : 0;
    if (w->kind != 5) {
        dc_object_init(&w->wire, w->model, w->wire_model, 0);
        if (wire2 == -1)
            dc_object_set_wire(&w->wire, model_pnb_get_wire(stage_models, model));
        else
            dc_object_set_wire(&w->wire, *((int *)pnb_get_entry(stage_models, wire2)));
    }
    dc_object_init(&w->body, w->model, w->wire_model, 0);
#ifndef VERSION_US
    player_get_frame_vel(&v);
    v3_sub(&w->pos, &w->pos, &v);
#endif
}

/* 0x00b0b988 */
void en_shiva_break_release(EnShivaBreak *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->wire);
}

/* 0x00b0b9b8 */
void *en_shiva_break_spawn(void *obj, Vec3 *pos, int *ang, unsigned int kind)
{
    EnShivaBreakWork *w;
#ifdef VERSION_US
    Vec3 abs_pos, org;

    /* relative to the camera origin */
    player_get_frame_vel(&org);
    v3_sum(&abs_pos, pos, &org);
    switch (kind) {
    case 1:
    case 2:
    case 4:
    case 7:
    case 9:
        return en_shiva_break_spawn_plain(obj, &abs_pos, ang, kind);
    case 8:
    case 10:
        return en_shiva_break_spawn_tuned(obj, &abs_pos, ang, kind);
#else

    switch (kind) {
    case 1:
    case 2:
    case 4:
    case 7:
    case 9:
        return en_shiva_break_spawn_plain(obj, pos, ang, kind);
    case 8:
    case 10:
        return en_shiva_break_spawn_tuned(obj, pos, ang, kind);
#endif
    case 0:
    default:
        w = task_create_sibling(obj, &en_shiva_break_desc, 256);
        v3_copy(&w->pos, pos);
        i3_copy(w->ang, ang);
        w->owner = obj;
        w->kind = kind;
        en_shiva_break_init(w);
        return w;
    }
}

/* 0x00b0bb00 */
void en_shiva_break_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
