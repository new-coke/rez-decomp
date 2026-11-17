/* Seraphim opening effect, Area 5: one-shot model at a launcher as the Seraphim opens. */

#include "stg05.h"
#include "enemy_status.h"
#include "fog.h"
#include "model.h"
#include "model_wire_break.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "open_eff.h"

extern struct TaskDesc open_eff_desc;

typedef struct OpenEffWork {
    void *models;               /* 0x00 */
    void *tex;                  /* 0x04 */
    ModelPart body;                /* 0x08: entries 190/191 of the model bank */
    OpenEffArg arg;             /* 0x64: frames counts down */
    int total;                  /* 0x70 */
} OpenEffWork;

/* steer(cast) */
/* 0x00b12840 */
void open_eff_init(OpenEffWork *w, OpenEffArg *prm)
{
    ModelPart *part = &w->body;
    int *root;

    root = ((int *)pnb_get_entry(w->models, 190));
    dc_object_init(part, *root, *((struct DcClip **)pnb_get_entry(w->models, 191)), 0);
    dc_object_rebuild(part);
    w->arg = *prm;
    w->total = prm->frames;
}

/* 0x00b128d8 */
void open_eff_release(OpenEffWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b128e0 */
void open_eff_exec(OpenEffWork *w)
{
    OpenEffArg *prm = &w->arg;

    w->arg.frames -= enemy_status_get_ticks();
    if (w->arg.frames < 0) {
        w->body.frame = w->body.frames;
        FINISH_IF(w);
    } else {
        w->body.frame = w->body.frames * (1.0f - prm->frames / (float)w->total);
    }
}

/* 0x00b12970 */
void open_eff_disp(OpenEffWork *w)
{
    OpenEffArg *prm = &w->arg;
    void *fog;
    int dl;

    fog = fog_set_enable(0);
    dl = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(w->tex);
    ulMatrixPush();
    do {
        Vec3 *p_ = w->arg.pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    open_eff_rotate_yxz(prm->ang);
    model_node_draw_tree2_anim(w->body.root, w->body.clip, w->body.frame);
    ulMatrixPop();
    fog_set_enable(fog);
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b12a20 */
void *open_eff_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &open_eff_desc, 116, prm);
}

/* 0x00b12a48 */
void open_eff_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
