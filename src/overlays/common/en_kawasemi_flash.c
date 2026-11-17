/*
 * Muzzle flash of the Hunter and the Chaser: two billboard models played once at a gun point,
 * following it or left in place and carried with the player; alpha from a motion curve or sine.
 */

#include "types_common.h"

#include "types_overlays.h"

#define PART_MOTION(p) ((ModelPart *)(p))

#include "debug.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "task.h"
#include "en_kawasemi_flash.h"
#include "en_perfectwall.h"
#include "system.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

extern int ulMatrixPush(void);

extern void *stage_textures;
extern void *stage_models;

extern struct TaskDesc en_kawasemi_flash_desc;
extern float en_kawasemi_flash_scales[];
extern float en_kawasemi_flash_rgba[4];
extern int en_kawasemi_flash_models[2];
extern int en_kawasemi_flash_motions[2];
extern int en_kawasemi_flash_sources[2];
extern float en_kawasemi_flash_lengths[];

/* 0x00b087a0 stg04, 0x00b04dc0 stg22 */
void en_kawasemi_flash_exec(EnKawasemiFlashWork *w)
{
    Vec3 v;
    int i;
    int j;

    if (!w->follow) {
        player_get_frame_vel(&v);
        v3_add(&w->pos, &v);
    }
    switch (w->kind) {
    case 0:
    case 1:
        for (i = 1; i >= 0; i--) {
            ModelPart *part = &w->part[i];

            dc_object_advance_time(part);
            w->alpha[i] = motion_sample_alpha(w->motion[i], part->frame, 0.0f, 0.0f, 0.0f, 0.0f) - 1.0f;
        }
        break;
    case 2:
        for (j = 1; j >= 0; j--) {
            ModelPart *part = &w->part[j];

            dc_object_advance_time(part);
            w->alpha[j] = -ease_sine_in_out(part->frame / part->frames);
        }
        break;
    }
    if (w->part[0].flags2 & 1)
        FINISH_IF(w);
}

/* 0x00b08920 stg04, 0x00b04f40 stg22 */
/* steer(cast) */
void en_kawasemi_flash_disp(EnKawasemiFlashWork *w)
{
    Vec3 *pos = w->follow ? w->follow : &w->pos;
    int blend;
    int dl;
    int i;
    float scl;

    if (!view_sphere_visible(pos, w->part[0].radius))
        return;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    dl = RenderSetCurrentDisplayList(6);
    scl = en_kawasemi_flash_scales[w->kind];
    ulGxSetTextureList(stage_textures);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
    for (i = 2; --i >= 0;) {
        en_kawasemi_flash_rgba[0] = w->alpha[i];
        draw_set_override_color(en_kawasemi_flash_rgba);
        dc_object_draw(&w->part[i]);
    }
    RenderSetCurrentDisplayList(dl);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b08aa8 stg04, 0x00b050c8 stg22 */
/* steer(cast) */
void en_kawasemi_flash_init(void *work, void *pt, int mode)
{
    EnKawasemiFlashWork *w = work;
    Vec3 *pos = pt;
    Vec3 v;
    int i;
    int *mdl;
    int *mot;

    w->kind = mode;
    if (mode == 0)
        w->follow = pos;
    else if ((unsigned int)mode < 3)
        v3_copy(&w->pos, pos);
    if (!w->follow) {
        player_get_frame_vel(&v);
        v3_sub(&w->pos, &w->pos, &v);
    }
    for (i = 1; i >= 0; i--) {
        mdl = ((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, en_kawasemi_flash_models[i]);
        mot = ((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, en_kawasemi_flash_motions[i]);
        dc_object_init(&w->part[i], *mdl, *mot, 0);
        dc_object_rebuild(&w->part[i]);
        PART_MOTION(&w->part[i])->speed = PART_MOTION(&w->part[i])->frames / en_kawasemi_flash_lengths[mode];
        w->motion[i] = (unsigned int *)*((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, en_kawasemi_flash_sources[i]);
    }
}

/* 0x00b08c50 stg04, 0x00b05270 stg22 */
void en_kawasemi_flash_release(EnKawasemiFlashWork *w)
{
    int i;
    ModelPart *part = &w->part[2];

    for (i = 2; i != 0; i--) {
        part--;
        dc_object_release(part);
    }
}

/* 0x00b08c98 stg04, 0x00b052b8 stg22 */
void *en_kawasemi_flash_create(void *parent, void *pos, int mode)
{
    void *w = task_create(parent, &en_kawasemi_flash_desc, 220);

    en_kawasemi_flash_init(w, pos, mode);
    return w;
}
