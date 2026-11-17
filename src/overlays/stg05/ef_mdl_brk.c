/*
 * Model break-up effect, Area 5: polygons fly apart, shrink and fall, optionally tinted; absolute
 * or player-relative positions. Used by exploding sidewinder shots and homing missiles.
 */

#include "stg05.h"
#include "math_util.h"
#include "modbreak.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "ef_mdl_brk.h"

extern struct TaskDesc ef_mdl_brk_desc;
extern struct TaskDesc ef_mdl_brk_relative_desc;

typedef struct EfMdlBrkWork {
    char pad_00[4];
    void *tex;                  /* 0x04 */
    int sub[8];                 /* 0x08: the compiled model (modbreak_compile); released by modbreak_work_free */
    int colored;                /* 0x28: draw with the colour */
    Rgba4 color;                /* 0x2c */
    float decay;                /* 0x3c: piece scale factor per frame */
    float fall;                 /* 0x40: added to the piece speed y per frame */
    int fixed;                  /* 0x44: absolute positions (not player relative) */
} EfMdlBrkWork;

/* 40 bytes (modbreak) */
typedef struct EfMdlBrkPiece {
    Vec3 pos;
    Vec3 vel;                   /* 0x0c */
    int ang[2];                 /* 0x18: x, y */
    float scale;                /* 0x20 */
    int done;                   /* 0x24 */
} EfMdlBrkPiece;

/* 0x00b28b08 */
void ef_mdl_brk_init(EfMdlBrkWork *w, EfMdlBrkArg *prm)
{
    Vec3 p;
    Vec3 pl_vel;

    ulMatrixPush();
    if (prm->one)
        v3_copy(&p, &prm->pos);
    else {
        player_get_vec(&pl_vel);
        v3_sub(&p, &prm->pos, &pl_vel);
    }
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ef_mdl_brk_rotate_yxz(prm->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    modbreak_compile(w->sub, prm->gfx);
    modbreak_work_scatter(w->sub, prm->amp, &prm->scatter_dir, prm->scale, prm->range);
    ulMatrixPop();
    w->colored = prm->colored;
    if (w->colored)
        w->color = prm->color;
    w->decay = prm->decay;
    w->fixed = prm->one;
    w->fall = prm->fall;
}

/* 0x00b28c60 */
void ef_mdl_brk_init_relative(EfMdlBrkWork *w, EfMdlBrkArg *prm)
{
    Vec3 p;

    ulMatrixPush();
    player_get_vec(&p);
    p.x = -p.x;
    p.y = -p.y;
    p.z = -p.z;
    do {
        Vec3 *p_ = &p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    modbreak_compile(w->sub, prm->gfx);
    modbreak_work_scatter(w->sub, prm->amp, &prm->scatter_dir, prm->scale, prm->range);
    ulMatrixPop();
    w->colored = prm->colored;
    if (w->colored)
        w->color = prm->color;
    w->decay = prm->decay;
    w->fall = prm->fall;
    task_set_dlist(w, 6);
}

/* 0x00b28d58 */
void ef_mdl_brk_release(EfMdlBrkWork *w)
{
    modbreak_work_free(w->sub);
}

/* 0x00b28d60 */
void ef_mdl_brk_exec(EfMdlBrkWork *w)
{
    Vec3 base;
    Vec3 cur;
    int alive;
    int n;
    EfMdlBrkPiece *piece;
    char *rec;
    float x, y, z;

    if (w->fixed) {
        base.x = 0;
        base.y = 0;
        base.z = 0;
    } else
        player_get_vec(&base);
    ulMatrixPush();
    alive = 0;
    rec = (char *)w->sub[6] + 8;
    n = w->sub[0];
    piece = (EfMdlBrkPiece *)w->sub[5];
    matrix_unit_set(0);
    for (n--; n != -1; piece++, n--, rec += 72) {
        if (piece->done)
            continue;
        ulMatrixPush();
        cur.x = x = piece->pos.x + piece->vel.x + base.x;
        cur.y = y = piece->pos.y + piece->vel.y + base.y;
        cur.z = z = piece->pos.z + piece->vel.z + base.z;
        do {
            Vec3 *p_ = &cur;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        vu0_matrix_rotate_y(ulMatrixCur, piece->ang[1]);
        vu0_matrix_rotate_x(ulMatrixCur, piece->ang[0]);
        cur.x = -piece->pos.x;
        cur.y = -piece->pos.y;
        cur.z = -piece->pos.z;
        do {
            Vec3 *p_ = &cur;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        piece->pos.x = x;
        piece->pos.y = y;
        piece->pos.z = z;
        matrix_apply_pairs(rec, rec, 3);
        ulMatrixPop();
        piece->vel.y += w->fall;
        piece->scale *= w->decay;
        if (piece->scale <= 0.02f) {
            piece->scale = 0;
            piece->done = 1;
        }
        alive = 1;
    }
    if (!alive)
        FINISH_IF(w);
    ulMatrixPop();
}

/* 0x00b28f90 */
void ef_mdl_brk_disp(EfMdlBrkWork *w)
{
    int prev;

    ulGxSetTextureList(w->tex);
    draw_set_blend(4, 5);
    if (w->colored) {
        prev = draw_set_override_flags(16);
        draw_push_override_color();
        draw_set_override_color(w->color.rgba);
        draw_set_blend(4, 5);
        modbreak_work_draw(w->sub);
        draw_pop_override_color();
        draw_set_override_flags(prev);
    } else
        modbreak_work_draw(w->sub);
}

/* 0x00b29028 */
void *ef_mdl_brk_create(void *w, void *prm)
{
    return task_create_sibling_inherit_arg(w, &ef_mdl_brk_desc, 72, prm);
}

/* 0x00b29050 */
void *ef_mdl_brk_start(void *w, EfMdlBrkArg *prm)
{
    prm->decay = 0.97f;
    prm->amp = 728.0f;
    prm->scale = 0.33333334f;
    prm->range = 0.06666667f;
    prm->fall = -0.00272222236f;
    return ef_mdl_brk_create(w, prm);
}

/* 0x00b290b8 */
void *ef_mdl_brk_create_relative(void *w, void *prm)
{
    return task_create_sibling_inherit_arg(w, &ef_mdl_brk_relative_desc, 72, prm);
}

/* 0x00b290e0 */
void *ef_mdl_brk_start_relative(void *w, EfMdlBrkArg *prm)
{
    prm->decay = 0.97f;
    prm->amp = 728.0f;
    prm->scale = 0.33333334f;
    prm->range = 0.06666667f;
    prm->fall = -0.00272222236f;
    return ef_mdl_brk_create_relative(w, prm);
}

/* 0x00b29148 */
void ef_mdl_brk_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
