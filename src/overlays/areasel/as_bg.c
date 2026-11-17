/* Score attack area select: the selected area's background and label panel. */

#include "areasel.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "fog.h"
#include "heap.h"
#include "libc.h"
#include "mathdef.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "as_bg.h"

extern void font_set(int id);

/* 16 bytes; one table per area (as_bg_areas) */
typedef struct AsObj {
    int light;                  /* 1: lit with the model's light */
    int color;                  /* 0x04 */
    int model;                  /* 0x08 */
    int clip;                   /* 0x0c: < 0 = none */
} AsObj;

typedef struct AsArea {          /* 12 bytes, one per area */
    AsObj *objs;
    int count;                  /* 0x04 */
    unsigned int fog_color;     /* 0x08 */
} AsArea;

typedef struct AsLabel {        /* 8 bytes, one per label position */
    unsigned int kind;          /* 0..4, see as_bg_labels */
    unsigned char **text;       /* 0x04 */
} AsLabel;

extern AsArea as_bg_areas[6];

/* panel node flag of each label position */
extern unsigned int as_bg_node_masks[7];

extern const AsLabel as_bg_labels[7];

extern struct TaskDesc as_bg_desc;

#define ROT_Z(ang, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        float s_, c_; \
        ulSinCos(a_, &s_, &c_); \
        x = rot_u * c_ - rot_v * s_; \
        y = rot_u * s_ + rot_v * c_; \
    } while (0)

#define V3_GET(v, x, y, z) \
    do { \
        Vec3 *v_ = (v); \
        x = v_->x; \
        y = v_->y; \
        z = v_->z; \
    } while (0)

extern RankRec *ConfigGetRankRecord(int t, int i);
/* per language */
extern unsigned char *as_bg_high_score_texts[];

extern unsigned char *as_bg_score_attack_texts[];

extern unsigned char *artist_label_texts[];
extern unsigned char *track_label_texts[];
extern unsigned char *area_artist_names[];
extern unsigned char *area_track_titles[];

/* kind: 0 large title, 1 text by language, 2 artist, 3 two-line track title, 4 best score */
const AsLabel as_bg_labels[7] = {
    { 1, as_bg_high_score_texts },
    { 1, artist_label_texts },
    { 1, track_label_texts },
    { 0, as_bg_score_attack_texts },
    { 4, 0 },
    { 2, area_artist_names },
    { 3, area_track_titles },
};

/* 0x00ca0700 */
void as_bg_preload_lights(AsBg *w)
{
    int i;
    AsObj *ob;
    int n;

    for (i = 0; i < 6; i++) {
        ob = as_bg_areas[i].objs;
        n = as_bg_areas[i].count;
        if (ob) {
            do {
                if (ob->light == 1)
                    model_pnb_get_wire(w->models, ob->model);
                ob++;
            } while (--n);
        }
    }
}

/* 0x00ca07c8 */
Part *as_bg_parts_create(AsBg *w, AsObj *ob, int n)
{
    Part *parts = heap_block_tag(game_malloc(n * 96), "as_bg.c", 295);
    Part *p = parts;
    void *root;
    void *clip;

    do {
        root = pnb_get_entry(w->models, ob->model)->src;
        clip = ob->clip >= 0 ? pnb_get_entry(w->models, ob->clip)->src : 0;
        dc_object_init(p, root, clip, 0);
        if (ob->light) {
            if (ob->light == 1) {
                dc_object_set_wire(p, model_pnb_get_wire(w->models, ob->model));
                p->color = ob->color;
            }
        }
        ob++;
        p++;
    } while (--n);
    return parts;
}

/* 0x00ca08d8 */
void as_bg_parts_release(Part *p, int n)
{
    if (p) {
        if (n > 0) {
            do {
                dc_object_release(p);
                p++;
            } while (--n);
        }
    }
}

/* Stores each label node's camera-space position and direction. */
/* 0x00ca0928 */
int as_bg_node_cb(unsigned int *flags, int node_idx, AsBg *w)
{
    Vec3 *v = w->pos;
    unsigned int fl = *flags;
    unsigned int *mask = as_bg_node_masks;
    float *mat;
    Vec3 axis;
    int i;

    for (i = 6; i >= 0; i--) {
        mat = camera_get_inv_mtx();
        if (fl & *mask) {
            ulMatrixGetTranslation(ulMatrixCur, v);
            ulMatrixApplyPoint(mat, v, v);
            axis.x = 1.0f;
            axis.y = 0;
            axis.z = 0;
            ulMatrixApplyVector(ulMatrixCur, &axis, &axis);
            ulMatrixApplyVector(mat, &axis, v + 7);
        }
        v++;
        mask++;
    }
    return 1;
}

/* steer(cast) */
/* 0x00ca0a10 */
void as_bg_init(AsBg *w, int area)
{
    void *root;
    void *clip;

    w->req = area;
    do {
        fog_set(COL_BLACK, 4, 1000.0f, 4000.0f);
    } while (0);
    fog_set_enable(1);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    w->state = 100;
    dc_object_init((struct MapPart *)&w->base, pnb_get_entry(w->models, 0)->src, 0, 0);
    dc_object_set_callback(&w->base, as_bg_node_cb, w);
    root = pnb_get_entry(w->models, 3)->src;
    clip = pnb_get_entry(w->models, 4)->src;
    dc_object_init((struct MapPart *)&w->anim, root, clip, 0);
    as_bg_preload_lights(w);
}

/* 0x00ca0ae0 */
void as_bg_release(AsBg *w)
{
    dc_object_release((struct MapPart *)&w->anim);
    as_bg_parts_release(w->parts, w->count);
    heap_free(w->parts, 0);
    w->parts = 0;
}

/* 0x00ca0b28 */
void as_bg_exec(AsBg *w)
{
    AsArea *area_rec;
    int done;

    w->anim.rate = w->rate;
    dc_object_advance_time((struct MapPart *)&w->anim);
    done = w->anim.flags & 1;
    switch (w->state) {
    case 0:
        if (w->area != w->req) {
            w->state = 1;
            w->rate = -1.0f;
        }
        break;
    case 1:
        if (done)
            w->state = 100;
        break;
    case 100:
        if (w->parts) {
            as_bg_parts_release(w->parts, w->count);
            heap_free(w->parts, 0);
        }
        w->area = w->req;
        area_rec = &as_bg_areas[w->req];
        fog_req_color(area_rec->fog_color);
        w->parts = as_bg_parts_create(w, area_rec->objs, w->count = area_rec->count);
        w->rate = 1.0f;
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

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

/* 0x00ca0c48 */
void as_bg_disp(AsBg *w)
{
    Vec3 pos;
    Vec3 scale;
    int rot[3];
    Vec3 off;
    char buf[32];
    GameState *gs;
    const AsLabel *lbl;
    Vec3 *p;
    Vec3 *dir;
    Part *part;
    int n;
    unsigned int i;
    int color;
    unsigned char *str;
    int len;
    RankRec *rec;
    int area;
    int lang;

    ulGxSetTextureList(w->textures);
    if (w->parts) {
        part = w->parts;
        n = w->count;
        do {
            draw_set_color(part->color);
            dc_object_draw(part);
            part++;
        } while (--n);
    }
    dc_object_draw((struct MapPart *)&w->base);
    dc_object_draw((struct MapPart *)&w->anim);
    RenderSetCurrentDisplayList(6);
    area = w->area;
    gs = &game_state;
    color = gs->menu_color;
    lang = gs->lang;
    draw_set_fc_mode(0x2000);
    p = w->pos;
    dir = w->dir;
    lbl = as_bg_labels;
    for (i = 0; i < 7; i++, lbl++, p++, dir++) {
        switch (lbl->kind) {
        case 0:
            font_set(3);
            v3_mul(&scale, dir, 36.0f);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            font_set(2);
            rot[2] = 5643;
            rot[0] = 0;
            rot[1] = 0;
            v3_mul(&scale, dir, 12.0f);
            break;
        }
        switch (lbl->kind) {
        case 0:
            str = lbl->text[lang];
            pos.x = p->x + 18.0f;
            pos.y = p->y - 21.0f;
            pos.z = p->z;
            font_print3d(str, &pos, &scale, 0, 0, color);
            break;
        case 1:
            font_print3d(lbl->text[lang], p, &scale, rot, 0, color);
            break;
        case 2: {
            float x, y, z;

            str = lbl->text[area];
            len = line_length(str);
            V3_GET(&scale, x, y, z);
            ROT_Z(rot[2], x, y);
            pos.x = p->x - x * len;
            pos.y = p->y - y * len;
            pos.z = p->z - z * len;
            font_print3d(str, &pos, &scale, rot, 0, color);
            break;
        }
        case 3:
            {
                float x, y, z;

                str = lbl->text[area * 2];
                len = line_length(str);
                V3_GET(&scale, x, y, z);
                ROT_Z(rot[2], x, y);
                pos.x = p->x - x * len;
                pos.y = p->y - y * len;
                pos.z = p->z - z * len;
                font_print3d(str, &pos, &scale, rot, 0, color);
            }
            {
                float x, y, z;

                str = lbl->text[area * 2 + 1];
                len = line_length(str);
                v3_copy(&pos, p);
                V3_GET(&scale, x, y, z);
                ROT_Z(rot[2], x, y);
                pos.x -= x * len;
                pos.y -= y * len;
                pos.z -= z * len;
                v3_mul(&off, dir, 19.0f);
                V3_GET(&off, x, y, z);
                ROT_Z(rot[2] + ANG_90, x, y);
                pos.x -= x;
                pos.y -= y;
                pos.z -= z;
                font_print3d(str, &pos, &scale, rot, 0, color);
            }
            break;
        case 4: {
            float x, y, z;

            rec = ConfigGetRankRecord(area, 0);
            sprintf(buf, "%8d", rec->score);
            len = line_length(buf);
            V3_GET(&scale, x, y, z);
            ROT_Z(rot[2], x, y);
            pos.x = p->x - x * len;
            pos.y = p->y - y * len;
            pos.z = p->z - z * len;
            font_print3d((unsigned char *)buf, &pos, &scale, rot, 0, color);
            break;
        }
        }
    }
}

/* 0x00ca1328 */
void *as_bg_create(void *parent, int area)
{
    return task_create_inherit_arg(parent, &as_bg_desc, 384, area);
}

/* 0x00ca1350 */
void as_bg_set_area(AsBg *w, int area)
{
    w->req = area;
}
