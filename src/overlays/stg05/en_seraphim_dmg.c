/*
 * Seraphim damage pieces, Area 5: a falling, fading copy of a model part (mode 0) or fading
 * debris thrown around it (mode 1).
 */

#include "stg05.h"
#include "mathdef.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_seraphim_dmg.h"
#include "stage05.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern struct TaskDesc en_seraphim_dmg_desc;

typedef struct EnSeraphimDmgWork {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnSeraphimDmgRec *type;     /* 0x08 */
    int count;                  /* 0x0c: frames */
    Vec3 pos;                   /* 0x10 */
    int ang[3];                 /* 0x1c */
    ModelPart body;                /* 0x28 */
    EnSeraphimDmgArg arg;              /* 0x84 */
} EnSeraphimDmgWork;

/* group name, label, 2 values */
typedef struct EnSeraphimDmgSet {
    char *name;
    char *label;                /* 0x04 */
    int v[2];                   /* 0x08 */
} EnSeraphimDmgSet;

extern EnSeraphimDmgSet en_seraphim_dmg_types[4];
extern VarDef en_seraphim_dmg_vardef;
extern VarDef en_seraphim_dmg_vardefs[2];

/* view of en_seraphim_dmg_types */
typedef struct EnSeraphimDmgRec {
    char *name;
    char *label;                /* 0x04 */
    float accel;                /* 0x08: fall acceleration */
    float life;                 /* 0x0c: seconds */
} EnSeraphimDmgRec;

/* 0x00b12100 */
void en_seraphim_dmg_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_seraphim_dmg_vardef);
    for (i = 0; i < 4; i++) {
        def = en_seraphim_dmg_vardefs;
        val = en_seraphim_dmg_types[i].v;
        en_seraphim_dmg_vardef.name = en_seraphim_dmg_types[i].name;
        en_seraphim_dmg_vardef.fmt = en_seraphim_dmg_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_seraphim_dmg_vardef);
        for (j = 0; j < 2; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_seraphim_dmg_vardefs);
    }
}

/* 0x00b12218 */
void en_seraphim_dmg_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* steer(cast) */
/* 0x00b12220 */
void en_seraphim_dmg_init(EnSeraphimDmgWork *w, EnSeraphimDmgArg *prm)
{
    w->type = (EnSeraphimDmgRec *)&en_seraphim_dmg_types[prm->set * 2 + prm->mode];
    w->arg = *prm;
    i3_copy(w->ang, prm->ang);
    v3_copy(&w->pos, prm->pos);
    w->count = 0;
    if (prm->mode == 0) {
        dc_object_init(&w->body, prm->src->root, prm->src->clip, 0);
        w->body.frame = prm->src->frame;
    } else {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, rand_lcg_next_b() % 3 + 186)), 0, 0);
        w->pos.x += prm->off.x;
        w->pos.y += prm->off.y;
        w->pos.z += prm->off.z;
    }
    dc_object_rebuild(&w->body);
}

/* 0x00b123b8 */
void en_seraphim_dmg_release(EnSeraphimDmgWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b123c0 */
void en_seraphim_dmg_exec(EnSeraphimDmgWork *w)
{
    EnSeraphimDmgArg *prm = &w->arg;
    EnSeraphimDmgRec *typ;
    Vec3 dir;
    SpawnDesc desc;
    float dis, k, v;
    int map;

    typ = w->type;
    if (w->arg.delay-- > 0)
        return;
    w->count++;
    v = 0.0f;
    k = w->count / FPS;
    v += typ->accel * k;
    k = FRAME_SEC;
    v *= k;
    dis = v + typ->accel * 0.5f * k * k;
    map = map_get_current();
    do {
        if (map == 20)
            v3_set(&dir, 0.0f, 0.0f, 1.0f);
        else
            v3_set(&dir, 0.0f, -1.0f, 0.0f);
    } while (0);
    v3_scale(&dir, dis);
    v3_add(&w->pos, &dir);
    switch (prm->mode) {
    case 0:
        if (typ->life * FPS < w->count) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0;
            desc.models = stage_models;
            desc.type = prm->type;
            v3_copy(&desc.pos, &w->pos);
            i3_copy(desc.ang, w->ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            desc.v.x = 0.0f;
            desc.v.y = 0.0f;
            desc.v.z = 0.0f;
            en_shatter_spawn(w, &desc, 4);
            FINISH_IF(w);
        }
        break;
    case 1:
        if (typ->life * FPS < w->count)
            FINISH_IF(w);
        break;
    }
}

/* steer(cast) */
/* 0x00b12620 */
void en_seraphim_dmg_disp(EnSeraphimDmgWork *w)
{
    EnSeraphimDmgArg *prm = &w->arg;
    EnSeraphimDmgRec *typ;
    float col_add[4];
    void *fog;
    int prev;

    typ = w->type;
    ulGxSetTextureList(w->tex);
    switch (prm->mode) {
    case 0: {
        void *prev_fog = fog_set_enable(0);

        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_seraphim_dmg_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        ulMatrixPop();
        fog_set_enable(prev_fog);
        break;
    }
    case 1:
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        fog = fog_set_enable(0);
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        heap_bzero(col_add, 16);
        col_add[0] = -(float)w->count / (typ->life * FPS);
        draw_push_override_color();
        draw_set_override_color(col_add);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_seraphim_dmg_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        ulMatrixPop();
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
        fog_set_enable(fog);
        break;
    }
}

/* 0x00b127c8 */
void *en_seraphim_dmg_create(void *w, void *prm)
{
    return task_create_sibling_inherit_arg(w, &en_seraphim_dmg_desc, 172, prm);
}

/* 0x00b127f0 */
void en_seraphim_dmg_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
