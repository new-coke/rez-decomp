/*
 * Option menu: on/off box (sound mode, up/down, vibration, HUD, points). Nodes light the set side;
 * turning vibration on gives a test pulse.
 */

#include "title_shared.h"
#include "camera.h"
#include "config.h"
#include "debug.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "pnb.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "title_option.h"

/* 80 bytes, table box_onoff_items */
typedef struct OnOffItem {
    unsigned char *name[6];
    unsigned char *on[6];       /* 0x18 */
    unsigned char *off[6];      /* 0x30 */
    int (*get)(void);           /* 0x48 */
    void (*set)(int v);         /* 0x4c */
} OnOffItem;

/* 148 bytes */
typedef struct BoxOnOff {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int item;                   /* 0x08 */
    Anim anim;                  /* 0x0c */
    Vec3 on_pos;                /* 0x68 */
    Vec3 off_pos;               /* 0x74 */
    Vec3 name_pos;              /* 0x80 */
    int ready;                  /* 0x8c: opened (read and cleared by box_onoff_take_ready) */
    int state;                  /* 0x90 */
} BoxOnOff;

extern OnOffItem box_onoff_items[];
extern unsigned int box_onoff_node_masks[3];
extern struct TaskDesc box_onoff_desc;
int box_onoff_node_cb(unsigned int *flags, int node_no, BoxOnOff *w);
void box_onoff_print_centered(Vec3 *center, unsigned char *str);
void box_onoff_exec_close(BoxOnOff *w);

/* 0x00af8418 */
int sound_mode_get(void)
{
    return option_get_stereo();
}

/* 0x00af8438 */
void sound_mode_set(int v)
{
    option_set_stereo(v);
}

/* 0x00af8440 */
int updown_get(void)
{
    return ConfigGetUpdown();
}

/* 0x00af8460 */
void updown_set(int v)
{
    ConfigSetUpdown(v);
}

/* 0x00af8468 */
int vibration_get(void)
{
    return ConfigGetVibration();
}

/* 0x00af8488 */
void vibration_set(int v)
{
    ConfigSetVibration(v);
    if (v)
        peri_act_play_number(5);
}

/* 0x00af84c0 */
int heads_up_display_get(void)
{
    return ConfigGetDispConsole();
}

/* 0x00af84e0 */
void heads_up_display_set(int v)
{
    ConfigSetDispConsole(v);
}

/* 0x00af84e8 */
int point_display_get(void)
{
    return ConfigGetDispScore();
}

/* 0x00af8508 */
void point_display_set(int v)
{
    ConfigSetDispScore(v);
}

/* 0x00af8510 */
void box_onoff_print_centered(Vec3 *center, unsigned char *str)
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

/* 0x00af85b8 */
void box_onoff_init(BoxOnOff *w, int item)
{
    void *root;

    w->item = item;
    root = pnb_get_entry(w->models, 5)->src;
    dc_object_init((struct MapPart *)&w->anim, root, pnb_get_entry(w->models, 6)->src, 0);
    dc_object_set_callback(&w->anim, (void *)box_onoff_node_cb, w);
    w->ready = 0;
}

/* 0x00af8638 */
void box_onoff_release(BoxOnOff *w)
{
    dc_object_release((struct MapPart *)&w->anim);
}

/* 0x00af8640 */
void box_onoff_exec(BoxOnOff *w)
{
    OnOffItem *rec;
    int v;
    int prev;
    unsigned int trig;

    if (w->state == 0) {
        dc_object_advance_time((struct MapPart *)&w->anim);
        if (w->anim.flags & 1) {
            w->ready = 1;
            w->state++;
        }
    }
    rec = &box_onoff_items[w->item];
    v = rec->get ? rec->get() : 0;
    prev = v;
    trig = pad_main->menu.trig;
    if (trig & 0x40)
        v = 0;
    if (trig & 0x80)
        v = 1;
    if (v != prev) {
        if (rec->set)
            rec->set(v);
        SoundRhythmSe_Common(0);
    }
}

/* 0x00af8718 */
void box_onoff_exec_close(BoxOnOff *w)
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

/* 0x00af8770 */
void box_onoff_disp(BoxOnOff *w)
{
    OnOffItem *rec;
    int lang;

    ulGxSetTextureList(w->textures);
    dc_object_draw((struct MapPart *)&w->anim);
    draw_set_fc_mode(0);
    font_set(2);
    rec = &box_onoff_items[w->item];
    lang = game_lang;
    box_onoff_print_centered(&w->name_pos, rec->name[lang]);
    box_onoff_print_centered(&w->on_pos, rec->on[lang]);
    box_onoff_print_centered(&w->off_pos, rec->off[lang]);
}

/* 0x00af8810 */
void *box_onoff_create(void *parent, int item)
{
    return task_create_inherit_arg(parent, &box_onoff_desc, 148, item);
}

/* 0x00af8838 */
void box_onoff_close(BoxOnOff *w)
{
    w->state = 0;
    TASK_OF(w)->exec = (void (*)(void *))box_onoff_exec_close;
}

/* 0x00af8850 */
int box_onoff_take_ready(BoxOnOff *w)
{
    int r = w->ready;

    w->ready = 0;
    return r;
}

/* Stores text positions; shows the "off"/"on" marker nodes only on the set side. */
/* 0x00af8860 */
int box_onoff_node_cb(unsigned int *flags, int node_no, BoxOnOff *w)
{
    Vec3 *v = &w->on_pos;
    unsigned int bits = *flags;
    unsigned int *mask = box_onoff_node_masks;
    OnOffItem *rec;
    float *mat;
    int i;

    for (i = 2; i >= 0; i--) {
        mat = camera_get_inv_mtx();
        if (bits & *mask) {
            ulMatrixGetTranslation(ulMatrixCur, v);
            ulMatrixApplyPoint(mat, v, v);
        }
        v++;
        mask++;
    }
    if (bits & 0x4000000) {
        rec = &box_onoff_items[w->item];
        return !rec->get();
    }
    if (bits & 0x8000000) {
        rec = &box_onoff_items[w->item];
        return rec->get();
    }
    return 1;
}
