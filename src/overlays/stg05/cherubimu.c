/*
 * Cherubim, Area 5 enemy: flies in, brakes with a swing, turns and accelerates away, or tackles
 * the player. Hits play damage motions and break off wing pieces.
 */

#include "stg05.h"
#include "mathdef.h"
#include "approach.h"
#include "debug.h"
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
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "../common/en_backfire.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int cherubimu_init_arg;
extern void (*cherubimu_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*cherubimu_disp_table[])(void *o, TypeEnemyWork *w);
void cherubimu_vars_register(void);
void cherubimu_vars_unregister(void);
int cherubimu_vars_init(int *prm);
int cherubimu_vars_release(void);
int cherubimu_next_state(void *obj, TypeEnemyWork *w, int n, int state);
void cherubimu_exec(void *obj, TypeEnemyWork *w);
void cherubimu_disp(void *obj, TypeEnemyWork *w);
void cherubimu_rotate_yxz(int *ang);
void cherubimu_shift_broken_wings(void *obj, TypeEnemyWork *w);

/* group name, label, 73 values */
typedef struct CherubimuSet {
    char *name;
    char *label;                /* 0x04 */
    int v[73];                  /* 0x08 */
} CherubimuSet;

extern CherubimuSet cherubimu_types[1];
extern VarDef cherubimu_vardef;
extern VarDef cherubimu_vardefs[73];

/* 56 bytes: posed from the model, flies off when broken */
typedef struct CherubimuWing {
    char pad_00[4];
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    Vec3 vel;                   /* 0x1c */
    int angv[3];                /* 0x28 */
    int t;                      /* 0x34: 0 attached, frames since broken, -1 gone */
} CherubimuWing;

/* 28 bytes */
typedef struct CherubimuDamage {
    Vec3 dir;
    int ang[3];                 /* 0x0c */
    int t;                      /* 0x18: frames */
} CherubimuDamage;

typedef struct CherubimuWork {
    struct CherubimuRec *type;
    int state;                  /* 0x04 */
    int count;                  /* 0x08 */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    ModelPart parts[6];            /* 0x18: models 100, 96, 102, 98, 103, 99 */
    float speed;                /* 0x240: the creation speed */
    Vec3 side;                  /* 0x244: side axis (sign of parameter 3) turned by the angles */
    Vec3 up;                    /* 0x250: the up axis turned by the angles */
    float fire;                 /* 0x25c: back fire size (link en_backfire_create) */
    int roll;                   /* 0x260 */
    float side_k;               /* 0x264: 0 or the side sign */
    float side_s;               /* 0x268: 1 or the side sign */
    int ang[3];                 /* 0x26c */
    int prev_ang[3];
    CherubimuWing wingA[2];          /* 0x284: pieces of node flag 0x4000000 */
    CherubimuWing wingB[2];          /* 0x2f4: pieces of node flag 0x2000000 */
    char pad_364[28];
    int tackle_done;                   /* 0x380: hit the player or left the view (tackle) */
    char pad_384[12];
    int pitch;                  /* 0x390: the pitch the stop ended with */
    int rollz;                  /* 0x394: the roll the stop ended with */
    int roll2;                  /* 0x398 */
    int ndmg;                   /* 0x39c: damage motions; cleared by cherubimu_clear_damage */
    CherubimuDamage dmg[10];            /* 0x3a0 */
    Vec3 dmg_pos;                  /* 0x4b8 */
    int dmg_ang[3];                /* 0x4c4 */
    Vec3 dir;                   /* 0x4d0: the tackle direction */
} CherubimuWork;

void cherubimu_shatter_wings(void *obj, TypeEnemyWork *w);
void cherubimu_update(void *obj, TypeEnemyWork *w);

/* 300 bytes; names in cherubimu_vardefs */
typedef struct CherubimuRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_reduce;            /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    int roll_spd;               /* 0x20: degrees per second */
    int roll_spd_turn;          /* 0x24 */
    int roll_spd_tackle;        /* 0x28 */
    float backfire_scl;         /* 0x2c */
    float backfire_scl2;        /* 0x30 */
    float break_sec;            /* 0x34 */
    int break_ang;              /* 0x38 */
    float break_up_dis;         /* 0x3c */
    float break_pow;            /* 0x40 */
    float stop_sec;             /* 0x44 */
    int stop_pitch_ang;         /* 0x48 */
    float stop_pitch_pow;       /* 0x4c */
    float stop_pitch_pow2;      /* 0x50 */
    int stop_roll_ang;          /* 0x54 */
    float stop_roll_pow;        /* 0x58 */
    float stop_roll_pow2;       /* 0x5c */
    float stop_swing_dis;       /* 0x60 */
    float stop_down_dis;        /* 0x64 */
    float turn_acc;             /* 0x68 */
    float turn_rate;            /* 0x6c */
    float turn_down_rate;       /* 0x70 */
    int turn_pitch_ang;         /* 0x74 */
    int turn_roll_ang;          /* 0x78 */
    int turn_roll_time;         /* 0x7c */
    float accel_sec;            /* 0x80 */
    int accel_ang_spd;          /* 0x84 */
    float accel_ang_pow;        /* 0x88 */
    float accel_fall_dis;       /* 0x8c */
    int accel_pitch;            /* 0x90 */
    int accel_pitch_ang;        /* 0x94 */
    float accel_pitch_ang_pow;  /* 0x98 */
    float away_acc;             /* 0x9c */
    float away_sec;             /* 0xa0 */
    float tackle_spd_start;     /* 0xa4 */
    float tackle_spd_end;       /* 0xa8 */
    float tackle_dis_start;     /* 0xac */
    float tackle_dis_end;       /* 0xb0 */
    float tackle_acc;           /* 0xb4 */
    float tackle_curve_rate;    /* 0xb8 */
    int tackle_clip_ang;        /* 0xbc */
    float tackle_ang_sec;       /* 0xc0 */
    float wing_break_spd;       /* 0xc4 */
    float wing_break_roll_rate; /* 0xc8 */
    int wing_roll_spd_x;        /* 0xcc */
    int wing_roll_spd_y;        /* 0xd0 */
    float wing_break_sec;       /* 0xd4 */
    float wing_break_sec_rate;  /* 0xd8 */
    float wing_density;         /* 0xdc */
    float wing_break_spd_min;   /* 0xe0 */
    float wing_roll_spd_min;    /* 0xe4 */
    float wing_break_spd_limit; /* 0xe8 */
    float wing_roll_spd_limit;  /* 0xec */
    unsigned int wing_wire_col; /* 0xf0 */
    float damage_motion_back_sec; /* 0xf4 */
    float damage_motion_back_spd; /* 0xf8 */
    float damage_motion_ret_sec; /* 0xfc */
    int damage_motion_ang;      /* 0x100 */
    int damage_motion_roll;     /* 0x104 */
    int wire;                   /* 0x108 */
    int wire_linear;            /* 0x10c */
    unsigned int wire_col;      /* 0x110 */
    unsigned int wire_col2;     /* 0x114 */
    float wire_near;            /* 0x118 */
    float wire_far;             /* 0x11c */
    float wire_near_rate;       /* 0x120 */
    float wire_far_rate;        /* 0x124 */
    int wire_beat;              /* 0x128 */
} CherubimuRec;

typedef struct CherubimuNodeArg {
    int nb;                     /* wing pieces of flag 0x2000000 taken */
    int na;                     /* 0x04: of flag 0x4000000 */
    CherubimuWing *wing_b;                /* 0x08 */
    CherubimuWing *wing_a;                /* 0x0c */
} CherubimuNodeArg;

extern char cherubimu_lock_name[];
extern Vec3 cherubimu_init_scale;
int cherubimu_wing_node_cb(unsigned int *n, int idx, CherubimuNodeArg *prm);
int cherubimu_break_wing_piece(void *obj, CherubimuWork *w, CherubimuWing *pieces, int n);
void cherubimu_record_wings(void *obj, CherubimuWork *w);
void cherubimu_break_wings(void *obj, CherubimuWork *w);
void cherubimu_move_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type);
void cherubimu_shift_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type);
void cherubimu_spawn_wing_debris(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type);
void cherubimu_draw_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, ModelPart *body, ModelPart *wire);
void cherubimu_draw_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, ModelPart *body);
void cherubimu_clear_damage(void *obj, CherubimuWork *w);
extern char cherubimu_name[];
extern char cherubimu_funcs[];
extern void (*cherubimu_states[])(void *o, CherubimuWork *w);

/* 0x00afea08 */
void cherubimu_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&cherubimu_vardef);
    for (i = 0; i < 1; i++) {
        def = cherubimu_vardefs;
        val = cherubimu_types[i].v;
        cherubimu_vardef.name = cherubimu_types[i].name;
        cherubimu_vardef.fmt = cherubimu_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&cherubimu_vardef);
        for (j = 0; j < 73; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)cherubimu_vardefs);
    }
}

/* 0x00afeb20 */
void cherubimu_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afeb28 */
int cherubimu_get_init_arg(void)
{
    return cherubimu_init_arg;
}

/* 0x00afeb38 */
int cherubimu_vars_init(int *prm)
{
    cherubimu_init_arg = *prm;
    cherubimu_vars_register();
    return 0;
}

/* 0x00afeb60 */
int cherubimu_vars_release(void)
{
    cherubimu_vars_unregister();
    return 0;
}

/* 0x00afeb80 */
int cherubimu_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 95);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* steer(cast) */
/* 0x00afebd8 */
int cherubimu_wing_node_cb(unsigned int *n, int idx, CherubimuNodeArg *prm)
{
    if (*n & 0x4000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &prm->wing_a[prm->na].pos.x);
        ((void (*)(int *))model_camera_angles)(prm->wing_a[prm->na].ang);
        prm->na++;
    }
    if (*n & 0x2000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &prm->wing_b[prm->nb].pos.x);
        ((void (*)(int *))model_camera_angles)(prm->wing_b[prm->nb].ang);
        prm->nb++;
    }
    return 0;
}

/* 0x00afecc0 */
void cherubimu_record_wings(void *obj, CherubimuWork *w)
{
    CherubimuNodeArg prm;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);

    prm.wing_a = w->wingA;
    prm.wing_b = w->wingB;
    prm.nb = 0;
    prm.na = 0;
    dc_object_set_callback(gfx, cherubimu_wing_node_cb, &prm);
    ulMatrixPush();
    matrix_unit_set(0);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00afed50 */
void cherubimu_clear_damage(void *obj, CherubimuWork *w)
{
    w->ndmg = 0;
}

/* 0x00afed58 */
void cherubimu_add_damage_motion(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ;
    CherubimuDamage *mot;
    float sc[2];

    mot = w->dmg;
    if (w->ndmg < 10) {
        mot += w->ndmg++;
        typ = w->type;
        vec3_normalize(&mot->dir, &w->hitv);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sc[0], &sc[1]);
        mot->ang[0] = sc[0] * typ->damage_motion_ang;
        mot->ang[2] = sc[1] * typ->damage_motion_ang;
        mot->t = 0;
    }
}

/* 0x00afee40 */
void cherubimu_apply_damage_motions(void *obj, CherubimuWork *w, Vec3 *pos, int *ang)
{
    CherubimuRec *typ = w->type;
    Vec3 v;
    Vec3 ofs;
    int dang[3];
    float sn, cs;
    int back, ret;
    float spd, rate, k;
    CherubimuDamage *mot;
    int i;

    i3_set(ang, 0, 0, 0);
    back = typ->damage_motion_back_sec * FPS;
    ret = typ->damage_motion_ret_sec * FPS;
    spd = typ->damage_motion_back_spd * FRAME_SEC;
    v3_set(pos, 0, 0, 0);
    for (i = 0, mot = w->dmg; i < w->ndmg; i++, mot++) {
        if (mot->t < back) {
            v3_mul(&v, &mot->dir, spd * mot->t);
            v3_add(pos, &v);
        } else {
            rate = (float)(mot->t - back) / ret;
            ulSinCos(rate * ANG_HALF + ANG_QUARTER, &sn, &cs);
            k = spd * back * (sn + 1.0f) * 0.5f;
            v3_mul(&ofs, &mot->dir, k);
            v3_add(pos, &ofs);
            ulSinCos(typ->damage_motion_roll * rate, &sn, &cs);
            i3_mul(dang, mot->ang, sn * (1.0f - rate));
            i3_sub(ang, ang, dang);
        }
    }
}

/* 0x00aff108 */
void cherubimu_age_damage_motions(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    CherubimuDamage *mot = w->dmg;
    int back = (int)(typ->damage_motion_back_sec * FPS);
    int n = (int)(typ->damage_motion_ret_sec * FPS);
    int i;

    for (i = 0; i < w->ndmg; i++, mot++) {
        if (++mot->t >= back + n) {
            *mot = w->dmg[w->ndmg - 1];
            i--;
            mot--;
            w->ndmg--;
        }
    }
}

/* 0x00aff1d8 */
int cherubimu_break_wing_piece(void *obj, CherubimuWork *w, CherubimuWing *pieces, int n)
{
    CherubimuRec *typ = w->type;
    Vec3 p;
    Vec3 scl;
    Vec3 wpos;
    Vec3 dv;
    int ang[3];
    int i, k;
    float spd;
    CherubimuWing *piece;

    k = 0;
    piece = pieces;
    for (i = 0; i < n; i++, pieces++)
        if (!pieces->t)
            k++;
    for (i = 0; i < n; i++, piece++) {
        if (piece->t)
            continue;
        if (--k != 0)
            continue;
        piece->t = 1;
        enemy_task_get_pos(obj, &p);
        enemy_task_get_rot(obj, ang);
        enemy_task_get_scl(obj, &scl);
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        cherubimu_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        ulMatrixApplyVector((float *)ulMatrixCur, &piece->pos.x, &piece->vel.x);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll2 - w->roll);
        ulMatrixApplyVector((float *)ulMatrixCur, &piece->pos.x, &wpos.x);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll - w->roll2);
        do {
            Vec3 *p_ = &piece->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        cherubimu_rotate_yxz(piece->ang);
        ulMatrixGetTranslation((float *)ulMatrixCur, &piece->pos.x);
        model_camera_angles(piece->ang);
        ulMatrixPop();
        v3_sub(&dv, &piece->vel, &wpos);
        v3_scale(&dv, typ->wing_break_roll_rate);
        vec3_normalize(&piece->vel, &piece->vel);
        spd = typ->wing_break_spd * FRAME_SEC;
        piece->vel.x = piece->vel.x * spd + dv.x;
        piece->vel.y = piece->vel.y * spd + dv.y;
        piece->vel.z = piece->vel.z * spd + dv.z;
        v3_scale(&piece->vel, RAND_FLOAT_B() * (1.0f - typ->wing_break_spd_min) + typ->wing_break_spd_min);
        piece->angv[0] = w->type->wing_roll_spd_x / FPS;
        piece->angv[1] = w->type->wing_roll_spd_y / FPS;
        piece->angv[2] = 0;
        {
            float spin = RAND_FLOAT_B() * (1.0f - typ->wing_roll_spd_min) + typ->wing_roll_spd_min;

            i3_set(piece->angv, piece->angv[0] * spin, piece->angv[1] * spin, piece->angv[2] * spin);
        }
        return 1;
    }
    return 0;
}

/* 0x00aff580 */
void cherubimu_break_wing(void *obj, CherubimuWork *w)
{
    if (rand_lcg_next_b() & 1) {
        if (!cherubimu_break_wing_piece(obj, w, w->wingB, 2))
            cherubimu_break_wing_piece(obj, w, w->wingA, 2);
    } else {
        if (!cherubimu_break_wing_piece(obj, w, w->wingA, 2))
            cherubimu_break_wing_piece(obj, w, w->wingB, 2);
    }
}

/* 0x00aff628 */
void cherubimu_break_wings(void *obj, CherubimuWork *w)
{
    int i;

    for (i = 1; i >= 0; i--)
        cherubimu_break_wing_piece(obj, w, w->wingB, 2);
    for (i = 1; i >= 0; i--)
        cherubimu_break_wing_piece(obj, w, w->wingA, 2);
}

/* 0x00aff6c0 */
void cherubimu_move_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type)
{
    CherubimuRec *typ = w->type;
    Vec3 v;
    int dang[3];
    SpawnDesc desc;
    float rate, k;

    for (n--; n != -1; n--, piece++) {
        if (piece->t <= 0)
            continue;
        rate = piece->t / (typ->wing_break_sec * FPS);
        piece->t++;
        if (rate < typ->wing_break_sec_rate)
            rate = 0.0f;
        else
            rate = 1.0f - (rate - typ->wing_break_sec_rate) / (1.0f - typ->wing_break_sec_rate);
        v3_mul(&v, &piece->vel, rate + typ->wing_break_spd_limit);
        v3_add(&piece->pos, &v);
        i3_mul(dang, piece->angv, rate + typ->wing_roll_spd_limit);
        i3_add(piece->ang, dang);
        if (typ->wing_break_sec * FPS < piece->t) {
            piece->t = -1;
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0;
            desc.models = stage_models;
            desc.type = type;
            desc.lock = enemy_task_lock_ptr(obj);
            v3_copy(&desc.pos, &piece->pos);
            i3_copy(desc.ang, piece->ang);
            enemy_task_get_scl(obj, &desc.scale);
            enemy_task_get_param_vec(obj, &desc.v);
            en_shatter_spawn(obj, &desc, 3);
        }
    }
}

/* 0x00aff930 */
void cherubimu_move_broken_wings(void *obj, CherubimuWork *w)
{
    cherubimu_move_broken_wing(obj, w, w->wingB, 2, 101);
    cherubimu_move_broken_wing(obj, w, w->wingA, 2, 97);
}

/* 0x00aff980 */
void cherubimu_shift_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type)
{
    Vec3 org;
    Vec3 fwd;

    player_get_frame_vel(&org);
    player_get_frame_accel(&fwd);
    v3_sub(&org, &org, &fwd);
    for (n--; n != -1; n--, piece++)
        if (piece->t > 0)
            v3_add(&piece->pos, &org);
}

/* 0x00affa40 */
void cherubimu_shift_broken_wings(void *obj, TypeEnemyWork *w)
{
    cherubimu_shift_broken_wing(obj, (CherubimuWork *)w, ((CherubimuWork *)w)->wingB, 2, 101);
    cherubimu_shift_broken_wing(obj, (CherubimuWork *)w, ((CherubimuWork *)w)->wingA, 2, 97);
}

/* 0x00affa90 */
void cherubimu_spawn_wing_debris(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, int type)
{
    SpawnDesc desc;

    for (n--; n != -1; n--, piece++) {
        if (piece->t == -1)
            continue;
        nj_fill8(&desc, 0, sizeof desc);
        desc.kind = 0;
        desc.models = stage_models;
        desc.type = type;
        desc.lock = enemy_task_lock_ptr(obj);
        v3_copy(&desc.pos, &piece->pos);
        i3_copy(desc.ang, piece->ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        en_shatter_spawn(obj, &desc, 3);
    }
}

/* 0x00affb90 */
void cherubimu_shatter_wings(void *obj, TypeEnemyWork *w)
{
    cherubimu_break_wings(obj, (CherubimuWork *)w);
    cherubimu_spawn_wing_debris(obj, (CherubimuWork *)w, ((CherubimuWork *)w)->wingB, 2, 101);
    cherubimu_spawn_wing_debris(obj, (CherubimuWork *)w, ((CherubimuWork *)w)->wingA, 2, 97);
}

/* 0x00affbf0 */
void cherubimu_draw_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, ModelPart *body, ModelPart *wire)
{
    CherubimuRec *typ = w->type;

    for (n--; n != -1; piece++, n--) {
        if (piece->t)
            continue;
        ulMatrixPush();
        do {
            Vec3 *p_ = &piece->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        cherubimu_rotate_yxz(piece->ang);
        dc_object_draw(body);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f, typ->wire_near,
                                          typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            dc_object_draw(wire);
        }
        ulMatrixPop();
    }
}

/* 0x00affd08 */
void cherubimu_draw_wings(void *obj, CherubimuWork *w)
{
    cherubimu_draw_wing(obj, w, w->wingB, 2, &w->parts[1], &w->parts[5]);
    cherubimu_draw_wing(obj, w, w->wingA, 2, &w->parts[0], &w->parts[4]);
}

/* 0x00affd60 */
void cherubimu_draw_broken_wing(void *obj, CherubimuWork *w, CherubimuWing *piece, int n, ModelPart *body)
{
    for (n--; n != -1; piece++, n--) {
        if (piece->t <= 0)
            continue;
        ulMatrixPush();
        do {
            Vec3 *p_ = &piece->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        cherubimu_rotate_yxz(piece->ang);
        dc_object_draw(body);
        ulMatrixPop();
    }
}

/* 0x00affe10 */
void cherubimu_draw_broken_wings(void *obj, CherubimuWork *w)
{
    draw_set_color(w->type->wing_wire_col);
    draw_set_blend(4, 1);
    cherubimu_draw_broken_wing(obj, w, w->wingB, 2, &w->parts[3]);
    cherubimu_draw_broken_wing(obj, w, w->wingA, 2, &w->parts[2]);
}

/* steer(cast) */
/* 0x00affe80 */
void cherubimu_init(void *obj, CherubimuWork *w, int prm, int *k)
{
    CherubimuRec *typ;
    Vec3 v;
    int ang[3];
    BackFireLink lnk;
    float sc[4][2];
    float x, y, z, fzero, nf;
    int n;

    typ = (CherubimuRec *)&cherubimu_types[*k];
    w->type = typ;
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((ObjMT *)obj)->models = m_;
        ((ObjMT *)obj)->tex = t_;
    } while (0);
    dc_object_init(&w->parts[1], *((struct DcNode **)pnb_get_entry(stage_models, 96)), 0, 0);
    dc_object_rebuild(&w->parts[1]);
    dc_object_init(&w->parts[0], *((struct DcNode **)pnb_get_entry(stage_models, 100)), 0, 0);
    dc_object_rebuild(&w->parts[0]);
    dc_object_init(&w->parts[3], *((struct DcNode **)pnb_get_entry(stage_models, 98)), 0, 1280);
    dc_object_set_wire(&w->parts[3], model_pnb_get_wire(((void **)obj)[0], 98));
    dc_object_init(&w->parts[2], *((struct DcNode **)pnb_get_entry(stage_models, 102)), 0, 1280);
    dc_object_set_wire(&w->parts[2], model_pnb_get_wire(((void **)obj)[0], 102));
    dc_object_init(&w->parts[4], *((struct DcNode **)pnb_get_entry(stage_models, 103)), 0, 1280);
    dc_object_set_wire(&w->parts[4], model_pnb_get_wire(((void **)obj)[0], 103));
    dc_object_init(&w->parts[5], *((struct DcNode **)pnb_get_entry(stage_models, 99)), 0, 1280);
    dc_object_set_wire(&w->parts[5], model_pnb_get_wire(((void **)obj)[0], 99));
    lockon_obj_set_name(enemy_task_lock_ptr(obj), cherubimu_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    enemy_task_clear_flags(obj, 64);
    enemy_task_get_param_vec(obj, &v);
    if (v3_is_zero(&v)) {
        w->speed = 0;
        v.z = -1.0f;
        v.x = 0;
        v.y = 0;
        ang[0] = 0;
        ang[1] = 0;
    } else {
        w->speed = vec3_length(&v);
        vec3_normalize(&v, &v);
        do {
            ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
    }
    ang[2] = 0;
    do {
        enemy_task_set_param_vec(obj, &v);
    } while (0);
    {
        float x, y, z;

        x = 0.0f;
        enemy_task_set_rot(obj, ang);
        i3_copy(w->ang, ang);
        w->roll2 = 0;
        w->roll = 0;
        w->state = 0;
        w->count = 1;
        enemy_task_set_scl(obj, &cherubimu_init_scale);
        y = 1.0f;
        z = 0.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&w->up, x, y, z);
    }
    fzero = 0.0f;
    n = enemy_task_get_param(obj, 3);
    nf = n;
    if (fabsf(nf) < FLT_EPSILON) {
        w->side_k = fzero;
        x = fzero;
        w->side_s = 1.0f;
    } else {
        x = 0 < nf ? 1.0f : -1.0f;
        w->side_k = x;
        w->side_s = x;
    }
    /* X rotation of (x, 0, 0); both inputs 0 */
    {
        float k = 0.0f;

        ulSinCos(ang[0], &sc[2][0], &sc[2][1]);
        y = fzero * sc[2][1] - fzero * sc[2][0];
        z = k * sc[2][0] + fzero * sc[2][1];
    }
    ROT_Y(ang[1], sc[3], x, z);
    v3_set(&w->side, x, y, z);
    heap_bzero(&lnk, 52);
    lnk.gfx = enemy_task_gfx_ptr(obj);
    lnk.pos = enemy_task_pos_ptr(obj);
    lnk.ang = enemy_task_rot_ptr(obj);
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.scale = &w->fire;
    lnk.flags = 0x1000000;
    lnk.use_pose = 1;
    en_backfire_create(obj, &lnk, 0);
    w->fire = typ->backfire_scl;
    heap_bzero(w->wingA, 112);
    heap_bzero(w->wingB, 112);
    cherubimu_record_wings(obj, w);
    w->tackle_done = 0;
    cherubimu_clear_damage(obj, w);
    v3_set(&w->dmg_pos, 0.0f, 0.0f, 0.0f);
    i3_set(w->dmg_ang, 0, 0, 0);
}

/* 0x00b00470 */
void cherubimu_release(void *obj, CherubimuWork *w)
{
    dc_object_release(&w->parts[1]);
    dc_object_release(&w->parts[0]);
    dc_object_release(&w->parts[3]);
    dc_object_release(&w->parts[2]);
    dc_object_release(&w->parts[5]);
    dc_object_release(&w->parts[4]);
}

/* 0x00b004c0 */
int cherubimu_next_state(void *obj, TypeEnemyWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00b004f0 */
void cherubimu_state_arrive(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    Vec3 p;
    Vec3 v;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    w->roll += typ->roll_spd / FPS * w->side_s;
    cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 1);
}

/* 0x00b00620 */
void cherubimu_state_break(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    int n = typ->break_sec * FPS;
    float rate = w->count / (float)n;
    float prev = (w->count - 1) / (float)n;
    int ang[3];
    Vec3 p;
    Vec3 v;
    float sc[2][2];
    float spd, acc, k;
    float dt;
    int pitch;

    enemy_task_get_pos(obj, &p);
    spd = w->speed * FPS;
    acc = -spd / typ->break_sec;
    dt = typ->break_sec * rate;
    spd += acc * dt;
    dt = 1.0f / FPS;
    spd *= dt;
    spd += acc * 0.5f * dt * dt;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, spd);
    v3_add(&p, &v);
    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    v3_mul(&v, &w->up, typ->break_up_dis * (sc[0][0] + 1.0f) * 0.5f);
    v3_add(&p, &v);
    ulSinCos(prev * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    v3_mul(&v, &w->up, typ->break_up_dis * (sc[0][0] + 1.0f) * 0.5f);
    p.x -= v.x;
    p.y -= v.y;
    p.z -= v.z;
    enemy_task_set_pos(obj, &p);
    k = 1.0f - safe_powf(1.0f - rate, typ->break_pow);
    pitch = typ->break_ang * k;
    ulMatrixPush();
    matrix_unit_set(0);
    cherubimu_rotate_yxz(w->ang);
    vu0_matrix_rotate_x(ulMatrixCur, pitch);
    model_camera_angles(ang);
    ulMatrixPop();
    w->roll += typ->roll_spd / FPS * w->side_s;
    enemy_task_set_rot(obj, ang);
    ulSinCos(rate * 49152.0f + ANG_QUARTER, &sc[1][0], &sc[1][1]);
    w->fire = sc[1][0] * typ->backfire_scl;
    if (!(fabsf((int)enemy_task_get_param(obj, 3)) < FLT_EPSILON) || enemy_task_get_param(obj, 2) < 0.5f)
        cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 2);
    else if (cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 4)) {
        i3_copy(w->prev_ang, ang);
        cherubimu_break_wings(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->life = 1;
    }
}

/* 0x00b00a68 */
void cherubimu_state_stop(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    int n = typ->stop_sec * FPS;
    float rate = w->count / (float)n;
    float prev = (w->count - 1) / (float)n;
    int ang[3];
    Vec3 p;
    Vec3 v;
    float ease, cs, ease_prev;
    float us, uc;
    int phase;
    int phase_prev;
    int pitch, rz;
    float k;

    enemy_task_get_pos(obj, &p);
    phase = rate * ANG_HALF - ANG_QUARTER;
    ulSinCos(phase, &ease, &cs);
    ease = (ease + 1.0f) * 0.5f;
    phase_prev = prev * ANG_HALF - ANG_QUARTER;
    ulSinCos(phase_prev, &ease_prev, &cs);
    ease = ease - (ease_prev + 1.0f) * 0.5f;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, -ease * enemy_task_get_param(obj, 4));
    v3_add(&p, &v);
    v3_mul(&v, &w->side, -(typ->stop_swing_dis * ease));
    v3_add(&p, &v);
    ulSinCos(phase, &ease, &cs);
    ease = (ease + 1.0f) * 0.5f - 1.0f;
    v3_mul(&v, &w->up, -(typ->stop_down_dis * sqrtf(1.0f - ease * ease)));
    v3_add(&p, &v);
    ulSinCos(phase_prev, &ease, &cs);
    ease = (ease + 1.0f) * 0.5f - 1.0f;
    v3_mul(&v, &w->up, -(typ->stop_down_dis * sqrtf(1.0f - ease * ease)));
    p.x -= v.x;
    p.y -= v.y;
    p.z -= v.z;
    enemy_task_set_pos(obj, &p);
    ulSinCos(safe_powf(rate, typ->stop_pitch_pow) * ANG_HALF - ANG_QUARTER, &us, &uc);
    us = (us + 1.0f) * 0.5f;
    pitch = (typ->break_ang + typ->stop_pitch_ang) * safe_powf(us, typ->stop_pitch_pow2);
    ulSinCos(safe_powf(rate, typ->stop_roll_pow) * ANG_HALF - ANG_QUARTER, &us, &uc);
    us = (us + 1.0f) * 0.5f;
    k = safe_powf(us, typ->stop_roll_pow2);
    rz = typ->stop_roll_ang * k * w->side_k;
    rz = -rz;
    ulMatrixPush();
    matrix_unit_set(0);
    cherubimu_rotate_yxz(w->ang);
    vu0_matrix_rotate_z(ulMatrixCur, rz);
    vu0_matrix_rotate_x(ulMatrixCur, typ->break_ang - pitch);
    model_camera_angles(ang);
    ulMatrixPop();
    w->roll += typ->roll_spd / FPS * (1.0f - rate) * w->side_s;
    enemy_task_set_rot(obj, ang);
    if (cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 3)) {
        i3_copy(w->prev_ang, ang);
        w->pitch = typ->break_ang - pitch;
        w->rollz = rz;
    }
}

/* 0x00b00f98 */
void cherubimu_state_turn(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    int n = fabsf((int)enemy_task_get_param(obj, 3)) * 60.0f / 60.0f + 1.0f;
    float fn = n;
    float rate = w->count / fn;
    float prev = (w->count - 1) / fn;
    float acc = typ->turn_acc;
    int ang[3];
    Vec3 p;
    Vec3 v;
    float sc[2][2];
    float sn;
    float total_sec, spd_start, dec, dt, spd, k;

    if (n < 2) {
        rate = w->count / FPS;
        n = 60;
        prev = (w->count - 1) / FPS;
        fn = 60.0f;
        acc = 0.0f;
    }
    enemy_task_get_pos(obj, &p);
    if (rate < typ->turn_rate) {
        spd_start = acc * ((w->count - 1) / FPS);
        k = 1.0f / FPS;
        spd = spd_start * k + acc * 0.5f * k * k;
        v3_mul(&v, &w->side, spd);
        v3_add(&p, &v);
    } else {
        total_sec = fn / FPS;
        spd_start = acc * total_sec * typ->turn_rate;
        dec = -spd_start / (total_sec * (1.0f - typ->turn_rate));
        dt = (w->count - 1) / FPS - total_sec * typ->turn_rate;
        spd_start += dec * dt;
        k = 1.0f / FPS;
        spd = spd_start * k + dec * 0.5f * k * k;
        v3_mul(&v, &w->side, spd);
        v3_add(&p, &v);
    }
    k = fn / 60.0f * typ->turn_rate;
    spd = acc * 0.5f * k * k * typ->turn_down_rate;
    if (rate < typ->turn_rate)
        ulSinCos(rate * ANG_HALF / typ->turn_rate - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    else
        ulSinCos((rate - typ->turn_rate) * ANG_HALF / (1.0f - typ->turn_rate) + ANG_QUARTER,
                 &sc[0][0], &sc[0][1]);
    sc[0][0] = (sc[0][0] + 1.0f) * 0.5f;
    v3_mul(&v, &w->up, -sc[0][0] * spd);
    v3_add(&p, &v);
    if (prev < typ->turn_rate)
        ulSinCos(prev * ANG_HALF / typ->turn_rate - ANG_QUARTER, &sc[0][0], &sc[0][1]);
    else
        ulSinCos((prev - typ->turn_rate) * ANG_HALF / (1.0f - typ->turn_rate) + ANG_QUARTER,
                 &sc[0][0], &sc[0][1]);
    sc[0][0] = (sc[0][0] + 1.0f) * 0.5f;
    v3_mul(&v, &w->up, -sc[0][0] * spd);
    p.x -= v.x;
    p.y -= v.y;
    p.z -= v.z;
    enemy_task_set_pos(obj, &p);
    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[1][0], &sc[1][1]);
    sc[1][0] = (sc[1][0] + 1.0f) * 0.5f;
    ulSinCos(rate * ANG_QUARTER, &sn, &sc[1][1]);
    ulMatrixPush();
    matrix_unit_set(0);
    cherubimu_rotate_yxz(w->ang);
    vu0_matrix_rotate_z(ulMatrixCur, -(typ->turn_roll_time << 16) * w->side_k * sc[1][0] + w->rollz * sc[1][1]);
    vu0_matrix_rotate_x(ulMatrixCur, w->pitch * sc[1][1]);
    model_camera_angles(ang);
    ulMatrixPop();
    yuki_lerp3(ang, w->prev_ang, ang, rate);
    w->roll = w->roll * (1.0f - rate);
    enemy_task_set_rot(obj, ang);
    if (cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 4)) {
        i3_copy(w->prev_ang, ang);
        if (0.0f < enemy_task_get_param(obj, 2)) {
            cherubimu_break_wings(obj, w);
            ((LockObj *)enemy_task_lock_ptr(obj))->life = 1;
            SoundRhythmSe_Enemy(cherubimu_init_arg, 5);
        }
    }
}

/* 0x00b01658 */
void cherubimu_state_accel(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    int n = typ->accel_sec * FPS;
    float rate = w->count / (float)n;
    float prev = (w->count - 1) / (float)n;
    float fzero = 0.0f;
    Vec3 p;
    int ang[3];
    Vec3 pl;
    int oang[3];
    struct {
        float s, c, s0;
        float u[2], b[2], sc[2], sc2[2];
    } k;
    float x, y, z;

    enemy_task_get_pos(obj, &p);
    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &k.s, &k.c);
    k.s = (k.s + 1.0f) * 0.5f;
    ulSinCos(prev * ANG_HALF - ANG_QUARTER, &k.s0, &k.c);
    k.s = k.s - (k.s0 + 1.0f) * 0.5f;
    p.y -= typ->accel_fall_dis * k.s;
    enemy_task_set_pos(obj, &p);
    ulSinCos(safe_powf(rate, typ->accel_pitch_ang_pow) * ANG_HALF - ANG_QUARTER, &k.u[0], &k.u[1]);
    ulMatrixPush();
    matrix_unit_set(0);
    cherubimu_rotate_yxz(w->prev_ang);
    vu0_matrix_rotate_x(ulMatrixCur, typ->accel_pitch_ang * (k.u[0] + 1.0f) * 0.5f);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    ulSinCos(rate * 49152.0f, &k.b[0], &k.b[1]);
    w->fire = k.b[0] * typ->backfire_scl2;
    if (cherubimu_next_state(obj, (TypeEnemyWork *)w, n, 0.0f < enemy_task_get_param(obj, 2) ? 6 : 5)) {
        enemy_task_get_rot(obj, oang);
        x = fzero;
        y = fzero;
        z = -1.0f;
        ROT_X(oang[0], k.sc, y, z);
        ROT_Y(oang[1], k.sc2, x, z);
        v3_set(&w->dir, x, y, z);
        enemy_task_set_param_vec(obj, &w->dir);
        player_get_pos(&pl);
        w->speed = ulVec3Distance(&p, &pl);
        i3_copy(w->prev_ang, oang);
    }
}

/* 0x00b01998 */
void cherubimu_state_away(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    float rate = w->count / (float)(int)(typ->away_sec * FPS);
    float k = 1.0f < rate ? 1.0f : rate;
    Vec3 p;
    Vec3 v;
    int ang[3];
    int nang[3];
    float sc[2][2];
    float spd, x, y, z;

    enemy_task_get_pos(obj, &p);
    x = 0.0f;
    y = x;
    spd = typ->away_acc * ((w->count - 1) / FPS) * (1.0f / FPS) + typ->away_acc * 0.5f * (1.0f / FPS) * (1.0f / FPS);
    enemy_task_get_rot(obj, ang);
    z = -1.0f;
    ROT_X(ang[0], sc[0], y, z);
    ROT_Y(ang[1], sc[1], x, z);
    v.x = x * spd;
    v.y = y * spd;
    v.z = z * spd;
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    yuki_lerp3(nang, w->prev_ang, w->ang, k);
    enemy_task_set_rot(obj, nang);
    w->roll += typ->roll_spd / FPS * w->side_s;
    w->count++;
}

/* steer(cast) */
/* 0x00b01bd0 */
void cherubimu_state_tackle(void *obj, CherubimuWork *w)
{
    CherubimuRec *typ = w->type;
    float rate = w->count / (typ->tackle_ang_sec * FPS);
    Vec3 next;
    Vec3 p;
    Vec3 pl;
    Vec3 v;
    Vec3 pl_pos;
    Vec3 to_pl;
    Vec3 cur;
    int ang[3];
    float sc[2];
    float dist, spd, k;

    player_get_pos(&pl);
    enemy_task_get_pos(obj, &p);
    dist = ulVec3Distance(&pl, &p);
    if (typ->tackle_dis_start < dist)
        spd = typ->tackle_spd_start;
    else if (dist < typ->tackle_dis_end)
        spd = typ->tackle_spd_end;
    else
        spd = (typ->tackle_spd_end - typ->tackle_spd_start) * ((typ->tackle_dis_start - dist) / (typ->tackle_dis_start - typ->tackle_dis_end))
              + typ->tackle_spd_start;
    spd = MIN(spd, w->count / FPS * typ->tackle_acc);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, spd * (1.0f / FPS));
    v3_sum(&next, &p, &v);
    enemy_task_set_pos(obj, &next);
    player_get_pos(&pl_pos);
    v3_sub(&to_pl, &pl_pos, &next);
    vec3_normalize(&to_pl, &to_pl);
    v3_copy(&cur, &w->dir);
    ulSinCos(MIN(MAX_GE(rate, 0.0f), 1.0f) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    vec3_approach(&to_pl, &cur, (sc[0] + 1.0f) * 0.5f);
    vec3_normalize(&cur, &cur);
    enemy_task_set_param_vec(obj, &cur);
    enemy_task_get_rot(obj, ang);
    yuki_draw_aimed(&next, &p, ang);
    enemy_task_set_rot(obj, ang);
    k = typ->roll_spd_tackle / FPS * w->side_s;
    w->roll += rate < 1.0f ? k * rate : k;
    if (!w->tackle_done && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        if (player_sphere_test(&next, typ->crash_r) == 1) {
            player_status_add_damage(typ->crash_damage);
            enemy_task_set_state(obj, 3);
            w->tackle_done = 1;
        }
        if (!yuki_camera_relative(&next, typ->tackle_clip_ang, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius)) {
            w->tackle_done = 1;
            enemy_task_set_state(obj, 3);
        }
    }
    w->count++;
}

static inline void pos_ang_sub(Vec3 *p, int *ang, Vec3 *dv, int *da)
{
    p->x -= dv->x;
    ang[0] -= da[0];
    p->y -= dv->y;
    ang[1] -= da[1];
    p->z -= dv->z;
    ang[2] -= da[2];
}

static inline void pos_ang_add(Vec3 *p, int *ang, Vec3 *dv, int *da)
{
    p->x += dv->x;
    ang[0] += da[0];
    p->y += dv->y;
    ang[1] += da[1];
    p->z += dv->z;
    ang[2] += da[2];
}

/* 0x00b01f90 */
void cherubimu_update(void *obj, TypeEnemyWork *work)
{
    CherubimuWork *w = (CherubimuWork *)work;
    Vec3 p;
    int ang[3];
    Vec3 npos;
    int nang[3];

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    pos_ang_sub(&p, ang, &w->dmg_pos, w->dmg_ang);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    w->roll2 = w->roll;
    if (cherubimu_states[w->state])
        cherubimu_states[w->state](obj, w);
    w->roll = w->roll & 0x8000 ? w->roll | ~0xffff : w->roll & 0xffff;
    cherubimu_age_damage_motions(obj, w);
    cherubimu_apply_damage_motions(obj, w, &w->dmg_pos, w->dmg_ang);
    enemy_task_get_pos(obj, &npos);
    enemy_task_get_rot(obj, nang);
    pos_ang_add(&npos, nang, &w->dmg_pos, w->dmg_ang);
    enemy_task_set_pos(obj, &npos);
    enemy_task_set_rot(obj, nang);
    cherubimu_move_broken_wings(obj, w);
}

/* 0x00b02190 */
void cherubimu_exec_appear(void *obj, TypeEnemyWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_appear_scale(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        enemy_task_appear_scale(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        cherubimu_update(obj, w);
        enemy_task_appear_scale(obj, w);
    }
}

/* 0x00b02240 */
void cherubimu_exec_normal(void *obj, TypeEnemyWork *w)
{
    float frm;

    if (enemy_task_get_phase(obj) != 1)
        return;
    cherubimu_update(obj, w);
    frm = enemy_task_get_time(obj);
    if (enemy_task_get_param(obj, 0) * 60.0f / 60.0f < frm)
        enemy_task_set_state(obj, 2);
}

/* 0x00b022d8 */
void cherubimu_exec_leave(void *obj, TypeEnemyWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_vanish_scale(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        enemy_task_vanish_scale(obj, w);
        break;
    default:
        cherubimu_update(obj, w);
        enemy_task_vanish_scale(obj, w);
    }
}

/* 0x00b02380 */
void cherubimu_exec_destroyed(void *obj, TypeEnemyWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 104;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &desc.v);
        en_shatter_spawn(obj, &desc, 3);
        cherubimu_shatter_wings(obj, w);
        FINISH_IF(obj);
    }
}

/* 0x00b02448 */
void cherubimu_pre_exec(void *obj, TypeEnemyWork *w)
{
    cherubimu_shift_broken_wings(obj, w);
}

/* 0x00b02450 */
void cherubimu_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (cherubimu_exec_table[i])
        cherubimu_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b024a8 */
void cherubimu_post_exec(void *obj, CherubimuWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);
    SpawnDesc desc;
    Vec3 vpos;

    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        cherubimu_break_wing(obj, w);
        cherubimu_add_damage_motion(obj, w);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 105;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 3);
        if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
            enemy_task_set_state(obj, 3);
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            if (w->state < 4)
                SoundRhythmSe_EnemyPos(cherubimu_init_arg, 1, &vpos.x);
            else
                SoundRhythmSe_EnemyPos(cherubimu_init_arg, 4, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(cherubimu_init_arg, 3, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(cherubimu_init_arg, 2, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(cherubimu_init_arg, 0, &vpos.x);
            break;
        }
    }
}

/* 0x00b026d8 */
void cherubimu_disp_body(void *obj, CherubimuWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scl;

    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, gfx->radius)) {
        enemy_task_get_rot(obj, ang);
        enemy_task_get_scl(obj, &scl);
        ulGxSetTextureList(((void **)obj)[1]);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        cherubimu_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        cherubimu_draw_wings(obj, w);
        ulMatrixPop();
        cherubimu_draw_broken_wings(obj, w);
    }
}

/* 0x00b027e0 */
void cherubimu_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (cherubimu_disp_table[i])
        cherubimu_disp_table[i](obj, w);
}

/* 0x00b02838 */
int cherubimu_create(void *obj, ShotArg *place)
{
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)cherubimu_funcs;
    int k[1];
    CherubimuRec *typ = (CherubimuRec *)cherubimu_types;

    k[0] = 0;
    nj_fill8(&desc, 0, sizeof desc);
    desc.arg = place;
    desc.kind = k;
    desc.name = cherubimu_name;
    desc.funcs = &funcs;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 95;
    desc.model_no2 = -1;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = place->move * 60.0f / 60.0f;
    desc.size = 1244;
    return enemy_task_task_create(obj, &desc);
}

/* 0x00b02988 */
void cherubimu_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
