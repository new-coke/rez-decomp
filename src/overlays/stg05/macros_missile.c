/*
 * Area 5 homing projectiles: MacrosMissile from Powers (steers to a point ahead with a random aim
 * offset, slows near the player, bursts into a model break-up) and SeraphimBullet from Seraphim
 * missiles (accelerates, then cruises at the player). Both can be shot and hurt on contact.
 */

#include "stg05.h"
#include "mathdef.h"
#include "approach.h"
#include "debug.h"
#include "enemy_task.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "player.h"
#include "player_status.h"
#include "ul_math.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "macros_missile.h"
#include "ef_mdl_brk.h"
#include "stage05.h"
#include "../common/air_mng.h"
#include "../common/en_backfire.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int macros_missile_init_arg;
extern int seraphim_bullet_init_arg;

/* 72 bytes */
typedef struct MacrosMissileWork {
    struct MacrosMissileRec *type;
    Vec3 hitv;                  /* 0x04: the lock-on record's vector when hit */
    int state;                  /* 0x10: indexes macros_missile_states */
    int count;                  /* 0x14: frames of the current action */
    char pad_18[8];
    Vec3 dir;                   /* 0x20: the course toward the path point */
    int hit;                    /* 0x2c: touched the player */
    float act;                  /* 0x30: seconds of one action */
    float speed;                /* 0x34: distance per second */
    float speed0;               /* 0x38: the speed it was created with */
    float act0;                 /* 0x3c */
    float ramp;                 /* 0x40: seconds to reach the record's speed and action */
    float fire;                 /* 0x44: the back fire size */
} MacrosMissileWork;

extern void (*macros_missile_exec_table[])(void *o, MacrosMissileWork *w);
extern void (*macros_missile_disp_table[])(void *o, MacrosMissileWork *w);

typedef struct SeraphimBulletWork {
    struct SeraphimBulletRec *type;
    int state;                  /* 0x04: indexes seraphim_bullet_states */
    int count;                  /* 0x08: frames */
    SeraphimBulletArg arg;           /* 0x0c */
    int hit;                    /* 0x28: touched the player or left the view */
    void *owner;                /* 0x2c */
    float fire;                 /* 0x30: the back fire size */
    int roll;                   /* 0x34 */
    char pad_38[4];
    int started;                /* 0x3c */
} SeraphimBulletWork;

extern void (*seraphim_bullet_exec_table[])(void *o, SeraphimBulletWork *w);
extern EShotSet macros_missile_types[1];
extern VarDef macros_missile_vardef;
extern VarDef macros_missile_vardefs[24];

/* group name, label, 19 values */
typedef struct SeraphimBulletSet {
    char *name;
    char *label;                /* 0x04 */
    int v[19];                  /* 0x08 */
} SeraphimBulletSet;

extern SeraphimBulletSet seraphim_bullet_types[2];
extern VarDef seraphim_bullet_vardef;
extern VarDef seraphim_bullet_vardefs[19];

/* view of seraphim_bullet_types */
typedef struct SeraphimBulletRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_recover;           /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float accel_sec;            /* 0x20 */
    float accel;                /* 0x24 */
    float fire_pow;             /* 0x28 */
    float fire_pow2;            /* 0x2c */
    float fire_size;            /* 0x30 */
    float accel_ang_sec;        /* 0x34 */
    int roll_spd;               /* 0x38: degrees per second */
    float speed0;               /* 0x3c */
    float speed_far;            /* 0x40 */
    float speed_near;           /* 0x44 */
    float dist_far;             /* 0x48 */
    float dist_near;            /* 0x4c */
    int area;                   /* 0x50: for yuki_camera_relative */
} SeraphimBulletRec;

/* ObjDesc.kind points to it */
typedef struct SeraphimBulletKind {
    int idx;
    SeraphimBulletArg *arg;          /* 0x04 */
    void *owner;                /* 0x08 */
} SeraphimBulletKind;

extern void (*seraphim_bullet_states[])(void *o, SeraphimBulletWork *w);
extern void (*seraphim_bullet_disp_table[])(void *o, SeraphimBulletWork *w);
extern char seraphim_bullet_lock_name[];
extern char seraphim_bullet_name[];
extern ObjFuncs seraphim_bullet_funcs;

/* view of macros_missile_types */
typedef struct MacrosMissileRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_recover;           /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float life;                 /* 0x20: seconds */
    float tail_sec;             /* 0x24 */
    float tail_width;           /* 0x28 */
    int tail_skip;              /* 0x2c */
    float speed;                /* 0x30: distance per second */
    float dist_far;             /* 0x34: slow-down distances */
    float dist_near;            /* 0x38 */
    float slow;                 /* 0x3c: speed factor when near */
    float pow;                  /* 0x40: ease exponent - 1 of the forward move */
    float pow2;                 /* 0x44: of the sideways move */
    float act;                  /* 0x48: seconds of one action */
    float path_sec;             /* 0x4c: how far ahead on the path it aims */
    float noise;                /* 0x50: random aim offset size */
    float aim;                  /* 0x54: weight of the path direction in the facing */
    float aim_sec;              /* 0x58: seconds until the aim offset is gone */
    int clip;                   /* 0x5c: for yuki_camera_relative */
    float fire_scl;             /* 0x60 */
    int roll_spd;               /* 0x64: degrees per second */
} MacrosMissileRec;

/* ObjDesc.kind points to it */
typedef struct MacrosMissileKind {
    int idx;
    float act;                  /* 0x04 */
    float ramp;                 /* 0x08 */
    float speed;                /* 0x0c: per frame */
} MacrosMissileKind;

extern void (*macros_missile_states[])(void *o, MacrosMissileWork *w);
extern char macros_missile_lock_name[];
extern char macros_missile_name[];
extern ObjFuncs macros_missile_funcs;

/* 0x00b234e8 */
void macros_missile_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&macros_missile_vardef);
    for (i = 0; i < 1; i++) {
        def = macros_missile_vardefs;
        val = macros_missile_types[i].v;
        macros_missile_vardef.name = macros_missile_types[i].name;
        macros_missile_vardef.fmt = macros_missile_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&macros_missile_vardef);
        for (j = 0; j < 24; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)macros_missile_vardefs);
    }
}

/* 0x00b23600 */
void macros_missile_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b23608 */
int macros_missile_get_init_arg(void)
{
    return macros_missile_init_arg;
}

/* 0x00b23618 */
int macros_missile_vars_init(int *prm)
{
    macros_missile_init_arg = *prm;
    macros_missile_vars_register();
    return 0;
}

/* 0x00b23640 */
int macros_missile_vars_release(void)
{
    macros_missile_vars_unregister();
    return 0;
}

/* 0x00b23660 */
void macros_missile_steer(void *obj, MacrosMissileWork *w)
{
    MacrosMissileRec *typ = w->type;
    Vec3 target;
    Vec3 p;
    Vec3 n;
    int ang[3];
    float sc[3][2];
    float k, z;

    z = 0.0f;
    if (fabsf(typ->aim_sec) < FLT_EPSILON) {
        k = z;
    } else {
        k = 1.0f - enemy_task_get_time(obj) / (typ->aim_sec * FPS);
        k = MIN(MAX_GE(k, 0.0f), 1.0f);
    }
    player_pass_pos_a(&target, typ->path_sec * FPS * k);
    enemy_task_get_pos(obj, &p);
    v3_sub(&w->dir, &target, &p);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-w->dir.x, -w->dir.z));
        ang[0] = RAD_TO_ANG(atan2f(w->dir.y, sqrtf(w->dir.x * w->dir.x + w->dir.z * w->dir.z)));
    } while (0);
    ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0][0], &sc[0][1]);
    ROT_X(ang[0], sc[1], sc[0][1], z);
    ROT_Y(ang[1], sc[2], sc[0][0], z);
    v3_set(&n, sc[0][0], sc[0][1], z);
    v3_scale(&n, ulVec3Distance(&target, &p) * typ->noise);
    v3_add(&w->dir, &n);
    vec3_normalize(&w->dir, &w->dir);
}

/* 0x00b23990 */
void macros_missile_init(void *obj, MacrosMissileWork *w, int prm, MacrosMissileKind *k)
{
    MacrosMissileRec *typ;
    BackFireLink lnk;
    TrailArg trail;
    Vec3 v;

    typ = (MacrosMissileRec *)&macros_missile_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    lockon_obj_set_name(enemy_task_lock_ptr(obj), macros_missile_lock_name);
    heap_bzero(&lnk, 52);
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.flags = 0x2000000;
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->fire;
    w->fire = typ->fire_scl;
    lnk.use_pose = 1;
    en_backfire_create(obj, &lnk, 0);
    heap_bzero(&trail, 40);
    trail.body = enemy_task_gfx_ptr(obj);
    trail.pos = enemy_task_pos_ptr(obj);
    trail.ang = enemy_task_rot_ptr(obj);
    trail.tail_seg = 1;
    trail.frames = typ->tail_sec * FPS;
    trail.tail_tex = 1;
    trail.tail_mask = 0x1000000;
    trail.tail_width = typ->tail_width;
    trail.one = 1;
    trail.tail_skip = typ->tail_skip;
    air_mng_create(obj, &trail);
    w->count = 1;
    w->state = 0;
    w->speed0 = w->speed = k->speed * FPS;
    w->act0 = w->act = k->act;
    w->ramp = k->ramp;
    macros_missile_steer(obj, w);
    enemy_task_get_param_vec(obj, &v);
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
    v3_add(&w->dir, &v);
    vec3_normalize(&w->dir, &w->dir);
    w->hit = 0;
}

/* 0x00b23bc8 */
void macros_missile_release(void)
{
}

/* steer(cast) */
/* 0x00b23bd0 */
void macros_missile_state_fly(void *obj, MacrosMissileWork *w)
{
    MacrosMissileRec *typ;
    Vec3 p;
    Vec3 next;
    Vec3 v;
    Vec3 mv;
    Vec3 pl;
    Vec3 target;
    Vec3 aim;
    Vec3 to_target;
    Vec3 fwd;
    int ang[3];
    float rate, prev, spd_slow, speed, dist, pw, step, k, unit, fps;
    int n;
    int i;

    n = w->act * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    typ = w->type;
    enemy_task_get_pos(obj, &p);
    spd_slow = w->speed * typ->slow;
    player_get_pos(&pl);
    dist = ulVec3Distance(&pl, &p);
    if (typ->dist_far < dist)
        speed = w->speed;
    else if (dist < typ->dist_near)
        speed = spd_slow;
    else
        speed = (spd_slow - w->speed) * ((typ->dist_far - dist) / (typ->dist_far - typ->dist_near)) + w->speed;
    unit = 1.0f;
    fps = FPS;
    pw = typ->pow;
    pw += unit;
    step = unit - safe_powf(unit - rate, pw);
    step *= speed;
    step /= pw;
    step *= w->act;
    step -= (unit - safe_powf(unit - prev, pw)) * speed / pw * w->act;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, step);
    v3_sum(&next, &p, &v);
    pw = typ->pow2;
    pw += unit;
    step = safe_powf(rate, pw);
    step *= speed;
    step /= pw;
    step *= w->act;
    step -= safe_powf(prev, pw) * speed / pw * w->act;
    v3_mul(&mv, &w->dir, step);
    v3_add(&next, &mv);
    enemy_task_set_pos(obj, &next);
    player_pass_pos_a(&target, typ->path_sec * fps * rate);
    v3_sub(&to_target, &target, &next);
    vec3_normalize(&to_target, &to_target);
    v3_sub(&fwd, &next, &p);
    vec3_normalize(&fwd, &fwd);
    aim.x = to_target.x * typ->aim + fwd.x * (unit - typ->aim);
    aim.y = to_target.y * typ->aim + fwd.y * (unit - typ->aim);
    aim.z = to_target.z * typ->aim + fwd.z * (unit - typ->aim);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-aim.x, -aim.z));
        ang[0] = RAD_TO_ANG(atan2f(aim.y, sqrtf(aim.x * aim.x + aim.z * aim.z)));
    } while (0);
    ang[2] = enemy_task_get_time(obj) * typ->roll_spd / fps;
    enemy_task_set_rot(obj, ang);
    dc_object_advance_time(enemy_task_gfx_ptr(obj));
    if (++w->count > n) {
        pw = enemy_task_get_time(obj) / (w->ramp * fps);
        pw = 0.0f <= pw ? (pw < unit ? (0.0f <= pw ? pw : 0.0f) : unit) : 0.0f;
        w->count = 1;
        enemy_task_set_param_vec(obj, &w->dir);
        w->speed = (typ->speed - w->speed0) * pw + w->speed0;
        w->act = (typ->act - w->act0) * pw + w->act0;
        macros_missile_steer(obj, w);
    }
    i = player_sphere_test(&next, typ->crash_r);
    if (i == 1 && !w->hit) {
        player_status_add_damage(typ->crash_damage);
        w->hit = i;
        if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 2);
    }
}

/* steer(cast) */
/* 0x00b24168 */
void macros_missile_update(void *obj, MacrosMissileWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    void (*fn)(void *obj, MacrosMissileWork *w) = macros_missile_states[w->state];
    Vec3 p;
    Vec3 pos;

    if (fn)
        fn(obj, w);
    enemy_task_get_pos(obj, &p);
    if (!yuki_camera_relative(&p, w->type->clip, gfx->radius) && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 2);
    enemy_task_get_pos(obj, &pos);
    if (pos.y < stage_ground_height(&pos) + gfx->radius && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b24268 */
void macros_missile_exec_appear(void *obj, MacrosMissileWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 0);
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        macros_missile_update(obj, w);
    }
}

/* 0x00b24310 */
void macros_missile_exec_normal(void *obj, MacrosMissileWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    macros_missile_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (w->type->life * FPS < frm)
        enemy_task_set_state(obj, 2);
}

/* 0x00b24390 */
void macros_missile_exec_leave(void *obj, MacrosMissileWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        heap_bzero(&desc, 84);
        desc.type = 213;
        desc.models = stage_models;
        desc.kind = 1;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        en_shatter_spawn(obj, &desc, 2);
        FINISH_IF(obj);
    }
}

/* 0x00b24430 */
void macros_missile_exec_destroyed(void *obj, MacrosMissileWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        heap_bzero(&desc, 84);
        desc.type = 213;
        desc.models = stage_models;
        desc.kind = 1;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        en_shatter_spawn(obj, &desc, 2);
        FINISH_IF(obj);
    }
}

/* 0x00b244d0 */
void macros_missile_pre_exec(void)
{
}

/* 0x00b244d8 */
void macros_missile_exec(void *obj, MacrosMissileWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (macros_missile_exec_table[i])
        macros_missile_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b24530 */
void macros_missile_post_exec(void *obj, MacrosMissileWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b245c8 */
void macros_missile_disp(void *obj, MacrosMissileWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (macros_missile_disp_table[i])
        macros_missile_disp_table[i](obj, w);
}

/* 0x00b24620 */
int macros_missile_create(void *obj, Vec3 *pos, Vec3 *vel, float act, float ramp)
{
    MacrosMissileKind k;
    ObjDesc desc;
    ObjFuncs funcs = macros_missile_funcs;
    ShotArg prm;
    MacrosMissileRec *typ;

    k.act = act;
    k.ramp = ramp;
    k.idx = 0;
    k.speed = vec3_length(vel);
    typ = (MacrosMissileRec *)&macros_missile_types[k.idx];
    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    prm.kind = macros_missile_init_arg;
    prm.fx_arg = -1;
    prm.pos = *pos;
    do {
        prm.ang[1] = RAD_TO_ANG(atan2f(-vel->x, -vel->z));
        prm.ang[0] = RAD_TO_ANG(atan2f(vel->y, sqrtf(vel->x * vel->x + vel->z * vel->z)));
    } while (0);
    prm.ang[2] = 0;
    prm.vel.x = vel->x;
    prm.vel.y = vel->y;
    prm.vel.z = vel->z;
    desc.name = macros_missile_name;
    desc.funcs = &funcs;
    desc.arg = &prm;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 17;
    desc.dc_flags = 4096;
    desc.model_no = 46;
    desc.model_no2 = -1;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = typ->life * FPS;
    desc.size = 72;
    return enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}

/* 0x00b248a8 */
void seraphim_bullet_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&seraphim_bullet_vardef);
    for (i = 0; i < 2; i++) {
        def = seraphim_bullet_vardefs;
        val = seraphim_bullet_types[i].v;
        seraphim_bullet_vardef.name = seraphim_bullet_types[i].name;
        seraphim_bullet_vardef.fmt = seraphim_bullet_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&seraphim_bullet_vardef);
        for (j = 0; j < 19; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)seraphim_bullet_vardefs);
    }
}

/* 0x00b249c0 */
void seraphim_bullet_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b249c8 */
int seraphim_bullet_get_init_arg(void)
{
    return seraphim_bullet_init_arg;
}

/* 0x00b249d8 */
int seraphim_bullet_vars_init(int *prm)
{
    seraphim_bullet_init_arg = *prm;
    seraphim_bullet_vars_register();
    return 0;
}

/* 0x00b24a00 */
int seraphim_bullet_vars_release(void)
{
    seraphim_bullet_vars_unregister();
    return 0;
}

/* 0x00b24a20 */
void seraphim_bullet_init(void *obj, SeraphimBulletWork *w, int prm, SeraphimBulletKind *k)
{
    BackFireLink lnk;

    w->type = (SeraphimBulletRec *)&seraphim_bullet_types[k->idx];
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    w->arg = *k->arg;
    heap_bzero(&lnk, 52);
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.use_pose = 1;
    w->fire = 0.0f;
    lnk.scale = &w->fire;
    lnk.flags = 0x1000000;
    en_backfire_create(obj, &lnk, 0);
    w->count = 1;
    w->started = 0;
    w->state = 0;
    w->hit = 0;
    w->roll = 0;
    w->owner = k->owner;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), seraphim_bullet_lock_name);
}

/* 0x00b24b78 */
void seraphim_bullet_release(void)
{
}

/* 0x00b24b80 */
int seraphim_bullet_next_state(void *obj, SeraphimBulletWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        return 1;
    }
    return 0;
}

/* 0x00b24bb0 */
void seraphim_bullet_state_accel(void *obj, SeraphimBulletWork *w)
{
    SeraphimBulletRec *typ = w->type;
    Vec3 next;
    Vec3 p;
    Vec3 pl;
    Vec3 v;
    Vec3 to_pl;
    int ang[3];
    float sc[2][2];
    float rate, dist, speed, spd_max;
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
    vec3_approach(&to_pl, &v, (sc[0][0] + 1.0f) * 0.5f);
    vec3_normalize(&v, &v);
    dist = ulVec3Distance(&p, &pl);
    if (typ->dist_far < dist)
        speed = typ->speed_far;
    else if (dist < typ->dist_near)
        speed = typ->speed_near;
    else
        speed = (typ->speed_near - typ->speed_far) * ((typ->dist_far - dist) / (typ->dist_far - typ->dist_near)) + typ->speed_far;
    spd_max = typ->speed0 + w->count / FPS * typ->accel;
    speed = speed < spd_max ? speed : spd_max;
    v3_scale(&v, speed * FRAME_SEC);
    v3_sum(&next, &p, &v);
    enemy_task_set_pos(obj, &next);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&next, &p, ang);
    enemy_task_set_rot(obj, ang);
    w->roll = w->roll + typ->roll_spd / FPS * rate;
    ulSinCos(safe_powf(rate, typ->fire_pow2) * ANG_HALF, &sc[1][0], &sc[1][1]);
    w->fire = (sc[1][0] + safe_powf(rate, typ->fire_pow)) * typ->fire_size;
    seraphim_bullet_next_state(obj, w, n, 1);
}

/* 0x00b24f08 */
void seraphim_bullet_state_cruise(void *obj, SeraphimBulletWork *w)
{
    SeraphimBulletRec *typ = w->type;
    Vec3 next;
    Vec3 p;
    Vec3 pl;
    Vec3 v;
    Vec3 to_pl;
    int ang[3];
    float sn, cs;
    float dist, speed;

    enemy_task_get_pos(obj, &p);
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, &p);
    vec3_normalize(&to_pl, &to_pl);
    enemy_task_get_param_vec(obj, &v);
    sn = (w->count + typ->accel_sec * FPS) / ((typ->accel_sec + typ->accel_ang_sec) * FPS);
    ulSinCos(sn < 1.0f ? sn * ANG_HALF - ANG_QUARTER : ANG_90, &sn, &cs);
    vec3_approach(&to_pl, &v, (sn + 1.0f) * 0.5f);
    vec3_normalize(&v, &v);
    dist = ulVec3Distance(&p, &pl);
    if (typ->dist_far < dist)
        speed = typ->speed_far;
    else if (dist < typ->dist_near)
        speed = typ->speed_near;
    else
        speed = (typ->speed_near - typ->speed_far) * ((typ->dist_far - dist) / (typ->dist_far - typ->dist_near)) + typ->speed_far;
    v3_scale(&v, speed * FRAME_SEC);
    v3_sum(&next, &p, &v);
    enemy_task_set_pos(obj, &next);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&next, &p, ang);
    enemy_task_set_rot(obj, ang);
    w->roll += typ->roll_spd / FPS;
    w->count++;
}

/* steer(cast) */
/* 0x00b251b0 */
void seraphim_bullet_update(void *obj, SeraphimBulletWork *w)
{
    void (*fn)(void *obj, SeraphimBulletWork *w);
    SeraphimBulletRec *typ;
    Vec3 p;
    int touch;

    if (!w->started) {
        w->started = 1;
    } else {
        fn = seraphim_bullet_states[w->state];
        if (fn)
            fn(obj, w);
        if (!w->hit) {
            typ = w->type;
            enemy_task_get_pos(obj, &p);
            touch = player_sphere_test(&p, typ->crash_r);
            if (touch == 1) {
                player_status_add_damage(typ->crash_damage);
                if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
                    enemy_task_set_state(obj, 3);
                w->hit = touch;
            }
            if (!yuki_camera_relative(&p, typ->area, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius)) {
                w->hit = 1;
                if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
                    enemy_task_set_state(obj, 3);
            }
        }
    }
}

/* 0x00b252d0 */
void seraphim_bullet_exec_appear(void *obj, SeraphimBulletWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_set_state(obj, 0);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        seraphim_bullet_update(obj, w);
    }
}

/* 0x00b25368 */
void seraphim_bullet_exec_normal(void *obj, void *w)
{
    int n = enemy_task_get_phase(obj);

    if (n && n != 2)
        seraphim_bullet_update(obj, w);
}

/* 0x00b253c8 */
void seraphim_bullet_exec_leave(void *obj)
{
    if (!enemy_task_get_phase(obj))
        FINISH_IF(obj);
}

/* 0x00b25410 */
void seraphim_bullet_exec_destroyed(void *obj)
{
    EfMdlBrkArg prm;

    if (enemy_task_get_phase(obj) == 0) {
        heap_bzero(&prm, 84);
        enemy_task_get_pos(obj, &prm.pos);
        enemy_task_get_rot(obj, prm.ang);
        prm.scatter_dir.x = 0.0f;
        prm.scatter_dir.y = 0.0f;
        prm.scatter_dir.z = 0.0f;
        prm.gfx = enemy_task_gfx_ptr(obj);
        prm.one = 1;
        ef_mdl_brk_start(obj, &prm);
        FINISH_IF(obj);
    }
}

/* 0x00b254a0 */
void seraphim_bullet_pre_exec(void *obj)
{
    Vec3 p, org;

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

/* 0x00b25550 */
void seraphim_bullet_exec(void *obj, SeraphimBulletWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (seraphim_bullet_exec_table[i])
        seraphim_bullet_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b255a8 */
void seraphim_bullet_post_exec(void *obj)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if ((lock->flags & LOCK_HIT) && lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* 0x00b25610 */
void seraphim_bullet_disp(void *obj, SeraphimBulletWork *w)
{
    void (*fn)(void *obj, SeraphimBulletWork *w);
    int ang[3];
    void *fog;
    int i;

    i = enemy_task_get_pstate(obj);
    fn = seraphim_bullet_disp_table[i];
    if (fn) {
        fog = fog_set_enable(0);
        enemy_task_get_rot(obj, ang);
        ang[2] += w->roll;
        enemy_task_set_rot(obj, ang);
        fn(obj, w);
        ang[2] -= w->roll;
        enemy_task_set_rot(obj, ang);
        fog_set_enable(fog);
    }
}

/* 0x00b256d0 */
int seraphim_bullet_create(void *obj, SeraphimBulletArg *prm)
{
    SeraphimBulletKind k;
    ObjDesc desc;
    ObjFuncs funcs = seraphim_bullet_funcs;
    ObjFuncs *pfuncs = &funcs;
    ShotArg place;
    SeraphimBulletRec *typ;
    int none = -1;

    k.idx = MAX_GE(prm->type, 0) != 0;
    k.arg = prm;
    k.owner = obj;
    typ = (SeraphimBulletRec *)&seraphim_bullet_types[k.idx];
    nj_fill8(&place, 0, sizeof place);
    place.kind = seraphim_bullet_init_arg;
    place.fx_arg = none;
    place.pos = *prm->pos;
    *(Vec3 *)place.ang = *(Vec3 *)prm->ang;
    v3_copy(&place.vel, &prm->vel);
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = seraphim_bullet_name;
    desc.funcs = pfuncs;
    desc.arg = &place;
    desc.kind = &k.idx;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 185;
    desc.model_no2 = none;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_recover;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = 0;
    desc.size = 64;
    return enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}
