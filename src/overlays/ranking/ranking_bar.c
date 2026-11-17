/*
 * Score ranking screen: board rows (rank, score, name, three rates) and the paging camera, which
 * slides 1200 units per page.
 */

#include "ranking.h"
#include "mathdef.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ranking_bar.h"

extern RankRec *ConfigGetRankRecord(int t, int i);

/* (body, clip) per row mode */
extern int bar_models[][2];

/* per language */
extern unsigned char *rank_labels[][10];

/* node flag of each text position */
extern unsigned int bar_node_masks[6];

extern struct TaskDesc bar_desc;

extern char demo_camera_hooks[];

extern struct TaskDesc demo_camera_desc;

/* Stores each text anchor node's camera-space position in w->lab. */
/* 0x00ca0a98 */
int bar_node_cb(unsigned int *flags, int node_idx, Bar *w)
{
    Vec3 *v = w->lab;
    unsigned int bits = *flags;
    unsigned int *mask = bar_node_masks;
    float *mtx;
    int i;

    for (i = 5; i >= 0; i--) {
        mtx = camera_get_inv_mtx();
        if (bits & *mask) {
            ulMatrixGetTranslation(ulMatrixCur, v);
            ulMatrixApplyPoint(mtx, v, v);
        }
        v++;
        mask++;
    }
    return 1;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

/* 0x00ca0b40 */
void bar_init(Bar *w, int rank, int table, Vec3 *pos, int mode)
{
    void *root;
    int wire;
    RankRec *rp;

    w->rank = rank;
    w->rec = ConfigGetRankRecord(table, rank);
    w->mode = mode;
    root = pnb_get_entry(w->models, bar_models[mode][0])->src;
    dc_object_init(&w->body, root, pnb_get_entry(w->models, bar_models[mode][1])->src, 0);
    w->body.speed = 0;
    root = pnb_get_entry(w->models, 6)->src;
    wire = model_pnb_get_wire(w->models, 6);
    dc_object_init(&w->wire_obj, root, 0, 0);
    dc_object_set_wire(&w->wire_obj, wire);
    dc_object_set_callback(&w->body, bar_node_cb, w);
    if (w->mode != 2) {
        rp = w->rec;
        format_number_commas(w->score, rp->score);
        sprintf(w->val[0], "%19.2f", rp->val[0]);
        sprintf(w->val[1], "%19.2f", rp->val[1]);
        sprintf(w->val[2], "%19.2f", rp->val[2]);
    }
    w->pos.x = 0;
    w->pos.z = 0;
    w->pos.y = (float)(9 - rank) * 30.0f;
    v3_add(&w->pos, pos);
}

/* 0x00ca0d30 */
void bar_release(Bar *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->wire_obj);
}

/* 0x00ca0d60 */
void bar_exec(Bar *w)
{
    dc_object_advance_time(&w->body);
    dc_object_advance_time(&w->wire_obj);
}

/* 0x00ca0d90 */
void bar_disp(Bar *w)
{
    int lang;
    Vec3 pos;
    Vec3 adv;
    Vec3 rot;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    draw_set_color(RGBA(0x40, 0x40, 0x40, 0x80));
    dc_object_draw(&w->body);
    dc_object_draw(&w->wire_obj);
    ulMatrixPop();
    if (w->mode != 2) {
        lang = game_lang;
        adv.x = 12.0f;
        adv.y = 0;
        adv.z = 0;
        rot.x = 0;
        rot.y = 0;
        rot.z = 0;
        draw_set_fc_mode(0x2000);
        font_set(2);
        v3_copy(&pos, &w->lab[0]);
        pos.x -= 42.0f;
        font_print3d(rank_labels[lang][w->rank], &pos, &adv, 0, 0, COL_WHITE);
        v3_copy(&pos, &w->lab[1]);
        pos.x -= 6.0f;
        font_print3d_rev((unsigned char *)w->score, &pos, &adv, 0, 0, COL_WHITE);
        v3_copy(&pos, &w->lab[2]);
        pos.x -= 12.0f;
        font_print3d((unsigned char *)w->rec->name, &pos, &adv, 0, 0, COL_WHITE);
        v3_copy(&pos, &w->lab[3]);
        pos.x -= 6.0f;
        font_print3d_rev((unsigned char *)w->val[0], &pos, &adv, 0, 0, COL_WHITE);
        v3_copy(&pos, &w->lab[4]);
        pos.x -= 6.0f;
        font_print3d_rev((unsigned char *)w->val[1], &pos, &adv, 0, 0, COL_WHITE);
        v3_copy(&pos, &w->lab[5]);
        pos.x -= 6.0f;
        font_print3d_rev((unsigned char *)w->val[2], &pos, &adv, 0, 0, COL_WHITE);
    }
}

/* 0x00ca1030 */
Bar *bar_create(Box *parent, int rank, int table, Vec3 *pos, int mode)
{
    Bar *w = task_create_inherit(parent, &bar_desc, 412);

    bar_init(w, rank, table, pos, mode);
    return w;
}

/* mode bit 0 forward, bit 1 back at half speed */
/* 0x00ca10b0 */
void bar_play(Bar *w, int mode)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END)) {
        if (mode & 1)
            w->wire_obj.speed = w->body.speed = 1.0f;
        if (mode & 2)
            w->wire_obj.speed = w->body.speed = -0.5f;
        w->state = mode;
    }
}

/* 0x00ca1108 */
void demo_camera_node_init(void)
{
}

/* 0x00ca1110 */
void demo_camera_node_update(Camera *out, PageCam *w)
{
    *out = w->cam;
    v3_add(&out->eye, &w->off);
    v3_add(&out->center, &w->off);
}

/* 0x00ca11d0 */
void demo_camera_node_release(void)
{
}

/* 0x00ca11d8 */
void demo_camera_init(PageCam *w)
{
    Camera *cur = camera_get_current();

    w->motion = pnb_get_entry(w->models, 0)->src;
    w->target = pnb_get_entry(w->models, 1)->src;
    w->node = camera_node_add(2, demo_camera_hooks, w);
    w->cam = *cur;
    w->t = 0;
    w->end = (float)*((unsigned int *)w->target + 1) - 1.0f;
}

/* 0x00ca12e0 */
void demo_camera_release(PageCam *w)
{
    camera_node_remove(w->node);
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

/* 0x00ca12e8 */
void demo_camera_exec(PageCam *w)
{
    switch (w->state) {
    case 0:
        v3_set(&w->off, 0, 0, 0);
        w->dt = 1.0f;
        w->state = 1;
        w->done = 0;
        w->t = 0;
        break;
    case 1:
        if (w->done)
            w->state = 2;
        break;
    case 2:
        w->motion = pnb_get_entry(w->models, 2)->src;
        w->target = pnb_get_entry(w->models, 3)->src;
        w->end = (float)*((unsigned int *)w->target + 1) - 1.0f;
        w->state = 100;
    case 100:
        w->t = 0;
        w->state++;
        w->dt = 0;
        w->done = 0;
        w->off.x = (float)w->page * 1200.0f;
        break;
    case 101:
        if (w->dir) {
            if (w->dir < 0) {
                w->t = w->end;
                w->dt = -1.0f;
                w->off.x = (float)(w->page - 1) * 1200.0f;
            } else {
                w->t = 0;
                w->dt = 1.0f;
                w->off.x = (float)w->page * 1200.0f;
            }
            w->state++;
        }
        break;
    case 102:
        if (w->done) {
            w->state = 100;
            w->page += w->dir;
            w->dir = 0;
        }
        break;
    }
    camera_eval_motion(&w->cam, w->motion, w->target, w->t);
    if (!(fabsf(w->dt) < FLT_EPSILON)) {
        w->t += w->dt;
        if (w->end <= w->t) {
            w->t = w->end;
            w->done = 1;
        } else if (w->t <= 0.0f) {
            w->t = 0.0f;
            w->done = 1;
        }
    }
}

/* 0x00ca1560 */
void demo_camera_disp(void)
{
}

/* 0x00ca1568 */
PageCam *demo_camera_create(void *parent)
{
    return task_create_inherit(parent, &demo_camera_desc, 100);
}

/* 0x00ca1590 */
int demo_camera_get_page(PageCam *w)
{
    return w->page;
}

/* 0x00ca1598 */
void demo_camera_request_page(PageCam *w, int up)
{
    if (up)
        w->dir = 1;
    else
        w->dir = -1;
}
