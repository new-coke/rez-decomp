/*
 * Eden boss small effects: shine flares for hits and shots (three kinds), the four-bladed beam
 * flash, and the screen bars that fade in during the part cuts.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "billboard.h"
#include "color.h"
#include "fog.h"
#include "gw_varedit.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_effect.h"
#include "../common/boss_common.h"

extern void *scr_root;
extern char eden_effect_vardef[];
extern GwVarDef eden_effect_vardefs[];
extern struct GwVarNode *eden_effect_vars_group;
extern struct TaskDesc eden_effect_screen_man_desc;
extern float cmd_ds_rr;
extern struct TaskDesc eden_effect_screen_desc;
extern float cmd_sc_fade;
extern unsigned int cm_sh_col;
extern struct TaskDesc eden_effect_dshine_ex_desc;
extern struct TaskDesc eden_effect_dshine2_desc;
extern struct TaskDesc eden_effect_bflash_desc;
extern struct TaskDesc eden_effect_bflash2_desc;
extern float cmd_ds2_rm;
extern float cmd_ds2_t;
extern float cmd_fl_st;
extern float cmd_ds_rm;
extern unsigned int cm_r_tex_froce;
extern float cmd_ds_ar;
extern int cmd_ds_frm;
extern float cmd_fl_size;
extern struct TaskDesc eden_effect_dshine_desc;
extern float cmd_sc_ty;
extern float cmd_sc_by;
extern TaVtx eden_effect_flash_quad[4];
extern TaVtx eden_effect_flash_quad2[4];
extern const float cmd_fl_fin;
extern float cmd_fl_len;
extern unsigned int cm_sc_col[2];
extern const float cmd_sc_wait;

/* a shine on a point: grows and spins (dshine), fades (dshine2), pulses (dshine_ex); 44 bytes */
typedef struct EdenShineWork {
    struct PnbHead *pack;
    void *tex_list;
    Vec3 *pos;
    int tex;
    int frm;
    int time;
    int len;
    int ang;
    int dang;
    float scl;
    float scl_max;
} EdenShineWork;

/* a flash of crossed quads on a cell, 48 bytes */
typedef struct EdenFlashWork {
    struct PnbHead *pack;
    void *tex_list;
    Vec3 *pos;
    int *ang;
    int state;                  /* 0 growing, 1 fading */
    int tex;
    int time;
    int len;
    float len_z;
    unsigned int col;
    unsigned int base_col;
    int rx;
} EdenFlashWork;

/* the screen bars during a part's camera cut, 84 bytes */
typedef struct EdenScreenWork {
    int state;                  /* 0 in, 1 hold, 2 out, 3 held to the end */
    int time;
    int len;
    int end;
    TrailVtx vtx[4];
    unsigned int col;
} EdenScreenWork;

/* 8 bytes */
typedef struct EdenScreenManWork {
    char pad_00[4];
    EdenScreenWork *screen;     /* 0x04 */
} EdenScreenManWork;

/* 0x00cb1588 */
void eden_effect_vars_add(void)
{
    eden_effect_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_effect_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_effect_vars_group, eden_effect_vardefs);
}

/* 0x00cb15d8 */
void eden_effect_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_effect_vardef);
}

/* 0x00cb15f0 */
void eden_effect_dshine_exec(EdenShineWork *w)
{
    float t = cmd_ds_rr;

    w->scl = w->scl * t + cmd_ds_rm * (1.0f - t);
    w->ang = (float)w->ang * cmd_ds_ar;
    if (++w->time >= cmd_ds_frm && ++w->frm > 0)
        task_end_with_exit(w);
}

/* 0x00cb1680 */
void eden_effect_dshine_disp(EdenShineWork *w)
{
    void *fog;
    float *k = &cmd_ds_rm;

    ulGxSetTextureList(w->tex_list);
    fog = fog_set_enable(0);
    draw_set_blend(4, 1);
    billboard_draw_rot(w->tex + w->frm, cm_r_tex_froce, w->ang + w->dang, (int)w->pos, *k, *k);
    fog_set_enable(fog);
}

/* 0x00cb1708 */
int *eden_effect_dshine_create(void *parent, int arg)
{
    EdenShineWork *w = task_create_inherit(parent, &eden_effect_dshine_desc, 44);

    w->pos = (Vec3 *)arg;
    w->tex = 0;
    w->dang = DEG_TO_ANG(RAND_FLOAT_B() * 360.0f - 180.0f);
    return (int *)w;
}

/* 0x00cb17a8 */
void eden_effect_dshine2_exec(EdenShineWork *w)
{
    float t = cmd_ds_rr;

    w->scl = w->scl * t + w->scl_max * (1.0f - t);
    if (--w->time <= 0)
        FINISH_IF(w);
}

/* 0x00cb1800 */
void eden_effect_dshine2_disp(EdenShineWork *w)
{
    void *fog;
    float t, k;
    int alpha;

    ulGxSetTextureList(w->tex_list);
    fog = fog_set_enable(0);
    t = w->scl_max - w->scl;
    k = 1.0f - t / w->scl_max;
    draw_set_blend(4, 1);
    alpha = ((int)(color_get_a(cm_sh_col) * k) + 1) >> 1;
    flare_draw(w->tex, (cm_sh_col & 0xffffff) | alpha << 24, w->pos, 0, t);
    fog_set_enable(fog);
}

/* 0x00cb18e0 */
int *eden_effect_dshine2_create(void *parent, int arg)
{
    EdenShineWork *w = task_create_inherit(parent, &eden_effect_dshine2_desc, 44);

    task_set_dlist(w, 6);
    w->tex = 2;
    w->pos = (Vec3 *)arg;
    w->scl_max = cmd_ds2_rm;
    w->time = cmd_ds2_t * FPS;
    return (int *)w;
}

/* 0x00cb1960 */
void eden_effect_dshine_scale(EdenShineWork *w, float k)
{
    w->scl_max *= k;
}

/* 0x00cb1970 */
void eden_effect_dshine_ex_exec(EdenShineWork *w)
{
    float t = 1.0f - (float)w->time / (float)w->len;

    if (t <= 0.25f)
        w->scl = w->scl_max * 4.0f * t;
    else {
        t -= 0.25f;     /* subtracted twice (also on the next line) */
        t = 1.0f - (t - 0.25f) / 0.75f;
        w->scl = w->scl_max * t;
    }
    if (--w->time <= 0 && w)
        FINISH(w);
}

/* 0x00cb1a20 */
void eden_effect_dshine_ex_disp(EdenShineWork *w)
{
    void *fog;

    ulGxSetTextureList(w->tex_list);
    fog = fog_set_enable(0);
    draw_set_blend(4, 1);
    flare_draw(w->tex, cm_sh_col, w->pos, 0, w->scl);
    fog_set_enable(fog);
}

/* 0x00cb1a88 */
int *eden_effect_dshine_ex_create(void *parent, int arg, int frm, float scl)
{
    EdenShineWork *w = task_create_inherit(parent, &eden_effect_dshine_ex_desc, 44);

    task_set_dlist(w, 6);
    w->pos = (Vec3 *)arg;
    w->tex = 2;
    w->time = w->len = frm;
    w->scl_max = scl;
    return (int *)w;
}

/* 0x00cb1b08 */
void eden_effect_flash_quad_draw(Vec3 *pos, int *ang, int rx, float scl_z, unsigned int col, float scl_xy)
{
    int i;

    eden_effect_flash_quad[0].color = eden_effect_flash_quad[1].color = eden_effect_flash_quad[2].color = eden_effect_flash_quad[3].color = eden_effect_flash_quad2[0].color = eden_effect_flash_quad2[1].color = eden_effect_flash_quad2[2].color = eden_effect_flash_quad2[3].color = col;
    draw_set_blend(4, 1);
    ulMatrixPush();
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    eden_effect_rotate_yxz(ang);
    if (rx)
        vu0_matrix_rotate_x(ulMatrixCur, rx);
    vu0_matrix_scale(ulMatrixCur, scl_xy, scl_xy, scl_z);
    tex_strip_begin(1);
    for (i = 4; i; i--) {
        poly_draw_tex(eden_effect_flash_quad, 4);
        poly_draw_tex(eden_effect_flash_quad2, 4);
        vu0_matrix_rotate_z(ulMatrixCur, ANG_90);
    }
    tex_strip_end();
    ulMatrixPop();
}

/* 0x00cb1c40 */
void eden_effect_bflash_exec(EdenFlashWork *w)
{
    float t;

    switch (w->state) {
    case 0:
        t = 1.0f - (float)w->time / (float)w->len;
        w->len_z = cmd_fl_len * t;
        w->col = (w->base_col & 0xffffff) | (color_get_a(COL_WHITE) + 1) >> 1 << 24;
        if (--w->time <= 0) {
            w->state++;
            w->time = w->len = (int)(cmd_fl_fin * FPS);
        }
        break;
    case 1:
        t = (float)w->time / (float)w->len;
        w->len_z = cmd_fl_len * t;
        w->col = (w->base_col & 0xffffff) | ((int)(color_get_a(COL_WHITE) * t) + 1) >> 1 << 24;
        if (--w->time <= 0)
            FINISH_IF(w);
        break;
    }
}

/* 0x00cb1dc8 */
void eden_effect_bflash_disp(EdenFlashWork *w)
{
    int old_flags, old_mode;

    ulGxSetTextureList(w->tex_list);
    old_flags = draw_set_ztest_mode(0x60000);
    old_mode = draw_set_test_mode(0);
    ulGxSelectTexture(ulGxGetTextureList(), w->tex);
    eden_effect_flash_quad_draw(w->pos, w->ang, w->rx, w->len_z, w->col, cmd_fl_size);
    draw_set_ztest_mode(old_flags);
    draw_set_test_mode(old_mode);
}

/* 0x00cb1e58 */
int *eden_effect_bflash_create(void *parent, int arg, int ang)
{
    EdenFlashWork *w = task_create_inherit(parent, &eden_effect_bflash_desc, 48);

    w->pos = (Vec3 *)arg;
    w->ang = (int *)ang;
    w->tex = 3;
    w->base_col = 0x808080;
    w->time = w->len = cmd_fl_st * FPS;
    return (int *)w;
}

/* 0x00cb1ed8 */
int *eden_effect_bflash2_create(void *parent, int arg, int ang)
{
    EdenFlashWork *w = task_create_inherit(parent, &eden_effect_bflash2_desc, 48);

    w->pos = (Vec3 *)arg;
    w->ang = (int *)ang;
    w->tex = 3;
    w->base_col = 0x808080;
    w->rx = ANG_90;
    w->time = w->len = cmd_fl_st * FPS;
    return (int *)w;
}

/* 0x00cb1f60 */
void eden_effect_screen_exec(EdenScreenWork *w)
{
    switch (w->state) {
    case 0:
        w->col = boss_color_blend(cm_sc_col[0], cm_sc_col[1], (float)w->time / (float)w->len);
        if (--w->time <= 0) {
            w->state++;
            w->time = w->len = (int)(cmd_sc_wait * FPS);
        }
        break;
    case 1:
        w->col = cm_sc_col[1];
        if (--w->time <= 0) {
            w->state = 2;
            if (w->end)
                w->state = 3;
            w->time = w->len = (int)(cmd_sc_fade * FPS);
        }
        break;
    case 2: {
        int col = boss_color_blend(cm_sc_col[1], cm_sc_col[0], (float)w->time / (float)w->len);
        int cnt = --w->time;

        w->col = col;
        if (cnt <= 0) {
            do {
                FINISH(w);
            } while (0);
        }
        break;
    }
    }
    w->vtx[0].c = w->vtx[1].c = w->vtx[2].c = w->vtx[3].c = w->col;
}

/* 0x00cb20f0 */
void eden_effect_screen_disp(EdenScreenWork *w)
{
    void *fog;
    float sw = 640.0f;

    fog = fog_set_enable(0);
    draw_set_blend(4, 5);
    nj_color_strip_begin(1);
    w->vtx[0].x = 0;
    w->vtx[2].x = sw;
    w->vtx[3].x = sw;
    w->vtx[0].y = 0;
    w->vtx[1].x = 0;
    w->vtx[2].y = 0;
    w->vtx[1].y = cmd_sc_ty;
    w->vtx[3].y = cmd_sc_ty;
    nj_color_strip_vertices((struct NjColVtx *)w->vtx, 4);
    w->vtx[2].x = sw;
    w->vtx[3].x = sw;
    w->vtx[0].x = 0;
    w->vtx[1].x = 0;
    w->vtx[2].y = 480.0f;
    w->vtx[0].y = 480.0f;
    w->vtx[1].y = 480.0f - cmd_sc_by;
    w->vtx[3].y = 480.0f - cmd_sc_by;
    nj_color_strip_vertices((struct NjColVtx *)w->vtx, 4);
    nj_color_strip_end();
    fog_set_enable(fog);
}

/* 0x00cb21d8 */
int eden_effect_screen_create(void *parent)
{
    EdenScreenWork *w = task_create(parent, &eden_effect_screen_desc, 84);

    w->vtx[0].q = w->vtx[1].q = w->vtx[2].q = w->vtx[3].q = 1.0f;
    w->time = w->len = cmd_sc_fade * FPS;
    return (int)w;
}

/* 0x00cb2240 */
void eden_effect_screen_man_exec(EdenScreenManWork *w)
{
    if (w->screen && (TASK_OF(w->screen)->flags & TASK_F_END))
        w->screen = 0;
}

/* 0x00cb2268 */
void eden_effect_screen_man_release(EdenScreenManWork *w)
{
    FINISH_IF(w->screen);
}

/* 0x00cb2288 */
int *eden_effect_screen_man_create(void *parent)
{
    EdenScreenManWork *w = task_create(parent, &eden_effect_screen_man_desc, 8);

    w->screen = (EdenScreenWork *)eden_effect_screen_create(scr_root);
    return (int *)w;
}

/* 0x00cb22d0 */
void eden_effect_screen_man_end(EdenScreenManWork *w)
{
    EdenScreenWork *screen = w->screen;

    if (screen)
        screen->end = 1;
}

/* 0x00cb22e8 */
void eden_effect_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
