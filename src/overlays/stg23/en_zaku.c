/*
 * Trance Mission enemy "zaku": drifts along its placement's direction, rolling, weaving across
 * its path (dst_cycle wide, every frm_cycle frames) and wobbling.
 */

#include "stg23.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "map_manager.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_zaku.h"
#include "en_color.h"
#include "en_zaku_generator.h"
#include "stage_base.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_zaku_params;
extern VarDef en_zaku_vardefs[6];
extern int en_zaku_init_arg;

/* 116 bytes beyond the object */
typedef struct EnZakuWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10 */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    int model;                  /* 0x20 */
    int anim;                   /* 0x24 */
    float rgba[4];              /* 0x28 */
    unsigned int color;         /* 0x38 */
    Vec3 swing;                 /* 0x3c: the swing offset added last frame */
    float swing_t;              /* 0x48 */
    int cycle_step;             /* 0x4c: 65536 / placement frm_cycle */
    Vec3 ofs;                   /* 0x50: the side swing offset applied to the position */
    int dir_ang[3];             /* 0x5c: from the param velocity: [0] pitch, [1] yaw, [2] the creation roll */
    char pad_68[4];
    float frame;                /* 0x6c */
    float frames;               /* 0x70 */
} EnZakuWork;

#define EN_ZAKU_REC ((EnUniParams *)&en_zaku_params)
extern void (*en_zaku_disp_table[])(void *o, EnZakuWork *w);
extern char en_zaku_lock_name[];
extern char en_zaku_name[];
extern void (*en_zaku_exec_table[])(void *o, EnZakuWork *w);
extern void (*en_zaku_states[])(void *o, EnZakuWork *w);

static const char str_en_zaku_pos[] = "POS:%d,%d,%d";

static const char str_en_zaku_vec[] = "VEC:%f,%f,%f";
static const char str_en_zaku_ang[] = "ANG:%d,%d,%d";
static const char str_en_zaku_cnt[] = "CNT:%d/%d";

/* 0x00b00210 */
void en_zaku_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_zaku_params.name;
    def.fmt = en_zaku_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_zaku_vardefs;
    fld = en_zaku_params.v;
    for (i = 0; i < 6; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_zaku_vardefs);
    en_zaku_generator_vars_register();
}

/* 0x00b002c8 */
void en_zaku_vars_unregister(void)
{
    en_zaku_generator_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00b002e8 */
int en_zaku_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 1);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b00340 */
int en_zaku_vars_init(int *id)
{
    en_zaku_vars_register();
    en_zaku_init_arg = *id;
    return 0;
}

/* 0x00b00378 */
int en_zaku_vars_release(void)
{
    en_zaku_vars_unregister();
    return 0;
}

/* 0x00b00398 */
void en_zaku_swing(void *obj, EnZakuWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->swing_t += EN_ZAKU_REC->spd_swing;
    r = EN_ZAKU_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->swing);
    v3_set(&w->swing, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, &w->swing);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00b004c8 */
void en_zaku_life_scale(void *obj, EnZakuWork *w)
{
    Vec3 scl;
    Vec3 scl_out;
    float life = enemy_task_get_param(obj, 1);
    float fade = 30.0f;
    float t = enemy_task_get_state_time(obj);

    if (t <= fade) {
        float rate = t / fade;
        v3_set(&scl, rate, rate, rate);
        enemy_task_set_scl(obj, &scl);
    } else if (!(t <= life - fade)) {
        if (t <= life) {
            float rate = 1.0f - (t - (life - fade)) / fade;
            v3_set(&scl_out, rate, rate, rate);
            enemy_task_set_scl(obj, &scl_out);
        } else
            enemy_task_set_state(obj, 2);
    }
}

/* steer(cast) */
/* 0x00b005b0 */
void en_zaku_state_drift(void *obj, EnZakuWork *w)
{
    int start_ang[3];
    Vec3 vel;
    Vec3 pos;
    int ang[3];
    float sc[2];
    int roll;
    float frm, side;

    if (w->step != 1) {
        if (w->step != 0)
            return;
        v3_set(&w->ofs, 0.0f, 0.0f, 0.0f);
        enemy_task_get_rot(obj, start_ang);
        enemy_task_get_param_vec(obj, &vel);
        do {
            w->dir_ang[1] = (int)RAD_TO_ANG(atan2f(-vel.x, -vel.z));
            w->dir_ang[0] = (int)RAD_TO_ANG(atan2f(vel.y, sqrtf(vel.x * vel.x + vel.z * vel.z)));
        } while (0);
        w->dir_ang[2] = start_ang[2];
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
    }
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    v3_add(&pos, enemy_task_param_vec_ptr(obj));
    roll = ang[2];
    ang[2] = 0;
    v3_sub(&pos, &pos, &w->ofs);
    ulSinCos((int)(w->cycle_step * w->frame), &sc[0], &sc[1]);
    side = enemy_task_get_param(obj, 3) * sc[0];
    util_rotate_xyz(w->dir_ang, side, 0.0f, 0.0f, &w->ofs);
    v3_add(&pos, &w->ofs);
    ang[2] += roll + (int)DEG_TO_ANG(EN_ZAKU_REC->ang_spd_rotate_z * FRAME_SEC);
    enemy_task_set_pos(obj, &pos);
    enemy_task_set_rot(obj, ang);
    frm = w->frame += 1.0f;
    if (enemy_task_get_param(obj, 1) < frm)
        enemy_task_set_state(obj, 2);
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b008d0 */
void en_zaku_exec_normal(void *obj, EnZakuWork *w)
{
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        if (en_zaku_states[w->phase])
                en_zaku_states[w->phase](obj, w);
    }
}

/* 0x00b00948 */
void en_zaku_exec_destroyed(void *obj, EnZakuWork *w)
{
    SpawnDesc desc;
    Vec3 spos;
    int live = 1;
    unsigned int se;
    int mode = enemy_task_get_phase(obj);

    if (mode != live) {
        if (mode == 0) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.type = live;
            desc.kind = 0;
            desc.models = stage_models;
            v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            enemy_shatter_create(obj, &desc, 0, 0);
            enemy_task_get_pos(obj, &spos);
            ulMatrixApplyPoint((float *)ulMatrixCur, &spos.x, &spos.x);
            project_point(&spos, &spos);
            switch (map_manager_get_scene_id((int)map_menu_get())) {
            case 1:
                se = 1;
                break;
            case 3:
                se = 2;
                break;
            case 7:
                se = 3;
                break;
            case 10:
                se = 4;
                break;
            case 0:
                se = 0;
                break;
            default:
                se = 0;
                break;
            }
            SoundRhythmSe_EnemyPos(en_zaku_init_arg, se, &spos.x);
        }
    } else
        enemy_task_next_phase(obj);
}

/* 0x00b00aa8 */
void en_zaku_exec(void *obj, EnZakuWork *w)
{
    Vec3 prev;
    int dang[3];
    int ang[3];
    int aim[3];
    Vec3 move;
    void (**tbl)(void *obj, EnZakuWork *w);
    Vec3 *pos;
    unsigned int state;
    int len;
    float scl;

    enemy_task_get_pos(obj, &prev);
    if (enemy_task_get_phase(obj) == 1) {
        en_zaku_life_scale(obj, w);
        en_zaku_swing(obj, w);
    }
    tbl = en_zaku_exec_table;
    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    enemy_task_get_rot(obj, ang);
    pos = enemy_task_pos_ptr(obj);
    move.x = pos->x - prev.x;
    move.y = pos->y - prev.y;
    move.z = pos->z - prev.z;
    do {
        aim[1] = (int)RAD_TO_ANG(atan2f(-move.x, -move.z));
        aim[0] = (int)RAD_TO_ANG(atan2f(move.y, sqrtf(move.x * move.x + move.z * move.z)));
    } while (0);
    dang[0] = aim[0] - ang[0];
    dang[1] = aim[1] - ang[1];
    dang[2] = aim[2] - ang[2];
    do {    /* wrap angle deltas to signed 16 bits */
        dang[0] = (dang[0] & 0x8000) ? dang[0] | ~0xffff : dang[0] & 0xffff;
        dang[1] = (dang[1] & 0x8000) ? dang[1] | ~0xffff : dang[1] & 0xffff;
        dang[2] = (dang[2] & 0x8000) ? dang[2] | ~0xffff : dang[2] & 0xffff;
    } while (0);
    dang[2] = 0;
    len = model_ivec3_length(dang);
    if (len >= 547) {
        scl = 546.0f / len;
        dang[0] = dang[0] * scl;
        dang[1] = dang[1] * scl;
        dang[2] = dang[2] * scl;
    }
    dang[0] += ang[0];
    dang[1] += ang[1];
    dang[2] += ang[2];
    enemy_task_set_rot(obj, dang);
}

/* 0x00b00d50 */
void en_zaku_pre_exec(void)
{
}

/* 0x00b00d58 */
void en_zaku_post_exec(void *obj, int *w)
{
    w[14] = en_color_wire_at(enemy_task_pos_ptr(obj));
}

/* steer(cast) */
/* 0x00b00d88 */
void en_zaku_disp_body(void *obj, EnZakuWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int blend;

    if (view_sphere_visible(pos, gfx->radius)) {
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_zaku_rotate_yxz(enemy_task_rot_ptr(obj));
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, enemy_task_scl_ptr(obj)->x, enemy_task_scl_ptr(obj)->y, enemy_task_scl_ptr(obj)->z);
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_set_override_color(w->rgba);
        dc_object_draw(gfx);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        draw_set_blend(4, 5);
        draw_set_color(w->color);
        model_draw_kind_list(w->model, w->anim);
        ulMatrixPop();
    }
}

/* 0x00b00f08 */
void en_zaku_disp(void *obj, EnZakuWork *w)
{
    void (**tbl)(void *obj, EnZakuWork *w) = en_zaku_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 vel;
    int ang[3];

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_ZAKU_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_zaku_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf(str_en_zaku_vec, vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_zaku_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        debug_printf(str_en_zaku_cnt, (int)w->frame, (int)w->frames);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b010e0 */
void en_zaku_init(void *obj, EnZakuWork *w)
{
    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->model = *((int *)pnb_get_entry(stage_models, 2));
    w->anim = model_pnb_get_wire(stage_models, 2);
    en_color_body_rgba(w->rgba);
    w->cycle_step = ANG_TURN / enemy_task_get_param(obj, 2);
    lockon_obj_set_name((void *)enemy_task_lock_ptr(obj), en_zaku_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    enemy_task_clear_flags(obj, 64);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b011f0 */
void en_zaku_release(void)
{
}

/* 0x00b011f8 */
int en_zaku_create(void *parent, TypeRec *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_zaku_init;
    funcs.fn[1] = en_zaku_release;
    funcs.fn[2] = en_zaku_exec;
    funcs.fn[3] = en_zaku_disp;
    funcs.fn[4] = en_zaku_pre_exec;
    funcs.fn[5] = en_zaku_post_exec;
    desc.name = en_zaku_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 0;
    desc.dc_flags = 0;
    desc.model_no = 1;
    desc.model_no2 = -1;
    desc.hp = EN_ZAKU_REC->hp;
    desc.score = EN_ZAKU_REC->score;
    desc.time = place->param1;
    desc.size = 116;
    desc.crash_r = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b01328 */
void en_zaku_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
