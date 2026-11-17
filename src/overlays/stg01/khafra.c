/*
 * Khafra, Area 1 enemy: flies in rolling, brakes and turns, charges an attack ray and fires, then
 * speeds off (two pattern variants). Beat-coloured wire pass and ring.
 */

#include "stg01.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "pnb.h"
#include "ps_gs.h"
#include "rez_wire_compile.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern char khafra_name[];
extern ObjFuncs khafra_funcs;
extern void (*khafra_disp_table[])(void *o, KhafraWork *w);
extern void (*khafra_states[])(void *o, KhafraWork *w);
extern void (*khafra_exec_table[])(void *o, KhafraWork *w);
void khafra_update(void *act, KhafraWork *w);
int khafra_next_state(void *act, KhafraWork *w, int n, int state);
void khafra_animate_shot(void *act, KhafraWork *w);
extern char khafra_lock_name[];
extern int khafra_init_arg;
extern KhafraVars khafra_types[1];
extern VarDef khafra_vardef;
extern VarDef khafra_vardefs[70];
void khafra_vars_unregister(void);
int khafra_vars_release(void);
void khafra_rotate_yxz(int *ang);

/* 0x00af9db0 */
void khafra_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *vd;
    int *fld;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&khafra_vardef);
    for (i = 0; i < 1; i++) {
        vd = khafra_vardefs;
        fld = (int *)&khafra_types[i].p;
        khafra_vardef.name = khafra_types[i].name;
        khafra_vardef.fmt = khafra_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&khafra_vardef);
        for (j = 0; j < 70; j++) {
            vd->ptr = fld;
            fld++;
            vd++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)khafra_vardefs);
    }
}

/* 0x00af9ec8 */
void khafra_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00af9ed0 */
int khafra_get_init_arg(void)
{
    return khafra_init_arg;
}

/* 0x00af9ee0 */
int khafra_vars_init(int *prm)
{
    khafra_init_arg = *prm;
    khafra_vars_register();
    return 0;
}

/* 0x00af9f08 */
int khafra_vars_release(void)
{
    khafra_vars_unregister();
    return 0;
}

/* 0x00af9f28 */
int khafra_marker_draw(int obj, void *act)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 0);
    return obj_draw_at(obj, act, (void *)*ent);
}

/* 0x00af9f80 */
int khafra_node_cb(unsigned int *nd, int ni, KhafraWork *w)
{
    if (*nd & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    if (*nd & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    return 0;
}

/* 0x00af9ff0 */
void khafra_pose_model(void *act, KhafraWork *w)
{
    Vec3 cpos;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(act);

    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    dc_object_set_callback(gfx, khafra_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, cpos.x, cpos.y, cpos.z);
    khafra_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* steer(cast) */
/* 0x00afa0c0 */
void khafra_init(void *act, KhafraWork *w, int plp, int *kind)
{
    Vec3 v;
    int ang[3];
    BackFireLink lnk;
    float sc[2][2];
    float scu[3][2];
    KhafraVars *typ;
    float bdis, frm, x, y, z;
    float ux, uy, uz;
    int base;

    bdis = enemy_task_get_param(act, 4);
    typ = &khafra_types[*kind];
    w->type = typ;
    lockon_obj_init(&w->lock, act, &w->pos, 0, khafra_lock_name, enemy_task_get_status_slot(act), typ->p.hp,
                  typ->p.same_lock, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    enemy_task_clear_flags(act, 64);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)act, 2)), *((struct DcClip **)pnb_get_entry(*(void **)act, 3)), 0x500);
    ((ModelPart *)enemy_task_gfx_ptr(act))->radius = 26.0f;
    w->body.radius = 26.0f;
    dc_object_set_wire(&w->body, model_pnb_get_wire(*(void **)act, 2));
    dc_object_init(&w->ring, *((struct DcNode **)pnb_get_entry(*(void **)act, 4)), *((struct DcClip **)pnb_get_entry(*(void **)act, 5)), 0x1000);
    dc_object_rebuild(&w->ring);
    w->ring.radius = 3.5f;
    enemy_task_get_param_vec(act, &v);
    enemy_task_get_rot(act, ang);
    if (v3_is_zero(&v)) {
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        w->speed = 0.0f;
        v3_set(&v, x, y, z);
    } else {
        w->speed = vec3_length(&v);
        vec3_normalize(&v, &v);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
    }
    enemy_task_set_param_vec(act, &v);
    enemy_task_set_rot(act, ang);
    i3_copy(w->ang, ang);
    w->roll = 0;
    frm = enemy_task_get_param(act, 1);
    w->fire = 0;
    w->shot = (int)frm > 0;
    if (bdis > 0.0f) {
        w->state = 0;
        w->end = enemy_task_get_param(act, 3) * 60.0f / 60.0f <= 1.0f ? 1.0f
                 : enemy_task_get_param(act, 3) * 60.0f / 60.0f;
        base = w->end + enemy_task_get_param(act, 4) / typ->p.break_spd * FPS;
        frm = typ->p.shot_motion_sec_dif * FPS;
        w->end = (int)(base + frm);
        w->start = (int)(w->end - typ->p.shot_motion_sec * FPS);
    } else if (!w->shot) {
        w->state = 4;
    } else {
        w->state = 6;
        base = enemy_task_get_param(act, 3) * 60.0f / 60.0f;
        frm = (typ->p.break2_sec + typ->p.shot2_sec) * FPS;
        w->end = (int)(base + frm);
        w->start = (int)(w->end - typ->p.shot_motion_sec * FPS);
    }
    w->frames = 1;
    enemy_task_set_appear(act, 2, 30);
    enemy_task_set_vanish(act, 2, 60);
    ux = 0.0f;
    uy = 1.0f;
    uz = 0.0f;
    ROT_Z(ang[2], scu[0], ux, uy);
    ROT_X(ang[0], scu[1], uy, uz);
    ROT_Y(ang[1], scu[2], ux, uz);
    v3_set(&w->up, ux, uy, uz);
    v3_set(&w->sway, 0.0f, 0.0f, 0.0f);
    heap_bzero(&lnk, sizeof(lnk));
    lnk.gfx = enemy_task_gfx_ptr(act);
    lnk.pos = enemy_task_pos_ptr(act);
    lnk.ang = enemy_task_rot_ptr(act);
    lnk.obj_scale = enemy_task_scl_ptr(act);
    lnk.scale = &w->backfire_scl;
    lnk.len = typ->p.backfire_len;
    lnk.width = typ->p.backfire_width;
    lnk.len_scale = &w->backfire2_scl;
    lnk.width_scale = &w->backfire2_scl;
    lnk.alpha = &w->alpha;
    lnk.flags = 0x2000000;
    lnk.use_pose = 0;
    en_backfire_beam_create(act, &lnk);
    w->backfire_scl = typ->p.backfire_scl;
    w->backfire2_scl = typ->p.backfire2_scl;
}

/* 0x00afa730 */
void khafra_release(void *act, KhafraWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->ring);
    lockon_remove(&w->lock);
}

/* 0x00afa768 */
int khafra_next_state(void *act, KhafraWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00afa798 */
void khafra_animate_shot(void *act, KhafraWork *w)
{
    float rate;
    float frm;
    ModelPart *gfx;
    AttackRayArg ray;

    rate = (enemy_task_get_time(act) - w->start) / (w->end - w->start);
    gfx = enemy_task_gfx_ptr(act);
    frm = gfx->frames;
    if (rate < 1.0f)
        frm *= rate;
    if (w->shot && !(frm < 0.0f)) {
        gfx->frame = frm;
        if (frm < 0.0f)
            return;
        switch (w->shot) {
        case 1:
            ray.pos = &w->tip;
            ray.ang = enemy_task_rot_ptr(act);
            ray.scale = &w->scale;
            ray.dur = (w->end - w->start) * 29.0f / gfx->frames;
            attack_ray_create(act, &ray);
            w->shot = 2;
            break;
        case 2:
            if (!(frm < 29.0f)) {
                w->shot = 3;
                w->fire = 1;
            }
            break;
        case 3:
            w->fire = 0;
            break;
        }
    }
}

/* 0x00afa928 */
void khafra_state_move(void *act, KhafraWork *w)
{
    int n;
    Vec3 cpos;
    Vec3 v;

    n = enemy_task_get_param(act, 3) * 60.0f / 60.0f <= 1.0f ? 1.0f : enemy_task_get_param(act, 3) * 60.0f / 60.0f;
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, w->speed);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / FPS;
    khafra_animate_shot(act, w);
    khafra_next_state(act, w, n, 1);
}

/* 0x00afaaa8 */
void khafra_state_break(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v, ofs;
    int ang[3];
    Vec3 dv, ax;
    float mtx[16];
    int nang[4];
    float sc[5][2];
    float fps, uno, half_turn, quarter, half;
    float dur, rate, rprev, rem, dis, amp, crv, x, y, z;
    int n;

    fps = FPS;
    dur = fabsf(enemy_task_get_param(act, 4)) / typ->p.break_spd;
    uno = 1.0f;
    half_turn = ANG_HALF;
    quarter = ANG_QUARTER;
    half = 0.5f;
    x = 0.0f;
    y = 0.0f;
    n = (int)(dur * fps);
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(act, &cpos);
    v3_sub(&cpos, &cpos, &w->sway);
    rem = uno - rate;
    dis = -(w->speed * fps) / (typ->p.break_pow + uno) * dur * (safe_powf(rem, typ->p.break_pow + uno) - uno);
    dis -= -(w->speed * fps) / (typ->p.break_pow + uno) * dur
         * (safe_powf(uno - rprev, typ->p.break_pow + uno) - uno);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    amp = enemy_task_get_param(act, 4) * (typ->p.break_r_over_rate + uno);
    ulSinCos((int)(safe_powf(rate, typ->p.break_r_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    crv = safe_powf((sc[0][0] + uno) * half, typ->p.break_r_pow2);
    ulSinCos((int)(safe_powf(rprev, typ->p.break_r_pow) * half_turn - quarter), &sc[0][0], &sc[0][1]);
    crv -= safe_powf((sc[0][0] + uno) * half, typ->p.break_r_pow2);
    amp *= crv;
    v3_mul(&ofs, &w->up, amp);
    v3_add(&cpos, &ofs);
    ulSinCos((int)(safe_powf(rate, typ->p.break_dis_pow) * ANG_TURN - quarter), &sc[1][0], &sc[1][1]);
    z = -safe_powf((sc[1][0] + uno) * half, typ->p.break_dis_pow2) * typ->p.break_dis;
    enemy_task_get_rot(act, ang);
    ROT_X(ang[0], sc[2], y, z);
    ROT_Y(ang[1], sc[3], x, z);
    v3_set(&w->sway, x, y, z);
    v3_add(&cpos, &w->sway);
    enemy_task_set_pos(act, &cpos);
    enemy_task_get_param_vec(act, &dv);
    ulVec3Cross(&ax, &dv, &w->up);
    ulSinCos((int)(safe_powf(rate, typ->p.break_ang_pow) * half_turn - quarter), &sc[4][0], &sc[4][1]);
    crv = safe_powf((sc[4][0] + uno) * half, typ->p.break_ang_pow2);
    matrix_from_axis_angle(mtx, &ax, (int)((typ->p.break_ang_over_rate + uno) * half_turn * crv));
    ulMatrixPush();
    ulMatrixLoad(mtx);
    khafra_rotate_yxz(w->ang);
    model_camera_angles(nang);
    ulMatrixPop();
    enemy_task_set_rot(act, nang);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / fps * (uno - rate);
    if (w->shot) {
        w->backfire_scl = safe_powf((uno - rate), typ->p.break_backfire_pow) * typ->p.backfire_scl;
        w->backfire2_scl = safe_powf((uno - rate), typ->p.break_backfire2_pow) * typ->p.backfire2_scl;
    } else {
        w->backfire_scl = safe_powf((uno - rate), typ->p.break_backfire_pow) * typ->p.backfire_scl + safe_powf(rate, typ->p.break_backfire_pow2) * typ->p.break_backfire_scl;
        w->backfire2_scl = safe_powf((uno - rate), typ->p.break_backfire2_pow) * typ->p.backfire2_scl + safe_powf(rate, typ->p.break_backfire2_pow2) * typ->p.break_backfire2_scl;
    }
    khafra_animate_shot(act, w);
    if (w->shot)
        khafra_next_state(act, w, n, 5);
    else
        khafra_next_state(act, w, n, 2);
}

/* 0x00afb0f8 */
void khafra_state_shot(void *act, KhafraWork *w)
{
    int n = w->type->p.shot_motion_sec_dif * FPS;
    ModelPart *gfx;

    khafra_animate_shot(act, w);
    if (khafra_next_state(act, w, n, 2)) {
        gfx = enemy_task_gfx_ptr(act);
        gfx->frame = gfx->frames;
    }
}

/* 0x00afb178 */
void khafra_state_accel(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v, ofs;
    Vec3 dv, ax;
    float mtx[16];
    int nang[4];
    float sc[2][2];
    float fps, uno, full, quarter, half, half_turn;
    float rate, rprev, rem, dis, amp, crv, acrv;
    int n;

    fps = FPS;
    uno = 1.0f;
    full = ANG_TURN;
    half_turn = ANG_HALF;
    quarter = ANG_QUARTER;
    half = 0.5f;
    n = typ->p.accel_sec * fps;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(act, &cpos);
    rem = uno - rate;
    dis = w->speed * fps / (typ->p.accel_pow + uno) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + uno)
        * safe_powf(rate, typ->p.accel_pow + uno);
    dis -= w->speed * fps / (typ->p.accel_pow + uno) * typ->p.accel_sec * (typ->p.accel_spd_over_rate + uno)
         * safe_powf(rprev, typ->p.accel_pow + uno);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, -dis);
    v3_add(&cpos, &v);
    amp = enemy_task_get_param(act, 4) * typ->p.break_r_over_rate;
    ulSinCos((int)(safe_powf(rate, typ->p.accel_r_pow) * full - quarter), &sc[0][0], &sc[0][1]);
    crv = safe_powf((sc[0][0] + uno) * half, typ->p.accel_r_pow2) * rem;
    ulSinCos((int)(safe_powf(rprev, typ->p.accel_r_pow) * full - quarter), &sc[0][0], &sc[0][1]);
    crv -= safe_powf((sc[0][0] + uno) * half, typ->p.accel_r_pow2) * rem;
    v3_mul(&ofs, &w->up, -amp * crv);
    v3_add(&cpos, &ofs);
    enemy_task_set_pos(act, &cpos);
    enemy_task_get_param_vec(act, &dv);
    ulVec3Cross(&ax, &dv, &w->up);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow) * half_turn - quarter), &sc[1][0], &sc[1][1]);
    acrv = safe_powf((sc[1][0] + uno) * half, typ->p.accel_ang_pow2);
    ulSinCos((int)(safe_powf(rate, typ->p.accel_ang_pow3) * full - quarter), &sc[1][0], &sc[1][1]);
    acrv += safe_powf((sc[1][0] + uno) * half, typ->p.accel_ang_pow4) * typ->p.accel_ang_rate;
    acrv = uno - acrv;
    matrix_from_axis_angle(mtx, &ax, (int)((typ->p.break_ang_over_rate * acrv + uno) * half_turn));
    ulMatrixPush();
    ulMatrixLoad(mtx);
    khafra_rotate_yxz(w->ang);
    model_camera_angles(nang);
    ulMatrixPop();
    enemy_task_set_rot(act, nang);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / fps * rate;
    if (!w->shot) {
        w->backfire_scl = (typ->p.backfire_scl - typ->p.break_backfire_scl) * safe_powf(rate, typ->p.accel_backfire_pow) + typ->p.break_backfire_scl;
        w->backfire2_scl = (typ->p.backfire2_scl - typ->p.break_backfire2_scl) * safe_powf(rate, typ->p.accel_backfire_pow2) + typ->p.break_backfire2_scl;
    } else {
        w->backfire_scl = typ->p.backfire_scl * safe_powf(rate, typ->p.accel_backfire2_pow);
        w->backfire2_scl = typ->p.backfire2_scl * safe_powf(rate, typ->p.accel_backfire2_pow2);
    }
    khafra_next_state(act, w, n, 3);
}

/* 0x00afb690 */
void khafra_state_finish(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v;
    float fps, sspd, espd, dis;
    int n;

    fps = FPS;
    sspd = w->speed * fps;
    n = typ->p.finish_sec * fps;
    espd = typ->p.accel_spd_over_rate * sspd + sspd;
    dis = ((sspd - espd) * (w->frames / (float)n) * 0.5f + espd) * (w->frames / fps)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / fps);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, -dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / fps;
    if (khafra_next_state(act, w, n, 4))
        w->speed = -w->speed;
}

/* 0x00afb860 */
void khafra_state_cruise(void *act, KhafraWork *w)
{
    Vec3 cpos, v;

    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, w->speed);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / FPS;
}

/* 0x00afb950 */
void khafra_state_move2(void *act, KhafraWork *w)
{
    Vec3 cpos, v;
    float fps, frm;

    enemy_task_get_pos(act, &cpos);
    fps = FPS;
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, w->speed);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / fps;
    khafra_animate_shot(act, w);
    frm = enemy_task_get_time(act);
    if (frm > (int)(enemy_task_get_param(act, 3) * fps / fps)) {
        w->state = 7;
        w->frames = 1;
    }
}

/* 0x00afbab0 */
void khafra_state_break2(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v;
    float uno, rate, rprev, dis;
    int n;

    uno = 1.0f;
    n = typ->p.break2_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(act, &cpos);
    rate = uno - rate;
    dis = -(w->speed * FPS) / (typ->p.break2_pow + uno) * typ->p.break2_sec
        * (safe_powf(rate, typ->p.break2_pow + uno) - uno);
    dis -= -(w->speed * FPS) / (typ->p.break2_pow + uno) * typ->p.break2_sec
         * (safe_powf(uno - rprev, typ->p.break2_pow + uno) - uno);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) / FPS * rate;
    w->backfire_scl = safe_powf(rate, typ->p.break2_backfire_pow) * typ->p.backfire_scl;
    w->backfire2_scl = safe_powf(rate, typ->p.break2_backfire2_pow) * typ->p.backfire2_scl;
    khafra_animate_shot(act, w);
    khafra_next_state(act, w, n, 8);
}

/* 0x00afbd00 */
void khafra_state_shot2(void *act, KhafraWork *w)
{
    int n = w->type->p.shot2_sec * FPS;
    ModelPart *gfx;

    khafra_animate_shot(act, w);
    if (khafra_next_state(act, w, n, 9)) {
        gfx = enemy_task_gfx_ptr(act);
        gfx->frame = gfx->frames;
    }
}

/* 0x00afbd80 */
void khafra_state_accel2(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v;
    float rate, rprev, dis;
    int n;

    n = typ->p.accel2_sec * FPS;
    rate = w->frames / (float)n;
    rprev = (w->frames - 1) / (float)n;
    enemy_task_get_pos(act, &cpos);
    dis = w->speed * FPS / (typ->p.accel2_pow + 1.0f) * typ->p.accel2_sec * (typ->p.accel2_spd_over_rate + 1.0f)
        * safe_powf(rate, typ->p.accel2_pow + 1.0f);
    dis -= w->speed * FPS / (typ->p.accel2_pow + 1.0f) * typ->p.accel2_sec * (typ->p.accel2_spd_over_rate + 1.0f)
         * safe_powf(rprev, typ->p.accel2_pow + 1.0f);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2)) * (typ->p.accel2_roll_spd_rate + 1.0f) / FPS
                * safe_powf(rate, typ->p.accel2_roll_pow);
    w->backfire_scl = typ->p.accel2_backfire_scl * safe_powf(rate, typ->p.accel2_backfire_pow);
    w->backfire2_scl = typ->p.accel2_backfire2_scl * safe_powf(rate, typ->p.accel2_backfire2_pow);
    khafra_next_state(act, w, n, 10);
}

/* 0x00afbfe0 */
void khafra_state_finish2(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    Vec3 cpos, v;
    float uno, sspd, espd, dis, rate;
    int n;

    uno = 1.0f;
    n = typ->p.finish2_sec * FPS;
    espd = w->speed * FPS * (typ->p.accel2_spd_over_rate + 1.0f);
    sspd = w->speed * FPS;
    rate = w->frames / (float)n;
    dis = ((sspd - espd) * rate * 0.5f + espd) * (w->frames / FPS)
        - ((sspd - espd) * ((w->frames - 1) / (float)n) * 0.5f + espd) * ((w->frames - 1) / FPS);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_param_vec(act, &v);
    v3_scale(&v, dis);
    v3_add(&cpos, &v);
    enemy_task_set_pos(act, &cpos);
    w->roll = w->roll - (int)DEG_TO_ANG(enemy_task_get_param(act, 2))
                * (typ->p.accel2_roll_spd_rate * (uno - rate) + uno) / FPS;
    khafra_next_state(act, w, n, 4);
}

/* steer(cast) */
/* 0x00afc1d0 */
void khafra_update(void *act, KhafraWork *w)
{
    int ang[3];
    Vec3 dv;
    float sc[2][2];
    float x, y, z;
    int n;

    if (khafra_states[w->state])
        khafra_states[w->state](act, w);
    khafra_pose_model(act, w);
    if (w->fire) {
        n = ((float (*)(float))float_frac)(enemy_task_get_param(act, 1)) * 1000.0f + 4.0f + 0.5f;
        enemy_task_get_rot(act, ang);
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&dv, x, y, z);
        n = n < 0 ? 0 : n;
        e_shot_fire(act, &w->tip, &dv, n > 7 ? 7 : n);
        w->fire = 0;
    }
}

/* 0x00afc348 */
void khafra_exec_appear(void *act, KhafraWork *w)
{
    enemy_task_appear_scale(act, w);
    if (w->alpha > 0.0f)
        w->lock.flags |= LOCK_LOCKABLE;
    switch (enemy_task_get_phase(act)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        w->lock.flags |= LOCK_LOCKABLE;
        break;
    default:
        khafra_update(act, w);
    }
}

/* 0x00afc3e8 */
void khafra_exec_normal(void *act, KhafraWork *w)
{
    float t;

    if (enemy_task_get_phase(act) != 1)
        return;
    khafra_update(act, w);
    t = enemy_task_get_time(act);
    if (fabsf(enemy_task_get_param(act, 0)) * 60.0f / 60.0f < t)
        enemy_task_set_state(act, 2);
}

/* 0x00afc488 */
void khafra_exec_leave(void *act, KhafraWork *w)
{
    do { enemy_task_vanish_scale(act, w); } while (0);
    switch (enemy_task_get_phase(act)) {
    case 0:
        w->lock.flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        khafra_update(act, w);
    }
}

/* 0x00afc500 */
void khafra_exec_destroyed(void *act, KhafraWork *w)
{
    SpawnDesc dsc;

    if (enemy_task_get_phase(act) == 0) {
        nj_fill8(&dsc, 0, sizeof dsc);
        dsc.type = 6;
        dsc.models = stage_models;
        dsc.lock = &w->lock;
        dsc.kind = 0;
        enemy_task_get_pos(act, &dsc.pos);
        enemy_task_get_rot(act, dsc.ang);
        enemy_task_get_scl(act, &dsc.scale);
        enemy_task_get_param_vec(act, &dsc.v);
        enemy_shatter_create(act, &dsc, 0, 0);
        FINISH_IF(act);
    }
}

/* 0x00afc5c0 */
void khafra_pre_exec(void)
{
}

/* 0x00afc5c8 */
void khafra_exec(void *act, KhafraWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (khafra_exec_table[i])
        khafra_exec_table[i](act, w);
}

/* steer(cast) */
/* 0x00afc620 */
void khafra_post_exec(void *act, KhafraWork *w)
{
    KhafraVars *typ = w->type;
    LockObj *lk = &w->lock;
    Vec3 cpos;
    float alp;
    int s;

    enemy_task_get_scl(act, &w->scale);
    v3_scale(&w->scale, typ->p.attack_ray_scl);
    if (!enemy_status_get_ticks())
        khafra_pose_model(act, w);
    switch (((unsigned int)enemy_task_get_pstate(act))) {
    case 1:
        alp = enemy_task_get_state_time(act) / 30.0f;
        w->alpha = alp;
        alp = MIN(alp >= 0.0f ? alp : 0.0f, 1.0f);
        w->alpha = alp;
        break;
    case 0:
        w->alpha = 1.0f;
        break;
    case 2:
        alp = 1.0f - enemy_task_get_state_time(act) / FPS;
        w->alpha = alp;
        alp = MIN(alp >= 0.0f ? alp : 0.0f, 1.0f);
        w->alpha = alp;
        break;
    default:
        w->alpha = 0.0f;
        break;
    }
    if (lk->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lk->imp.v);
        if (lk->life <= 0 && ((unsigned int)enemy_task_get_pstate(act)) < 2) {
            int mult = lockon_hits_max_shot_cnt(lk);

            enemy_task_get_pos(act, &cpos);
            enemy_status_score_popup(enemy_task_get_status_slot(act), &cpos, ((int *)&typ->p)[2], mult);
            enemy_task_set_state(act, 3);
        }
    }
}

/* Wire colour from the type's wire_* parameters and the beat; all faded by w->alpha. */
/* steer(cast) */
/* 0x00afc840 */
void khafra_disp_body(void *act, KhafraWork *w)
{
    Vec3 cpos;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    KhafraVars *typ;
    ModelPart *ring;
    int bl;

    gfx = enemy_task_gfx_ptr(act);
    enemy_task_get_pos(act, &cpos);
    enemy_task_get_rot(act, ang);
    enemy_task_get_scl(act, &scl);
    if (view_sphere_visible(&cpos, gfx->radius)) {
        ring = &w->ring;
        bl = 0;
        typ = w->type;
        if (w->alpha < 1.0f) {
            bl = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(rgba, 16);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            rgba[0] = w->alpha - 1.0f;
            draw_set_override_color(rgba);
            psGsSetAlphaTest(0, 1, 128, 1);
        }
        ulGxSetTextureList(((void **)act)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &cpos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        khafra_rotate_yxz(ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat,
                                          typ->p.wire_linear, w->alpha, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate,
                                          typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw(&w->body, gfx->frame);
        }
        ulMatrixPop();
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        khafra_rotate_yxz(ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        model_object_draw(ring, ring->frames * yuki_beat_phase(typ->p.parts_beat));
        ulMatrixPop();
        if (w->alpha < 1.0f) {
            psGsSetAlphaTest(1, 5, 128, 1);
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(bl);
        }
    }
}

/* 0x00afcae0 */
void khafra_disp(void *act, KhafraWork *w)
{
    unsigned int i = enemy_task_get_pstate(act);

    if (khafra_disp_table[i])
        khafra_disp_table[i](act, w);
}

/* steer(cast) */
/* 0x00afcb38 */
int khafra_create(void *parent, Placement *plp)
{
    ObjDesc dsc;
    ObjFuncs fns = khafra_funcs;
    int kind;
    KhafraVars *typ;

    ((float (*)(float))float_frac)(plp->param0);
    typ = khafra_types;
    kind = 0;
    nj_fill8(&dsc, 0, sizeof dsc);
    dsc.name = khafra_name;
    dsc.arg = plp;
    dsc.kind = &kind;
    dsc.funcs = &fns;
    dsc.models = stage_models;
    dsc.tex = stage_textures;
    dsc.hp = typ->p.hp;
    dsc.same_lock = typ->p.same_lock;
    dsc.flags = 18;
    dsc.dc_flags = 0;
    dsc.model_no = 0;
    dsc.model_no2 = 1;
    dsc.score = typ->p.score;
    dsc.hp_reduce = typ->p.hp_reduce;
    dsc.crash_r = typ->p.crash_r;
    dsc.crash_damage = typ->p.crash_damage;
    dsc.time = fabsf(plp->param0) * 60.0f / 60.0f;
    dsc.size = 380;
    return enemy_task_task_create(parent, &dsc);
}

/* 0x00afcc90 */
void khafra_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
