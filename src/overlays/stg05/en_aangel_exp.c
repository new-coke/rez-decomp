/*
 * Archangel explosions, Area 5: explosion model shown after a delay, chained when the Archangel
 * dies and used by its children and some shots; counts itself off its owner's explosion counter.
 */

#include "stg05.h"
#include "mathdef.h"
#include "libc.h"
#include "model.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_aangel_exp.h"

typedef struct EnAAngelExpWork {
    void *models;
    void *tex;                  /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    ModelPart body;                /* 0x14 */
    int *count;                 /* 0x70: decremented when the task ends */
    void *owner;                /* 0x74 */
    int timer;                  /* 0x78: frames before it shows */
    float scale;                /* 0x7c */
} EnAAngelExpWork;

typedef struct EnAAngelExpArg {
    void *owner;
    Vec3 *pos;                  /* 0x04 */
    int count, timer;
    int big;                    /* 0x10: picks models 79/80 over 77/78 */
    float scale;                /* 0x14 */
} EnAAngelExpArg;

extern struct TaskDesc en_aangel_exp_desc;
extern struct TaskDesc en_aangel_exp_scaled_desc;

/* 0x00afe650 */
void en_aangel_exp_init(EnAAngelExpWork *w, EnAAngelExpArg *prm)
{
    int *root;
    int *clip;

    do {
        void *m_ = stage_models, *t_ = stage_textures;
        w->models = m_;
        w->tex = t_;
    } while (0);
    {
        Vec3 *s_ = prm->pos;
        v3_copy(&w->pos, s_);
    }
    if (prm->big) {
        root = pnb_get_entry(stage_models, 79);
        clip = pnb_get_entry(stage_models, 80);
        dc_object_init(&w->body, *root, *clip, 0);
        w->body.radius = 50.0f;
    } else {
        root = pnb_get_entry(stage_models, 77);
        clip = pnb_get_entry(stage_models, 78);
        dc_object_init(&w->body, *root, *clip, 0);
        w->body.radius = 100.0f;
    }
    dc_object_rebuild(&w->body);
    w->count = (int *)prm->count;
    w->owner = prm->owner;
    if (w->count)
        (*w->count)++;
    w->timer = prm->timer;
    w->scale = prm->scale;
}

/* 0x00afe790 */
void en_aangel_exp_release(EnAAngelExpWork *w)
{
    dc_object_release(&w->body);
    if (w->count)
        (*w->count)--;
}

/* 0x00afe7d0 */
void en_aangel_exp_exec(EnAAngelExpWork *w)
{
    Vec3 mv;

    player_get_frame_vel(&mv);
    v3_add(&w->pos, &mv);
    if (--w->timer <= 0) {
        dc_object_advance_time(&w->body);
        if (fabsf(w->body.frame - w->body.frames) < FLT_EPSILON)
            FINISH_IF(w);
        if (w->count && (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END))) {
            w->owner = 0;
            w->count = 0;
        }
    }
}

/* 0x00afe8b8 */
void en_aangel_exp_disp(EnAAngelExpWork *w)
{
    int dl;

    if (w->timer < 0) {
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->tex);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        vu0_matrix_scale(ulMatrixCur, w->scale, w->scale, w->scale);
        dc_object_draw(&w->body);
        ulMatrixPop();
        RenderSetCurrentDisplayList(dl);
    }
}

/* 0x00afe970 */
void *en_aangel_exp_create(void *w, Vec3 *pos, int counter, int timer, int big)
{
    EnAAngelExpArg arg;

    arg.owner = w;
    arg.pos = pos;
    arg.count = counter;
    arg.timer = timer;
    arg.big = big;
    arg.scale = 1.0f;
    return task_create_sibling_inherit_arg(w, &en_aangel_exp_desc, 128, &arg);
}

/* 0x00afe9c0 */
void *en_aangel_exp_create_scaled(void *w, Vec3 *pos, int counter, int timer, int big, float scale)
{
    EnAAngelExpArg arg;

    arg.owner = w;
    arg.pos = pos;
    arg.count = counter;
    arg.timer = timer;
    arg.big = big;
    arg.scale = scale;
    return task_create_sibling_inherit_arg(w, &en_aangel_exp_scaled_desc, 128, &arg);
}
