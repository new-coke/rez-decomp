/*
 * Powers, Area 5 enemy: two-legged walker riding the path. Transforms, turns to the player, opens
 * and fires homing missiles (macros_missile), closes; may vanish ahead and return. Hits and swings
 * tilt it; when destroyed its parts fall away on the beat.
 */

#include "stg05.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
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
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "macros_missile.h"
#include "stage05.h"
#include "../common/en_backfire.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int powers_init_arg;
extern void (*powers_exec_table[])(void *o, TypeEnemyWork *w);
extern void (*powers_disp_table[])(void *o, TypeEnemyWork *w);
void powers_vars_register(void);
void powers_vars_unregister(void);
int powers_vars_init(int *prm);
int powers_vars_release(void);
int powers_next_state(void *obj, TypeEnemyWork *w, int n, int state);
void powers_exec(void *obj, TypeEnemyWork *w);
void powers_disp(void *obj, TypeEnemyWork *w);
void powers_rotate_yxz(int *ang);

/* group name, label, 52 values */
typedef struct PowersSet {
    char *name;
    char *label;                /* 0x04 */
    int v[52];                  /* 0x08 */
} PowersSet;

extern PowersSet powers_types[1];
extern VarDef powers_vardef;
extern VarDef powers_vardefs[52];

/* 44 bytes: damage or swing motion */
typedef struct PowersMotion {
    Vec3 dir;                   /* hit direction */
    int ang[3];                 /* 0x0c: tilt */
    int t;                      /* 0x18: frames */
    int back;                   /* 0x1c: frames of the push back */
    int ret;                    /* 0x20: frames of the return */
    float spd;                  /* 0x24 */
    int roll;                   /* 0x28 */
} PowersMotion;

typedef struct PowersWork {
    struct PowersRec *type;
    int state;                  /* 0x04: indexes powers_states */
    int count;                  /* 0x08: frames of the current state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    int life;                   /* 0x18 */
    Vec3 pos;                   /* 0x1c */
    int ang[3];                 /* 0x28 */
    ModelPart parts[4];            /* 0x34: models 47/48, 57/60, 62/63, 50/56 */
    Vec3 lp[2];                 /* 0x1a4: model nodes of flag 0x2000000 */
    int la[2][3];               /* 0x1bc */
    Vec3 tp[2];                 /* 0x1d4: model nodes of flag 0x1000000 */
    int ta[2][3];               /* 0x1ec */
    int fall_frames[4];
    int k2;                     /* 0x214: creation word 2 */
    int k2pos;                  /* 0x218: k2 > 0 */
    int beats;                  /* 0x21c: die_beat */
    int ndmg;                   /* 0x220: damage motions; cleared by powers_clear_damage */
    PowersMotion dmg[10];             /* 0x224 */
    float fscale;               /* 0x3dc: the model scale at the leg node */
} PowersWork;

extern void (*powers_states[])(void *o, TypeEnemyWork *w);

extern PowersWork *powers_backfire_work;
int powers_backfire_node_cb(unsigned int *n);

/* 216 bytes; names in powers_vardefs */
typedef struct PowersRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_reduce;            /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float appear_sec;           /* 0x20 */
    float disappear_sec;        /* 0x24 */
    float disappear_acc;        /* 0x28 */
    int disappear_ang;          /* 0x2c */
    float disappear_ang_pow;    /* 0x30 */
    float disappear_back_spd;   /* 0x34 */
    float shot_sec;             /* 0x38 */
    float bullet_spd;           /* 0x3c */
    float bullet_act_sec;       /* 0x40 */
    float bullet_act_chg_sec;   /* 0x44 */
    float dir_sec;              /* 0x48 */
    float open_sec;             /* 0x4c */
    float wait_sec;             /* 0x50 */
    float close_sec;            /* 0x54 */
    int die_beat;               /* 0x58 */
    float die_back_rate;        /* 0x5c */
    float die_back_rate_parts;  /* 0x60 */
    int die_roll;               /* 0x64 */
    int die_roll_parts;         /* 0x68 */
    float trans_start_sec;      /* 0x6c */
    float trans_end_sec;        /* 0x70 */
    float damage_motion_back_sec; /* 0x74 */
    float damage_motion_back_spd; /* 0x78 */
    float damage_motion_ret_sec; /* 0x7c */
    int damage_motion_ang;      /* 0x80 */
    int damage_motion_roll;     /* 0x84 */
    float swing_motion_back_sec; /* 0x88 */
    float swing_motion_back_spd; /* 0x8c */
    float swing_motion_ret_sec; /* 0x90 */
    int swing_motion_ang;       /* 0x94 */
    int swing_motion_roll;      /* 0x98 */
    float die_damage_motion;    /* 0x9c */
    float fall_acc;             /* 0xa0 */
    float fall_acc_parts;       /* 0xa4 */
    int attack_roll_ang;        /* 0xa8 */
    float leg_backfire_len;     /* 0xac */
    float leg_backfire_width;   /* 0xb0 */
    int wire;                   /* 0xb4 */
    int wire_linear;            /* 0xb8 */
    unsigned int wire_col;      /* 0xbc */
    unsigned int wire_col2;     /* 0xc0 */
    float wire_near;            /* 0xc4 */
    float wire_far;             /* 0xc8 */
    float wire_near_rate;       /* 0xcc */
    float wire_far_rate;        /* 0xd0 */
    int wire_beat;              /* 0xd4 */
} PowersRec;

extern Vec3 powers_backfire_root;
extern char powers_lock_name[];
extern Vec3 powers_init_scale;
extern Vec3 powers_backfire_tip;
void powers_add_damage_motion(void *obj, PowersWork *w);
void powers_add_swing_motion(void *obj, PowersWork *w);
void powers_apply_motions(void *obj, PowersWork *w, int *out);
void powers_walk_nodes(unsigned int *n, Vec3 **lp, int **la, Vec3 **tp, int **ta);
void powers_update_nodes(void *obj, PowersWork *w);
void powers_walk_bullet_nodes(unsigned int *n, Vec3 **bpos, int **bang);
void powers_update_bullet_nodes(Vec3 *bpos, int *bang, Vec3 *pos, int *ang, void *obj, PowersWork *w);
void powers_path_pos(void *obj, PowersWork *w, Vec3 *pos, int *ang);
void powers_aim_angles(void *obj, PowersWork *w, int *ang);
void powers_die(void *obj, PowersWork *w);
void powers_draw_band(Vec3 *from, Vec3 *to, float u0, float u1, float width);
void powers_draw_backfire(float width);
extern char powers_name[];
extern char powers_funcs[];

/* 0x00b09ce0 */
void powers_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&powers_vardef);
    for (i = 0; i < 1; i++) {
        def = powers_vardefs;
        val = powers_types[i].v;
        powers_vardef.name = powers_types[i].name;
        powers_vardef.fmt = powers_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&powers_vardef);
        for (j = 0; j < 52; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)powers_vardefs);
    }
}

/* 0x00b09df8 */
void powers_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b09e00 */
int powers_get_init_arg(void)
{
    return powers_init_arg;
}

/* 0x00b09e10 */
int powers_vars_init(int *prm)
{
    powers_init_arg = *prm;
    powers_vars_register();
    return 0;
}

/* 0x00b09e38 */
int powers_vars_release(void)
{
    powers_vars_unregister();
    return 0;
}

/* 0x00b09e58 */
int powers_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 49);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b09eb0 */
void powers_clear_damage(void *obj, PowersWork *w)
{
    w->ndmg = 0;
}

/* 0x00b09eb8 */
void powers_add_damage_motion(void *obj, PowersWork *w)
{
    PowersRec *typ;
    PowersMotion *mot;
    float sc[2];

    mot = w->dmg;
    if (w->ndmg < 10) {
        mot += w->ndmg++;
        typ = w->type;
        vec3_normalize(&mot->dir, &w->hitv);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0], &sc[1]);
        mot->ang[0] = sc[0] * typ->damage_motion_ang;
        mot->ang[2] = sc[1] * typ->damage_motion_ang;
        mot->t = 0;
        mot->back = typ->damage_motion_back_sec * FPS;
        mot->ret = typ->damage_motion_ret_sec * FPS;
        mot->spd = typ->damage_motion_back_spd * (1.0f / FPS);
        mot->roll = typ->damage_motion_roll;
    }
}

/* 0x00b09fe8 */
void powers_add_swing_motion(void *obj, PowersWork *w)
{
    PowersRec *typ;
    PowersMotion *mot;
    float sc[2];

    mot = w->dmg;
    if (w->ndmg < 10) {
        mot += w->ndmg++;
        typ = w->type;
        vec3_normalize(&mot->dir, &w->hitv);
        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[0], &sc[1]);
        mot->ang[0] = sc[0] * typ->swing_motion_ang;
        mot->ang[2] = sc[1] * typ->swing_motion_ang;
        mot->t = 0;
        mot->back = typ->swing_motion_back_sec * FPS;
        mot->ret = typ->swing_motion_ret_sec * FPS;
        mot->spd = typ->swing_motion_back_spd * (1.0f / FPS);
        mot->roll = typ->swing_motion_roll;
    }
}

/* 0x00b0a118 */
void powers_apply_motions(void *obj, PowersWork *w, int *out)
{
    Vec3 p;
    int dang[3];
    Vec3 mv;
    Vec3 v;
    Vec3 ofs;
    float sc[2];
    PowersMotion *mot;
    float rate, k;
    int i;

    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    mv.x = 0;
    mv.y = 0;
    mv.z = 0;
    for (i = 0, mot = w->dmg; i < w->ndmg; i++, mot++) {
        if (++mot->t < mot->back) {
            v3_mul(&v, &mot->dir, mot->spd * mot->t);
            v3_add(&mv, &v);
        } else {
            rate = (float)(mot->t - mot->back) / mot->ret;
            ulSinCos(rate * ANG_HALF + ANG_QUARTER, &sc[0], &sc[1]);
            v3_mul(&ofs, &mot->dir, mot->spd * mot->back * (sc[0] + 1.0f) * 0.5f);
            v3_add(&mv, &ofs);
            ulSinCos(mot->roll * rate, &sc[0], &sc[1]);
            i3_mul(dang, mot->ang, sc[0] * (1.0f - rate));
            i3_sub(out, out, dang);
        }
        if (mot->t >= mot->back + mot->ret) {
            *mot = w->dmg[w->ndmg - 1];
            w->ndmg--;
            i--;
            mot--;
        }
    }
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &mv);
    enemy_task_set_pos(obj, &p);
}

/* steer(cast) */
/* 0x00b0a478 */
void powers_walk_nodes(unsigned int *n, Vec3 **lp, int **la, Vec3 **tp, int **ta)
{
    do {
        ulMatrixPush();
        anim_apply_node(n);
        if (n[0] & 0x2000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &(*lp)->x);
            (*lp)++;
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            ((void (*)(int *))model_camera_angles)(*la);
            *la += 3;
            ulMatrixPop();
        }
        if (n[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &(*tp)->x);
            (*tp)++;
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            ((void (*)(int *))model_camera_angles)(*ta);
            *ta += 3;
            ulMatrixPop();
        }
        if (!(n[0] & 0x10))
            powers_walk_nodes((unsigned int *)n[11], lp, la, tp, ta);
        ulMatrixPop();
        n = (unsigned int *)n[12];
    } while (n);
}

/* 0x00b0a5c8 */
void powers_update_nodes(void *obj, PowersWork *w)
{
    ModelPart *gfx;
    Vec3 p;
    int ang[3];
    Vec3 scl;
    Vec3 *lp = w->lp;
    Vec3 *tp = w->tp;
    int *la = w->la[0];
    int *ta = w->ta[0];

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    anim_start((void *)gfx->clip, gfx->frame);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
    powers_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    powers_walk_nodes((unsigned int *)gfx->root, &lp, &la, &tp, &ta);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b0a6c0 */
void powers_walk_bullet_nodes(unsigned int *n, Vec3 **bpos, int **bang)
{
    do {
        ulMatrixPush();
        anim_apply_node(n);
        if (n[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &(*bpos)->x);
            (*bpos)++;
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            ((void (*)(int *))model_camera_angles)(*bang);
            *bang += 3;
            ulMatrixPop();
        }
        if (!(n[0] & 0x10))
            powers_walk_bullet_nodes((unsigned int *)n[11], bpos, bang);
        ulMatrixPop();
        n = (unsigned int *)n[12];
    } while (n);
}

/* 0x00b0a798 */
void powers_update_bullet_nodes(Vec3 *bpos, int *bang, Vec3 *pos, int *ang, void *obj, PowersWork *w)
{
    ModelPart *part = &w->parts[2];

    anim_start((void *)part->clip, part->frame);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    powers_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    powers_walk_bullet_nodes((unsigned int *)part->root, &bpos, &bang);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00b0a840 */
void powers_init(void *obj, PowersWork *w, void *prm, int *k)
{
    PowersRec *typ = (PowersRec *)&powers_types[*k];
    Vec3 v;
    BackFireLink lnk;
    ModelPart *gfx;
    ModelPart *body, *legs, *hatch, *wire_part;
    int i;
    int n;

    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((ObjMT *)obj)->models = m_;
        ((ObjMT *)obj)->tex = t_;
    } while (0);
    body = &w->parts[0];
    legs = &w->parts[1];
    hatch = &w->parts[2];
    wire_part = &w->parts[3];
    dc_object_init(body, *((struct DcNode **)pnb_get_entry(stage_models, 47)), *((struct DcClip **)pnb_get_entry(stage_models, 48)), 4096);
    ((ModelPartAnim *)&w->parts[0])->time = ((ModelPartAnim *)&w->parts[0])->length * RAND_FLOAT_A();
    dc_object_init(legs, *((struct DcNode **)pnb_get_entry(stage_models, 57)), *((struct DcClip **)pnb_get_entry(stage_models, 60)), 4096);
    dc_object_rebuild(legs);
    dc_object_init(hatch, *((struct DcNode **)pnb_get_entry(stage_models, 62)), *((struct DcClip **)pnb_get_entry(stage_models, 63)), 0);
    dc_object_rebuild(hatch);
    dc_object_init(wire_part, *((struct DcNode **)pnb_get_entry(stage_models, 50)), *((struct DcClip **)pnb_get_entry(stage_models, 56)), 0);
    dc_object_set_wire(wire_part, model_pnb_get_wire(((void **)obj)[0], 50));
    gfx = enemy_task_gfx_ptr(obj);
    gfx->radius = w->parts[3].radius = 65.0f;
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), powers_lock_name);
    enemy_task_set_scl(obj, &powers_init_scale);
    w->state = 0;
    w->count = 1;
    w->life = k[1];
    enemy_task_get_pos(obj, &w->pos);
    enemy_task_get_param_vec(obj, &v);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
        w->ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    w->ang[2] = 0;
    n = k[2];
    w->k2 = n;
    w->k2pos = 0 < n;
    w->beats = typ->die_beat;
    powers_update_nodes(obj, w);
    powers_clear_damage(obj, w);
    nj_fill8(w->fall_frames, 0, 16);
    heap_bzero(&lnk, sizeof lnk);
    lnk.gfx = legs;
    lnk.flags = 0x1000000;
    lnk.obj_scale = enemy_task_scl_ptr(obj);
    lnk.use_pose = 0;
    for (i = 0; i < 2; i++) {
        lnk.pos = &w->lp[i];
        lnk.ang = w->la[i];
        en_backfire_create(obj, &lnk, 0);
    }
}

/* 0x00b0abe0 */
void powers_release(void *obj, PowersWork *w)
{
    dc_object_release(&w->parts[0]);
    dc_object_release(&w->parts[1]);
    dc_object_release(&w->parts[2]);
    dc_object_release(&w->parts[3]);
}

/* 0x00b0ac20 */
int powers_next_state(void *obj, TypeEnemyWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00b0ac50 */
void powers_path_pos(void *obj, PowersWork *w, Vec3 *pos, int *ang)
{
    ModelPart *part = &w->parts[0];
    float frm = (int)enemy_task_get_param(obj, 0);
    Vec3 ppos;
    int pang[3];
    int buf[4];
    void *arg[3];
    float sc[2][2];
    float x, y, z;

    arg[0] = pos;
    arg[1] = ang;
    arg[2] = buf;
    anim_eval_hierarchy(part->root, part->clip, arg, part->frame);
    player_pass_pos_a(&ppos, frm);
    player_pass_rot_a(pang, frm);
    pos->x += ppos.x;
    pos->y += ppos.y;
    pos->z += ppos.z;
    ang[0] += pang[0];
    ang[1] += pang[1];
    ang[2] += pang[2];
    x = enemy_task_get_param(obj, 2);
    y = enemy_task_get_param(obj, 3);
    z = enemy_task_get_param(obj, 4);
    ROT_X(pang[0], sc[0], y, z);
    ROT_Y(pang[1], sc[1], x, z);
    pos->x += x;
    pos->y += y;
    pos->z += z;
}

/* 0x00b0ae18 */
void powers_aim_angles(void *obj, PowersWork *w, int *ang)
{
    Vec3 p;
    Vec3 pl;
    Vec3 to_pl;
    int pr[3];
    float sc[2][2];

    enemy_task_get_pos(obj, &p);
    player_get_pos(&pl);
    v3_sub(&to_pl, &pl, &p);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-to_pl.x, -to_pl.z));
        ang[0] = RAD_TO_ANG(atan2f(to_pl.y, sqrtf(to_pl.x * to_pl.x + to_pl.z * to_pl.z)));
    } while (0);
    player_get_rot(pr);
    ROT_Y(-pr[1], sc[0], to_pl.x, to_pl.z);
    ROT_X(-pr[0], sc[1], to_pl.y, to_pl.z);
    if (0.0f < to_pl.x)
        ang[2] = w->type->attack_roll_ang;
    else if (to_pl.x < 0.0f)
        ang[2] = -w->type->attack_roll_ang;
}

/* 0x00b0b008 */
void powers_state_appear(void *obj, PowersWork *w)
{
    int n = w->type->appear_sec * FPS;
    float rate = w->count / (float)n;
    int ang[3];
    Vec3 p;
    Vec3 v;
    int da[3];
    float sc[2];
    float k;
    Vec3 *kept = &w->pos;

    enemy_task_get_param_vec(obj, &v);
    v3_add(kept, &v);
    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    k = 1.0f - sc[0];
    p.x = p.x * sc[0] + kept->x * k;
    p.y = p.y * sc[0] + kept->y * k;
    p.z = p.z * sc[0] + kept->z * k;
    enemy_task_set_pos(obj, &p);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    powers_next_state(obj, (TypeEnemyWork *)w, n, 1);
}

/* steer(cast) */
/* 0x00b0b200 */
void powers_state_move(void *obj, PowersWork *w)
{
    PowersRec *typ = w->type;
    int n = enemy_task_get_param(obj, 1);
    Vec3 p;
    int ang[3];
    int da[3];
    Vec3 pl;
    int pr[3];
    Vec3 ofs;
    float sc[2][2];

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    if (w->k2 > 0) {
        if (powers_next_state(obj, (TypeEnemyWork *)w, n, 4)) {
            dc_object_set_clip(&w->parts[1], *((struct DcClip **)pnb_get_entry(stage_models, 59)));
            w->parts[1].frame = 0;
        }
    } else if (powers_next_state(obj, (TypeEnemyWork *)w, n, 2)) {
        player_get_pos(&pl);
        player_get_rot(pr);
        v3_sub(&ofs, &p, &pl);
        ROT_Y(-pr[1], sc[0], ofs.x, ofs.z);
        ROT_X(-pr[0], sc[1], ofs.y, ofs.z);
        ofs.y = 0;
        ofs.z = 0;
        vec3_normalize(&ofs, &ofs);
        ofs.z = typ->disappear_back_spd;
        enemy_task_set_param_vec(obj, &ofs);
    }
}

/* 0x00b0b420 */
void powers_state_disappear(void *obj, PowersWork *w)
{
    PowersRec *typ = w->type;
    int n = typ->disappear_sec * FPS;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int da[3];
    int pr[3];
    Vec3 v;
    float sc[2][2];
    float k;

    dc_object_next_time(&w->parts[0], w->parts[0].frame + w->parts[0].speed * (1.0f - rate));
    powers_path_pos(obj, w, &p, ang);
    enemy_task_get_param_vec(obj, &v);
    if (0.0f < v.x)
        ang[2] -= typ->disappear_ang * safe_powf(rate, typ->disappear_ang_pow);
    else if (v.x < 0.0f)
        ang[2] += typ->disappear_ang * safe_powf(rate, typ->disappear_ang_pow);
    player_get_rot(pr);
    ROT_X(pr[0], sc[0], v.y, v.z);
    ROT_Y(pr[1], sc[1], v.x, v.z);
    k = typ->disappear_acc * 0.5f * (typ->disappear_sec * rate) * (typ->disappear_sec * rate);
    v3_scale(&v, k);
    v3_add(&p, &v);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    powers_next_state(obj, (TypeEnemyWork *)w, n, 3);
}

/* 0x00b0b6e0 */
void powers_state_gone(void *obj, PowersWork *w)
{
    PowersRec *typ = w->type;
    Vec3 p;
    int ang[3];
    int pr[3];
    int da[3];
    Vec3 v;
    float sc[2][2];
    float k;

    powers_path_pos(obj, w, &p, ang);
    enemy_task_get_param_vec(obj, &v);
    if (0.0f < v.x)
        ang[2] -= typ->disappear_ang;
    else if (v.x < 0.0f)
        ang[2] += typ->disappear_ang;
    player_get_rot(pr);
    ROT_X(pr[0], sc[0], v.y, v.z);
    ROT_Y(pr[1], sc[1], v.x, v.z);
    k = typ->disappear_acc * 0.5f * (typ->disappear_sec + w->count / FPS) * (typ->disappear_sec + w->count / FPS);
    v3_scale(&v, k);
    v3_add(&p, &v);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    w->count++;
}

/* steer(cast) */
/* 0x00b0b910 */
void powers_state_transform(void *obj, PowersWork *w)
{
    int n = w->type->trans_start_sec * FPS;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int da[3];

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    gfx->frame = gfx->frames * rate;
    w->parts[1].frame = w->parts[1].frames * rate;
    powers_update_nodes(obj, w);
    if (powers_next_state(obj, (TypeEnemyWork *)w, n, 5)) {
        ModelPart *legs = &w->parts[1];
        Vec3 *v = &w->hitv;

        player_get_vec(v);
        v->x = -v->x;
        v->y = -v->y;
        v->z = -v->z;
        powers_add_swing_motion(obj, w);
        dc_object_set_clip(legs, *((struct DcClip **)pnb_get_entry(stage_models, 60)));
        w->parts[1].frame = 0;
    }
}

/* 0x00b0bab0 */
void powers_state_turn(void *obj, PowersWork *w)
{
    int n = w->type->dir_sec * FPS;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int aim[3];
    int da[3];

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    powers_aim_angles(obj, w, aim);
    yuki_lerp3(ang, ang, aim, rate);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    powers_next_state(obj, (TypeEnemyWork *)w, n, 6);
}

/* steer(cast) */
/* 0x00b0bbf8 */
void powers_state_open(void *obj, PowersWork *w)
{
    int n = w->type->open_sec * FPS;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int da[3];
    ModelPart *gfx;

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_aim_angles(obj, w, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    w->parts[2].frame = w->parts[2].frames * rate;
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    if (powers_next_state(obj, (TypeEnemyWork *)w, n, 7)) {
        gfx = enemy_task_gfx_ptr(obj);
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 52)));
        gfx->frame = 0;
        dc_object_set_clip(&w->parts[3], *((struct DcClip **)pnb_get_entry(stage_models, 53)));
    }
}

/* steer(cast) */
/* 0x00b0bd80 */
void powers_state_shot(void *obj, PowersWork *w)
{
    PowersRec *typ = w->type;
    int n = typ->shot_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    int da[3];
    Vec3 bpos[2];
    int bang[2][3];
    float sc_0_0, sc_0_1, sc_1_0, sc_1_1, sc_2_0, sc_2_1;
    Vec3 v;
    float x, y, z;

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_aim_angles(obj, w, ang);
    powers_apply_motions(obj, w, da);
    {
        int i;
        int j;

        ang[0] += da[0];
        ang[1] += da[1];
        ang[2] += da[2];
        enemy_task_set_pos(obj, &p);
        enemy_task_set_rot(obj, ang);
        if (w->k2pos > 0)
            gfx->frame = gfx->frames * rate;
        powers_update_nodes(obj, w);
        dc_object_advance_time(&w->parts[1]);
        if (35.0f < gfx->frame && w->k2) {
            w->k2 = ((int)w & 1) >> 1; /* steer(zero) */
            for (i = 0; i < 2; i++) {
                powers_update_bullet_nodes(bpos, bang[0], &w->tp[i], w->ta[i], obj, w);
                for (j = 0; j < 2; j++) {
                    x = 0.0f;
                    y = typ->bullet_spd * (1.0f / FPS);
                    z = x;
                    ROTS_Z(bang[j][2], sc_0_0, sc_0_1, x, y);
                    ROTS_X(bang[j][0], sc_1_0, sc_1_1, y, z);
                    ROTS_Y(bang[j][1], sc_2_0, sc_2_1, x, z);
                    v.x = x;
                    v.y = y;
                    v.z = z;
                    macros_missile_create(obj, &bpos[j], &v, typ->bullet_act_sec, typ->bullet_act_chg_sec);
                }
            }
        }
    }
    if (powers_next_state(obj, (TypeEnemyWork *)w, n, 8)) {
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 55)));
        {
            void *m_ = stage_models;

            gfx->frame = gfx->frames;
            dc_object_set_clip(&w->parts[3], *((struct DcClip **)pnb_get_entry(m_, 56)));
        }
    }
}

/* 0x00b0c120 */
void powers_state_wait(void *obj, PowersWork *w)
{
    int n = w->type->wait_sec * FPS;
    Vec3 p;
    int ang[3];
    int da[3];

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_aim_angles(obj, w, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    powers_next_state(obj, (TypeEnemyWork *)w, n, 9);
}

/* steer(cast) */
/* 0x00b0c228 */
void powers_state_close(void *obj, PowersWork *w)
{
    int n = w->type->close_sec * FPS;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int da[3];

    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    powers_aim_angles(obj, w, ang);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    w->parts[2].frame = w->parts[2].frames * (1.0f - rate);
    powers_update_nodes(obj, w);
    dc_object_advance_time(&w->parts[1]);
    if (powers_next_state(obj, (TypeEnemyWork *)w, n, 10)) {
        i3_copy(w->ang, ang);
        dc_object_set_clip(&w->parts[1], *((struct DcClip **)pnb_get_entry(stage_models, 59)));
        w->parts[1].frame = w->parts[1].frames;
    }
}

/* steer(cast) */
/* 0x00b0c3c0 */
void powers_state_return(void *obj, PowersWork *w)
{
    PowersRec *typ = w->type;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int n = typ->trans_end_sec * FPS;
    float rate = w->count / (float)n;
    Vec3 p;
    int ang[3];
    int da[3];
    Vec3 pl;
    int pr[3];
    Vec3 ofs;
    float sc[2][2];

    gfx->frame = gfx->frames * (1.0f - rate);
    w->parts[1].frame = w->parts[1].frames * (1.0f - rate);
    dc_object_advance_time(&w->parts[0]);
    powers_path_pos(obj, w, &p, ang);
    yuki_lerp3(ang, w->ang, ang, rate);
    powers_apply_motions(obj, w, da);
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    powers_update_nodes(obj, w);
    if (powers_next_state(obj, (TypeEnemyWork *)w, n, 2)) {
        dc_object_set_clip(&w->parts[1], *((struct DcClip **)pnb_get_entry(stage_models, 60)));
        w->parts[1].frame = 0;
        player_get_pos(&pl);
        player_get_rot(pr);
        v3_sub(&ofs, &p, &pl);
        ROT_Y(-pr[1], sc[0], ofs.x, ofs.z);
        ROT_X(-pr[0], sc[1], ofs.y, ofs.z);
        ofs.y = 0;
        ofs.z = 0;
        vec3_normalize(&ofs, &ofs);
        ofs.z = typ->disappear_back_spd;
        enemy_task_set_param_vec(obj, &ofs);
    }
}

/* 0x00b0c608 */
void powers_update(void *obj, TypeEnemyWork *w)
{
    void (*fn)(void *obj, TypeEnemyWork *w) = powers_states[w->state];

    if (fn)
        fn(obj, w);
}

/* 0x00b0c648 */
void powers_die(void *obj, PowersWork *w)
{
    Vec3 p;
    Vec3 lp0[2];
    Vec3 tp0[2];
    int ang[3];
    int da[3];
    Vec3 v;
    Vec3 mv;
    Vec3 fwd;
    Vec3 down;
    Vec3 fall;
    SpawnDesc desc;
    PowersRec *typ = w->type;
    int *cnt = w->fall_frames;
    int *cnt_p = cnt;
    float rate, prev_sec, drop, floor_h;
    int i;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    player_get_frame_accel(&fwd);
    v3_scale(&fwd, typ->die_back_rate);
    v3_sub(&p, &p, &fwd);
    do {
        if (map_get_current() == 20)
            v3_set(&down, 0.0f, 0.0f, 1.0f);
        else
            v3_set(&down, 0.0f, -1.0f, 0.0f);
    } while (0);
    rate = w->count / FPS;
    prev_sec = (w->count - 1) / FPS;
    drop = typ->fall_acc * 0.5f * (rate * rate - prev_sec * prev_sec);
    v3_mul(&v, &down, drop);
    v3_add(&p, &v);
    floor_h = stage_ground_height(&p) + ((ModelPart *)enemy_task_gfx_ptr(obj))->radius;
    if (p.y < floor_h) {
        p.y = floor_h;
        w->beats = 0;
    }
    enemy_task_get_rot(obj, ang);
    ang[2] += typ->die_roll / FPS;
    powers_apply_motions(obj, w, da);
    da[0] *= typ->die_damage_motion;
    da[1] *= typ->die_damage_motion;
    da[2] *= typ->die_damage_motion;
    ang[0] += da[0];
    ang[1] += da[1];
    ang[2] += da[2];
    dc_object_advance_time(&w->parts[1]);
    enemy_task_set_pos(obj, &p);
    enemy_task_set_rot(obj, ang);
    v3_mul(&mv, &v, typ->die_back_rate_parts);
    player_get_frame_vel_old(&fwd);
    v3_add(&mv, &fwd);
    nj_copy8(lp0, w->lp, sizeof lp0);
    nj_copy8(tp0, w->tp, sizeof tp0);
    powers_update_nodes(obj, w);
    for (i = 0; i < 2; i++, cnt_p++) {
        if (*cnt_p == 0)
            v3_add(&w->lp[i], &v);
        else {
            float rate, prev_sec, drop, floor_h;
            rate = *cnt_p / FPS;
            prev_sec = (*cnt_p - 1) / FPS;
            v3_sum(&w->lp[i], &lp0[i], &mv);
            drop = typ->fall_acc_parts * 0.5f * (rate * rate - prev_sec * prev_sec);
            v3_mul(&fall, &down, drop);
            v3_add(&w->lp[i], &fall);
            floor_h = stage_ground_height(&w->lp[i]) + w->parts[1].radius;
            if (w->lp[i].y < floor_h) {
                w->lp[i].y = floor_h;
                w->beats = 0;
            }
            (*cnt_p)++;
        }
    }
    for (i = 0; i < 2; i++, cnt_p++) {
        if (*cnt_p == 0)
            v3_add(&w->tp[i], &v);
        else {
            float rate, prev_sec, drop, floor_h;
            rate = *cnt_p / FPS;
            prev_sec = (*cnt_p - 1) / FPS;
            v3_sum(&w->tp[i], &tp0[i], &mv);
            drop = typ->fall_acc_parts * 0.5f * (rate * rate - prev_sec * prev_sec);
            v3_mul(&fall, &down, drop);
            v3_add(&w->tp[i], &fall);
            floor_h = stage_ground_height(&w->tp[i]) + w->parts[2].radius;
            if (w->tp[i].y < floor_h) {
                w->tp[i].y = floor_h;
                w->beats = 0;
            }
            (*cnt_p)++;
        }
    }
    w->count++;
    if (beat_grid_hit(0)) {
        w->beats--;
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 105;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 2);
        {
            int slot = rand_lcg_next_b() % 4;
            w->fall_frames[slot] = 1;
        }
    }
    if (w->beats <= 0)
        enemy_task_next_phase(obj);
}

/* 0x00b0cd78 */
void powers_exec_appear(void *obj, TypeEnemyWork *w)
{
    enemy_task_appear_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        w->frames = 0;
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        powers_update(obj, w);
    }
}

/* 0x00b0ce10 */
void powers_exec_normal(void *obj, ObjLifeWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    powers_update(obj, (TypeEnemyWork *)w);
    if (w->life > 0 && --w->life <= 0)
        enemy_task_set_state(obj, 2);
}

/* 0x00b0ce78 */
void powers_exec_leave(void *obj, TypeEnemyWork *w)
{
    enemy_task_vanish_scale(obj, w);
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        powers_update(obj, w);
    }
}

/* steer(cast) */
/* 0x00b0cef8 */
void powers_exec_destroyed(void *obj, PowersWork *w)
{
    SpawnDesc desc;
    Vec3 v;
    int i;

    switch (enemy_task_get_phase(obj)) {
    case 0:
        w->count = 1;
        dc_object_set_clip(&w->parts[1], *((struct DcClip **)pnb_get_entry(stage_models, 61)));
        w->parts[1].frame = 0;
        dc_object_clear_flags(&w->parts[1], 4096);
        break;
    case 2:
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 54;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_param_vec(obj, &v);
        v3_copy(&desc.v, &v);
        en_shatter_spawn(obj, &desc, 2);
        desc.type = 58;
        for (i = 0; i < 2; i++) {
            v3_copy(&desc.pos, &w->lp[i]);
            i3_copy(desc.ang, w->la[i]);
            en_shatter_spawn(obj, &desc, 2);
        }
        desc.type = 64;
        for (i = 0; i < 2; i++) {
            v3_copy(&desc.pos, &w->tp[i]);
            i3_copy(desc.ang, w->ta[i]);
            en_shatter_spawn(obj, &desc, 2);
        }
        FINISH_IF(obj);
        break;
    default:
        powers_die(obj, w);
    }
}

/* 0x00b0d108 */
void powers_pre_exec(void *obj, PowersWork *w)
{
    Vec3 org;

    player_get_frame_vel(&org);
    v3_add(&w->pos, &org);
}

/* 0x00b0d168 */
void powers_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (powers_exec_table[i])
        powers_exec_table[i](obj, w);
}

/* 0x00b0d1c0 */
void powers_post_exec(void *obj, PowersWork *w)
{
    Vec3 vpos;
    SpawnDesc desc;
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (!enemy_status_get_ticks())
        powers_update_nodes(obj, w);
    if (lock->flags & LOCK_HIT) {
        enemy_task_get_pos(obj, &vpos);
        ulMatrixApplyPoint((float *)ulMatrixCur, &vpos.x, &vpos.x);
        project_point(&vpos, &vpos);
        switch (lock->life) {
        case 0:
            SoundRhythmSe_EnemyPos(powers_init_arg, 1, &vpos.x);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(powers_init_arg, 3, &vpos.x);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(powers_init_arg, 2, &vpos.x);
            break;
        case 3:
            SoundRhythmSe_EnemyPos(powers_init_arg, 0, &vpos.x);
            break;
        }
        v3_copy(&w->hitv, &lock->imp.v);
        powers_add_damage_motion(obj, w);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 105;
        desc.models = stage_models;
        desc.kind = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        en_shatter_spawn(obj, &desc, 2);
    }
}

/* 0x00b0d388 */
int powers_backfire_node_cb(unsigned int *n)
{
    Vec3 v;
    Vec3 scl;

    if (n[0] & 0x1000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &powers_backfire_root.x);
        v3_set(&v, 0, 0, powers_backfire_work->type->leg_backfire_len);
        yuki_matrix_get_scale(&scl);
        powers_backfire_work->fscale = scl.x;
        ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &powers_backfire_tip.x);
    }
    return 0;
}

/* 0x00b0d418 */
void powers_draw_legs(void *obj, PowersWork *w)
{
    ModelPart *part = &w->parts[1];

    powers_backfire_work = w;
    dc_object_set_callback(part, powers_backfire_node_cb, 0);
    ulMatrixPush();
    matrix_unit_set(0);
    dc_object_draw(part);
    ulMatrixPop();
    dc_object_set_callback(part, 0, 0);
}

/* 0x00b0d480 */
void powers_draw_band(Vec3 *from, Vec3 *to, float u0, float u1, float width)
{
    UvVtx v[4];
    float x0, y0, z0, x1, y1, z1, dx, dy, len, iz0, iz1;

    do {
        x0 = from->x;
        y0 = from->y;
        z0 = from->z;
    } while (0);
    do {
        x1 = to->x;
        y1 = to->y;
        z1 = to->z;
    } while (0);
    iz0 = 1.0f / z0;
    iz1 = 1.0f / z1;
    dx = y0 - y1;
    dy = x1 - x0;
    len = sqrtf(dx * dx + dy * dy);
    len = 1.0f / len * width;
    dy *= len;
    dx *= len;
    do {
        v[0].x = x0 + dx;
        v[0].y = y0 + dy;
        v[0].q = iz0;
        v[0].t = 0.0f;
        v[0].s = u0;
        v[0].c = RGBA(0x60, 0x60, 0x60, 0x80);
    } while (0);
    do {
        v[1].x = x1 + dx;
        v[1].y = y1 + dy;
        v[1].q = iz1;
        v[1].t = 0.0f;
        v[1].s = u1;
        v[1].c = RGBA(0x60, 0x60, 0x60, 0x80);
    } while (0);
    do {
        v[2].x = x0 - dx;
        v[2].y = y0 - dy;
        v[2].q = iz0;
        v[2].t = 1.0f;
        v[2].s = u0;
        v[2].c = RGBA(0x60, 0x60, 0x60, 0x80);
    } while (0);
    do {
        v[3].x = x1 - dx;
        v[3].y = y1 - dy;
        v[3].q = iz1;
        v[3].t = 1.0f;
        v[3].s = u1;
        v[3].c = RGBA(0x60, 0x60, 0x60, 0x80);
    } while (0);
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip((unsigned int *)v, 4);
    ulGxEndTexStrip();
}

/* 0x00b0d628 */
void powers_draw_backfire(float width)
{
    Vec3 root, tip;
    float k[3];
    float zn, cut, u0, u1, ax, ay, az, bx, by, bz;
    int dl;
    void *fog;

    dl = RenderSetCurrentDisplayList(6);
    fog = fog_set_enable(0);
    u0 = 0;
    u1 = 1.0f;
    draw_set_blend(4, 1);
    ulGxSetTextureList(stage_textures);
    ulGxSelectTexture(ulGxGetTextureList(), 1);
    ulMatrixApplyPoint((float *)ulMatrixCur, &powers_backfire_root.x, &root.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &powers_backfire_tip.x, &tip.x);
    k[0] = width;
    k[2] = root.z;
    k[1] = u0;
    project_point_abs(k, k);
    width = k[0];
    do {
        ax = root.x;
        ay = root.y;
        az = root.z;
    } while (0);
    do {
        Vec3 *v_ = &tip;
        bx = v_->x;
        by = v_->y;
        bz = v_->z;
    } while (0);
    zn = -cam_near;
    if (az < zn && bz < zn) {
        if (zn < az) {
            cut = (zn - az) / (bz - az);
            az = zn;
            ax += (bx - ax) * cut;
            ay += (by - ay) * cut;
            u0 = cut + u0;
        } else if (zn < bz) {
            cut = (zn - bz) / (az - bz);
            bz = zn;
            bx += (ax - bx) * cut;
            by += (ay - by) * cut;
            u1 = -cut + u1;
        }
        project_xyz(ax, ay, az, &root.x);
        project_xyz(bx, by, bz, &tip.x);
        if (yuki_is_offscreen(&root.x) || yuki_is_offscreen(&tip.x)) {
            fog_set_enable(fog);
            return;
        }
        powers_draw_band(&root, &tip, u0, u1, width);
    }
    fog_set_enable(fog);
    RenderSetCurrentDisplayList(dl);
    ulGxSetTextureList(stage_textures);
}

/* 0x00b0d880 */
void powers_disp_body(void *obj, PowersWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 scl;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    PowersRec *typ;
    int i;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        powers_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
        dc_object_draw(gfx);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f, typ->wire_near,
                                           typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            model_object_draw(&w->parts[3], gfx->frame);
        }
        ulMatrixPop();
        powers_draw_legs(obj, w);
        for (i = 0; i < 2; i++) {
            ulMatrixPush();
            do {
                Vec3 *p_ = &w->lp[i];
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            powers_rotate_yxz(w->la[i]);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
            dc_object_draw(&w->parts[1]);
            powers_draw_backfire(w->fscale * w->type->leg_backfire_width);
            ulMatrixPop();
        }
        for (i = 0; i < 2; i++) {
            ulMatrixPush();
            do {
                Vec3 *p_ = &w->tp[i];
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            powers_rotate_yxz(w->ta[i]);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
            dc_object_draw(&w->parts[2]);
            ulMatrixPop();
        }
    }
}

/* 0x00b0dae8 */
void powers_disp(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (powers_disp_table[i])
        powers_disp_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b0db40 */
int powers_create(void *obj, ShotArg *place)
{
    int k[3];
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)powers_funcs;
    PowersRec *typ = (PowersRec *)powers_types;
    float delay = place->delay;

    k[0] = 0;
    k[2] = ((float (*)(float))float_frac)(delay) * 1000.0f + 0.5f;
    k[1] = (int)((typ->appear_sec + typ->disappear_sec) * FPS + delay * 60.0f / 60.0f)
           + (typ->trans_start_sec + typ->trans_end_sec + typ->dir_sec + typ->open_sec + typ->shot_sec + typ->wait_sec
              + typ->close_sec) * FPS;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = powers_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = k;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 49;
    desc.model_no2 = 55;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = k[1];
    desc.size = 992;
    return enemy_task_task_create(obj, &desc);
}

/* 0x00b0dd38 */
void powers_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
