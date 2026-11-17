/*
 * Area choose screen: the selected area's background and record panel (artist, track, best
 * analysis, shot down and item rates).
 */

#include "areacho.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "fog.h"
#include "heap.h"
#include "libc.h"
#include "model.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "ps_gs.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ac_bg.h"

/* 16 bytes; tables per area at ac_bg_areas */
typedef struct AcObj {
    int light;                  /* 1: lit with the model's light */
    unsigned int color;         /* 0x04 */
    int model;                  /* 0x08 */
    int clip;                   /* 0x0c: < 0 = none */
} AcObj;

typedef struct AcArea {         /* 12 bytes */
    AcObj *objs;
    int count;                  /* 0x04 */
    unsigned int fog;           /* 0x08 */
} AcArea;

typedef struct AcVtx {
    float x, y, z;
    float u, v;
    unsigned int color;
} AcVtx;

extern AcArea ac_bg_areas[6];
extern unsigned int ac_bg_node_masks[8];
extern struct TaskDesc ac_bg_desc;
extern unsigned char *artist_label_texts[];
extern unsigned char *track_label_texts[];
extern unsigned char *area_artist_names[];
extern unsigned char *area_track_titles[][2];

typedef union { float f; int i; } AcWord;

/* Stores each text anchor node's camera-space position in w->pts. */
/* 0x00ca0a40 */
int ac_bg_node_cb(unsigned int *flags, int node_idx, AcBg *w)
{
    Vec3 *v = &w->pts[0];
    unsigned int fl = *flags;
    unsigned int *mask = ac_bg_node_masks;
    float *mat;
    int i;

    for (i = 7; i >= 0; i--) {
        mat = camera_get_inv_mtx();
        if (fl & *mask) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &v->x);
            ulMatrixApplyPoint(mat, &v->x, &v->x);
        }
        v++;
        mask++;
    }
    return 1;
}

/* 0x00ca0ae8 */
void ac_bg_preload_lights(AcBg *w)
{
    AcObj *ob;
    int i, n;

    for (i = 0; i < 6; i++) {
        ob = ac_bg_areas[i].objs;
        n = ac_bg_areas[i].count;
        if (ob)
            do {
                if (ob->light == 1)
                    model_pnb_get_wire(w->models, ob->model);
                ob++;
            } while (--n);
    }
}

/* steer(cast) */
/* 0x00ca0bb0 */
AcPart *ac_bg_parts_create(AcBg *w, AcObj *ob, int n)
{
    AcPart *parts, *p;
    int root, clip;

    parts = heap_block_tag(game_malloc(n * 96), "ac_bg.c", 310);
    p = parts;
    do {
        root = *((int *)pnb_get_entry(w->models, ob->model));
        clip = ob->clip >= 0 ? *((int *)pnb_get_entry(w->models, ob->clip)) : 0;
        dc_object_init(&p->p, root, clip, 0);
        switch (ob->light) {
        case 0:
            break;
        case 1:
            dc_object_set_wire(&p->p, model_pnb_get_wire(w->models, ob->model));
            p->color = ob->color;
            break;
        }
        ob++;
        p++;
    } while (--n);
    return parts;
}

/* 0x00ca0cc0 */
void ac_bg_parts_release(AcPart *p, int n)
{
    if (p && n > 0)
        do {
            dc_object_release(&p->p);
            p++;
        } while (--n);
}

/* 0x00ca0d10 */
void ac_bg_draw_quad(float x, float y, float u, float v, float width, float height)
{
    AcVtx vtx[4];
    float u_left, u_right, v_top, v_bottom, right, bottom;

    vtx[3].color = COL_WHITE;
    vtx[2].color = COL_WHITE;
    vtx[1].color = COL_WHITE;
    vtx[0].color = COL_WHITE;
    vtx[3].z = -413.0f;
    vtx[2].z = -413.0f;
    vtx[1].z = -413.0f;
    vtx[0].z = -413.0f;
    bottom = y - height;
    u_left = u / 256.0f;
    u_right = (u + width) / 256.0f;
    v_bottom = (v + height) / 256.0f;
    right = x + width;
    v_top = v / 256.0f;
    vtx[0].x = right;
    vtx[0].y = y;
    vtx[0].u = u_right;
    vtx[0].v = v_top;
    vtx[1].x = right;
    vtx[1].y = bottom;
    vtx[1].u = u_right;
    vtx[1].v = v_bottom;
    vtx[2].x = x;
    vtx[2].y = y;
    vtx[2].u = u_left;
    vtx[2].v = v_top;
    vtx[3].x = x;
    vtx[3].y = bottom;
    vtx[3].u = u_left;
    vtx[3].v = v_bottom;
    nj_clip_tex_strip_begin(1);
    nj_clip_tex_strip_vertices(vtx, 4);
    nj_clip_tex_strip_end();
}

/* steer(cast) */
/* 0x00ca0de0 */
void ac_bg_init(AcBg *w, int area)
{
    ModelPart *pnl;
    int root;

    w->area = area;
    do {
        fog_set(COL_BLACK, 4, 1000.0f, 4000.0f);
    } while (0);
    fog_set_enable((void *)1);
    pnl = &w->part;
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    w->state = 100;
    root = *((int *)pnb_get_entry(w->models, 2));
    dc_object_init(pnl, root, *((struct DcClip **)pnb_get_entry(w->models, 3)), 0);
    dc_object_set_callback(pnl, (int (*)())ac_bg_node_cb, w);
    ac_bg_preload_lights(w);
}

/* 0x00ca0e98 */
void ac_bg_release(AcBg *w)
{
    dc_object_release(&w->part);
    ac_bg_parts_release(w->parts, w->count);
    heap_free(w->parts, 0);
    w->parts = 0;
}

/* 0x00ca0ee0 */
void ac_bg_exec(AcBg *w)
{
    AcArea *area_rec;
    int done;

    w->part.speed = w->speed;
    dc_object_advance_time(&w->part);
    done = w->part.flags2 & 1;
    switch (w->state) {
    case 0:
        if (w->cur != w->area) {
            w->state = 1;
            w->speed = -1.0f;
        }
        break;
    case 1:
        if (done)
            w->state = 100;
        break;
    case 100:
        if (w->parts) {
            ac_bg_parts_release(w->parts, w->count);
            heap_free(w->parts, 0);
        }
        area_rec = &ac_bg_areas[w->area];
        w->cur = w->area;
        fog_req_color(area_rec->fog);
        w->parts = ac_bg_parts_create(w, area_rec->objs, w->count = area_rec->count);
        w->speed = 1.0f;
        w->state = 0;
        break;
    }
}

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* 0x00ca1000 */
void ac_bg_disp(AcBg *w)
{
    Vec3 pos, tpos, step;
    AcPart *p;
    unsigned char *str;
    float *rec;
    int n, prev_dl, col, lang;
    float x, y, u, bar_w, bar_h, full, char_w, bar_len, line_h, dy, bar_v;

    ulGxSetTextureList(w->textures);
    psGsSetAlphaTest(0, 1, 128, 1);
    if (w->parts) {
        p = w->parts;
        n = w->count;
        do {
            draw_set_color(p->color);
            dc_object_draw(&p->p);
            p++;
        } while (--n);
    }
    dc_object_draw(&w->part);
    u = 87.0f;
    psGsSetAlphaTest(1, 5, 128, 1);
    bar_h = 14.0f;
    full = 100.0f;
    prev_dl = RenderSetCurrentDisplayList(6);
    y = -113.0f;
    bar_w = 169.0f;
    draw_set_blend(4, 5);
    draw_set_fc_mode(0);
    bar_v = 28.0f;
    char_w = 12.0f;
    rec = ConfigGetStageRecord(w->cur);
    line_h = 19.0f;
    dy = 2.0f;
    sprintf(w->buf[0], "%18.2f%%", (double)rec[0]);
    sprintf(w->buf[1], "%18.2f%%", (double)rec[1]);
    sprintf(w->buf[2], "%18.2f%%", (double)rec[2]);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    ac_bg_draw_quad(full, y, u, 0.0f, bar_w, bar_h);
    x = full;
    bar_len = bar_w;
    full = 0.0f;
    bar_w = 0.0f;
    ac_bg_draw_quad(x, y, u, bar_h, rec[0] * bar_len / x, bar_h);
    bar_h = 5.0f;
    y = -148.0f;
    ac_bg_draw_quad(x, y, u, bar_v, bar_len, bar_h);
    y = -178.0f;
    ac_bg_draw_quad(x, y, u, bar_v, bar_len, bar_h);
    bar_v = 33.0f;
    y = -148.0f;
    ac_bg_draw_quad(x, y, u, bar_v, rec[1] * bar_len / x, bar_h);
    y = -178.0f;
    ac_bg_draw_quad(x, y, u, bar_v, rec[2] * bar_len / x, bar_h);
    col = game_state.menu_color;
    lang = game_state.lang;
    draw_set_fc_mode(0);
    font_set(2);
    step.x = char_w;
    ((AcWord *)&step.y)->i = 0; /* steer(alias) */
    ((AcWord *)&step.z)->i = 0;
    v3_copy(&pos, &w->pts[0]);
    str = artist_label_texts[lang];
    tpos.x = pos.x - line_length((char *)str) * 12;
    tpos.y = pos.y;
    tpos.z = pos.z;
    font_print3d(str, &tpos, &step, 0, 0, col);
    pos.y -= line_h;
    str = area_artist_names[w->cur];
    tpos.x = pos.x - line_length((char *)str) * 12;
    tpos.y = pos.y;
    tpos.z = pos.z;
    font_print3d(str, &tpos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[1]);
    str = track_label_texts[lang];
    tpos.x = pos.x - line_length((char *)str) * 12;
    tpos.y = pos.y;
    tpos.z = pos.z;
    font_print3d(str, &tpos, &step, 0, 0, col);
    pos.y -= line_h;
    str = area_track_titles[w->cur][0];
    tpos.x = pos.x - line_length((char *)str) * 12;
    tpos.y = pos.y;
    tpos.z = pos.z;
    font_print3d(str, &tpos, &step, 0, 0, col);
    str = area_track_titles[w->cur][1];
    tpos.x = pos.x - line_length((char *)str) * 12;
    tpos.y = pos.y - line_h;
    tpos.z = pos.z;
    font_print3d(str, &tpos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[2]);
    font_print3d((unsigned char *)"analyzation", &pos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[3]);
    font_print3d((unsigned char *)"shot down", &pos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[4]);
    font_print3d((unsigned char *)"support item", &pos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[5]);
    font_print3d_rev(w->buf[0], &pos, &step, 0, 0, col);
    font_set(2);
    step.x = char_w;
    ((AcWord *)&step.y)->i = 0;
    ((AcWord *)&step.z)->i = 0;
    v3_copy(&pos, &w->pts[6]);
    pos.y += dy;
    font_print3d_rev(w->buf[1], &pos, &step, 0, 0, col);
    v3_copy(&pos, &w->pts[7]);
    pos.y += dy;
    font_print3d_rev(w->buf[2], &pos, &step, 0, 0, col);
    RenderSetCurrentDisplayList(prev_dl);
    /* dead stores in the original */
    n = 1;
    n = 2;
    n = 3;
    n = 4;
    n = 5;
    n = 6;
    n = 7;
    n = 8;
}

/* 0x00ca17b8 */
void *ac_bg_create(void *parent, int area)
{
    return task_create_inherit_arg(parent, &ac_bg_desc, 380, (void *)area);
}

/* 0x00ca17e0 */
void ac_bg_set_area(AcBg *w, int area)
{
    w->area = area;
}
