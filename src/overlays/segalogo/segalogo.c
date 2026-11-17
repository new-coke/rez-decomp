/*
 * Boot logos, cross-faded in turn; each stays two seconds or until a press where skippable
 * (always in debug mode). The disc directory is read during the first fade-in.
 */

#include "mathdef.h"
#include "types_common.h"
#include "billboard.h"
#include "dir_read.h"
#include "fade.h"
#include "float_bits.h"
#include "fog.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pnb.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_texture.h"
#include "task.h"

/* 20 bytes, table at logo_table */
typedef struct Logo {
    int kind;                   /* 0 full-screen image, 1 textured quad at x, y */
    int skip;                   /* non-zero: a press skips it early */
    int idx;
    float x;
    float y;
} Logo;

typedef struct SegalogoWork {
    void *textures;
    int state;
    int timer;
    int logo;
    float fade;                 /* 0..1 */
    int visible;
} SegalogoWork;

/* 0x40 bytes; the work area follows */
extern Pad *pad_main;

extern unsigned int pad_decide_mask;
extern unsigned int system_flags;
extern char logo_textures[];

extern Logo logo_table[];

extern struct TaskDesc segalogo_desc;

#include "color.h"
void logo_draw_quad(void *textures, int idx, int color, float z, float x, float y);
int logo_draw(void *textures, int i, int color, float z, int layer);

/* steer(cast) */
/* 0x00af6700 */
void segalogo_init(SegalogoWork *w)
{
    system_flags |= SYS_TITLE_LOOP;
    w->textures = model_create_timed(logo_textures, 0x4000);
    w->state = 0;
    w->timer = 0;
    w->logo = 0;
    fog_set_enable(0);
    fog_set(COL_BLACK, 0, 1000.0f, 2000.0f);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    fade_start(0, 1, 0);
    pad_input_disable(0);
    w->fade = 0;
    w->visible = 1;
}

/* 0x00af67a8 */
void segalogo_release(SegalogoWork *w)
{
    model_list_remove(w->textures);
}

/* 0x00af67b0 */
void segalogo_exec(SegalogoWork *w)
{
    Logo *ent = &logo_table[w->logo];
    int pressed = 0;

    if (pad_main->menu.trig & ((pad_cancel_mask | 8) | pad_decide_mask))
        pressed = 1;

    switch (w->state) {
    case 0:
        rofs_dir_read_all();
        pad_input_enable(0);
        fade_start_out(60);
        w->state++;
        break;
    case 1:
        if (fade_is_idle() == 1) {
            w->state = 100;
            w->timer = 0;
        }
        break;
    case 100:
        if ((ent->skip || (system_flags & SYS_DEBUG)) && pressed)
            w->state = 200;
        if (120.0f <= (float)w->timer++)
            w->state = 200;
        break;
    case 200:
        if ((unsigned)w->logo >= 2) {
            w->state = 400;
        } else {
            w->fade += 0.02f;
            if (1.0f <= w->fade) {
                w->state = 100;
                w->logo++;
                w->timer = 0;
                w->fade = 0;
            }
        }
        break;
    case 400:
        fade_start(0, 1, 60);
        pad_input_disable(0);
        w->state++;
        break;
    case 401:
        if (fade_is_idle() == 1 && w)
            ((Task *)w - 1)->flags |= 1;
        break;
    }
}

/* 0x00af69c8 */
int segalogo_disp(SegalogoWork *w)
{
    float z;
    float scale;
    float rate;
    int i;
    int gray;
    int level;

    if (w->visible) {
        z = 0.002f;
        ulGxSetTextureList(w->textures);
        ulGxSetTexFilter(1);
        rate = w->fade;
        if (fabsf(rate) < FLT_EPSILON) {
            return logo_draw(w->textures, w->logo, COL_WHITE, z, 0);
        } else {
            scale = 255.0f;
            i = w->logo;
            level = (int)((1.0f - rate) * scale);
            gray = ((level + 1) >> 1) & 0xff;
            logo_draw(w->textures, i++, RGBA(gray, gray, gray, 0x80), z, 0);
            draw_set_blend(4, 1);
            z = float_add_bits16(1, z);
            level = (int)(w->fade * scale);
            return logo_draw(w->textures, i, RGBA(0x80, 0x80, 0x80, (level + 1) >> 1), z, 1);
        }
    }
}

/* 0x00af6b18 */
void *segalogo_create(void *parent)
{
    return task_create(parent, &segalogo_desc, 24);
}

/* 0x00af6b40 */
void logo_draw_screen(void *textures, int idx, int color, float z)
{
    screen_image_draw(idx, color, z);
}

/* 0x00af6b50 */
void logo_draw_quad(void *textures, int idx, int color, float z, float x, float y)
{
    float wd = ulTextureGetWidth(ulTexListGet(textures, idx));

    tex_rect_draw(idx, color, x, y, z, wd, ulTextureGetHeight(ulTexListGet(textures, idx)));
}

/* 0x00af6c00 */
int logo_draw(void *textures, int i, int color, float z, int layer)
{
    Logo *ent = &logo_table[i];

    switch (ent->kind) {
    case 0:
        logo_draw_screen(textures, ent->idx, color, z);
        break;
    case 1:
        logo_draw_quad(textures, ent->idx, color, z, ent->x, ent->y);
        break;
    }
}
