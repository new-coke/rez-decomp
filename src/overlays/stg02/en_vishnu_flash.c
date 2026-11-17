/* Vishnu's hand flash, Area 2: muzzle glow while a hand charges its laser. */

#include "stg02.h"
#include "debug.h"
#include "model.h"
#include "pnb.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_vishnu_flash.h"

extern struct TaskDesc en_vishnu_flash_desc;

typedef struct EnVishnuFlashWork {
    Vec3 *pos;
    ModelPart part;                /* 0x04 */
} EnVishnuFlashWork;

/* 0x00b0f608 */
void en_vishnu_flash_exec(int *w)
{
    dc_object_advance_time((struct MapPart *)(w + 1));
    if (w[7] & 1)
        FINISH_IF(w);
}

/* 0x00b0f658 */
void en_vishnu_flash_disp(EnVishnuFlashWork *w)
{
    if (view_sphere_visible(w->pos, w->part.radius)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        dc_object_draw(&w->part);
        ulMatrixPop();
    }
}

/* steer(cast) */
/* 0x00b0f6e0 */
void en_vishnu_flash_init(void *work, Vec3 *pos)
{
    EnVishnuFlashWork *w = work;
    int *ent;

    w->pos = pos;
    ent = ((int *)pnb_get_entry(stage_models, 87));
    dc_object_init(&w->part, *ent, *((struct DcClip **)pnb_get_entry(stage_models, 88)), 0);
    dc_object_rebuild(&w->part);
    w->part.speed = w->part.frames / FPS;
}

/* 0x00b0f778 */
void en_vishnu_flash_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 4));
}

/* 0x00b0f780 */
EnVishnuFlashView *en_vishnu_flash_create(void *parent, Vec3 *pos)
{
    EnVishnuFlashView *w = task_create(parent, &en_vishnu_flash_desc, 96);

    en_vishnu_flash_init(w, pos);
    return w;
}
