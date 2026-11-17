/* Marimo break pieces, Area 4: drift, fade and shatter; some kinds can still be shot. */

#include "stg04.h"
#include "color.h"
#include "enemy_status.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_marimo_break.h"
#include "../common/en_break.h"
#include "../common/enemy_shatter.h"
#include "../common/util.h"

/* 20 bytes, at en_marimo_break_kinds */
typedef struct EnMarimoBreakRec {
    int model;
    int clip;                   /* 0x04: -1 without */
    int type;                   /* 0x08: the break effect's model */
    int wire;                   /* 0x0c */
    float end;                  /* 0x10: frames shown */
} EnMarimoBreakRec;

extern const EnMarimoBreakRec en_marimo_break_kinds[];
extern struct TaskDesc en_marimo_break_desc;

/* 0x00b0c3f8 */
void en_marimo_break_shatter(EnMarimoBreakWork *w)
{
    SpawnDesc desc;
    ShatterParams prm;
    Task *parent;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    desc.type = en_marimo_break_kinds[w->kind].type;
    desc.v.x = 0;
    desc.v.y = 0;
    desc.v.z = 0;
    v3_copy(&desc.pos, &w->pos);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.kind = 0;
    desc.models = stage_models;
    i3_copy(desc.ang, w->ang);
    prm.Gravity = 10.0f;
    prm.PowVec = 0.6f;
    prm.PowAng = 0.4f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    prm.force_wire = 0;
    prm.TimeAlphaEnd = 80.0f;
    prm.pcol_wire = RGBA(0x34, 0x0f, 0, 0x40);
    prm.b_acc = 0;
    prm.frm_acc = 0;
    parent = TASK_OF(w)->parent;
    enemy_shatter_create(parent ? parent + 1 : 0, &desc, (float *)&prm, 0);
}

/* 0x00b0c568 */
void en_marimo_break_calc_target(EnMarimoBreakWork *w)
{
    Vec3 ofs;

    if (w->lock) {
        switch (w->kind) {
        case 2:
            ofs.x = 8.2242f;
            ofs.y = -59.7908f;
            ofs.z = 19.0522f;
            break;
        case 3:
            ofs.x = -8.2242f;
            ofs.y = -59.7908f;
            ofs.z = 19.0522f;
            break;
        }
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = &w->pos;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_marimo_break_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        ulMatrixApplyPoint((float *)ulMatrixCur, &ofs.x, &w->target->x);
        ulMatrixPop();
    }
}

/* 0x00b0c658 */
int en_marimo_break_hit_check(EnMarimoBreakWork *w)
{
    LockObj *lock = w->lock;

    if (lock && (lock->flags & LOCK_HIT)) {
        if (lock->life <= 0) {
            enemy_status_score_popup(-1, w->target, 1000, lockon_hits_max_shot_cnt(lock));
            lock->flags &= ~LOCK_LOCKABLE;
            en_marimo_break_shatter(w);
            FINISH_IF(w);
            return 1;
        }
        en_break_spawn(w, w->target, w->ang, 1);
    }
    return 0;
}

/* 0x00b0c710 */
void en_marimo_break_exec(EnMarimoBreakWork *w)
{
    Vec3 pl_vel;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    if (en_marimo_break_hit_check(w))
        return;
    switch (w->kind) {
    case 0:
        w->pos.y += (0.5f - w->t) * 0.0022222223f;
        w->pos.z += 0.083333336f;
        w->ang[0] -= 18;
        w->ang[1] -= 24;
        w->ang[2] -= 15;
        w->rgba[0] = -w->t / w->end;
        break;
    case 1:
        w->pos.y += (0.5f - w->t) * 0.0027777778f;
        w->pos.z += 0.06666667f;
        w->ang[0] += 24;
        w->ang[1] += 18;
        w->ang[2] -= 15;
        w->rgba[0] = -w->t / w->end;
        break;
    case 2:
        w->pos.y += (0.5f - w->t) * 0.0022222223f;
        w->pos.z += 0.06666667f;
        w->ang[0] += 24;
        w->ang[1] += 24;
        w->ang[2] -= 15;
        w->rgba[0] = -w->t / w->end;
        break;
    case 3:
        w->pos.y += (0.5f - w->t) * 0.0022222223f;
        w->pos.z += 0.06666667f;
        w->ang[0] -= 24;
        w->ang[1] -= 24;
        w->ang[2] -= 15;
        w->rgba[0] = -w->t / w->end;
        break;
    }
    en_marimo_break_calc_target(w);
    w->t += 1.0f;
    if (w->end <= w->t) {
        if (w->kind < 4)
            en_marimo_break_shatter(w);
        FINISH(w);
    }
}

/* 0x00b0c9a8 */
void en_marimo_break_toggle_nodes(int *w)
{
    if (w[41])
        util_node_toggle((unsigned int *)((int *)w[36])[11]);
}

/* steer(cast) */
/* 0x00b0c9c8 */
void en_marimo_break_disp(EnMarimoBreakWork *w)
{
    int blend;

    ulGxSetTextureList(stage_textures);
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_marimo_break_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (w->kind < 4) {
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(w->rgba);
        en_marimo_break_toggle_nodes((int *)w);
        if (w->clip) {
            w->part.frame = w->time;
            dc_object_draw(&w->part);
        } else
            dc_object_draw(&w->part);
        en_marimo_break_toggle_nodes((int *)w);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        draw_set_blend(4, 1);
        draw_set_color(((int)((float)color_get_a(RGBA(0x34, 0x0f, 0, 0x40)) * (1.0f - w->rgba[0])) + 1) >> 1 << 24 | 0xf34);
        en_marimo_break_toggle_nodes((int *)w);
        if (w->clip)
            model_node_walk_list_anim((int)w->wire, (int)w->clip, w->time, w->cache);
        else
            model_draw_kind_list((struct DcNodePosView *)w->wire, w->cache);
        en_marimo_break_toggle_nodes((int *)w);
    }
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b0cb58 */
void en_marimo_break_init(EnMarimoBreakWork *w)
{
    EnMarimoBreakRec *rec;
    float end;
    int clip;

    rec = &en_marimo_break_kinds[w->kind];
    w->t = 1.0f;
    if (w->lock)
        end = rec->end;
    else
        end = rec->end * 0.5f;
    w->end = end;
    w->model = (void *)*((int *)pnb_get_entry(stage_models, rec->model));
    if (rec->clip != -1)
        clip = *((int *)pnb_get_entry(stage_models, rec->clip));
    else
        clip = 0;
    w->clip = (void *)clip;
    w->wire = (void *)*((int *)pnb_get_entry(stage_models, rec->wire));
    w->cache = model_pnb_get_wire(stage_models, rec->wire);
    dc_object_init(&w->part, (struct DcNode *)w->model, (struct DcClip *)w->clip, 0);
    dc_object_rebuild(&w->part);
    if (w->kind < 2)
        w->flag = 0;
    else if (w->kind < 4) {
        w->flag = 1;
        if (w->lock && w->lock->life >= 3)
            w->flag = 0;
    }
}

/* 0x00b0cc98 */
void en_marimo_break_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 52));
}

/* 0x00b0cca0 */
EnMarimoBreakWork *en_marimo_break_create(void *parent, EnMarimoBreakArg *prm)
{
    EnMarimoBreakWork *w = task_create(parent, &en_marimo_break_desc, 176);

    w->target = prm->target;
    w->time = prm->time;
    v3_copy(&w->pos, prm->pos);
    w->lock = prm->lock;
    w->kind = prm->kind;
    i3_copy(w->ang, prm->ang);
    en_marimo_break_init(w);
    return w;
}

/* 0x00b0cd48 */
void en_marimo_break_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
