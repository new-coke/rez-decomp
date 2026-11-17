/*
 * Option menu: choice box (shot and overdrive buttons). Values wrap and skip any the setting
 * refuses: the two actions cannot share a button.
 */

#include "title_shared.h"
#include "camera.h"
#include "config.h"
#include "debug.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "pad.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"

/* 44 bytes, table box_select_items */
typedef struct SelectItem {
    unsigned char *name[6];
    unsigned char *(*labels)[6]; /* 0x18: value labels per language */
    int min;                    /* 0x1c */
    int max;                    /* 0x20 */
    int (*get)(void);           /* 0x24 */
    int (*set)(int v);          /* 0x28: 0 = value not usable */
} SelectItem;

extern unsigned int button_masks[6];
extern SelectItem box_select_items[];
extern BoxIcon box_select_icons[2];
extern unsigned int box_select_node_masks[4];
extern struct TaskDesc box_select_desc;
int shot_button_get(void);
int overdrive_button_get(void);
int box_select_node_cb(unsigned int *flags, int node_no, BoxWork *w);
void box_select_print_centered(Vec3 *center, unsigned char *str);
void box_select_exec_close(BoxWork *w);

/* 0x00af8980 */
int shot_button_get(void)
{
    unsigned int bits = ConfigGetShotButton();
    int i;

    for (i = 0; i < 6; i++)
        if (bits & button_masks[i])
            return i;
    return 0;
}

/* 0x00af89d0 */
int shot_button_set(int btn)
{
    if (btn == overdrive_button_get())
        return 0;
    ConfigSetShotButton(button_masks[btn]);
    return 1;
}

/* 0x00af8a20 */
int overdrive_button_get(void)
{
    unsigned int bits = ConfigGetOdButton();
    int i;

    for (i = 0; i < 6; i++)
        if (bits & button_masks[i])
            return i;
    return 1;
}

/* 0x00af8a70 */
int overdrive_button_set(int btn)
{
    if (btn == shot_button_get())
        return 0;
    ConfigSetOdButton(button_masks[btn]);
    return 1;
}

/* 0x00af8ac0 */
void box_select_print_centered(Vec3 *center, unsigned char *str)
{
    Vec3 pos;
    int color = game_menu_color;
    int len = line_length((char *)str);

    pos.x = center->x;
    pos.y = center->y;
    pos.z = center->z;
    pos.x -= floorf((len - 1) * 12 * 0.5f);
    font_print3d(str, &pos, 0, 0, 0, color);
}

/* 0x00af8b68 */
void box_select_init(BoxWork *w, int item)
{
    void *root;

    w->item = item;
    root = pnb_get_entry(w->models, 7)->src;
    dc_object_init((struct MapPart *)&w->anim, root, pnb_get_entry(w->models, 8)->src, 0);
    dc_object_set_callback(&w->anim, (void *)box_select_node_cb, w);
    w->ready = 0;
}

/* 0x00af8be8 */
void box_select_release(BoxWork *w)
{
    dc_object_release((struct MapPart *)&w->anim);
}

/* 0x00af8bf0 */
void box_select_exec(BoxWork *w)
{
    SelectItem *rec;
    int v;
    int prev;
    int ok;

    if (w->state == 0) {
        dc_object_advance_time((struct MapPart *)&w->anim);
        if (w->anim.flags & 1) {
            w->ready = 1;
            w->state++;
        }
    }
    rec = &box_select_items[w->item];
    v = rec->get ? rec->get() : 0;
    prev = v;
    if (pad_system.pad[0]->menu.trig & PAD_LEFT) {
        do {
            v--;
            if (v < rec->min)
                v = rec->max;
            ok = rec->set ? rec->set(v) : 1;
        } while (!ok);
    }
    if (pad_system.pad[0]->menu.trig & PAD_RIGHT) {
        do {
            v++;
            if (rec->max < v)
                v = rec->min;
            ok = rec->set ? rec->set(v) : 1;
        } while (!ok);
    }
    if (v != prev)
        SoundRhythmSe_Common(0);
}

/* 0x00af8d48 */
void box_select_exec_close(BoxWork *w)
{
    switch (w->state) {
    case 0:
        w->state = 1;
        w->anim.rate = -2.0f;
        break;
    case 1:
        if ((w->anim.flags & 1) && w)
            FINISH(w);
        break;
    }
    dc_object_advance_time((struct MapPart *)&w->anim);
}

/* 0x00af8da0 */
void box_select_disp(BoxWork *w)
{
    int color = game_state.menu_color;
    BoxIcon *icon;
    SelectItem *rec;
    int i;
    int lang;
    int v;

    ulGxSetTextureList(w->textures);
    dc_object_draw((struct MapPart *)&w->anim);
    icon = box_select_icons;
    i = 2;
    do {
        i--;
        sprite_draw3d(&w->pos[icon->pos], 1.0f, color, icon->sprite, icon->halign, icon->valign);
        icon++;
    } while (i);
    draw_set_fc_mode(0);
    font_set(2);
    lang = game_state.lang;
    rec = &box_select_items[w->item];
    if (rec->name[lang])
        box_select_print_centered(&w->pos[0], rec->name[lang]);
    v = rec->get ? rec->get() : 0;
    if (rec->labels)
        box_select_print_centered(&w->pos[3], rec->labels[lang][v]);
}

/* 0x00af8f00 */
void *box_select_create(void *parent, int item)
{
    return task_create_inherit_arg(parent, &box_select_desc, 160, item);
}

/* 0x00af8f28 */
void box_select_close(BoxWork *w)
{
    w->state = 0;
    TASK_OF(w)->exec = (void (*)(void *))box_select_exec_close;
}

/* 0x00af8f40 */
int box_select_take_ready(BoxWork *w)
{
    int r = w->ready;

    w->ready = 0;
    return r;
}

/* 0x00af8f50 */
int box_select_node_cb(unsigned int *flags, int node_no, BoxWork *w)
{
    Vec3 *v = &w->pos[0];
    unsigned int bits = *flags;
    unsigned int *mask = box_select_node_masks;
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
