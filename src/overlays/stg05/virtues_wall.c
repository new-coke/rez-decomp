/*
 * Virtues walls, Area 5: raised in turn by an enVirtuesBase, rising out of the ground (plane_cut).
 * Contact hurts the player and stops the base's walls; so does shooting the lockable wall.
 */

#include "stg05.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "virtues_wall.h"
#include "en_virtues_base.h"
#include "plane_cut.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern int virtues_wall_init_arg;
extern void (*virtues_wall_exec_table[])(void *o, struct VirtuesWallWork *w);
extern void (*virtues_wall_disp_table[])(void *o, struct VirtuesWallWork *w);
extern EnShatterSet virtues_wall_types[1];
extern VarDef virtues_wall_vardef;
extern VarDef virtues_wall_vardefs[15];
extern void (*virtues_wall_states[])(void *o, struct VirtuesWallWork *w);

/* 16 bytes, virtues_wall_model_sets */
typedef struct VirtuesWallModels {
    int save_a;                 /* saved for drawing (first) */
    int body_a;                 /* 0x04: the body when not lockable */
    int save_b;                 /* 0x08 */
    int body_b;                 /* 0x0c: the body when lockable */
} VirtuesWallModels;

/* 24 bytes; ObjDesc.kind points to it */
typedef struct VirtuesWallArg {
    int idx;
    int locked;                 /* 0x04 */
    int mdl;                    /* 0x08: index into virtues_wall_model_sets */
    PosAngRec *rec;             /* 0x0c: the enVirtuesBase it hangs on */
    float y;                    /* 0x10: height offset */
    float h;                    /* 0x14: height */
} VirtuesWallArg;

extern VirtuesWallModels virtues_wall_model_sets[4];
extern void *virtues_wall_saves[4][2];
extern char virtues_wall_lock_name[];
extern char virtues_wall_name[];
extern ObjFuncs virtues_wall_funcs;

/* 0x00b205b8 */
void virtues_wall_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&virtues_wall_vardef);
    for (i = 0; i < 1; i++) {
        def = virtues_wall_vardefs;
        val = virtues_wall_types[i].v;
        virtues_wall_vardef.name = virtues_wall_types[i].name;
        virtues_wall_vardef.fmt = virtues_wall_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&virtues_wall_vardef);
        for (j = 0; j < 15; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)virtues_wall_vardefs);
    }
}

/* 0x00b206d0 */
void virtues_wall_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b206d8 */
int virtues_wall_get_init_arg(void)
{
    return virtues_wall_init_arg;
}

/* 0x00b206e8 */
int virtues_wall_vars_init(int *prm)
{
    virtues_wall_init_arg = *prm;
    virtues_wall_vars_register();
    return 0;
}

/* 0x00b20710 */
int virtues_wall_vars_release(void)
{
    virtues_wall_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00b20730 */
void virtues_wall_models_load(void)
{
    unsigned int i;

    for (i = 0; i < 4; i++) {
        virtues_wall_saves[i][0] = yuki_save_create((void *)*((int *)pnb_get_entry(stage_models, virtues_wall_model_sets[i].save_a)));
        virtues_wall_saves[i][1] = yuki_save_create((void *)*((int *)pnb_get_entry(stage_models, virtues_wall_model_sets[i].save_b)));
    }
}

/* 0x00b207c0 */
void virtues_wall_models_free(void)
{
    unsigned int i;
    int j;
    void **save;

    for (i = 0; i < 4; i++) {
        save = virtues_wall_saves[i];
        for (j = 0; j < 2; j++) {
            yuki_save_free(*save);
            *save = 0;
            save++;
        }
    }
}

/* steer(cast) */
/* 0x00b20840 */
void virtues_wall_init(void *obj, VirtuesWallWork *w, int place, VirtuesWallArg *prm)
{
    VirtuesWallRec *typ;
    Vec3 p;
    int ang[3];

    typ = (VirtuesWallRec *)&virtues_wall_types[prm->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    if (!prm->locked) {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, virtues_wall_model_sets[prm->mdl].body_a)), 0, 0x500);
        dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, virtues_wall_model_sets[prm->mdl].body_a));
        w->save = virtues_wall_saves[prm->mdl][0];
    } else {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, virtues_wall_model_sets[prm->mdl].body_b)), 0, 0x500);
        dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)obj, virtues_wall_model_sets[prm->mdl].body_b));
        w->save = virtues_wall_saves[prm->mdl][1];
    }
    w->state = 0;
    w->count = 0;
    if (prm->locked) {
        enemy_task_get_pos(obj, &w->pos);
        w->pos.y += prm->h * 0.5f;
        lockon_obj_init(&w->lock, obj, &w->pos, 0, virtues_wall_lock_name, enemy_task_get_status_slot(obj), typ->he, typ->hp, 0);
        lockon_entry(&w->lock);
        w->lock.flags &= ~LOCK_LOCKABLE;
    }
    w->locked = prm->locked;
    w->y = prm->y;
    w->hit = 0;
    w->mdl = prm->mdl;
    w->h = prm->h;
    w->rec = prm->rec;
    en_virtues_base_get_pose(w->rec, &p, ang);
    p.y += w->y;
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    v3_copy(&w->pos, &p);
    w->pos.y += w->h * 0.5f;
    SoundRhythmSe_Enemy(virtues_wall_init_arg, 2);
}

/* 0x00b20af8 */
void virtues_wall_release(void *obj, VirtuesWallWork *w)
{
    dc_object_release(&w->body);
    if (w->locked)
        lockon_remove(&w->lock);
}

/* 0x00b20b38 */
void virtues_wall_state_wait(void)
{
}

/* 0x00b20b40 */
void virtues_wall_state_touch(void *obj, VirtuesWallWork *w)
{
    VirtuesWallRec *typ = w->type;
    int touch;

    touch = player_sphere_test(&w->pos, typ->crash_r);
    if (touch == 1 && !w->hit) {
        player_status_add_damage(typ->crash_damage);
        w->hit = touch;
        en_virtues_base_stop_walls((EnVirtuesBaseWork *)w->rec);
    }
}

/* 0x00b20bc0 */
void virtues_wall_state_end(void)
{
}

/* 0x00b20bc8 */
void virtues_wall_update(void *obj, VirtuesWallWork *w)
{
    void (*fn)(void *obj, VirtuesWallWork *w) = virtues_wall_states[w->state];

    if (fn)
        fn(obj, w);
}

/* 0x00b20c08 */
void virtues_wall_exec_appear(void *obj, VirtuesWallWork *w)
{
    if (enemy_task_get_phase(obj) != 2)
        virtues_wall_update(obj, w);
}

/* 0x00b20c58 */
void virtues_wall_exec_normal(void *obj, VirtuesWallWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        dc_object_rebuild(enemy_task_gfx_ptr(obj));
        if (w->locked)
            w->lock.flags |= LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        virtues_wall_update(obj, w);
    }
}

/* 0x00b20cd8 */
void virtues_wall_exec_leave(void *obj, VirtuesWallWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 42;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00b20d78 */
void virtues_wall_exec_destroyed(void *obj, VirtuesWallWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 42;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_shatter_create(obj, &desc, 0, 0);
        FINISH_IF(obj);
    }
}

/* 0x00b20e18 */
void virtues_wall_pre_exec(void)
{
}

/* 0x00b20e20 */
void virtues_wall_exec(void *obj, VirtuesWallWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (virtues_wall_exec_table[i])
        virtues_wall_exec_table[i](obj, w);
}

/* 0x00b20e78 */
void virtues_wall_post_exec(void *obj, VirtuesWallWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 pos;
    LockObj *lock = &w->lock;
    VirtuesWallRec *typ;
    int n;

    en_virtues_base_get_pose(w->rec, &p, ang);
    p.y += w->y;
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    v3_copy(&w->pos, &p);
    w->pos.y += w->h * 0.5f;
    if (w->locked && (lock->flags & LOCK_HIT)) {
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life <= 0 && enemy_task_get_pstate(obj) != 3 && w->locked) {
            n = lockon_hits_max_shot_cnt(lock);
            typ = w->type;
            enemy_task_get_pos(obj, &pos);
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, typ->score, n);
            if (w->locked)
                en_virtues_base_stop_walls((EnVirtuesBaseWork *)w->rec);
        }
    }
}

/* 0x00b20fe8 */
void virtues_wall_disp_reflect(void *obj, VirtuesWallWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 scl;
    Vec3 base;
    int bang[3];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        ulGxSetTextureList(((void **)obj)[1]);
        en_virtues_base_get_pose(w->rec, &base, bang);
        plane_cut_draw_part(gfx, w->save, 0x80000000, &p, ang, base.y - p.y);
    }
}

/* 0x00b210a8 */
void virtues_wall_disp_body(void *obj, VirtuesWallWork *w)
{
    Vec3 p;
    int ang[3];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    VirtuesWallRec *typ;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    if (view_sphere_visible(&p, gfx->radius)) {
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        virtues_wall_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        if (typ->glow) {
            draw_set_color(yuki_beat_color(typ->glow_a, typ->glow_b, typ->glow_div, typ->glow_linear, 1.0f, typ->glow_znear,
                                          typ->glow_zfar, typ->glow_anear, typ->glow_afar));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
    }
}

/* 0x00b211c8 */
void virtues_wall_disp(void *obj, VirtuesWallWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (virtues_wall_disp_table[i])
        virtues_wall_disp_table[i](obj, w);
}

/* 0x00b21220 */
void *virtues_wall_create(EnVirtuesBaseWork *w, int first, int i, float y, float height)
{
    VirtuesWallArg k;
    ObjDesc desc;
    ObjFuncs funcs = virtues_wall_funcs;
    ShotArg prm;
    VirtuesWallRec *typ = (VirtuesWallRec *)virtues_wall_types;

    k.y = y;
    k.h = height;
    k.idx = 0;
    k.locked = first;
    k.mdl = i;
    k.rec = (PosAngRec *)w;
    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    prm.kind = virtues_wall_init_arg;
    prm.fx_arg = -1;
    desc.name = virtues_wall_name;
    desc.funcs = &funcs;
    desc.arg = &prm;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->he;
    desc.same_lock = typ->hp;
    desc.flags = 2;
    prm.pos.x = 0.0f;
    prm.pos.y = 0.0f;
    prm.pos.z = 0.0f;
    prm.ang[0] = 0;
    prm.ang[1] = 0;
    prm.ang[2] = 0;
    desc.kind = &k.idx;
    desc.dc_flags = 0;
    if (first)
        desc.model_no = virtues_wall_model_sets[i].save_b;
    else
        desc.model_no = virtues_wall_model_sets[i].save_a;
    desc.model_no2 = -1;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.size = 220;
    desc.time = 0;
    return (void *)enemy_task_task_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, &desc);
}

/* steer(cast) */
/* 0x00b213f8 */
void virtues_wall_destroy(void *obj)
{
    if (obj && !(TASK_OF(obj)->flags & TASK_F_END) && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b21458 */
void virtues_wall_set_height(void *obj, int on, float y)
{
    VirtuesWallWork *w = enemy_task_work_ptr(obj);

    w->y = y;
    if (on) {
        w->count = 0;
        w->state = 1;
        if (enemy_task_get_pstate(obj) == 1)
            enemy_task_set_state(obj, 0);
    }
}

/* 0x00b214d8 */
void virtues_wall_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
