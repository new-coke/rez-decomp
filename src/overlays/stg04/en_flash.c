/* Area 4 muzzle flash: plays once at the followed position when a chaser or hopper fires. */

#include "stg04.h"
#include "debug.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_flash.h"
#include "../common/en_perfectwall.h"

extern struct TaskDesc en_flash_desc;

/* 108 bytes */
typedef struct EnFlashWork {
    Vec3 *pos;                  /* followed */
    ModelPart part;                /* 0x04 */
    unsigned int *motion;       /* 0x60: model 138's entry (word 1: frame count) */
    float scale;                /* 0x64 */
    float alpha;                /* 0x68 */
} EnFlashWork;

extern float en_flash_color[4];

/* 0x00b03410 */
void en_flash_exec(EnFlashWork *w)
{
    ModelPart *part = &w->part;
    unsigned int *mot;
    float rate;

    dc_object_advance_time(part);
    mot = w->motion;
    rate = part->frame / part->frames;
    w->alpha = motion_sample_alpha(mot, ((float)mot[1] - 1.0f) * rate, 0.0f, 0.0f, 0.0f, 0.0f);
    if (part->flags2 & 1)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00b034d8 */
void en_flash_disp(EnFlashWork *w)
{
    ModelPart *part = &w->part;
    Vec3 *pos = w->pos;
    int blend;
    int dl;
    float scl;

    if (!view_sphere_visible(pos, part->radius))
        return;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    dl = RenderSetCurrentDisplayList(6);
    scl = w->scale;
    en_flash_color[0] = w->alpha - 1.0f;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_flash_color);
    ulGxSetTextureList(stage_textures);
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
    dc_object_draw(part);
    RenderSetCurrentDisplayList(dl);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* steer(cast) */
/* 0x00b03630 */
void en_flash_init(EnFlashWork *w, EnFlashArg *prm)
{
    ModelPart *part = &w->part;
    int *mdl;
    int *mot;

    w->pos = (Vec3 *)prm->v;
    w->scale = prm->scl;
    mdl = ((int *)pnb_get_entry(stage_models, 137));
    mot = ((int *)pnb_get_entry(stage_models, 138));
    dc_object_init(part, *mdl, *mot, 0);
    dc_object_rebuild(part);
    PART_MOTION(part)->speed = part->frames / prm->frm;
    w->motion = (unsigned int *)*((int *)pnb_get_entry(stage_models, 138));
}

/* 0x00b036f0 */
void en_flash_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 4));
}

/* 0x00b036f8 */
void *en_flash_create(void *parent, void *prm)
{
    return task_create_arg(parent, &en_flash_desc, 108, prm);
}

/* 0x00b03720 */
void *en_flash_spawn(void *parent, int pos)
{
    EnFlashArg prm;

    prm.v = pos;
    prm.frm = 24.0f;
    prm.scl = 1.0f;
    return en_flash_create(parent, &prm);
}
