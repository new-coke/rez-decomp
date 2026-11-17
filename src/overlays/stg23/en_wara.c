/*
 * Trance Mission enemy "wara", kept coming a few at a time by its generator: drifts along its
 * placement's direction, rolling, wobbling by the placement's radius and speed ('swing_r.spd').
 */

#include "stg23.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_wara.h"
#include "en_color.h"
#include "en_wara_generator.h"
#include "stage_base.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern ParamRec en_wara_params;
extern VarDef en_wara_vardefs[4];
extern int en_wara_init_arg;

/* 4 words after name and label */
typedef struct EnWaraParams {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float ang_spd_rotate_z;     /* 0x10 */
    int test;                   /* 0x14 */
} EnWaraParams;

/* 100 bytes beyond the object */
typedef struct EnWaraWork {
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
    int swing[3];
    float rad_swing;            /* 0x48: placement rad_swing */
    int drift_ang[4];
    float frame;                /* 0x5c */
    float frames;               /* 0x60 */
} EnWaraWork;

#define EN_WARA_REC ((EnWaraParams *)&en_wara_params)
extern void (*en_wara_disp_table[])(void *o, EnWaraWork *w);
extern char en_wara_lock_name[];
extern char en_wara_name[];
extern void (*en_wara_states[])(void *o, EnWaraWork *w);
extern void (*en_wara_exec_table[])(void *o, EnWaraWork *w);

static const char str_en_wara_pos[] = "POS:%d,%d,%d";

static const char str_en_wara_vec[] = "VEC:%f,%f,%f";
static const char str_en_wara_ang[] = "ANG:%d,%d,%d";
static const char str_en_wara_cnt[] = "CNT:%d/%d";

/* 0x00afe9f8 */
void en_wara_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_wara_params.name;
    def.fmt = en_wara_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_wara_vardefs;
    fld = en_wara_params.v;
    for (i = 0; i < 4; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_wara_vardefs);
    en_wara_generator_vars_register();
}

/* 0x00afeab0 */
void en_wara_vars_unregister(void)
{
    en_wara_generator_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00afead0 */
int en_wara_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 1);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00afeb28 */
int en_wara_vars_init(int *id)
{
    en_wara_vars_register();
    en_wara_init_arg = *id;
    return 0;
}

/* 0x00afeb60 */
int en_wara_vars_release(void)
{
    en_wara_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00afeb80 */
void en_wara_swing(void *obj, EnWaraWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->rad_swing += ((float (*)(float))float_frac)(enemy_task_get_param(obj, 2));
    r = (int)enemy_task_get_param(obj, 2);
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, (Vec3 *)w->swing);
    v3_set((Vec3 *)w->swing, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, (Vec3 *)w->swing);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00afecd0 */
void en_wara_life_scale(void *obj, EnWaraWork *w)
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

/* 0x00afedb8 */
void en_wara_state_drift(void *obj, EnWaraWork *w)
{
    int start_ang[3];
    Vec3 vel;
    Vec3 pos;
    int ang[3];
    float frm;

    if (w->step != 1) {
        if (w->step != 0)
            return;
        enemy_task_get_rot(obj, start_ang);
        enemy_task_get_param_vec(obj, &vel);
        do {
            w->drift_ang[1] = (int)RAD_TO_ANG(atan2f(-vel.x, -vel.z));
            w->drift_ang[0] = (int)RAD_TO_ANG(atan2f(vel.y, sqrtf(vel.x * vel.x + vel.z * vel.z)));
        } while (0);
        w->drift_ang[2] = start_ang[2];
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
    }
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    v3_add(&pos, enemy_task_param_vec_ptr(obj));
    ang[2] += (int)DEG_TO_ANG(EN_WARA_REC->ang_spd_rotate_z * FRAME_SEC);
    enemy_task_set_pos(obj, &pos);
    enemy_task_set_rot(obj, ang);
    frm = w->frame += 1.0f;
    if (enemy_task_get_param(obj, 1) < frm)
        enemy_task_set_state(obj, 2);
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00aff000 */
void en_wara_exec_normal(void *obj, EnWaraWork *w)
{
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        if (en_wara_states[w->phase])
                en_wara_states[w->phase](obj, w);
    }
}

/* 0x00aff078 */
void en_wara_exec_destroyed(void *obj, EnWaraWork *w)
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
            case 4:
                se = 1;
                break;
            case 6:
                se = 2;
                break;
            case 8:
                se = 3;
                break;
            case 9:
                se = 4;
                break;
            case 2:
                se = 0;
                break;
            default:
                se = 0;
                break;
            }
            SoundRhythmSe_EnemyPos(en_wara_init_arg, se, &spos.x);
        }
    } else
        enemy_task_next_phase(obj);
}

/* 0x00aff1d8 */
void en_wara_exec(void *obj, EnWaraWork *w)
{
    Vec3 prev;
    int ang[3];
    Vec3 move;
    void (**tbl)(void *obj, EnWaraWork *w);
    Vec3 *pos;
    unsigned int state;

    enemy_task_get_pos(obj, &prev);
    if (enemy_task_get_phase(obj) == 1) {
        en_wara_life_scale(obj, w);
        en_wara_swing(obj, w);
    }
    tbl = en_wara_exec_table;
    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    enemy_task_get_rot(obj, ang);
    pos = enemy_task_pos_ptr(obj);
    move.x = pos->x - prev.x;
    move.y = pos->y - prev.y;
    move.z = pos->z - prev.z;
    if (!v3_is_zero(&move)) {
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-move.x, -move.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(move.y, sqrtf(move.x * move.x + move.z * move.z)));
        } while (0);
        enemy_task_set_rot(obj, ang);
    }
}

/* 0x00aff3b8 */
void en_wara_pre_exec(void)
{
}

/* 0x00aff3c0 */
void en_wara_post_exec(void *obj, int *w)
{
    w[14] = en_color_wire_at(enemy_task_pos_ptr(obj));
}

/* steer(cast) */
/* 0x00aff3f0 */
void en_wara_disp_body(void *obj, EnWaraWork *w)
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
        en_wara_rotate_yxz(enemy_task_rot_ptr(obj));
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

/* 0x00aff570 */
void en_wara_disp(void *obj, EnWaraWork *w)
{
    void (**tbl)(void *obj, EnWaraWork *w) = en_wara_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 vel;
    int ang[3];

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_WARA_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_wara_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf(str_en_wara_vec, vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_wara_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        debug_printf(str_en_wara_cnt, (int)w->frame, (int)w->frames);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00aff748 */
void en_wara_init(void *obj, EnWaraWork *w)
{
    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->model = *((int *)pnb_get_entry(stage_models, 2));
    w->anim = model_pnb_get_wire(stage_models, 2);
    en_color_body_rgba(w->rgba);
    lockon_obj_set_name((void *)enemy_task_lock_ptr(obj), en_wara_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    w->rad_swing = enemy_task_get_param(obj, 5);
    enemy_task_clear_flags(obj, 64);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00aff840 */
void en_wara_release(void)
{
}

/* 0x00aff848 */
int en_wara_create(void *parent, TypeRec *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_wara_init;
    funcs.fn[1] = en_wara_release;
    funcs.fn[2] = en_wara_exec;
    funcs.fn[3] = en_wara_disp;
    funcs.fn[4] = en_wara_pre_exec;
    funcs.fn[5] = en_wara_post_exec;
    desc.name = en_wara_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 0;
    desc.dc_flags = 0;
    desc.model_no = 1;
    desc.model_no2 = -1;
    desc.hp = EN_WARA_REC->hp;
    desc.score = EN_WARA_REC->score;
    desc.time = place->param1;
    desc.size = 100;
    desc.crash_r = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00aff978 */
void en_wara_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
