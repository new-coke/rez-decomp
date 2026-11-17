/* Hopper landing ring, Area 4: spreads out and fades. */

#include "stg04.h"
#include "color.h"
#include "debug.h"
#include "gw_varedit.h"
#include "libc.h"
#include "node_tree.h"
#include "player.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_hopper_circle.h"
#include "../common/enemy_vars.h"

extern struct TaskDesc en_hopper_circle_desc;
extern int en_hopper_circle_div;
extern ParamRec en_hopper_circle_params;
extern VarDef en_hopper_circle_vardefs[6];

/* en_hopper_circle_params, 6 words */
typedef struct EnHopperCircleRec {
    char *name;
    char *label;                /* 0x04 */
    float frm_life;             /* 0x08: lifetime */
    float max_r;                /* 0x0c */
    int col;                    /* 0x10 */
    int div;                    /* 0x14 */
    float pow_r;                /* 0x18: safe_powf divisors of the eased radius and alpha */
    float pow_a;                /* 0x1c */
} EnHopperCircleRec;

#define EN_HOPPER_CIRCLE_REC ((EnHopperCircleRec *)&en_hopper_circle_params)

/* 0x00b055a0 */
void en_hopper_circle_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_hopper_circle_params.name;
    hdr.fmt = en_hopper_circle_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_hopper_circle_vardefs;
    val = en_hopper_circle_params.v;
    for (i = 0; i < 6; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_hopper_circle_vardefs);
}

/* 0x00b05650 */
void en_hopper_circle_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b05658 */
void en_hopper_circle_exec(EnHopperCircleWork *w)
{
    Vec3 v;
    float rate = w->t / w->end;

    player_get_frame_vel(&v);
    v3_add(&w->pos, &v);
    w->r = EN_HOPPER_CIRCLE_REC->max_r * safe_powf(ease_sine_in_out(rate), EN_HOPPER_CIRCLE_REC->pow_r);
    w->alpha = (int)(safe_powf(ease_sine_in_out_rev(rate), EN_HOPPER_CIRCLE_REC->pow_a) * 255.0f);
    w->t += 1.0f;
    if (w->end < w->t)
        FINISH_IF(w);
}

/* 0x00b05758 */
void en_hopper_circle_disp(EnHopperCircleWork *w)
{
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    draw_ring_xz(w->r, (w->alpha + 1) >> 1 << 24 | 0x800000, en_hopper_circle_div);
    ulMatrixPop();
}

/* 0x00b057c0 */
void en_hopper_circle_init(EnHopperCircleWork *w, Vec3 *pos)
{
    Vec3 v;

    v3_copy(&w->pos, pos);
    player_get_frame_vel(&v);
    v3_sub(&w->pos, &w->pos, &v);
    w->t = 1.0f;
    w->end = EN_HOPPER_CIRCLE_REC->frm_life * FPS;
}

/* 0x00b05858 */
void en_hopper_circle_release(void)
{
}

/* 0x00b05860 */
EnHopperCircleWork *en_hopper_circle_create(void *parent, Vec3 *pos)
{
    EnHopperCircleWork *w = task_create_sibling(parent, &en_hopper_circle_desc, 28);

    en_hopper_circle_init(w, pos);
    return w;
}
