/*
 * Archangel children, Area 5: launched in waves; fly toward points ahead on the path with noise,
 * fire a shot or attack ray while the player is in view, fall and explode. Leaders count toward
 * the Archangel's bonus.
 */

#include "stg05.h"
#include "mathdef.h"
#include "camera.h"
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
#include "player.h"
#include "pnb.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "arch_angel_child.h"
#include "en_aangel_exp.h"
#include "enb_sidewinder.h"
#include "stage05.h"
#include "../common/air_mng.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/enemy_vars.h"

extern int arch_angel_child_init_arg;
extern void (*arch_angel_child_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*arch_angel_child_disp_table[])(void *o, TypeEnemyWork *w);
extern int arch_angel_child_leader_init_arg;

/* group name, label, 32 values */
typedef struct ArchAngelChildSet {
    char *name;
    char *label;                /* 0x04 */
    int v[32];                  /* 0x08 */
} ArchAngelChildSet;

extern ArchAngelChildSet arch_angel_child_types[1];
extern VarDef arch_angel_child_vardef;
extern VarDef arch_angel_child_vardefs[32];

typedef struct ArchAngelChildWork {
    struct ArchAngelChildRec *type;
    Vec3 hitv;                  /* 0x04: the lock-on record's vector when hit */
    int state;                  /* 0x10: indexes arch_angel_child_states */
    int count;                  /* 0x14: frames of the current action */
    char pad_18[8];
    Vec3 dir;                   /* 0x20: the course toward the path point */
    float act;                  /* 0x2c: seconds of one action */
    float act0;                 /* 0x30 */
    float ramp;                 /* 0x34: seconds to reach the record's action time */
    int beat;                   /* 0x38: beats left of the death fall */
    Vec3 move;                  /* 0x3c: the last move */
    float scale;                /* 0x48: of the linked effect (en_backfire_create) */
    int wait;                   /* 0x4c: shot frames; negative while the shot effect runs */
    Vec3 mount;                 /* 0x50: a mount point of the model (arch_angel_child_node_cb) */
    void *shot;                 /* 0x5c: the shot effect task (attack_ray_create) */
    int life;                   /* 0x60: frames left */
    void *tail;                 /* 0x64: the trail task (air_mng_create) */
    LockObj lock;                  /* 0x68 */
    int leader;                 /* 0xa8 */
    void *parent;               /* 0xac: the creating object, cleared when it ends */
    int *kills;                 /* 0xb0: counted when shot down */
    int *lands;                 /* 0xb4: counted when it hits the ground */
    int landed;                 /* 0xb8 */
    int snd;                    /* 0xbc: plays the rhythm sound */
} ArchAngelChildWork;

extern void (*arch_angel_child_states[])(void *o, ArchAngelChildWork *w);

/* child launch record (arch_angel_child_spawn) */
typedef struct ShotRec {
    char pad_00[4];
    int pos[6];                   /* 0x04 */
    int vel[3];                   /* 0x1c */
    float n;                    /* 0x28 */
    float act, ramp;             /* 0x2c: frames */
} ShotRec;

/* 136 bytes; names in arch_angel_child_vardefs */
typedef struct ArchAngelChildRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    int score_leader;           /* 0x14 */
    float hp_reduce;            /* 0x18 */
    float crash_r;              /* 0x1c */
    float crash_damage;         /* 0x20 */
    float life_time_sec;        /* 0x24 */
    float tail_sec;             /* 0x28 */
    float tail_width;           /* 0x2c */
    int tail_skip;              /* 0x30 */
    float spd_start;            /* 0x34 */
    float spd_chg_start;        /* 0x38: distances of the speed change */
    float spd_chg_end;          /* 0x3c */
    float spd_chg_rate;         /* 0x40: speed factor when near */
    float break_pow;            /* 0x44: ease exponent - 1 of the forward move */
    float accel_pow;            /* 0x48: of the move along the aim */
    float action_sec;           /* 0x4c */
    float expect_sec;           /* 0x50: how far ahead on the path it aims */
    float noise_vec_size;       /* 0x54 */
    float ang_rate;             /* 0x58: weight of the path direction in the facing */
    float future_sec;           /* 0x5c */
    float die_spd_density;      /* 0x60: per-frame factor of the death move */
    int die_roll;               /* 0x64 */
    int die_beat;               /* 0x68 */
    float die_fall_acc;         /* 0x6c */
    float die_down_rate;        /* 0x70 */
    float shot_wait;            /* 0x74: seconds */
    float shot_rate;            /* 0x78 */
    int shot_ang;               /* 0x7c: for yuki_camera_relative */
    float shot_eff_sec;         /* 0x80 */
    float shot_dis_range;       /* 0x84 */
} ArchAngelChildRec;

/* ObjDesc.kind points to it */
typedef struct ArchAngelChildKind {
    int idx;
    int life;                   /* 0x04: frames */
    float act;                  /* 0x08: seconds of one action */
    float ramp;                 /* 0x0c */
    int leader;                 /* 0x10 */
    void *parent;               /* 0x14 */
    int *kills;                 /* 0x18 */
    int *lands;                 /* 0x1c */
} ArchAngelChildKind;

extern char arch_angel_child_lock_name[];
extern char arch_angel_child_leader_lock_name[];
extern char arch_angel_child_name[];
extern Vec3 arch_angel_child_die_dir;
extern char arch_angel_child_funcs[];

/* 0x00afc828 */
void arch_angel_child_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&arch_angel_child_vardef);
    for (i = 0; i < 1; i++) {
        def = arch_angel_child_vardefs;
        val = arch_angel_child_types[i].v;
        arch_angel_child_vardef.name = arch_angel_child_types[i].name;
        arch_angel_child_vardef.fmt = arch_angel_child_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&arch_angel_child_vardef);
        for (j = 0; j < 32; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)arch_angel_child_vardefs);
    }
}

/* 0x00afc940 */
void arch_angel_child_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afc948 */
int arch_angel_child_get_init_arg(void)
{
    return arch_angel_child_init_arg;
}

/* 0x00afc958 */
int arch_angel_child_leader_get_init_arg(void)
{
    return arch_angel_child_leader_init_arg;
}

/* 0x00afc968 */
int arch_angel_child_vars_init(int *prm)
{
    arch_angel_child_init_arg = *prm;
    arch_angel_child_vars_register();
    return 0;
}

/* 0x00afc990 */
int arch_angel_child_vars_release(void)
{
    arch_angel_child_vars_unregister();
    return 0;
}

/* 0x00afc9b0 */
int arch_angel_child_leader_vars_init(int *prm)
{
    arch_angel_child_leader_init_arg = *prm;
    return 0;
}

/* 0x00afc9c8 */
int arch_angel_child_leader_vars_release(void)
{
    return 0;
}

/* 0x00afc9d0 */
int arch_angel_child_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 67);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00afca28 */
void arch_angel_child_steer(void *obj, ArchAngelChildWork *w, int noise)
{
    ArchAngelChildRec *typ = w->type;
    Vec3 target;
    Vec3 p;
    Vec3 n;
    int ang[3];
    float sc[3][2];
    float k, z;

    z = 0.0f;
    if (fabsf(typ->future_sec) < FLT_EPSILON) {
        k = z;
    } else {
        k = 1.0f - enemy_task_get_time(obj) / (typ->future_sec * FPS);
        k = MIN(MAX_GE(k, 0.0f), 1.0f);
    }
    player_pass_pos_a(&target, typ->expect_sec * FPS * k);
    enemy_task_get_pos(obj, &p);
    v3_sub(&w->dir, &target, &p);
    if (noise) {
        ang[1] = RAD_TO_ANG(atan2f(-w->dir.x, -w->dir.z));
        ang[0] = RAD_TO_ANG(atan2f(w->dir.y, sqrtf(w->dir.x * w->dir.x + w->dir.z * w->dir.z)));
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0][0], &sc[0][1]);
        ROT_X(ang[0], sc[1], sc[0][1], z);
        ROT_Y(ang[1], sc[2], sc[0][0], z);
        v3_set(&n, sc[0][0], sc[0][1], z);
        v3_scale(&n, ulVec3Distance(&target, &p) * typ->noise_vec_size);
        v3_add(&w->dir, &n);
    }
    vec3_normalize(&w->dir, &w->dir);
}

/* 0x00afcd68 */
int arch_angel_child_node_cb(unsigned int *n, int idx, ArchAngelChildWork *w)
{
    if (*n & 0x4000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->mount.x);
    return 0;
}

/* 0x00afcda0 */
int arch_angel_child_billboard_cb(unsigned int *n, int idx, ArchAngelChildWork *w)
{
    if (*n & 0x1000000)
        ulMatrixUnitRotation((float *)ulMatrixCur);
    return 1;
}

/* 0x00afcdd8 */
void arch_angel_child_pose_model(void *obj, TypeEnemyWork *w)
{
    Vec3 p;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    dc_object_set_callback(gfx, arch_angel_child_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
    arch_angel_child_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00afcea8 */
void arch_angel_child_init(void *obj, ArchAngelChildWork *w, int prm, ArchAngelChildKind *k)
{
    ArchAngelChildRec *typ;
    TrailArg trail;
    BackFireLink lnk;
    Vec3 v;

    typ = (ArchAngelChildRec *)&arch_angel_child_types[k->idx];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    dc_object_set_callback(enemy_task_gfx_ptr(obj), arch_angel_child_billboard_cb, w);
    lockon_obj_set_name(enemy_task_lock_ptr(obj), k->leader ? arch_angel_child_leader_lock_name : arch_angel_child_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    lockon_obj_init(&w->lock, obj, enemy_task_pos_ptr(obj), 0, k->leader ? arch_angel_child_leader_lock_name : arch_angel_child_lock_name,
                  enemy_task_get_status_slot(obj), typ->hp, typ->same_lock, 128);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    heap_bzero(&trail, 40);
    trail.body = enemy_task_gfx_ptr(obj);
    trail.pos = enemy_task_pos_ptr(obj);
    trail.ang = enemy_task_rot_ptr(obj);
    trail.tail_seg = 1;
    trail.frames = typ->tail_sec * FPS;
    trail.tail_tex = 1;
    trail.tail_mask = 0x2000000;
    trail.tail_width = typ->tail_width;
    trail.one = 1;
    trail.tail_skip = typ->tail_skip;
    w->tail = air_mng_create(obj, &trail);
    heap_bzero(&lnk, 52);
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.flags = 0x8000000;
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->scale;
    w->scale = 1.0f;
    lnk.use_pose = 1;
    en_backfire_create(obj, &lnk, 0);
    w->count = 1;
    w->state = 0;
    w->act0 = w->act = k->act;
    w->life = k->life;
    w->ramp = k->ramp;
    w->beat = typ->die_beat;
    v3_set(&w->move, 0, 0, 0);
    w->wait = 0;
    w->shot = 0;
    arch_angel_child_steer(obj, w, 0);
    enemy_task_get_param_vec(obj, &v);
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
    v3_add(&w->dir, &v);
    vec3_normalize(&w->dir, &w->dir);
    arch_angel_child_pose_model(obj, (TypeEnemyWork *)w);
    w->kills = k->kills;
    w->snd = 1;
    w->leader = k->leader;
    w->parent = k->parent;
    w->lands = k->lands;
    w->landed = 0;
}

/* 0x00afd1f0 */
void arch_angel_child_release(void *obj, ArchAngelChildWork *w)
{
    lockon_remove(&w->lock);
}

/* steer(cast) */
/* 0x00afd1f8 */
void arch_angel_child_state_fly(void *obj, ArchAngelChildWork *w)
{
    ArchAngelChildRec *typ;
    Vec3 p;
    Vec3 next;
    Vec3 v;
    Vec3 mv;
    int ang[3];
    Vec3 pl;
    Vec3 target;
    Vec3 aim;
    Vec3 to_target;
    Vec3 fwd;
    AttackRayArg ray;
    ModelPart *gfx;
    float rate, prev, spd_chg, speed, dist, k, unit;
    int n;

    n = w->act * FPS;
    rate = w->count / (float)n;
    prev = (w->count - 1) / (float)n;
    typ = w->type;
    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    spd_chg = typ->spd_start * typ->spd_chg_rate;
    player_get_pos(&pl);
    dist = ulVec3Distance(&pl, &p);
    if (typ->spd_chg_start < dist)
        speed = typ->spd_start;
    else if (dist < typ->spd_chg_end)
        speed = spd_chg;
    else
        speed = (spd_chg - typ->spd_start) * ((typ->spd_chg_start - dist) / (typ->spd_chg_start - typ->spd_chg_end)) + typ->spd_start;
    unit = 1.0f;
    {
        float pw = typ->break_pow;
        float step;

        pw += unit;
        step = (unit - safe_powf(unit - rate, pw)) * speed / pw * w->act;

        step -= (unit - safe_powf(unit - prev, pw)) * speed / pw * w->act;
        enemy_task_get_param_vec(obj, &v);
        v3_scale(&v, step);
    }
    v3_sum(&next, &p, &v);
    {
        float pw = typ->accel_pow;
        float step;

        pw += unit;
        step = safe_powf(rate, pw) * speed / pw * w->act;

        step -= safe_powf(prev, pw) * speed / pw * w->act;
        v3_mul(&mv, &w->dir, step);
    }
    v3_add(&next, &mv);
    if (next.y < stage_ground_height(&next)) {
        next.y = stage_ground_height(&next);
        w->landed = 1;
        if (((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 3);
        if (w->leader && w->parent)
            (*w->lands)++;
    }
    enemy_task_set_pos(obj, &next);
    v3_sub(&w->move, &next, &p);
    player_pass_pos_a(&target, typ->expect_sec * FPS * rate);
    v3_sub(&to_target, &target, &next);
    vec3_normalize(&to_target, &to_target);
    v3_sub(&fwd, &next, &p);
    vec3_normalize(&fwd, &fwd);
    aim.x = to_target.x * typ->ang_rate + fwd.x * (1.0f - typ->ang_rate);
    aim.y = to_target.y * typ->ang_rate + fwd.y * (1.0f - typ->ang_rate);
    aim.z = to_target.z * typ->ang_rate + fwd.z * (1.0f - typ->ang_rate);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-aim.x, -aim.z));
        ang[0] = RAD_TO_ANG(atan2f(aim.y, sqrtf(aim.x * aim.x + aim.z * aim.z)));
    } while (0);
    ang[2] = 0;
    enemy_task_set_rot(obj, ang);
    dc_object_advance_time(gfx);
    if (w->shot && (TASK_OF(w->shot)->flags & TASK_F_END))
        w->shot = 0;
    if (w->wait < 0) {
        if (++w->wait >= 0) {
            if (yuki_camera_relative(&w->mount, w->type->shot_ang, gfx->radius))
                e_shot_fire(obj, &w->mount, 0, 4);
            else
                e_shot_fire(obj, &w->mount, 0, 5);
        }
    } else if (typ->shot_wait * FPS < ++w->wait) {
        player_get_pos(&aim);
        if (typ->shot_rate < RAND_FLOAT_A()) {
            w->wait = 0;
        } else if (vec3_distance_squared(&aim, &next) < typ->shot_dis_range * typ->shot_dis_range) {
            w->wait = 0;
        } else {
            ray.pos = &w->mount;
            ray.ang = enemy_task_rot_ptr(obj);
            ray.scale = enemy_task_scl_ptr(obj);
            ray.dur = typ->shot_eff_sec * FPS;
            w->shot = ((void *(*)(void *, AttackRayArg *))attack_ray_create)(obj, &ray);
            w->wait = -(typ->shot_eff_sec * FPS);
        }
    }
    if (++w->count > n) {
        w->count = 1;
        enemy_task_set_param_vec(obj, &w->dir);
        k = enemy_task_get_time(obj) / (w->ramp * FPS);
        k = MIN(MAX_GE(k, 0.0f), 1.0f);
        w->act = (typ->action_sec - w->act0) * k + w->act0;
        arch_angel_child_steer(obj, w, 1);
    }
}

/* 0x00afd938 */
void arch_angel_child_update(void *obj, ArchAngelChildWork *w)
{
    void (*fn)(void *obj, ArchAngelChildWork *w) = arch_angel_child_states[w->state];

    if (fn)
        fn(obj, w);
    arch_angel_child_pose_model(obj, (TypeEnemyWork *)w);
}

/* 0x00afd990 */
void arch_angel_child_die(void *obj, ArchAngelChildWork *w)
{
    ArchAngelChildRec *typ = w->type;
    Vec3 p;
    int cur[3];
    int ang[3];
    Vec3 dir = arch_angel_child_die_dir;
    Vec3 vpos;
    float rate, prev_sec;

    enemy_task_get_pos(obj, &p);
    v3_scale(&w->move, typ->die_spd_density);
    rate = w->count / FPS;
    prev_sec = (w->count - 1) / FPS;
    v3_add(&p, &w->move);
    w->scale *= typ->die_spd_density;
    p.y -= typ->die_fall_acc * 0.5f * (rate * rate - prev_sec * prev_sec);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = 0;
    enemy_task_get_rot(obj, cur);
    cur[2] += typ->die_roll / FPS;
    yuki_lerp3(cur, cur, ang, typ->die_down_rate / FPS * (w->count / FPS));
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, cur);
    arch_angel_child_pose_model(obj, (TypeEnemyWork *)w);
    w->count++;
    if (beat_grid_hit(0))
        w->beat--;
    if (w->beat <= 0) {
        enemy_task_next_phase(obj);
        if (w->lock.life > 0 && w->snd) {
            enemy_task_get_pos(obj, &vpos);
            ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
            project_point(&vpos, &vpos);
            SoundRhythmSe_EnemyPos(w->leader ? arch_angel_child_leader_init_arg : arch_angel_child_init_arg, 1, &vpos.x);
        }
    }
}

/* 0x00afdcb8 */
void arch_angel_child_exec_appear(void *obj, ArchAngelChildWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_set_state(obj, 0);
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        arch_angel_child_update(obj, w);
    }
}

/* 0x00afdd48 */
void arch_angel_child_exec_normal(void *obj, ArchAngelChildWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    arch_angel_child_update(obj, w);
    if (--w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00afddb0 */
void arch_angel_child_exec_leave(void *obj, ArchAngelChildWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        enemy_task_vanish_scale(obj, w);
        break;
    case 2:
        enemy_task_vanish_scale(obj, w);
        break;
    default:
        arch_angel_child_update(obj, w);
        enemy_task_vanish_scale(obj, w);
    }
}

/* 0x00afde58 */
void arch_angel_child_explode(void *obj, ArchAngelChildWork *w)
{
    Vec3 p;
    Vec3 eye;
    Vec3 dir;

    enemy_task_get_pos(obj, &p);
    camera_get_eye(&eye);
    v3_sub(&dir, &eye, &p);
    vec3_normalize(&dir, &dir);
    v3_scale(&dir, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius);
    v3_add(&p, &dir);
    en_aangel_exp_create(obj, &p, 0, 0, !w->leader);
}

/* 0x00afdf40 */
void arch_angel_child_exec_destroyed(void *obj, ArchAngelChildWork *w)
{
    Vec3 p;
    Vec3 vpos;
    LockObj *lock = &w->lock;

    switch (enemy_task_get_phase(obj)) {
    case 0:
        if (w->snd) {
            enemy_task_get_pos(obj, &p);
            ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
            project_point(&p, &p);
            SoundRhythmSe_EnemyPos(w->leader ? arch_angel_child_leader_init_arg : arch_angel_child_init_arg, 1, &p.x);
        }
        w->count = 1;
        air_mng_stop(w->tail);
        lock->flags |= LOCK_LOCKABLE;
        if (w->shot) {
            FINISH_IF(w->shot);
            w->shot = 0;
        }
        if (w->landed == 1)
            enemy_task_next_phase(obj);
        if (((LockObj *)enemy_task_lock_ptr(obj))->life <= 0 && w->leader && w->parent)
            (*w->kills)++;
        break;
    case 2:
        arch_angel_child_explode(obj, w);
        w->snd = 0;
        FINISH_IF(obj);
        break;
    default:
        arch_angel_child_die(obj, w);
        if (lock->life <= 0) {
            enemy_task_next_phase(obj);
            if (w->snd) {
                enemy_task_get_pos(obj, &vpos);
                ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
                project_point(&vpos, &vpos);
                SoundRhythmSe_EnemyPos(w->leader ? arch_angel_child_leader_init_arg : arch_angel_child_init_arg, 1, &vpos.x);
            }
        }
    }
}

/* 0x00afe120 */
void arch_angel_child_pre_exec(void)
{
}

/* 0x00afe128 */
void arch_angel_child_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (arch_angel_child_exec_table[i])
        arch_angel_child_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00afe180 */
void arch_angel_child_post_exec(void *obj, ArchAngelChildWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (!enemy_status_get_ticks())
        arch_angel_child_pose_model(obj, (TypeEnemyWork *)w);
    if (w->parent && (TASK_OF(w->parent)->flags & TASK_F_END))
        w->parent = 0;
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 3);
    }
}

/* 0x00afe258 */
void arch_angel_child_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (arch_angel_child_disp_table[i])
        arch_angel_child_disp_table[i](obj, w);
}

/* 0x00afe2b0 */
int arch_angel_child_create(void *obj, int *pos, int *vel, int n, int leader, int kills, int lands, float act, float ramp)
{
    ArchAngelChildKind k;
    ObjDesc od;
    ObjFuncs fn = *(ObjFuncs *)arch_angel_child_funcs;
    ShotArg sarg;
    ArchAngelChildRec *typ = (ArchAngelChildRec *)arch_angel_child_types;
    Vec3 *v = (Vec3 *)vel;

    k.idx = 0;
    k.act = act;
    k.ramp = ramp;
    k.life = n;
    k.leader = leader;
    k.parent = obj;
    k.kills = (int *)kills;
    k.lands = (int *)lands;
    nj_fill8(&od, 0, sizeof od);
    nj_fill8(&sarg, 0, sizeof sarg);
    sarg.kind = leader ? arch_angel_child_leader_init_arg : arch_angel_child_init_arg;
    sarg.fx_arg = -1;
    sarg.pos = *(Vec3 *)pos;
    do {
        sarg.ang[1] = RAD_TO_ANG(atan2f(-v->x, -v->z));
        sarg.ang[0] = RAD_TO_ANG(atan2f(v->y, sqrtf(v->x * v->x + v->z * v->z)));
    } while (0);
    sarg.ang[2] = 0;
    sarg.vel.x = v->x;
    sarg.vel.y = v->y;
    sarg.vel.z = v->z;
    od.name = arch_angel_child_name;
    od.funcs = &fn;
    od.arg = &sarg;
    od.kind = &k.idx;
    od.models = stage_models;
    od.tex = stage_textures;
    od.hp = typ->hp;
    od.same_lock = typ->same_lock;
    od.flags = 16;
    od.dc_flags = 4096;
    od.model_no = leader ? 71 : 67;
    od.model_no2 = -1;
    od.score = leader ? typ->score_leader : typ->score;
    od.hp_reduce = typ->hp_reduce;
    od.crash_r = typ->crash_r;
    od.crash_damage = typ->crash_damage;
    od.time = k.life;
    od.size = 192;
    return enemy_task_task_create(obj, &od);
}

/* steer(cast) */
/* 0x00afe558 */
void arch_angel_child_destroy(void *obj)
{
    if (((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        enemy_task_set_state(obj, 3);
        ((int *)enemy_task_work_ptr(obj))[47] = 0;
    }
}

/* 0x00afe5a0 */
int arch_angel_child_spawn(void *obj, ShotRec *rec)
{
    return arch_angel_child_create(obj, rec->pos, rec->vel, rec->n, 0, 0, 0, rec->act / FPS, rec->ramp / FPS);
}

/* 0x00afe600 */
void arch_angel_child_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
