/*
 * Option menu background: models, "options" caption and button marks (adjust/accept only when the
 * item has them). Its hooks give the positions the menu draws at.
 */

#include "title_shared.h"
#include "camera.h"
#include "color.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "title_option_bg.h"

typedef struct BgObj {          /* 12 bytes, table option_bg_objs */
    int light;                  /* 1: lit with the model's light */
    int model;
    void *draw;                 /* or 0 */
} BgObj;

extern BgObj option_bg_objs[];

/* per language */
extern unsigned char *option_bg_caption[];

extern struct TaskDesc option_bg_desc;

extern unsigned int option_bg_node_masks[4];

/* 0x00af8008 */
void option_bg_init(OptionBgWork *w)
{
    BgObj *obj;
    ModelPart *p;
    int n;

    w->count = 3;
    w->parts = heap_block_tag(game_malloc(276), "title_option_bg.c", 161);
    n = w->count;
    obj = option_bg_objs;
    p = w->parts;
    do {
        n--;
        dc_object_init((struct MapPart *)p, pnb_get_entry(w->models, obj->model)->src, 0, 0);
        if (obj->light == 1)
            dc_object_set_wire((struct MapPart *)p, model_pnb_get_wire(w->models, obj->model));
        if (obj->draw)
            dc_object_set_callback(p, obj->draw, w);
        obj++;
        p++;
    } while (n);
}

/* 0x00af80f8 */
void option_bg_release(OptionBgWork *w)
{
    ModelPart *p;
    int n;

    p = w->parts;
    n = w->count;
    do {
        dc_object_release((struct MapPart *)p);
        p++;
    } while (--n);
    heap_free(w->parts, 0);
}

/* 0x00af8150 */
void option_bg_exec(void)
{
}

/* 0x00af8158 */
void option_bg_disp(OptionBgWork *w)
{
    GameState *gs = &game_state;
    Vec3 pos;
    Vec3 *icon;
    ModelPart *p;
    int n;
    int lang;
    int color;
    float scl;

    lang = gs->lang;
    ulGxSetTextureList(w->textures);
    draw_set_color(RGBA(0x3c, 0x28, 0, 0x80));
    color = gs->menu_color;
    p = w->parts;
    n = w->count;
    do {
        dc_object_draw((struct MapPart *)p);
        p++;
    } while (--n);
    scl = 1.0f;
    draw_set_fc_mode(0);
    icon = &w->icons;
    pos.x = icon->x;
    pos.y = icon->y;
    pos.z = icon->z;
    sprite_draw3d(&pos, scl, color, 11, 0, 1);
    pos.x += 115.0f;
    if (w->icon12)
        sprite_draw3d(&pos, scl, color, 12, 0, 1);
    pos.x += 147.0f;
    if (w->icon14)
        sprite_draw3d(&pos, scl, color, 14, 0, 1);
    pos.x += 144.0f;
    sprite_draw3d(&pos, scl, color, 13, 0, 1);
    font_set(3);
    font_print3d(option_bg_caption[lang], &w->caption, 0, 0, 0, color);
}

/* 0x00af8318 */
void *option_bg_create(OptionWork *w)
{
    return task_create_inherit(w, &option_bg_desc, 72);
}

/* 0x00af8340 */
int option_bg_node_cb(unsigned int *flags, int node_no, OptionBgWork *w)
{
    Vec3 *v = &w->caption;
    unsigned int bits = *flags;
    unsigned int *mask = option_bg_node_masks;
    float *mat;
    int i;

    for (i = 3; i >= 0; i--) {
        mat = camera_get_inv_mtx();
        if (bits & *mask) {
            ulMatrixGetTranslation(ulMatrixCur, v);
            ulMatrixApplyPoint(mat, v, v);
        }
        v++;
        mask++;
    }
    return 1;
}

/* 0x00af83e8 */
void option_bg_get_anchor(OptionBgWork *w, Vec3 *out)
{
    Vec3 *anc = &w->anchor;

    out->x = anc->x;
    out->y = anc->y;
    out->z = anc->z;
}

/* 0x00af8408 */
void option_bg_set_icons(OptionBgWork *w, int icon12, int icon14)
{
    w->icon12 = icon12;
    w->icon14 = icon14;
}
