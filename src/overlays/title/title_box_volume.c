/* Option menu: music and effect volume box, a row of bars; clicks on change. */

#include "title_shared.h"
#include "billboard.h"
#include "camera.h"
#include "config.h"
#include "debug.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"

typedef struct VolumeItem { unsigned char *name[6]; } VolumeItem;
extern VolumeItem box_volume_names[];
extern BoxIcon box_volume_icons[3];
extern unsigned int box_volume_node_masks[4];
extern struct TaskDesc box_volume_desc;
int box_volume_node_cb(unsigned int *flags, int node_no, BoxWork *w);
void box_volume_exec_close(BoxWork *w);

/* 0x00af8ff8 */
void box_volume_init(BoxWork *w, int item)
{
    void *root;

    w->item = item;
    root = pnb_get_entry(w->models, 9)->src;
    dc_object_init((struct MapPart *)&w->anim, root, pnb_get_entry(w->models, 10)->src, 0);
    dc_object_set_callback(&w->anim, (void *)box_volume_node_cb, w);
    w->ready = 0;
}

/* 0x00af9078 */
void box_volume_release(BoxWork *w)
{
    dc_object_release((struct MapPart *)&w->anim);
}

/* 0x00af9080 */
void box_volume_exec(BoxWork *w)
{
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
    if (w->item == 0)
        v = ConfigGetVolSeq();
    else
        v = ConfigGetVolSe();
    prev = v;
    trig = pad_main->menu.trig;
    if (trig & 0x40)
        v = v - 1;
    if (trig & 0x80)
        v = v + 1;
    if (v != prev) {
        if (w->item == 0) {
            ConfigSetVolSeq(v);
            v = ConfigGetVolSeq();
        } else {
            ConfigSetVolSe(v);
            v = ConfigGetVolSe();
        }
        if (v != prev)
            SoundRhythmSe_Common(0);
    }
}

/* 0x00af9188 */
void box_volume_exec_close(BoxWork *w)
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

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* 0x00af91e0 */
void box_volume_disp(BoxWork *w)
{
    GameState *gs;
    Vec3 pos;
    Vec3 bar;
    Vec3 *ipos;
    int tex;
    float *uv;
    float sw;
    float sh;
    VolumeItem *rec;
    BoxIcon *icon;
    unsigned char *str;
    int color;
    int lang;
    int i;
    int n;
    float x;
    float y;
    int j;

    rec = &box_volume_names[w->item];
    gs = &game_state;
    color = gs->menu_color;
    lang = gs->lang;
    ulGxSetTextureList(w->textures);
    dc_object_draw((struct MapPart *)&w->anim);
    draw_set_fc_mode(0);
    str = rec->name[lang];
    x = floorf((line_length((char *)str) - 1) * 12 * 0.5f);
    pos.y = w->pos[0].y;
    pos.z = w->pos[0].z;
    pos.x = w->pos[0].x - x;
    font_set(2);
    font_print3d(str, &pos, 0, 0, 0, color);
    icon = box_volume_icons;
    i = 3;
    do {
        i--;
        ipos = &w->pos[icon->pos];
        sprite_draw3d(ipos, 1.0f, color, icon->sprite, icon->halign, icon->valign);
        icon++;
    } while (i);
    if (w->item == 0)
        n = ConfigGetVolSeq();
    else
        n = ConfigGetVolSe();
    if (n > 0) {
        v3_copy(&bar, &w->pos[1]);
        j = 0;
        sprite_get(4, &tex, &uv, &sw, &sh);
        x = bar.x - floorf(sw * 0.5f);
        y = bar.y;
        do {
            sprite_get(j++, &tex, &uv, &sw, &sh);
            n--;
            bar.x = x + floorf(sw * 0.5f);
            bar.y = y + floorf(sh * 0.5f);
            billboard_draw_uv(tex, color, &bar, uv, sw, sh);
            x += sw;
        } while (n);
    }
}

/* 0x00af9468 */
void *box_volume_create(void *parent, int item)
{
    return task_create_inherit_arg(parent, &box_volume_desc, 160, item);
}

/* 0x00af9490 */
void box_volume_close(BoxWork *w)
{
    w->state = 0;
    TASK_OF(w)->exec = (void (*)(void *))box_volume_exec_close;
}

/* 0x00af94a8 */
int box_volume_take_ready(BoxWork *w)
{
    int r = w->ready;

    w->ready = 0;
    return r;
}

/* 0x00af94b8 */
int box_volume_node_cb(unsigned int *flags, int node_no, BoxWork *w)
{
    Vec3 *v = &w->pos[0];
    unsigned int bits = *flags;
    unsigned int *mask = box_volume_node_masks;
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
