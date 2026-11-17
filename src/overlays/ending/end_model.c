/* Ending: model scene in letterbox; then the staff roll (endings 0-2) or the butterfly scene. */

#include "ending_shared.h"
#include "color.h"
#include "fade.h"
#include "heap.h"
#include "model.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "end_model.h"
#include "end_butterfly.h"
#include "end_staff_roll.h"
#include "ending.h"

/* 20 bytes, table at end_model_defs */
typedef struct EndModelDef {
    char pad_00[8];
    int model;                  /* 0x08 */
    int clip;                   /* 0x0c: < 0 = none */
    float radius;               /* 0x10 */
} EndModelDef;

/* 20 bytes */
typedef struct EndModel {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart *parts;              /* 0x08 */
    int count;                  /* 0x0c */
    int started;                /* 0x10 */
} EndModel;

extern struct TaskDesc end_model_desc;
extern EndModelDef end_model_defs[];
extern float screen_layout[];

/* steer(cast) */
/* 0x00af8348 */
void end_model_init(EndModel *w)
{
    EndModelDef *mdef;
    ModelPart *mdl;
    int n;
    int root, clip;

    task_set_dlist(w, 4);
    w->count = 1;
    w->parts = heap_block_tag(game_malloc(w->count * 92), "end_model.c", 127);
    n = w->count;
    mdef = end_model_defs;
    mdl = w->parts;
    if (n > 0)
        do {
            root = *((int *)pnb_get_entry(w->models, mdef->model));
            clip = mdef->clip >= 0 ? *((int *)pnb_get_entry(w->models, mdef->clip)) : 0;
            dc_object_init(mdl, root, clip, 0);
            mdl->speed *= frame_scale;
            mdl->radius = mdef->radius;
            mdef++;
            mdl++;
        } while (--n);
    w->started = 0;
}

/* 0x00af8448 */
void end_model_release(EndModel *w)
{
    ModelPart *mdl;
    int n;

    n = w->count;
    mdl = w->parts;
    if (n > 0)
        do {
            dc_object_release(mdl);
            mdl++;
        } while (--n);
    heap_free(w->parts, 0);
}

/* 0x00af84a8 */
void end_model_exec(EndModel *w)
{
    ModelPart *mdl;
    int n;
    Task *ptask;

    n = w->count;
    mdl = w->parts;
    if (n > 0)
        do {
            dc_object_advance_time(mdl);
            switch (ending_type_get()) {
            case 0:
            case 1:
            case 2:
                if (w->started) {
                    if (fade_is_idle() == 1) {
                        fade_start_out((int)(time_scale * FPS));
                        ptask = TASK_OF(w)->parent;
                        end_staff_roll_create(ptask ? ptask + 1 : 0);
                        continue;   /* the loop step still advances mdl */
                    }
                } else if (!(mdl->frame < 700.0f)) {
                    w->started = 1;
                    fade_start(0, 0, (int)(time_scale * FPS));
                }
                break;
            default:
                if (!w->started && !(mdl->frame < 600.0f)) {
                    w->started = 1;
                    ptask = TASK_OF(w)->parent;
                    end_butterfly_create(ptask ? ptask + 1 : 0, mdl->frame);
                }
                break;
            }
        } while (mdl++, --n);
}

/* 0x00af8618 */
void end_model_disp(EndModel *w)
{
    TrailVtx vtx[4];
    ModelPart *mdl;
    int n;
    float *scr;
    float vq, bar_y;

    ulGxSetTextureList(w->textures);
    n = w->count;
    mdl = w->parts;
    if (n > 0)
        do {
            dc_object_draw(mdl);
            mdl++;
        } while (--n);
    scr = screen_layout;
    vq = 1.0f;
    bar_y = 60.0f;
    vtx[0].x = 0.0f;
    vtx[0].q = vq;
    vtx[0].y = 0.0f;
    vtx[1].x = scr[0];
    vtx[1].q = vq;
    vtx[2].y = bar_y;
    vtx[2].q = vq;
    vtx[3].y = bar_y;
    vtx[3].q = vq;
    vtx[3].x = scr[0];
    vtx[1].y = 0.0f;
    vtx[2].x = 0.0f;
    vtx[0].c = COL_BLACK;
    vtx[1].c = COL_BLACK;
    vtx[2].c = COL_BLACK;
    vtx[3].c = COL_BLACK;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
    bar_y = vtx[0].y = scr[1] - bar_y;
    vtx[0].x = 0.0f;
    vtx[0].q = vq;
    vtx[1].x = scr[0];
    vtx[1].y = bar_y;
    vtx[1].q = vq;
    vtx[2].x = 0.0f;
    vtx[2].y = scr[1];
    vtx[2].q = vq;
    vtx[3].x = scr[0];
    vtx[3].y = scr[1];
    vtx[3].q = vq;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* 0x00af8760 */
void *end_model_create(void *parent, int prm)
{
    return task_create_inherit(parent, &end_model_desc, 20);
}
