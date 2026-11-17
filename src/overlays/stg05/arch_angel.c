/*
 * Archangel, Area 5 enemy: ring craft with a core and two side cores; launches its children
 * (arch_angel_child) in waves, turns away and back, then leaves. Shot-down leaders score bonuses;
 * dies in a chain of explosions (en_aangel_exp).
 */

#include "stg05.h"
#include "mathdef.h"
#include "camera.h"
#include "camera_demo.h"
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
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "arch_angel_child.h"
#include "en_aangel_exp.h"
#include "stage05.h"
#include "../common/enemy_vars.h"

extern int arch_angel_init_arg;
extern void (*arch_angel_exec_table[])(void *o, TypeEnemyWork *w);
void arch_angel_vars_register(void);
void arch_angel_vars_unregister(void);
int arch_angel_vars_init(int *prm);
int arch_angel_vars_release(void);
void arch_angel_exec(void *obj, TypeEnemyWork *w);
void arch_angel_rotate_yxz(int *ang);

/* 256 bytes; names in arch_angel_vardefs */
typedef struct ArchAngelRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08: of the core */
    int hp_dummy;               /* 0x0c: of the two side cores */
    int same_lock;              /* 0x10 */
    int same_lock_dummy;        /* 0x14 */
    int score;                  /* 0x18 */
    float hp_reduce;            /* 0x1c */
    float crash_r;              /* 0x20 */
    float crash_damage;         /* 0x24 */
    float start_sec;            /* 0x28 */
    float start_motion_rate;    /* 0x2c */
    float open_sec;             /* 0x30 */
    float normal_sec;           /* 0x34 */
    float close_sec;            /* 0x38 */
    float turn_sec;             /* 0x3c */
    float child_shot_wait_sec;  /* 0x40 */
    float child_spd;            /* 0x44 */
    float child_act_sec;        /* 0x48 */
    float child_act_chg_sec;    /* 0x4c */
    int roll_spd;               /* 0x50 */
    int roll_spd_core;          /* 0x54 */
    int roll_spd_core_add;      /* 0x58: per hit taken */
    int roll_spd_core_add_dmg;  /* 0x5c */
    int roll_spd_dcore;         /* 0x60 */
    int roll_spd_dcore_add;     /* 0x64 */
    int roll_spd_dcore_add_dmg; /* 0x68 */
    int roll_spd_pole;          /* 0x6c */
    float explode_range;        /* 0x70 */
    int explode_num;            /* 0x74 */
    int explode_num_max;        /* 0x78 */
    float explode_dif_sec;      /* 0x7c */
    int child_stock_num;        /* 0x80 */
    float time_rate1;           /* 0x84: the turn's ease curve (arch_angel_turn_ease) */
    float time_rate2;           /* 0x88 */
    float time_rate3;           /* 0x8c */
    float time_rate4;           /* 0x90 */
    float time_rate5;           /* 0x94 */
    float move_rate2;           /* 0x98 */
    float move_rate3;           /* 0x9c */
    float die_sec;              /* 0xa0 */
    int die_ang_spd;            /* 0xa4 */
    int die_ang_spd_z;          /* 0xa8 */
    float die_fall_acc;         /* 0xac */
    int leader_num;             /* 0xb0 */
    int leader_id[5];           /* 0xb4: children spawned as leaders */
    int leader_score[5];        /* 0xc8: by leaders killed */
    int wire;                   /* 0xdc */
    int wire_linear;            /* 0xe0 */
    unsigned int wire_col;      /* 0xe4 */
    unsigned int wire_col2;     /* 0xe8 */
    float wire_near;            /* 0xec */
    float wire_far;             /* 0xf0 */
    float wire_near_rate;       /* 0xf4 */
    float wire_far_rate;        /* 0xf8 */
    int wire_beat;              /* 0xfc */
} ArchAngelRec;

/* 1088 bytes */
typedef struct ArchAngelWork {
    ArchAngelRec *type;
    int state;                  /* 0x04: indexes arch_angel_states */
    int count;                  /* 0x08: frames of the current state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    int phase;                  /* 0x18: close states passed */
    LockObj lock;                  /* 0x1c: the core */
    LockObj lk[2];                 /* 0x5c: the two side cores */
    Vec3 lpos[2];               /* 0xdc: their positions (model nodes of flag 0x1000000) */
    int ang[3];                 /* 0xf4: the angles at the start of the state */
    int nchild;                 /* 0x100 */
    int maxchild;               /* 0x104 */
    void **child;               /* 0x108: the ArchAngelChild works */
    int wait;                   /* 0x10c: frames to the next launch */
    int spawned;                /* 0x110 */
    int base[3];                /* 0x114: the creation angles */
    int nexp;                   /* 0x120: explosions running */
    int unk_124;                /* 0x124: set to 0 by arch_angel_init (next to nexp/stock), never read */
    int stock;                  /* 0x128: children left to launch */
    int life;                   /* 0x12c: frames */
    int roll;                   /* 0x130 */
    int roll_core;              /* 0x134 */
    int roll_pole;              /* 0x138 */
    int roll_dcore[2];          /* 0x13c */
    int flash;                  /* 0x144: cleared by arch_angel_disp */
    ModelPart parts[8];            /* 0x148: models 83/84, 85, 93, 86, 94, 87, 81/82, 74 */
    int ang0[3];                /* 0x428 */
    int kills;                  /* 0x434: leader children killed */
    int lands;                  /* 0x438 */
    int se;                     /* 0x43c: the open sound was started */
} ArchAngelWork;

extern void (*arch_angel_states[])(void *o, TypeEnemyWork *w);

void arch_angel_prune_children(void *obj, TypeEnemyWork *w);
void arch_angel_update(void *obj, TypeEnemyWork *w);

/* group name, label, 62 values */
typedef struct ArchAngelSet {
    char *name;
    char *label;                /* 0x04 */
    int v[62];                  /* 0x08 */
} ArchAngelSet;

extern ArchAngelSet arch_angel_types[1];
extern VarDef arch_angel_vardef;
extern VarDef arch_angel_vardefs[62];

/* strip drawer screen quad */
typedef struct ScrQuad {
    struct { float x, y, q; unsigned int c; } v[4];
} ScrQuad;

extern char arch_angel_lock_name[];
extern char arch_angel_name[];
extern char str_arch_angel_bonus1[];
extern char str_arch_angel_bonus2[];
extern char str_arch_angel_bonus3[];
extern char str_arch_angel_bonus4[];
extern char str_arch_angel_bonus5[];
extern ScrQuad arch_angel_flash_quad;
extern char arch_angel_funcs[];
extern void (*arch_angel_disp_table[])(void *o, ArchAngelWork *w);
void arch_angel_add_bonus_score(void *obj, ArchAngelWork *w);
void arch_angel_ring_offset(Vec3 *out, Vec3 *dir, int ang, float rad);
float arch_angel_turn_ease(void *obj, ArchAngelWork *w, float x);
void arch_angel_update_core_rolls(void *obj, ArchAngelWork *w);
void arch_angel_launch_children(void *obj, ArchAngelWork *w);
void arch_angel_explode(void *obj, ArchAngelWork *w, int n, Vec3 *pos);
void arch_angel_die(void *obj, ArchAngelWork *w);
int arch_angel_lock_hit(void *obj, ArchAngelWork *w, int *dmg);

/* 0x00af8d88 */
void arch_angel_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&arch_angel_vardef);
    for (i = 0; i < 1; i++) {
        def = arch_angel_vardefs;
        val = arch_angel_types[i].v;
        arch_angel_vardef.name = arch_angel_types[i].name;
        arch_angel_vardef.fmt = arch_angel_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&arch_angel_vardef);
        for (j = 0; j < 62; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)arch_angel_vardefs);
    }
}

/* 0x00af8ea0 */
void arch_angel_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00af8ea8 */
int arch_angel_get_init_arg(void)
{
    return arch_angel_init_arg;
}

/* 0x00af8eb8 */
int arch_angel_vars_init(int *prm)
{
    arch_angel_init_arg = *prm;
    arch_angel_vars_register();
    return 0;
}

/* 0x00af8ee0 */
int arch_angel_vars_release(void)
{
    arch_angel_vars_unregister();
    return 0;
}

/* 0x00af8f00 */
int arch_angel_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 73);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00af8f58 */
void arch_angel_add_bonus_score(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;

    if (w->kills == 1)
        player_status_add_score_event(str_arch_angel_bonus1, typ->leader_score[0]);
    else if (w->kills == 2)
        player_status_add_score_event(str_arch_angel_bonus2, typ->leader_score[1]);
    else if (w->kills == 3)
        player_status_add_score_event(str_arch_angel_bonus3, typ->leader_score[2]);
    else if (w->kills == 4)
        player_status_add_score_event(str_arch_angel_bonus4, typ->leader_score[3]);
    else if (w->kills == 5)
        player_status_add_score_event(str_arch_angel_bonus5, typ->leader_score[4]);
}

/* 0x00af8fe0 */
void arch_angel_ring_offset(Vec3 *out, Vec3 *dir, int rang, float rad)
{
    int ang[3];
    float sc[3][2];
    float z;

    ulSinCos(rang, &sc[0][0], &sc[0][1]);
    z = 0.0f;
    sc[0][1] *= rad;
    sc[0][0] *= rad;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir->x, -dir->z));
        ang[0] = RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    } while (0);
    ROT_X(ang[0], sc[1], sc[0][0], z);
    ROT_Y(ang[1], sc[2], sc[0][1], z);
    {
        float y = sc[0][0];
        out->z = z;
        out->x = sc[0][1];
        out->y = y;
    }
}

/* 0x00af9178 */
float arch_angel_turn_ease(void *obj, ArchAngelWork *w, float x)
{
    ArchAngelRec *typ = w->type;
    float sc[4];

    if (x < typ->time_rate1)
        return 0.0f;
    if (x < typ->time_rate2)
        return typ->move_rate2 * (x - typ->time_rate1) / (typ->time_rate2 - typ->time_rate1);
    if (x < typ->time_rate3)
        return typ->move_rate2;
    if (x < typ->time_rate4) {
        ulSinCos((x - typ->time_rate3) * ANG_HALF / (typ->time_rate4 - typ->time_rate3)
                 - ANG_QUARTER, &sc[0], &sc[1]);
        return typ->move_rate2 + (sc[0] * (typ->move_rate3 - typ->move_rate2) + (typ->move_rate3 - typ->move_rate2)) * 0.5f;
    }
    if (x < typ->time_rate5) {
        ulSinCos((x - typ->time_rate4) * ANG_HALF / (typ->time_rate5 - typ->time_rate4)
                 - ANG_QUARTER, &sc[2], &sc[3]);
        return typ->move_rate3 + (1.0f - typ->move_rate3) * (sc[2] + 1.0f) * 0.5f;
    }
    return 1.0f;
}

/* 0x00af9308 */
void arch_angel_update_core_rolls(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    LockObj *lock = &w->lock;
    int i;

    w->roll_core -= (typ->roll_spd_core + typ->roll_spd_core_add * (lock->life_max - lock->life)) / FPS;
    for (i = 0; i < 2; i++) {
        int *roll = &w->roll_dcore[i];
        LockObj *k = &w->lk[i];

        *roll -= (typ->roll_spd_dcore + typ->roll_spd_dcore_add * (k->life_max - k->life)) / FPS;
    }
}

/* 0x00af93d0 */
float arch_angel_sin_plateau(int ang)
{
    float sc[4];
    float val;

    ang &= 0xffff;
    if (ang < ANG_90)
        val = 1.0f;
    else if (ang <= 0x7fff) {
        ulSinCos(ang, &sc[0], &sc[1]);
        val = sc[0];
    } else if (ang <= 0xbfff) {
        ulSinCos(ang, &sc[2], &sc[3]);
        val = -sc[2];
    } else
        val = 1.0f;
    return val;
}

/* steer(cast) */
/* 0x00af9440 */
void arch_angel_init(void *obj, ArchAngelWork *w, int i, int *k)
{
    ArchAngelRec *typ = (ArchAngelRec *)&arch_angel_types[*k];
    Vec3 p;
    Vec3 v;
    int n = 0;
    ModelPart *gfx = 0;

    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((ObjMT *)obj)->models = m_;
        ((ObjMT *)obj)->tex = t_;
    } while (0);
    dc_object_init(&w->parts[1], *((struct DcNode **)pnb_get_entry(stage_models, 85)), 0, 0);
    dc_object_rebuild(&w->parts[1]);
    dc_object_init(&w->parts[2], *((struct DcNode **)pnb_get_entry(stage_models, 93)), 0, 0);
    dc_object_rebuild(&w->parts[2]);
    dc_object_init(&w->parts[3], *((struct DcNode **)pnb_get_entry(stage_models, 86)), 0, 0);
    dc_object_rebuild(&w->parts[3]);
    dc_object_init(&w->parts[4], *((struct DcNode **)pnb_get_entry(stage_models, 94)), 0, 0);
    dc_object_rebuild(&w->parts[4]);
    dc_object_init(&w->parts[5], *((struct DcNode **)pnb_get_entry(stage_models, 87)), 0, 0);
    dc_object_rebuild(&w->parts[5]);
    {
        int *root = ((int *)pnb_get_entry(stage_models, 83));
        int *clip = ((int *)pnb_get_entry(stage_models, 84));

        dc_object_init(&w->parts[0], *root, *clip, 0);
    }
    dc_object_rebuild(&w->parts[0]);
    {
        int *root = ((int *)pnb_get_entry(stage_models, 81));
        int *clip = ((int *)pnb_get_entry(stage_models, 82));

        dc_object_init(&w->parts[6], *root, *clip, 0);
    }
    dc_object_rebuild(&w->parts[6]);
    dc_object_init(&w->parts[7], *((struct DcNode **)pnb_get_entry(stage_models, 74)), 0, 0);
    dc_object_set_wire(&w->parts[7], model_pnb_get_wire(((void **)obj)[0], 74));
    gfx = enemy_task_gfx_ptr(obj);
    gfx->radius = w->parts[6].radius = 14000.0f;
    w->parts[0].radius = 600.0f;
    lockon_obj_init(&w->lock, obj, enemy_task_pos_ptr(obj), 0, arch_angel_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same_lock, 128);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    for (i = 0; i < 2; i++) {
        Vec3 *lpos = &w->lpos[i];
        LockObj *lock = &w->lk[i];

        lockon_obj_init(lock, obj, lpos, 0, arch_angel_lock_name, enemy_task_get_status_slot(obj), typ->hp_dummy, typ->same_lock_dummy, 128);
        lockon_entry(lock);
        lock->flags &= ~LOCK_LOCKABLE;
    }
    enemy_task_get_param_vec(obj, &v);
    {
        int pitch;

        do {
            w->base[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            pitch = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        w->base[0] = pitch + ANG_90;
        w->base[2] = 0;
    }
    enemy_task_set_rot(obj, w->base);
    i3_copy(w->ang0, w->base);
    enemy_task_get_pos(obj, &p);
    yuki_model_positions_at(w->lpos, &p, w->base, enemy_task_gfx_ptr(obj), 0x1000000, 2);
    n = enemy_task_get_param(obj, 2);
    w->nchild = 0;
    w->wait = 0;
    if (n <= 0)
        n = 1;
    w->maxchild = n;
    w->child = yuki_alloc(n * 4);
    w->spawned = 0;
    heap_bzero(w->child, n * 4);
    w->count = 1;
    w->state = 0;
    w->phase = 0;
    w->nexp = 0;
    w->unk_124 = 0;
    w->stock = typ->child_stock_num;
    w->life = k[1];
    w->roll_core = 0;
    w->roll_pole = 0;
    for (i = 1; i >= 0; i--)
        w->roll_dcore[i] = 0;
    w->kills = 0;
    w->lands = 0;
    w->se = 0;
    oscilate_create(0.05f, 0.05f);
}

/* 0x00af99a0 */
void arch_angel_release(void *obj, ArchAngelWork *w)
{
    int i;

    dc_object_release(&w->parts[1]);
    dc_object_release(&w->parts[2]);
    dc_object_release(&w->parts[3]);
    dc_object_release(&w->parts[4]);
    dc_object_release(&w->parts[5]);
    dc_object_release(&w->parts[0]);
    dc_object_release(&w->parts[7]);
    dc_object_release(&w->parts[6]);
    lockon_remove(&w->lock);
    for (i = 0; i < 2; i++)
        lockon_remove(&w->lk[i]);
    yuki_free(w->child);
    w->child = 0;
}

/* 0x00af9a40 */
void arch_angel_prune_children(void *obj, TypeEnemyWork *work)
{
    ArchAngelWork *w = (ArchAngelWork *)work;
    int i;

    for (i = 0; i < w->nchild; i++) {
        if (w->child[i] && (TASK_OF(w->child[i])->flags & TASK_F_END)) {
            ((union { int n; void *p; void **pp; } *)w->child)[i--].p = w->child[w->nchild - 1];
            w->child[w->nchild - 1] = 0;
            w->nchild--;
        }
    }
}

/* 0x00af9ac8 */
void arch_angel_launch_children(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    Vec3 p;
    Vec3 pl;
    Vec3 to_pl;
    float sc_0, sc_1;
    int i;
    int leader;
    void *parent;

    if (w->nchild < w->maxchild - typ->child_stock_num)
        w->stock = typ->child_stock_num;
    else if (w->stock <= 0)
        return;
    enemy_task_get_pos(obj, &p);
    if (--w->wait > 0)
        return;
    if (w->nchild < w->maxchild - 1) {
        player_get_pos(&pl);
        to_pl.x = pl.x - p.x;
        to_pl.y = 0;
        to_pl.z = pl.z - p.z;
        ROTS_Y((unsigned int)((RAND_FLOAT_A() * 2.0f + -1.0f) * ANG_QUARTER), sc_0, sc_1, to_pl.x,
               to_pl.z);
        vec3_normalize(&to_pl, &to_pl);
        v3_scale(&to_pl, typ->child_spd * (1.0f / FPS));
        w->wait = typ->child_shot_wait_sec * FPS;
        for (i = 0; i < 2; i++) {
            leader = 0;
            if (w->spawned == typ->leader_id[0])
                leader = 1;
            else if (w->spawned == typ->leader_id[1])
                leader = 1;
            else if (w->spawned == typ->leader_id[2])
                leader = 1;
            else if (w->spawned == typ->leader_id[3])
                leader = 1;
            else if (w->spawned == typ->leader_id[4])
                leader = 1;
            else if (w->lands > 0) {
                w->lands--;
                leader = 1;
            }
            parent = TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0;
            w->child[w->nchild++] = (void *)arch_angel_child_create(parent, (int *)&w->lpos[i], (int *)&to_pl,
                                                               (float)w->life - enemy_task_get_time(obj), leader,
                                                               (int)&w->kills, (int)&w->lands, typ->child_act_sec,
                                                               typ->child_act_chg_sec);
            w->stock--;
            w->spawned++;
        }
    }
}

/* 0x00af9db8 */
void arch_angel_explode(void *obj, ArchAngelWork *w, int n, Vec3 *pos)
{
    Vec3 org;
    Vec3 ofs;
    int ang[3];
    float sc_0_0, sc_0_1, sc_1_0, sc_1_1, sc_2_0, sc_2_1, sc_3_0, sc_3_1, sc_4_0, sc_4_1, sc_5_0, sc_5_1;
    int delay = 0;
    ArchAngelRec *typ = w->type;
    int i;
    int rang;

    for (i = 0; i < n; i++) {
        enemy_task_get_pos(obj, &org);
        enemy_task_get_rot(obj, ang);
        v3_copy(&ofs, &w->hitv);
        ROTS_Y(-ang[1], sc_0_0, sc_0_1, ofs.x, ofs.z);
        ROTS_X(-ang[0], sc_1_0, sc_1_1, ofs.y, ofs.z);
        ROTS_Z(-ang[2], sc_2_0, sc_2_1, ofs.x, ofs.y);
        ofs.y = 0.0f;
        vec3_normalize(&ofs, &ofs);
        v3_scale(&ofs, 62.0f);
        ROTS_Z(ang[2], sc_3_0, sc_3_1, ofs.x, ofs.y);
        ROTS_X(ang[0], sc_4_0, sc_4_1, ofs.y, ofs.z);
        ROTS_Y(ang[1], sc_5_0, sc_5_1, ofs.x, ofs.z);
        v3_sub(&org, pos, &ofs);
        rang = DEG_TO_ANG(RAND_FLOAT_B() * 360.0f);
        arch_angel_ring_offset(&ofs, &ofs, rang, RAND_FLOAT_B() * typ->explode_range);
        v3_add(&org, &ofs);
        en_aangel_exp_create(obj, &org, (int)&w->nexp, delay, rand_lcg_next_a() % 2);
        delay = delay + typ->explode_dif_sec * FPS;
    }
}

/* 0x00afa1e0 */
int arch_angel_next_state(void *obj, ArchAngelWork *w, int n, int state)
{
    if (++w->count > n) {
        w->state = state;
        w->count = 1;
        i3_copy(w->ang, w->ang0);
        return 1;
    }
    return 0;
}

/* 0x00afa238 */
void arch_angel_state_approach(void *obj, ArchAngelWork *w)
{
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 v;
    float len, sec, acc, dis;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    len = vec3_length(&v) * FPS;
    sec = w->count / FPS;
    acc = -len / (enemy_task_get_param(obj, 1) / FPS);
    dis = len + acc * sec;
    sec = 1.0f / FPS;
    dis *= sec;
    dis += acc * 0.5f * sec * sec;
    vec3_normalize(&v, &v);
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 1))
        enemy_task_set_state(obj, 0);
}

/* steer(cast) */
/* 0x00afa410 */
void arch_angel_state_start(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    int n = typ->start_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scr;
    float k;
    int i;

    enemy_task_get_pos(obj, &p);
    ang[0] = 0;
    ang[1] = 0;
    ang[2] = 0;
    yuki_lerp3(w->ang0, w->ang, ang, arch_angel_turn_ease(obj, w, rate));
    enemy_task_set_rot(obj, w->ang0);
    if (rate < typ->start_motion_rate) {
        float mrate = rate / typ->start_motion_rate;

        gfx->frame = gfx->frames * mrate;
        w->parts[0].frame = w->parts[0].frames * mrate;
    } else {
        k = (rate - typ->start_motion_rate) / (1.0f - typ->start_motion_rate);
        if (!w->se) {
            ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &scr.x);
            project_point(&scr, &scr);
            SoundRhythmSe_EnemyPos(arch_angel_init_arg, 4, &scr.x);
            w->se = 1;
        }
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 76)));
        gfx->frame = 0.0f;
        w->parts[0].frame = w->parts[0].frames;
        w->lock.flags |= LOCK_LOCKABLE;
        w->roll += typ->roll_spd / FPS * k;
        w->roll_pole += typ->roll_spd_pole / FPS * k;
        gfx->frame = gfx->frames * k;
    }
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, w->ang0, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 3)) {
        for (i = 0; i < 2; i++) {
            LockObj *lock = &w->lk[i];
            lock->flags |= LOCK_LOCKABLE;
        }
    }
}

/* 0x00afa6b0 */
void arch_angel_state_open(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    int n = typ->open_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    int i;

    enemy_task_get_pos(obj, &p);
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    gfx->frame = gfx->frames * rate;
    w->roll += typ->roll_spd / FPS * rate;
    w->roll_pole += typ->roll_spd_pole / FPS * rate;
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 3)) {
        for (i = 0; i < 2; i++) {
            LockObj *lock = &w->lk[i];
            lock->flags |= LOCK_LOCKABLE;
        }
    }
}

/* 0x00afa870 */
void arch_angel_state_normal(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    int n = typ->normal_sec * FPS;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    int i;

    enemy_task_get_pos(obj, &p);
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    w->roll += typ->roll_spd / FPS;
    w->roll_pole += typ->roll_spd_pole / FPS;
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    arch_angel_launch_children(obj, w);
    if (arch_angel_next_state(obj, w, n, 4)) {
        for (i = 0; i < 2; i++) {
            LockObj *lock = &w->lk[i];
            lock->flags &= ~LOCK_LOCKABLE;
        }
    }
}

/* steer(cast) */
/* 0x00afaa08 */
void arch_angel_state_close(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    int n = typ->close_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    gfx->frame = gfx->frames * (1.0f - rate);
    w->roll += typ->roll_spd / FPS * (1.0f - rate);
    w->roll_pole += typ->roll_spd_pole / FPS * (1.0f - rate);
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 6)) {
        if (w->phase >= (int)enemy_task_get_param(obj, 0)) {
            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 75)));
            w->state = 7;
            gfx->frame = gfx->frames;
            w->lock.flags &= ~LOCK_LOCKABLE;
        } else if (!(w->phase & 1))
            w->state = 5;
        w->phase++;
    }
}

/* 0x00afac28 */
void arch_angel_state_turn(void *obj, ArchAngelWork *w)
{
    int n = w->type->turn_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scr;

    enemy_task_get_pos(obj, &p);
    i3_set(ang, 0, 0, ANG_90);
    yuki_lerp3(w->ang0, w->ang, ang, arch_angel_turn_ease(obj, w, rate));
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 2)) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &scr.x);
        project_point(&scr, &scr);
        SoundRhythmSe_EnemyPos(arch_angel_init_arg, 4, &scr.x);
    }
}

/* 0x00afadd0 */
void arch_angel_state_turn_back(void *obj, ArchAngelWork *w)
{
    int n = w->type->turn_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];
    Vec3 scr;

    enemy_task_get_pos(obj, &p);
    ang[0] = 0;
    ang[1] = 0;
    ang[2] = 0;
    yuki_lerp3(w->ang0, w->ang, ang, arch_angel_turn_ease(obj, w, rate));
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 2)) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &scr.x);
        project_point(&scr, &scr);
        SoundRhythmSe_EnemyPos(arch_angel_init_arg, 4, &scr.x);
    }
}

/* 0x00afaf78 */
void arch_angel_state_return(void *obj, ArchAngelWork *w)
{
    int n = w->type->turn_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 p;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    yuki_lerp3(w->ang0, w->ang, w->base, arch_angel_turn_ease(obj, w, rate));
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    gfx->frame = gfx->frames * (1.0f - rate);
    w->parts[0].frame = w->parts[0].frames * (1.0f - rate);
    arch_angel_update_core_rolls(obj, w);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    if (arch_angel_next_state(obj, w, n, 8))
        enemy_task_set_state(obj, 2);
}

/* 0x00afb108 */
void arch_angel_state_leave(void *obj, ArchAngelWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    float rad = camera_get_far() + ((ModelPart *)enemy_task_gfx_ptr(obj))->radius;
    Vec3 p;
    int ang[3];
    Vec3 pl;
    Vec3 v;
    float len, sec, acc, dis;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    len = vec3_length(&v) * FPS;
    sec = w->count / FPS;
    acc = len / (enemy_task_get_param(obj, 1) / FPS);
    len = acc * sec;
    sec = 1.0f / FPS;
    dis = len * sec;
    dis += acc * 0.5f * sec * sec;
    vec3_normalize(&v, &v);
    v3_scale(&v, dis);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    player_get_pos(&pl);
    if (vec3_distance_squared(&pl, &p) > rad * rad)
        FINISH_IF(obj);
    enemy_task_get_rot(obj, ang);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    w->count++;
}

/* 0x00afb2f0 */
void arch_angel_update(void *obj, TypeEnemyWork *w)
{
    void (*fn)(void *obj, TypeEnemyWork *w) = arch_angel_states[w->state];

    if (fn)
        fn(obj, w);
    arch_angel_prune_children(obj, w);
}

/* 0x00afb348 */
void arch_angel_die(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int n = typ->die_sec * FPS;
    float k, sec, prev_sec, fall;
    Vec3 p;
    int ang[3];
    Vec3 down;
    Vec3 mv;
    Vec3 eye;
    int i;

    k = (0.0f <= (w->count / (float)n) ? (w->count / (float)n) : 0.0f) < 1.0f ? (0.0f <= (w->count / (float)n) ? (w->count / (float)n) : 0.0f) : 1.0f;
    enemy_task_get_pos(obj, &p);
    do {
        if (map_get_current() == 20) {
            down.x = 0;
            down.y = 0;
            down.z = 1.0f;
        } else {
            down.x = 0;
            down.y = -1.0f;
            down.z = 0;
        }
    } while (0);
    sec = w->count / FPS;
    prev_sec = (w->count - 1) / FPS;
    fall = typ->die_fall_acc * 0.5f * (sec * sec - prev_sec * prev_sec);
    mv.x = down.x * fall;
    mv.y = down.y * fall;
    mv.z = down.z * fall;
    v3_add(&p, &mv);
    enemy_task_set_pos(obj, &p);
    w->ang0[0] += typ->die_ang_spd / FPS * k;
    w->ang0[2] += typ->die_ang_spd_z / FPS * k;
    ulMatrixPush();
    matrix_unit_set(0);
    arch_angel_rotate_yxz(w->ang0);
    vu0_matrix_rotate_y(ulMatrixCur, w->roll);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    camera_get_eye(&eye);
    v3_sub(&w->hitv, &p, &eye);
    arch_angel_explode(obj, w, 1, &p);
    for (i = 0; i < 2; i++) {
        v3_sub(&w->hitv, &w->lpos[i], &eye);
        arch_angel_explode(obj, w, 1, &w->lpos[i]);
    }
    if (w->count++ > n)
        FINISH_IF(obj);
}

/* 0x00afb6c8 */
void arch_angel_exec_appear(void *obj, TypeEnemyWork *w)
{
    int n = enemy_task_get_phase(obj);

    if (n) {
        if (n != 2)
            arch_angel_update(obj, w);
    } else
        w->unk_2c &= ~0x100;
}

/* 0x00afb738 */
void arch_angel_exec_normal(void *obj, TypeEnemyWork *w)
{
    if (enemy_task_get_phase(obj) == 1)
        arch_angel_update(obj, w);
}

/* 0x00afb788 */
void arch_angel_exec_leave(void *obj, ArchAngelWork *w)
{
    int n = enemy_task_get_phase(obj);
    int i;

    if (n) {
        if (n != 2)
            arch_angel_update(obj, (TypeEnemyWork *)w);
    } else {
        for (i = 0; i < w->nchild; i++)
            arch_angel_child_destroy(w->child[i]);
    }
}

/* 0x00afb818 */
void arch_angel_exec_destroyed(void *obj, ArchAngelWork *w)
{
    int n = enemy_task_get_phase(obj);
    ModelPart *gfx;
    LockObj *lock;
    int i, j;

    if (n) {
        if (n != 2)
            arch_angel_die(obj, w);
    } else {
        w->count = 0;
        gfx = enemy_task_gfx_ptr(obj);
        lock = &w->lock;
        lock->flags &= ~LOCK_LOCKABLE;
        w->parts[6].frame = gfx->frame;
        for (j = 0; j < 2; j++) {
            lock = &w->lk[j];
            lock->flags &= ~LOCK_LOCKABLE;
        }
        for (i = 0; i < w->nchild; i++)
            arch_angel_child_destroy(w->child[i]);
    }
}

/* 0x00afb900 */
void arch_angel_pre_exec(void)
{
}

/* 0x00afb908 */
void arch_angel_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (arch_angel_exec_table[i])
        arch_angel_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00afb960 */
int arch_angel_lock_hit(void *obj, ArchAngelWork *w, int *dmg)
{
    ArchAngelRec *typ;
    LockObj *lock = &w->lock;
    Vec3 p;
    int score = 0;
    int n;

    typ = w->type;
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        n = typ->explode_num_max - w->nexp;
        *dmg = MIN(n, typ->explode_num);
        enemy_task_get_pos(obj, &p);
        arch_angel_explode(obj, w, *dmg, &p);
        if (((unsigned int)enemy_task_get_pstate(obj)) < 2) {
            ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
            project_point(&p, &p);
            if (lock->life <= 0) {
                SoundRhythmSe_EnemyPos(arch_angel_init_arg, 1, &p.x);
                score = lockon_hits_max_shot_cnt(lock);
                enemy_task_set_state(obj, 3);
                peri_act_play_number(9);
            } else if (lock->life % 2)
                SoundRhythmSe_EnemyPos(arch_angel_init_arg, 0, &p.x);
            else
                SoundRhythmSe_EnemyPos(arch_angel_init_arg, 2, &p.x);
        }
    }
    return score;
}

/* steer(cast) */
/* 0x00afbac8 */
void arch_angel_post_exec(void *obj, ArchAngelWork *w)
{
    ArchAngelRec *typ = w->type;
    Vec3 p;
    int ang[3];
    int dmg;
    int score;
    int i;
    LockObj *lock = w->lk;

    enemy_task_get_pos(obj, &p);
    if (!enemy_status_get_ticks()) {
        ModelPart *gfx = enemy_task_gfx_ptr(obj);

        enemy_task_get_rot(obj, ang);
        yuki_model_positions_at(w->lpos, &p, ang, gfx, 0x1000000, 2);
    }
    score = arch_angel_lock_hit(obj, w, &dmg);
    if (((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
        project_point(&p, &p);
        for (i = 0; i < 2; i++, lock++) {
            if (lock->flags & LOCK_HIT) {
                v3_copy(&w->hitv, &lock->imp.v);
                dmg = MIN(typ->explode_num_max - w->nexp, typ->explode_num);
                arch_angel_explode(obj, w, dmg, &w->lpos[i]);
                if (lock->life <= 0) {
                    SoundRhythmSe_EnemyPos(arch_angel_init_arg, 1, &p.x);
                    enemy_task_set_state(obj, 3);
                    score = lockon_hits_max_shot_cnt(lock);
                } else
                    SoundRhythmSe_EnemyPos(arch_angel_init_arg, 3, &p.x);
            }
        }
    }
    if (score > 0) {
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &p, typ->score, score);
        arch_angel_add_bonus_score(obj, w);
    }
}

/* steer(cast) */
/* 0x00afbcc0 */
void arch_angel_disp_body(void *obj, ArchAngelWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ArchAngelRec *typ;
    Vec3 p;
    float col_add[4];
    int prev;
    float wave;
    int i;

    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, gfx->radius)) {
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        arch_angel_rotate_yxz(w->ang0);
        vu0_matrix_rotate_y(ulMatrixCur, w->roll + ANG_180);
        dc_object_draw(gfx);
        {
            int roll = w->roll - ANG_180;

            vu0_matrix_rotate_y(ulMatrixCur, w->roll_pole - roll);
        }
        dc_object_draw(&w->parts[0]);
        ulMatrixPop();
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
        col_add[0] = col_add[1] = col_add[2] = col_add[3] = 1.0f;
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        ulMatrixPush();
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        arch_angel_rotate_yxz(w->ang0);
        vu0_matrix_rotate_y(ulMatrixCur, w->roll_core + ANG_180);
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[5]);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f, typ->wire_near,
                                          typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            dc_object_draw(&w->parts[7]);
        }
        wave = arch_angel_sin_plateau(w->roll_core);
        col_add[0] = wave * 0.5f + 0.5f;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[1]);
        col_add[0] = (1.0f - wave) * 0.5f + 0.5f;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[3]);
        ulMatrixPop();
        for (i = 0; i < 2; i++) {
            ulMatrixPush();
            do {
                Vec3 *p_ = &w->lpos[i];
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            arch_angel_rotate_yxz(w->ang0);
            vu0_matrix_rotate_y(ulMatrixCur, w->roll_dcore[i] + ANG_180);
            wave = arch_angel_sin_plateau(w->roll_dcore[i]);
            col_add[0] = wave * 0.5f + 0.5f;
            draw_set_override_color(col_add);
            dc_object_draw(&w->parts[2]);
            col_add[0] = (1.0f - wave) * 0.5f + 0.5f;
            draw_set_override_color(col_add);
            dc_object_draw(&w->parts[4]);
            ulMatrixPop();
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* steer(cast) */
/* 0x00afc000 */
void arch_angel_disp_die(void *obj, ArchAngelWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ArchAngelRec *typ;
    Vec3 p;
    float col_add[4];
    int prev;
    float wave, k;
    int n;
    int i;

    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, gfx->radius)) {
        n = w->type->die_sec * FPS;
        k = MIN(MAX_GE(w->count / (float)n, 0.0f), 1.0f);
        typ = w->type;
        ulGxSetTextureList(((void **)obj)[1]);
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_push_override_color();
        col_add[0] = -k;
        col_add[1] = col_add[2] = col_add[3] = 0.0f;
        draw_set_override_color(col_add);
        ulMatrixPush();
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        arch_angel_rotate_yxz(w->ang0);
        vu0_matrix_rotate_y(ulMatrixCur, w->roll + ANG_180);
        dc_object_draw(gfx);
        {
            int roll = w->roll - ANG_180;

            vu0_matrix_rotate_y(ulMatrixCur, w->roll_pole - roll);
        }
        dc_object_draw(&w->parts[0]);
        ulMatrixPop();
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_push_override_color();
        ulMatrixPush();
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        arch_angel_rotate_yxz(w->ang0);
        vu0_matrix_rotate_y(ulMatrixCur, w->roll_core + ANG_180);
        col_add[0] = 1.0f - k;
        col_add[1] = col_add[2] = col_add[3] = 1.0f;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[5]);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, 1.0f - k,
                                          typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            dc_object_draw(&w->parts[7]);
        }
        wave = arch_angel_sin_plateau(w->roll_core);
        col_add[0] = wave * 0.5f + 0.5f - k;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[1]);
        col_add[0] = (1.0f - wave) * 0.5f + 0.5f - k;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[3]);
        ulMatrixPop();
        for (i = 0; i < 2; i++) {
            ulMatrixPush();
            do {
                Vec3 *p_ = &w->lpos[i];
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            arch_angel_rotate_yxz(w->ang0);
            vu0_matrix_rotate_y(ulMatrixCur, w->roll_dcore[i] + ANG_180);
            wave = arch_angel_sin_plateau(w->roll_dcore[i]);
            col_add[0] = wave * 0.5f + 0.5f - k;
            draw_set_override_color(col_add);
            dc_object_draw(&w->parts[2]);
            col_add[0] = (1.0f - wave) * 0.5f + 0.5f - k;
            draw_set_override_color(col_add);
            dc_object_draw(&w->parts[4]);
            ulMatrixPop();
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* 0x00afc430 */
void arch_angel_disp(void *obj, ArchAngelWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);
    ScrQuad quad;
    float sc[2];
    unsigned int alpha;

    w->flash = 0;
    if (arch_angel_disp_table[i])
        arch_angel_disp_table[i](obj, w);
    if (enemy_task_get_pstate(obj) == 1 && enemy_task_get_phase(obj) == 1) {
        quad = arch_angel_flash_quad;
        ulSinCos((w->count << 14) / (enemy_task_get_param(obj, 1) * 60.0f / 60.0f), &sc[0], &sc[1]);
        sc[1] = MIN(MAX_GE(sc[1], 0.0f), 1.0f);
        draw_set_blend(4, 5);
        alpha = ((int)(sc[1] * 64.0f) + 1) >> 1 << 24;
        quad.v[0].c = quad.v[1].c = quad.v[2].c = quad.v[3].c = alpha;
        ulGxBeginStrip(1);
        ulGxAddStrip(&quad, 4);
        ulGxEndStrip();
    }
}

/* 0x00afc638 */
int arch_angel_create(void *obj, ShotArg *place)
{
    int k[2];
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)arch_angel_funcs;
    ArchAngelRec *typ = (ArchAngelRec *)arch_angel_types;
    float cyc = typ->open_sec + typ->normal_sec + typ->close_sec + typ->turn_sec;

    k[0] = 0;
    k[1] = place->delay * 60.0f / 60.0f + (typ->start_sec + (place->move * cyc + cyc)) * FPS;
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = arch_angel_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.kind = k;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same_lock;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 73;
    desc.model_no2 = 75;
    desc.score = 0;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = k[1];
    desc.size = 1088;
    return enemy_task_task_create(obj, &desc);
}

/* 0x00afc7d8 */
void arch_angel_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
