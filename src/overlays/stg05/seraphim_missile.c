/*
 * Seraphim missiles, Area 5: four on each side of a Seraphim. Each opens out of its launcher,
 * homes on the player until close, then flies on firing bullets (seraphim_bullet_create).
 * Shot-down missiles are counted by their Seraphim.
 */

#include "stg05.h"
#include "mathdef.h"
#include "approach.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "player.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "seraphim_missile.h"
#include "macros_missile.h"
#include "../common/air_mng.h"
#include "../common/en_backfire.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int seraphim_missile_init_arg;
extern void (*seraphim_missile_exec_table[])(void *o, struct SeraphimMissileWork *w);
extern DominionsBombSet seraphim_missile_types[2];
extern VarDef seraphim_missile_vardef;
extern VarDef seraphim_missile_vardefs[18];
extern void (*seraphim_missile_states[])(void *o, SeraphimMissileWork *w);

/* ObjDesc.kind points to it */
typedef struct SeraphimMissileKind {
    int idx;
    SeraphimMissileArg *arg;         /* 0x04 */
    void *owner;                /* 0x08 */
} SeraphimMissileKind;

extern char seraphim_missile_lock_name[];
extern char seraphim_missile_name[];
extern ObjFuncs seraphim_missile_funcs;
extern void (*seraphim_missile_disp_table[])(void *o, SeraphimMissileWork *w);

/* 0x00b258c0 */
void seraphim_missile_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&seraphim_missile_vardef);
    for (i = 0; i < 2; i++) {
        def = seraphim_missile_vardefs;
        val = seraphim_missile_types[i].v;
        seraphim_missile_vardef.name = seraphim_missile_types[i].name;
        seraphim_missile_vardef.fmt = seraphim_missile_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&seraphim_missile_vardef);
        for (j = 0; j < 18; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)seraphim_missile_vardefs);
    }
}

/* 0x00b259d8 */
void seraphim_missile_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b259e0 */
int seraphim_missile_get_init_arg(void)
{
    return seraphim_missile_init_arg;
}

/* 0x00b259f0 */
int seraphim_missile_vars_init(int *prm)
{
    seraphim_missile_init_arg = *prm;
    seraphim_missile_vars_register();
    return 0;
}

/* 0x00b25a18 */
int seraphim_missile_vars_release(void)
{
    seraphim_missile_vars_unregister();
    return 0;
}

/* 0x00b25a38 */
int seraphim_missile_node_cb(unsigned int *n, int idx, SeraphimMissileWork *w)
{
    if (*n & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    if (*n & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->tip.x);
    return 0;
}

/* 0x00b25aa8 */
void seraphim_missile_pose_model(void *obj, SeraphimMissileWork *w)
{
    Vec3 p;
    int ang[3];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    dc_object_set_callback(gfx, seraphim_missile_node_cb, w);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
    seraphim_missile_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00b25b70 */
void seraphim_missile_init(void *obj, SeraphimMissileWork *w, int prm, SeraphimMissileKind *k)
{
    SeraphimMissileRec *typ;

    typ = (SeraphimMissileRec *)&seraphim_missile_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    lockon_obj_init(&w->lock, obj, &w->pos, 0, seraphim_missile_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same_lock, 128);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    w->arg = *k->arg;
    w->count = 1;
    w->state = 0;
    w->owner = k->owner;
    w->roll = 0;
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = 500.0f;
}

/* 0x00b25cc8 */
void seraphim_missile_release(void *obj, SeraphimMissileWork *w)
{
    lockon_remove(&w->lock);
}

/* 0x00b25cd0 */
int seraphim_missile_next_state(void *obj, SeraphimMissileWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        return 1;
    }
    return 0;
}

/* steer(cast) */
/* 0x00b25d00 */
void seraphim_missile_state_launch(void *obj, SeraphimMissileWork *w)
{
    SeraphimMissileRec *typ = w->type;
    Vec3 next;
    Vec3 p;
    Vec3 pl;
    Vec3 v;
    Vec3 to_pl;
    int ang[3];
    float sc[2][2];
    float rate;
    float k, spd;
    int n;

    n = typ->accel_sec * FPS;
    rate = w->count / (float)n;
    enemy_task_get_pos(obj, &p);
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, &p);
    vec3_normalize(&to_pl, &to_pl);
    enemy_task_get_param_vec(obj, &v);
    sc[0][0] = w->count / ((typ->accel_sec + typ->accel_ang_sec) * FPS);
    ulSinCos(sc[0][0] * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    vec3_approach(&to_pl, &v, (sc[0][0] + 1.0f) * 0.5f * typ->accel_ang_rate);
    vec3_normalize(&v, &v);
    spd = 0.0f;
    k = typ->accel_sec * rate;
    spd += typ->accel * k;
    k = FRAME_SEC;
    spd *= k;
    spd += typ->accel * 0.5f * k * k;
    v3_scale(&v, spd);
    v3_sum(&next, &p, &v);
    enemy_task_set_pos(obj, &next);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&next, &p, ang);
    enemy_task_set_rot(obj, ang);
    w->roll = w->roll + typ->roll_spd / FPS * rate;
    ulSinCos(safe_powf(rate, typ->fire_pow2) * ANG_HALF, &sc[1][0], &sc[1][1]);
    w->fire = (sc[1][0] + safe_powf(rate, typ->fire_pow)) * typ->fire_size;
    seraphim_missile_next_state(obj, w, n, 2);
    if (vec3_distance_squared(&next, &pl) < typ->separate * typ->separate) {
        if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 2);
        v3_copy(&w->dir, &v);
    }
}

/* steer(cast) */
/* 0x00b26048 */
void seraphim_missile_state_cruise(void *obj, SeraphimMissileWork *w)
{
    SeraphimMissileRec *typ = w->type;
    Vec3 next;
    Vec3 p;
    Vec3 pl;
    Vec3 v;
    Vec3 to_pl;
    int ang[3];
    float sa[2];

    enemy_task_get_pos(obj, &p);
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, &p);
    vec3_normalize(&to_pl, &to_pl);
    enemy_task_get_param_vec(obj, &v);
    sa[0] = (w->count + typ->accel_sec * FPS) / ((typ->accel_sec + typ->accel_ang_sec) * FPS);
    ulSinCos(sa[0] < 1.0f ? sa[0] * ANG_HALF - ANG_QUARTER : ANG_90, &sa[0], &sa[1]);
    vec3_approach(&to_pl, &v, (sa[0] + 1.0f) * 0.5f * typ->accel_ang_rate);
    vec3_normalize(&v, &v);
    v3_scale(&v, typ->accel_sec * typ->accel * FRAME_SEC);
    v3_sum(&next, &p, &v);
    enemy_task_set_pos(obj, &next);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&next, &p, ang);
    enemy_task_set_rot(obj, ang);
    w->roll += typ->roll_spd / FPS;
    w->count++;
    if (vec3_distance_squared(&next, &pl) < typ->separate * typ->separate) {
        if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 2);
        v3_copy(&w->dir, &v);
    }
}

/* 0x00b26300 */
void seraphim_missile_update(void *obj, SeraphimMissileWork *w)
{
    void (*fn)(void *obj, SeraphimMissileWork *w) = seraphim_missile_states[w->state];

    if (fn)
        fn(obj, w);
    seraphim_missile_pose_model(obj, w);
}

/* 0x00b26358 */
void seraphim_missile_exec_appear(void *obj, SeraphimMissileWork *w)
{
    SeraphimMissileArg *prm = &w->arg;
    ModelPart *gfx;
    int done = 0;
    int frm, n;

    frm = enemy_task_get_state_time(obj);
    gfx = enemy_task_gfx_ptr(obj);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        break;
    case 2:
        break;
    default:
        if (prm->lock_at < frm)
            w->lock.flags |= LOCK_LOCKABLE;
        if (frm < prm->open) {
            gfx->frame = frm / (float)prm->open * 16.0f;
        } else if (frm < (n = prm->open + prm->hold)) {
            gfx->frame = 16.0f;
        } else if (frm < n + prm->close) {
            frm -= n;
            gfx->frame = (gfx->frames - 16.0f) * (frm / (float)prm->close) + 16.0f;
        } else {
            gfx->frame = gfx->frames;
            frm -= n + prm->close;
            done = !(frm < prm->wait);
        }
        seraphim_missile_pose_model(obj, w);
        if (done) {
            enemy_task_set_state(obj, 0);
            SoundRhythmSe_Bullet(seraphim_missile_init_arg, 3);
            done = 0;
        }
    }
}

/* 0x00b264f0 */
void seraphim_missile_exec_normal(void *obj, SeraphimMissileWork *w)
{
    SeraphimMissileRec *typ = w->type;
    int ang[3];
    Vec3 v;
    BackFireLink lnk;
    TrailArg trail;
    float sc[2][2];
    float x, y, z;
    ModelPart *gfx;
    SeraphimMissileArg *prm;

    switch (enemy_task_get_phase(obj)) {
    case 0:
        gfx = enemy_task_gfx_ptr(obj);
        prm = &w->arg;
        w->state = 1;
        w->count = 1;
        gfx->frame = gfx->frames;
        w->lock.flags |= LOCK_LOCKABLE;
        enemy_task_set_pos(obj, prm->pos);
        enemy_task_set_rot(obj, prm->ang);
        enemy_task_get_rot(obj, ang);
        x = 0.0f;
        y = 0.0f;
        z = -1.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&v, x, y, z);
        enemy_task_set_param_vec(obj, &v);
        heap_bzero(&lnk, 52);
        lnk.gfx = enemy_task_gfx_ptr(obj);
        lnk.pos = enemy_task_pos_ptr(obj);
        lnk.ang = enemy_task_rot_ptr(obj);
        lnk.obj_scale = enemy_task_scl_ptr(obj);
        lnk.scale = &w->fire;
        lnk.flags = 0x2000000;
        lnk.use_pose = 1;
        en_backfire_create(obj, &lnk, 0);
        w->fire = 0.0f;
        heap_bzero(&trail, 40);
        trail.body = 0;
        trail.pos = enemy_task_pos_ptr(obj);
        trail.ang = enemy_task_rot_ptr(obj);
        trail.tail_seg = 1;
        trail.frames = w->type->tail_sec * FPS;
        trail.tail_tex = 1;
        trail.tail_width = w->type->tail_width;
        trail.one = 1;
        trail.tail_skip = typ->tail_skip;
        air_mng_create(obj, &trail);
        if (w->owner)
            *w->arg.launched = 1;
        break;
    case 2:
        break;
    default:
        seraphim_missile_update(obj, w);
    }
}

/* 0x00b26738 */
void seraphim_missile_exec_leave(void *obj, SeraphimMissileWork *w)
{
    SpawnDesc desc;
    SeraphimBulletArg barg;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.models = stage_models;
        desc.kind = 0x10000;
        desc.type = 184;
        desc.lock = &w->lock;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 5);
        barg.pos = &w->tip;
        barg.ang = enemy_task_rot_ptr(obj);
        barg.type = w->arg.type;
        vec3_normalize(&barg.vel, &w->dir);
        barg.speed = vec3_length(&w->dir) * FPS;
        seraphim_bullet_create(obj, &barg);
        FINISH_IF(obj);
    }
}

/* 0x00b26868 */
void seraphim_missile_exec_destroyed(void *obj, SeraphimMissileWork *w)
{
    SpawnDesc desc;
    ModelPart *gfx;

    if (enemy_task_get_phase(obj) == 0) {
        gfx = enemy_task_gfx_ptr(obj);
        nj_fill8(&desc, 0, sizeof desc);
        desc.kind = 0x40000;
        desc.models = stage_models;
        desc.type = 182;
        desc.motion = 183;
        desc.time = gfx->frame;
        desc.lock = &w->lock;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 2);
        FINISH_IF(obj);
    }
}

/* 0x00b26960 */
void seraphim_missile_pre_exec(void *obj)
{
    Vec3 p, org;

    if (enemy_task_get_pstate(obj) != 1) {
        enemy_task_get_pos(obj, &p);
        player_get_frame_vel(&org);
        p.x -= org.x;
        p.y -= org.y;
        p.z -= org.z;
        player_get_vec(&org);
        p.x += org.x;
        p.y += org.y;
        p.z += org.z;
        enemy_task_set_pos(obj, &p);
    }
}

/* 0x00b26a20 */
void seraphim_missile_exec(void *obj, SeraphimMissileWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (seraphim_missile_exec_table[i])
        seraphim_missile_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b26a78 */
void seraphim_missile_post_exec(void *obj, SeraphimMissileWork *w)
{
    LockObj *lock = &w->lock;
    SeraphimMissileRec *typ;
    Vec3 p;
    int n;

    if (((unsigned int)enemy_task_get_pstate(obj)) == 1) {
        if (!w->owner || (TASK_OF(w->owner)->flags & TASK_F_END)) {
            if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
                enemy_task_set_state(obj, 3);
            w->owner = 0;
        } else {
            enemy_task_set_pos(obj, w->arg.pos);
            enemy_task_set_rot(obj, w->arg.ang);
        }
    }
    if (!enemy_status_get_ticks())
        seraphim_missile_pose_model(obj, w);
    if (lock->flags & LOCK_HIT) {
        enemy_task_get_pos(obj, &p);
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
        project_point(&p, &p);
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life > 0) {
            SoundRhythmSe_BulletPos(seraphim_missile_init_arg, 2, &p);
        } else {
            n = lockon_hits_max_shot_cnt(lock);
            typ = w->type;
            enemy_status_score_popup(enemy_task_get_status_slot(obj), &w->pos, typ->score, n);
            SoundRhythmSe_BulletPos(seraphim_missile_init_arg, 1, &p);
            {
                int state = ((unsigned int)enemy_task_get_pstate(obj));

                if (state == 1 && w->owner)
                    *w->arg.killed = state;
            }
            if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
                enemy_task_set_state(obj, 3);
        }
    }
}

/* 0x00b26c40 */
void seraphim_missile_disp_body(void *obj, SeraphimMissileWork *w)
{
    Vec3 p;
    int ang[3];
    ModelPart *gfx;

    enemy_task_gfx_ptr(obj);
    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, gfx->radius)) {
        enemy_task_get_rot(obj, ang);
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        seraphim_missile_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        dc_object_draw(gfx);
        ulMatrixPop();
    }
}

/* 0x00b26d10 */
void seraphim_missile_disp(void *obj, SeraphimMissileWork *w)
{
    void (*fn)(void *obj, SeraphimMissileWork *w);
    void *fog;
    int i;

    i = enemy_task_get_pstate(obj);
    fn = seraphim_missile_disp_table[i];
    if (fn) {
        fog = fog_set_enable(0);
        fn(obj, w);
        fog_set_enable(fog);
    }
}

/* 0x00b26d98 */
int seraphim_missile_create(void *obj, SeraphimMissileArg *prm)
{
    SeraphimMissileKind k;
    ObjDesc desc;
    ObjFuncs funcs = seraphim_missile_funcs;
    ObjFuncs *pfuncs = &funcs;
    ShotArg place;
    SeraphimMissileRec *typ;
    int none = -1;

    k.idx = MAX_GE(prm->type, 0) != 0;
    k.arg = prm;
    k.owner = obj;
    typ = (SeraphimMissileRec *)&seraphim_missile_types[k.idx];
    nj_fill8(&place, 0, sizeof place);
    place.kind = seraphim_missile_init_arg;
    place.fx_arg = none;
    place.pos = *prm->pos;
    *(Vec3 *)place.ang = *(Vec3 *)prm->ang;
    place.vel.x = 0.0f;
    place.vel.y = 0.0f;
    place.vel.z = 0.0f;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = seraphim_missile_name;
    desc.funcs = pfuncs;
    desc.arg = &place;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 180;
    desc.model_no2 = 181;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = 0;
    desc.size = 176;
    return enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}

/* 0x00b26f78 */
void *seraphim_missile_get_link(void *obj)
{
    return (char *)enemy_task_work_ptr(obj) + 76;
}

/* 0x00b26f98 */
void seraphim_missile_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
