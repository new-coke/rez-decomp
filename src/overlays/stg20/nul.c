/*
 * Tutorial 'Nul' launcher: fades in, moves for its given frames, then after a delay fires a
 * 'Missile' at the player. Lockable.
 */

#include "types_gw.h"
#include "color.h"
#include "stg20.h"
#include "debug.h"
#include "debugdraw.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "mathdef.h"
#include "model.h"
#include "nj_system.h"
#include "nj_view.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "missile.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern void *nul_models;
extern char nul_lock_name[];
extern void *nul_textures;
extern char nul_pnb[];
extern GwVarDef nul_vardef[];
extern GwVarDef nul_vardefs[];
extern char nul_ptm[];
extern struct TaskDesc nul_desc;
void nul_rotate_yxz(int *ang);

/* 0x00af7a98 */
void nul_shatter(NulWork *w)
{
    SpawnDesc sdesc;

    nj_fill8(&sdesc, 0, sizeof sdesc);
    v3_copy(&sdesc.pos, &w->pos);
    v3_set(&sdesc.scale, 1.0f, 1.0f, 1.0f);
    sdesc.kind = 0;
    sdesc.type = 2;
    sdesc.models = nul_models;
    sdesc.lock = &w->lock;
    i3_copy(sdesc.ang, w->ang);
    v3_copy(&sdesc.v, &w->lock.imp.v);
    enemy_shatter_create(w, &sdesc, 0, 0);
}

/* steer(cast) */
/* 0x00af7b68 */
void nul_init(NulWork *w, ShotArg *prm)
{
    void *models;
    int root;
    int gwire;

    v3_copy(&w->pos, &prm->pos);
    v3_copy(&w->vel, &prm->vel);
    i3_copy(w->ang, prm->ang);
    w->move = prm->move;
    w->delay = prm->delay;
    w->fx = enemy_status_ent_create(prm, w);
    enemy_status_event(w->fx, 2, &w->pos);
    models = nul_models;
    w->fx_op = 3;
    root = *((int *)pnb_get_entry(models, 0));
    dc_object_init(&w->body, root, 0, 0);
    root = *((int *)pnb_get_entry(models, 1));
    gwire = model_pnb_get_wire(models, 1);
    dc_object_init(&w->glow, root, 0, 0);
    dc_object_set_wire(&w->glow, gwire);
    lockon_obj_init(&w->lock, w, &w->pos, 0, nul_lock_name, w->fx, 1, 1, 8);
    lockon_entry(&w->lock);
    w->fade = 0;
}

/* 0x00af7d00 */
void nul_release(NulWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->glow);
    lockon_remove(&w->lock);
    enemy_status_event(w->fx, w->fx_op, &w->pos);
}

/* 0x00af7d48 */
void nul_exec(NulWork *w)
{
    int n = enemy_status_get_ticks();

    if (n > 0) {
        do {
            dc_object_advance_time(&w->body);
            dc_object_advance_time(&w->glow);
            if (w->fade < 1.0f) {
                w->fade += 0.033333335f;
                if (w->fade >= 1.0f)
                    w->fade = 1.0f;
            }
            if (w->move > 0) {
                w->move--;
                v3_add(&w->pos, &w->vel);
            } else if (w->delay > 0) {
                w->delay--;
                if (w->delay <= 0)
                    missile_fire(w, &w->pos);
            }
            n--;
        } while (n);
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
        nul_shatter(w);
        FINISH_IF(w);
    }
}

/* steer(cast) */
/* 0x00af7e98 */
void nul_disp(NulWork *w)
{
    Vec3 *pos = &w->pos;
    float rgba[4];
    float full;
    int prev_blend;

    ulGxSetTextureList(nul_textures);
    ulMatrixPush();
    if (view_sphere_visible(pos, w->body.radius)) {
        full = 1.0f;
        prev_blend = draw_set_override_flags(0x8810);
        nj_cnk_set_override_mode(0x2500);
        rgba[0] = w->fade;
        rgba[1] = full;
        rgba[2] = full;
        rgba[3] = full;
        draw_push_strip_flags();
        draw_push_override_color();
        do {
            Vec3 *p_ = pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        nul_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        draw_set_override_color(rgba);
        if (w->fade < full)
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        else
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xfffff7ff, 0);
        dc_object_draw(&w->body);
        draw_set_color(((int)(beat_clock_get_phase() * w->fade * 255.0f) + 1) >> 1 << 24 | 0x80);
        draw_set_blend(4, 5);
        dc_object_draw(&w->glow);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev_blend);
    }
    ulMatrixPop();
}

/* 0x00af8020 */
void *nul_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &nul_desc, 312, prm);
}

/* Box at the placement; same unset-v bug as missile_marker_draw. */
/* 0x00af8048 */
int nul_marker_draw(void **w, ShotArg *prm)
{
    float r = 10.0f;
    Vec3 v;

    ulGxSetTextureList(nul_textures);
    ulMatrixPush();
    if (view_sphere_visible(&v, r)) {
        do {
            Vec3 *p_ = &prm->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        nul_rotate_yxz(prm->ang);
        debug_draw_box(COL_WHITE, r);
    }
    ulMatrixPop();
    return 0;
}

/* steer(cast) */
/* 0x00af80e0 */
int nul_vars_init(int *prm)
{
    nul_textures = ((void *(*)(void *, int))model_create_timed)(nul_ptm, prm[1]);
    nul_models = ((void *(*)(void *))model_pnb_register)(nul_pnb);
    gw_varedit_add_table(&gw_varedit_root,
                         gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), nul_vardef),
                         nul_vardefs);
    return 0;
}

/* 0x00af8160 */
int nul_vars_release(void)
{
    model_list_remove(nul_textures);
    model_free_all_buffers_checked(nul_models);
    nul_textures = 0;
    nul_models = 0;
    enemy_vars_group_release();
    return 0;
}

/* 0x00af81b0 */
void nul_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
