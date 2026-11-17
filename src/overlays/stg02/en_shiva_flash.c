/* Shiva's muzzle flash, Area 2: a sprite below the hatch, flares and dies over two seconds. */

#include "stg02.h"
#include "billboard.h"
#include "color.h"
#include "node_draw_state.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "en_shiva_flash.h"

extern struct TaskDesc en_shiva_flash_desc;

/* size follows en_shiva_flash_curve(t / 120) */
typedef struct EnShivaFlashWork {
    int pos;
    float size;                  /* 0x04 */
    float t;                    /* 0x08: frames */
} EnShivaFlashWork;

/* drawn 3 below the point it follows */
typedef struct EnShivaFlash {
    Vec3 *pos;
    float size;                 /* 0x04 */
} EnShivaFlash;

/* 0x00b0bb50 */
float en_shiva_flash_curve(float t)
{
    float sc[4];
    float val;

    if (t <= 0.2f) {
        ulSinCos((int)(t * ANG_QUARTER / 0.2f), &sc[0], &sc[1]);
        val = sc[0];
    } else {
        ulSinCos((int)((t - 0.2f) * ANG_QUARTER / 0.8f), &sc[2], &sc[3]);
        val = sc[3];
    }
    return val * 10.0f + 10.0f;
}

/* 0x00b0bc00 */
void en_shiva_flash_exec(EnShivaFlashWork *w)
{
    w->t += 1.0f;
    if (120.0f < w->t)
        FINISH_IF(w);
    else
        w->size = en_shiva_flash_curve(w->t / 120.0f);
}

/* 0x00b0bc78 */
void en_shiva_flash_disp(EnShivaFlash *w)
{
    Vec3 pos;
    int dl;

    draw_set_blend(4, 1);
    v3_copy(&pos, w->pos);
    pos.y += -3.0f;
    dl = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(stage_textures);
    billboard_draw(3, COL_WHITE, &pos, w->size, w->size);
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b0bd18 */
void en_shiva_flash_init(int *w, int pos)
{
    *w = pos;
}

/* 0x00b0bd20 */
void en_shiva_flash_release(void)
{
}

/* 0x00b0bd28 */
void *en_shiva_flash_create(void *parent, Vec3 *pos)
{
    void *w = task_create(parent, &en_shiva_flash_desc, 12);

    en_shiva_flash_init(w, (int)pos);
    return w;
}
