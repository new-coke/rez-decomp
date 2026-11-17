/*
 * Trance Mission enemy "gufu", spawned in groups with random positions, directions and timings;
 * like the dom grows in, drifts and escapes along its escape direction.
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
#include "node_tree.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_gufu.h"
#include "en_color.h"
#include "en_gufu_generator.h"
#include "stage_base.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_gufu_params;
extern VarDef en_gufu_vardefs[8];
extern int en_gufu_init_arg;
#define EN_GUFU_REC ((EnDomParams *)&en_gufu_params)
extern void (*en_gufu_states[])(void *o, EnDomWork *w);
extern void (*en_gufu_exec_table[])(void *o, EnDomWork *w);
extern void (*en_gufu_disp_table[])(void *o, EnDomWork *w);
extern char en_gufu_name[];
extern char en_gufu_lock_name[];

static const char str_en_gufu_pos[] = "POS:%d,%d,%d";

static const char str_en_gufu_vec[] = "VEC:%f,%f,%f";
static const char str_en_gufu_ang[] = "ANG:%d,%d,%d";
static const char str_en_gufu_cnt[] = "CNT:%d/%d";

/* 0x00afb0e0 */
void en_gufu_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_gufu_params.name;
    def.fmt = en_gufu_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_gufu_vardefs;
    fld = en_gufu_params.v;
    for (i = 0; i < 8; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_gufu_vardefs);
    en_gufu_generator_vars_register();
}

/* 0x00afb198 */
void en_gufu_vars_unregister(void)
{
    en_gufu_generator_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00afb1b8 */
int en_gufu_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 1);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00afb210 */
int en_gufu_vars_init(int *id)
{
    en_gufu_vars_register();
    en_gufu_init_arg = *id;
    return 0;
}

/* 0x00afb248 */
int en_gufu_vars_release(void)
{
    en_gufu_vars_unregister();
    return 0;
}

/* 0x00afb268 */
void en_gufu_swing(void *obj, EnDomWork *w)
{
    Vec3 pos;
    float t, r;

    t = w->swing_t += EN_GUFU_REC->spd_swing;
    r = EN_GUFU_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->swing);
    v3_set(&w->swing, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(&pos, &w->swing);
    enemy_task_set_pos(obj, &pos);
}

/* steer(cast) */
/* 0x00afb398 */
void en_gufu_state_drift(void *obj, EnDomWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 vel;
    Vec3 scl;
    int frm;
    float rate;

    switch (w->step) {
    case 0:
        w->frame = 1.0f;
        frm = enemy_task_get_param(obj, 1);
        w->step++;
        w->next_step++;
        w->frames = (int)(float)frm;
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        enemy_task_get_param_vec(obj, &vel);
        v3_add(&pos, &vel);
        ang[2] += (int)DEG_TO_ANG(EN_GUFU_REC->ang_spd_rotate_z * FRAME_SEC);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        if (w->frame <= 30.0f) {
            rate = w->frame / 30.0f;
            v3_set(&scl, rate, rate, rate);
            enemy_task_set_scl(obj, &scl);
        }
        w->frame += 1.0f;
        if (w->frames < w->frame) {
            v3_copy(&w->drift, &vel);
            i3_set(w->spin, 0, 0, (int)DEG_TO_ANG(EN_GUFU_REC->ang_spd_rotate_z * FRAME_SEC));
            w->frame = 1.0f;
            w->frames = (int)(((float (*)(float))float_frac)(enemy_task_get_param(obj, 1)) * 1000.0f + 0.5f);
            do { w->next_step = w->step + 1; } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        w->frame += 1.0f;
        if (w->frames < w->frame)
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
        break;
    }
}

/* 0x00afb658 */
void en_gufu_state_escape_anticipate(void *obj, EnDomWork *w)
{
    Vec3 vel;
    Vec3 pos;
    int ang[3];
    Vec3 back;
    float x, y, rate, ease;

    switch (w->step) {
    case 0:
        x = enemy_task_get_param(obj, 2);
        y = enemy_task_get_param(obj, 3);
        v3_set(&vel, x, y, enemy_task_get_param(obj, 4));
        util_dir_to_player_frame(&vel, &vel);
        enemy_task_set_param_vec(obj, &vel);
        w->step++;
        w->next_step++;
        i3_set(w->spin, 0, 0, -(int)DEG_TO_ANG(EN_GUFU_REC->ang_spd_rotate_z * FRAME_SEC));
        w->frame = 1.0f;
        w->frames = (int)(EN_GUFU_REC->frm_escape_anticipate * FPS);
    case 1:
        rate = w->frame / w->frames;
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        enemy_task_get_param_vec(obj, &back);
        ease = ease_sine_in_out_rev(rate);
        v3_scale(&back, ease);
        v3_sub(&pos, &pos, &back);
        ang[2] += (int)DEG_TO_ANG(EN_GUFU_REC->ang_spd_rotate_z * FRAME_SEC);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        w->frame += 1.0f;
        if (w->frames < w->frame) {
            v3_neg(&w->drift, &back);
            do {
                w->next = 2;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afb910 */
void en_gufu_state_escape(void *obj, EnDomWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 vel;
    Vec3 scl;
    float rate;

    switch (w->step) {
    case 0:
        w->frame = 1.0f;
        w->step++;
        w->next_step++;
        w->frames = (int)(EN_GUFU_REC->frm_escape * FPS);
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        enemy_task_get_param_vec(obj, &vel);
        v3_add(&pos, &vel);
        ang[2] += (int)DEG_TO_ANG(EN_GUFU_REC->ang_spd_rotate_z * FRAME_SEC);
        if (w->frames - 30.0f < w->frame) {
            rate = 1.0f - (w->frame - (w->frames - 30.0f)) / 30.0f;
            v3_set(&scl, rate, rate, rate);
            enemy_task_set_scl(obj, &scl);
        }
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        w->frame += 1.0f;
        if (w->frames < w->frame)
            enemy_task_set_state(obj, 2);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afbb10 */
void en_gufu_exec_normal(void *obj, EnDomWork *w)
{
    Vec3 pos;
    int ang[3];

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        v3_scale(&w->drift, 0.97f);
        i3_scale(w->spin, 0.96f);
        v3_add(&pos, &w->drift);
        i3_add(ang, w->spin);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        if (en_gufu_states[w->phase])
            en_gufu_states[w->phase](obj, w);
    }
}

/* 0x00afbca0 */
void en_gufu_exec_destroyed(void *obj, EnDomWork *w)
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
            case 5:
                se = 2;
                break;
            case 6:
                se = 3;
                break;
            case 7:
                se = 4;
                break;
            case 8:
                se = 5;
                break;
            case 9:
                se = 6;
                break;
            case 10:
                se = 7;
                break;
            case 3:
            default:
                se = 0;
                break;
            }
            SoundRhythmSe_EnemyPos(en_gufu_init_arg, se, &spos.x);
        }
    } else
        enemy_task_next_phase(obj);
}

/* 0x00afbe18 */
void en_gufu_exec(void *obj, EnDomWork *w)
{
    Vec3 prev;
    int dang[3];
    int ang[3];
    int aim[3];
    Vec3 move;
    void (**tbl)(void *obj, EnDomWork *w);
    Vec3 *pos;
    unsigned int state;
    int len;
    float scl;

    enemy_task_get_pos(obj, &prev);
    if (enemy_task_get_phase(obj) == 1)
        en_gufu_swing(obj, w);
    tbl = en_gufu_exec_table;
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

/* 0x00afc0b8 */
void en_gufu_pre_exec(void)
{
}

/* 0x00afc0c0 */
void en_gufu_post_exec(void *obj, int *w)
{
    w[14] = en_color_wire_at(enemy_task_pos_ptr(obj));
}

/* steer(cast) */
/* 0x00afc0f0 */
void en_gufu_disp_body(void *obj, EnDomWork *w)
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
        en_gufu_rotate_yxz(enemy_task_rot_ptr(obj));
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

/* 0x00afc270 */
void en_gufu_disp(void *obj, EnDomWork *w)
{
    void (**tbl)(void *obj, EnDomWork *w) = en_gufu_disp_table;
    unsigned int state;
    Vec3 pos;
    Vec3 vel;
    int ang[3];

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_GUFU_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_gufu_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf(str_en_gufu_vec, vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf(str_en_gufu_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        debug_printf(str_en_gufu_cnt, (int)w->frame, (int)w->frames);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00afc448 */
void en_gufu_init(void *obj, EnDomWork *w)
{
    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    w->model = *((int *)pnb_get_entry(stage_models, 2));
    w->anim = model_pnb_get_wire(stage_models, 2);
    en_color_body_rgba(w->rgba);
    lockon_obj_set_name((void *)enemy_task_lock_ptr(obj), en_gufu_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    enemy_task_clear_flags(obj, 64);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00afc530 */
void en_gufu_release(void)
{
}

/* 0x00afc538 */
int en_gufu_create(void *parent, void *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_gufu_init;
    funcs.fn[1] = en_gufu_release;
    funcs.fn[2] = en_gufu_exec;
    funcs.fn[3] = en_gufu_disp;
    funcs.fn[4] = en_gufu_pre_exec;
    funcs.fn[5] = en_gufu_post_exec;
    desc.name = en_gufu_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 0;
    desc.dc_flags = 0;
    desc.model_no = 1;
    desc.model_no2 = -1;
    desc.hp = EN_GUFU_REC->hp;
    desc.score = EN_GUFU_REC->score;
    desc.size = 112;
    desc.time = 0.0f;
    desc.crash_r = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00afc660 */
void en_gufu_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
