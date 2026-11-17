/*
 * Ishtar's core, Area 3: moves in with its two arms, opens, swings aside and attacks, then rolls
 * away. Shot down, it shatters and lets its arms go.
 */

#include "stg03.h"
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
#include "model_wire_break.h"
#include "nj_system.h"
#include "nj_view.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "e_shot.h"
#include "ishtar_arm.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* Named by the 'ishtar_core' VarDefs. */
typedef struct IshtarCoreParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float hp_reduce;            /* 0x0c */
    float crash_r;              /* 0x10 */
    float crash_damage;         /* 0x14 */
    float bullet_spd;           /* 0x18 */
    int start_pitch;            /* 0x1c */
    float break_sec;            /* 0x20 */
    int break_ang;              /* 0x24 */
    float break_up_dis;         /* 0x28 */
    float stop_sec;             /* 0x2c */
    int stop_pitch_ang;         /* 0x30 */
    float stop_pitch_pow;       /* 0x34 */
    float stop_pitch_pow2;      /* 0x38 */
    int stop_roll_ang;          /* 0x3c */
    float stop_roll_pow;        /* 0x40 */
    float stop_roll_pow2;       /* 0x44 */
    float stop_swing_dis;       /* 0x48 */
    float stop_down_dis;        /* 0x4c */
    float stop_back_dis;        /* 0x50 */
    float turn_sec;             /* 0x54 */
    float turn_down_dis;        /* 0x58 */
    int turn_roll_ang;          /* 0x5c */
    float away_up_acc;          /* 0x60 */
    int roll0_spd;              /* 0x64 */
    int roll1_spd;              /* 0x68 */
    float shield_sec;           /* 0x6c */
    float attack_ray_rate;      /* 0x70 */
    float attack_ray_rate2;     /* 0x74 */
    float damage_spd;           /* 0x78 */
    float damage_acc;           /* 0x7c */
    float damage_recover_spd;   /* 0x80 */
    float damage_ang_spd;       /* 0x84 */
    int damage_ang_recover_spd; /* 0x88 */
    int wire;                   /* 0x8c */
    int wire_linear;            /* 0x90 */
    int wire_col;               /* 0x94 */
    int wire_col2;              /* 0x98 */
    float wire_near;            /* 0x9c */
    float wire_far;             /* 0xa0 */
    float wire_near_rate;       /* 0xa4 */
    float wire_far_rate;        /* 0xa8 */
    int wire_beat;              /* 0xac */
} IshtarCoreParams;

typedef struct IshtarCoreVars {
    char *name;
    char *label;                /* 0x04 */
    IshtarCoreParams p;         /* 0x08 */
} IshtarCoreVars;

typedef struct IshtarCoreWork {
    IshtarCoreVars *type;
    Vec3 pos;                   /* 0x04: player relative */
    int frames;                 /* 0x10: frames in the state */
    int life;                   /* 0x14: frames left; 0 = no limit */
    Vec3 hitv;                  /* 0x18: the lock-on record's vector when hit */
    int state;                  /* 0x24 */
    Vec3 from;                  /* 0x28: position at the state's start */
    Vec3 dir;                   /* 0x34 */
    float speed;                /* 0x40 */
    int start_ang[3];           /* 0x44 */
    int from_ang[3];            /* 0x50 */
    Vec3 up;                    /* 0x5c */
    char pad_68[4];
    Vec3 side;                  /* 0x6c */
    int roll_rate;              /* 0x78: per frame, after the attack */
    Vec3 muzzle;                /* 0x7c */
    void *arm[2];               /* 0x88: IshtarArmWork tasks */
    int shield_kind;            /* 0x90 */
    int attack;                 /* 0x94: attack motion step */
    int roll0;                  /* 0x98 */
    int roll1;                  /* 0x9c */
    ModelPart shield;           /* 0xa0 */
    ModelPart cover;            /* 0xfc: the wire cover */
    int flash;                  /* 0x158: frames of the shield flash left */
    int blink;                  /* 0x15c */
    Vec3 core;                  /* 0x160: lock-on point */
    LockObj lock;               /* 0x16c */
    Vec3 flash_pos;             /* 0x1ac */
    ModelPart ray;              /* 0x1b8 */
    ModelPart ray_tip;          /* 0x214 */
    int aim_ang[3];             /* 0x270 */
    int attack_ang[3];          /* 0x27c: rotation at the attack's start */
    float shake_spd;            /* 0x288 */
    Vec3 shake;                 /* 0x28c */
    int shake_ang_spd[3];       /* 0x298 */
    int shake_ang[3];           /* 0x2a4 */
} IshtarCoreWork;

extern int ishtar_core_init_arg;
extern void (*ishtar_core_exec_table[])(void *o, IshtarCoreWork *w);
extern void (*ishtar_core_disp_table[])(void *o, IshtarCoreWork *w);
int ishtar_core_node_cb(unsigned int *node, int idx, IshtarCoreWork *w);
void ishtar_core_vars_register(void);
void ishtar_core_vars_unregister(void);
int ishtar_core_vars_init(int *fx_id);
int ishtar_core_vars_release(void);
void ishtar_core_pose_model(void *obj, IshtarCoreWork *w);
void ishtar_core_exec(void *obj, IshtarCoreWork *w);
void ishtar_core_disp(void *obj, IshtarCoreWork *w);
void ishtar_core_rotate_yxz(int *ang);
extern IshtarCoreVars ishtar_core_types[1];
extern VarDef ishtar_core_vardef;
extern VarDef ishtar_core_vardefs[44];
void ishtar_core_update(void *obj, IshtarCoreWork *w);
int ishtar_core_attack_motion(void *obj, IshtarCoreWork *w);
extern char ishtar_core_lock_name[];
void ishtar_core_shake_start(void *obj, IshtarCoreWork *w);
extern ObjFuncs ishtar_core_funcs;
extern char ishtar_core_name[];
void ishtar_core_disp_extras(void *obj, IshtarCoreWork *w, ModelPart *gfx, int *ang);
extern int (*ishtar_core_states[])(void *o, IshtarCoreWork *w);
void ishtar_core_knockback(void *obj, IshtarCoreWork *w);

/* 0x00b05488 */
void ishtar_core_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&ishtar_core_vardef);
    for (i = 0; i < 1; i++) {
        def = ishtar_core_vardefs;
        prm = (int *)&ishtar_core_types[i].p;
        ishtar_core_vardef.name = ishtar_core_types[i].name;
        ishtar_core_vardef.fmt = ishtar_core_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&ishtar_core_vardef);
        for (j = 0; j < 44; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)ishtar_core_vardefs);
    }
}

/* 0x00b055a0 */
void ishtar_core_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b055a8 */
int ishtar_core_vars_init(int *fx_id)
{
    ishtar_core_init_arg = *fx_id;
    ishtar_core_vars_register();
    return 0;
}

/* 0x00b055d0 */
int ishtar_core_vars_release(void)
{
    ishtar_core_vars_unregister();
    return 0;
}

/* 0x00b055f0 */
int ishtar_core_marker_draw(int obj, void *place)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 52);
    return obj_draw_at(obj, place, (void *)*ent);
}

/* 0x00b05648 */
int ishtar_core_get_init_arg(void)
{
    return ishtar_core_init_arg;
}

/* 0x00b05658 */
int ishtar_core_node_cb(unsigned int *node, int idx, IshtarCoreWork *w)
{
    if (*node & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&w->core);
    if (*node & 0x1000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&w->muzzle);
    if (*node & 0x8000000)
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&w->flash_pos);
    return 0;
}

/* 0x00b056f0 */
void ishtar_core_pose_model(void *obj, IshtarCoreWork *w)
{
    Vec3 pos;
    int ang[4];
    void *gfx = enemy_task_gfx_ptr(obj);

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    dc_object_set_callback(gfx, ishtar_core_node_cb, w);
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    ishtar_core_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00b057c0 */
void ishtar_core_shake_start(void *obj, IshtarCoreWork *w)
{
    IshtarCoreVars *typ = w->type;
    int amp;
    int ax, ay, az;

    amp = DEG_TO_ANG(typ->p.damage_ang_spd);
    w->shake_spd = typ->p.damage_spd;
    ax = RAND_FLOAT_A() * amp;
    ay = RAND_FLOAT_A() * amp;
    az = RAND_FLOAT_A() * amp;
    i3_set(w->shake_ang_spd, ax, ay, az);
}

/* 0x00b058c0 */
void ishtar_core_remove_offset(void *obj, IshtarCoreWork *w)
{
    Vec3 p;
    int ang[3];

    enemy_task_get_pos(obj, &p);
    v3_sub(&p, &p, &w->shake);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    i3_sub(ang, ang, w->shake_ang);
    enemy_task_set_rot(obj, ang);
}

/* 0x00b05990 */
void ishtar_core_knockback(void *obj, IshtarCoreWork *w)
{
    IshtarCoreVars *typ = w->type;
    int step = typ->p.damage_ang_recover_spd / FPS;
    int *aofs = w->shake_ang;
    float dis, vlen;
    Vec3 p;
    int ang[3];
    Vec3 dir;
    int v;

    if (fabsf(w->shake_spd) < FLT_EPSILON) {
        vlen = vec3_length(&w->shake);
        if (!(fabsf(vlen) < FLT_EPSILON)) {
            vlen -= typ->p.damage_recover_spd * FRAME_SEC;
            vlen = 0.0f <= vlen ? vlen : 0.0f;
            vec3_normalize(&w->shake, &w->shake);
            v3_scale(&w->shake, vlen);
        }
        do { aofs[0] = aofs[0] & 0x8000 ? aofs[0] | (int)0xffff0000 : aofs[0] & 0xffff; aofs[1] = aofs[1] & 0x8000 ? aofs[1] | (int)0xffff0000 : aofs[1] & 0xffff; aofs[2] = aofs[2] & 0x8000 ? aofs[2] | (int)0xffff0000 : aofs[2] & 0xffff; } while (0);
        if (aofs[0] > 0) {
            int v = aofs[0] - step;

            aofs[0] = v > -1 ? v : 0;
        } else {
            int v = aofs[0] + step;

            aofs[0] = v < 1 ? v : 0;
        }
        if (aofs[1] > 0) {
            int v = aofs[1] - step;

            aofs[1] = v > -1 ? v : 0;
        } else {
            int v = aofs[1] + step;

            aofs[1] = v < 1 ? v : 0;
        }
        if (aofs[2] > 0) {
            int v = aofs[2] - step;

            aofs[2] = v > -1 ? v : 0;
        } else {
            int v = aofs[2] + step;

            aofs[2] = v < 1 ? v : 0;
        }
    } else {
        dis = w->shake_spd * FRAME_SEC - typ->p.damage_acc * FRAME_SEC * FRAME_SEC * 0.5f;
        vec3_normalize(&dir, &w->hitv);
        v3_scale(&dir, dis);
        v3_add(&w->shake, &dir);
        {
            float nspd = w->shake_spd - typ->p.damage_acc * FRAME_SEC;

            nspd = nspd <= 0.0f ? 0.0f : nspd;
            w->shake_spd = nspd;
        }
        ang[0] = w->shake_ang_spd[0] * dis;
        ang[1] = w->shake_ang_spd[1] * dis;
        ang[2] = w->shake_ang_spd[2] * dis;
        aofs[0] += ang[0];
        aofs[1] += ang[1];
        aofs[2] += ang[2];
    }
    enemy_task_get_pos(obj, &p);
    v3_add(&p, &w->shake);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    ang[0] += aofs[0];
    ang[1] += aofs[1];
    ang[2] += aofs[2];
    enemy_task_set_rot(obj, ang);
}

/* steer(cast) */
/* 0x00b05d60 */
void ishtar_core_init(void *obj, IshtarCoreWork *w, char *flag, int *kind)
{
    IshtarCoreVars *typ = &ishtar_core_types[kind[0]];
    Vec3 p;
    Vec3 org;
    Vec3 v;
    Vec3 pl_vel;
    int ang[3];
    int arm[6];
    float sc[4][2];
    float side, spd, zero;

    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    w->life = enemy_task_get_param(obj, 0) * 60.0f / 60.0f;
    w->frames = 1;
    w->state = 0;
    lockon_obj_init(&w->lock, obj, &w->core, 0, ishtar_core_lock_name, enemy_task_get_status_slot(obj), typ->p.hp, typ->p.same_lock, 128);
    lockon_entry(&w->lock);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    spd = vec3_length(&v);
    w->speed = spd;
    if (!(fabsf(spd) < FLT_EPSILON)) {
        vec3_normalize(&w->dir, &v);

            do {
                ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
                ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
            } while (0);
            ang[0] -= typ->p.start_pitch;

        ang[2] = 0;
    } else {
        v3_set(&w->dir, 0.0f, 0.0f, 0.0f);
        i3_set(ang, 0, 0, 0);
    }
    enemy_task_set_rot(obj, ang);
    player_get_frame_pos(&org);
    player_get_frame_vel(&pl_vel);
    v3_add(&p, &pl_vel);
    i3_copy(w->start_ang, ang);
    v3_sub(&w->pos, &p, &org);
    v3_copy(&w->from, &w->pos);
    i3_copy(w->from_ang, ang);
    {
        float x, y, z;

        x = 0.0f;
        y = 1.0f;
        z = 0.0f;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1], sc[1], x, z);
        v3_set(&w->up, x, y, z);
    }
    {
        float x, y, z;

        z = 0.0f;
        side = enemy_task_get_param(obj, 4);
        if (fabsf(side) < FLT_EPSILON)
            x = 0.0f;
        else if (0.0f < side)
            x = 1.0f;
        else
            x = -1.0f;
        do {
            zero = 0.0f;
            ulSinCos(ang[0], &sc[2][0], &sc[2][1]);
            y = z * sc[2][1] - z * sc[2][0];
            z = zero * sc[2][0] + z * sc[2][1];
        } while (0);
        ROT_Y(ang[1], sc[3], x, z);
        v3_set(&w->side, x, y, z);
    }
    ishtar_core_pose_model(obj, w);
    arm[0] = (int)enemy_task_pos_ptr(obj);
    arm[1] = (int)enemy_task_rot_ptr(obj);
    arm[5] = w->life;
    arm[2] = 0;
    arm[3] = enemy_task_get_param(obj, 2) * 60.0f / 60.0f;
    {
        int n = (int)(enemy_task_get_param(obj, 2) * 1000.0f + 0.5f) % 1000;

        arm[5] = w->life;
        n = n > -1 ? n : 0;
        arm[4] = n < 16 ? n : 15;
    }
    w->arm[0] = (void *)ishtar_arm_create(obj, flag, arm);
    arm[2] = 1;
    arm[3] = enemy_task_get_param(obj, 3) * 60.0f / 60.0f;
    {
        int n = (int)(enemy_task_get_param(obj, 3) * 1000.0f + 0.5f) % 1000;

        n = n > -1 ? n : 0;
        arm[4] = n < 16 ? n : 15;
    }
    w->arm[1] = (void *)ishtar_arm_create(obj, flag, arm);
    w->attack = 0;
    {
        int n = (int)(enemy_task_get_param(obj, 5) * 1000.0f + 0.5f) % 1000;

        n = n > -1 ? n : 0;
        w->shield_kind = n < 16 ? n : 15;
        zero *= 1 + ((n & 1) >> 1); /* steer(zero) */
    }
    w->shake_spd = zero;
    v3_set(&w->shake, zero, zero, zero);
    i3_set(w->shake_ang_spd, 0, 0, 0);
    i3_set(w->shake_ang, 0, 0, 0);
    dc_object_init(&w->shield, *((struct DcNode **)pnb_get_entry(*(void **)obj, 44)), 0, 0);
    dc_object_rebuild(&w->shield);
    w->flash = 0;
    w->blink = 0;
    dc_object_init(&w->ray, *((struct DcNode **)pnb_get_entry(*(void **)obj, 58)), *((struct DcClip **)pnb_get_entry(*(void **)obj, 59)), 0);
    dc_object_rebuild(&w->ray);
    dc_object_init(&w->ray_tip, *((struct DcNode **)pnb_get_entry(*(void **)obj, 56)), *((struct DcClip **)pnb_get_entry(*(void **)obj, 57)), 0);
    dc_object_rebuild(&w->ray_tip);
    w->ray_tip.radius = 150.0f;
    dc_object_init(&w->cover, *((struct DcNode **)pnb_get_entry(*(void **)obj, 54)), 0, 0x500);
    dc_object_set_wire(&w->cover, model_pnb_get_wire(*(void **)obj, 54));
    {
        ModelPart *gfx = enemy_task_gfx_ptr(obj);

        w->roll0 = 0;
        w->roll1 = 0;
        gfx->radius = w->cover.radius = 91.0f;
    }
}

/* 0x00b064a8 */
void ishtar_core_release(void *obj, IshtarCoreWork *w)
{
    dc_object_release(&w->ray);
    dc_object_release(&w->ray_tip);
    dc_object_release(&w->shield);
    dc_object_release(&w->cover);
    lockon_remove(&w->lock);
}

/* 0x00b064f0 */
void ishtar_core_fire_shot(void *obj, IshtarCoreWork *w)
{
    e_shot_fire(obj, &w->muzzle, 0, 15);
}

/* 0x00b06500 */
int ishtar_core_next_state(void *obj, IshtarCoreWork *w, int n, int state)
{
    if (++w->frames > n) {
        v3_copy(&w->from, &w->pos);
        w->state = state;
        w->frames = 1;
        enemy_task_get_rot(obj, w->from_ang);
        return 1;
    }
    return 0;
}

/* steer(cast) */
/* 0x00b06570 */
int ishtar_core_attack_motion(void *obj, IshtarCoreWork *w)
{
    int ang[3];
    int nang[3];
    float sc[2][2];

    switch (w->attack) {
    case 0:
        if (w->arm[0] || w->arm[1])
            break;
        {
            ModelPart *gfx = enemy_task_gfx_ptr(obj);

            dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(*(void **)obj, 53)));
            gfx->frame = 0.0f;
            dc_object_set_clip(&w->cover, *((struct DcClip **)pnb_get_entry(*(void **)obj, 55)));
            w->attack = 1;
            enemy_task_get_rot(obj, w->attack_ang);
        }
        break;
    case 1:
        {
            IshtarCoreVars *typ = w->type;
            ModelPart *gfx = enemy_task_gfx_ptr(obj);
            ModelPart *part_b = &w->ray_tip;
            ModelPart *part_a = &w->ray;
            float time = gfx->frame;
            float rate = time / gfx->frames;

            ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[0][0], &sc[0][1]);
            e_shot_initial_angles(w->aim_ang, &w->muzzle, 0, 15);
            yuki_lerp3(ang, w->attack_ang, w->aim_ang, (sc[0][0] + 1.0f) * 0.5f);
            enemy_task_set_rot(obj, ang);
            dc_object_advance_time(gfx);
            rate = gfx->frames * (rate - typ->p.attack_ray_rate)
                / (gfx->frames * (1.0f - typ->p.attack_ray_rate) + part_b->frames * typ->p.attack_ray_rate2);
            part_a->frame = part_a->frames * rate;
            if (fabsf(time - gfx->frames) < FLT_EPSILON)
                w->attack = 2;
        }
        break;
    case 2:
        {
            IshtarCoreVars *typ = w->type;
            ModelPart *gfx = enemy_task_gfx_ptr(obj);
            ModelPart *part_b = &w->ray_tip;
            float rate = part_b->frame / part_b->frames;
            ModelPart *part_a;

            enemy_task_set_rot(obj, w->aim_ang);
            part_a = &w->ray;
            dc_object_advance_time(part_b);
            {
                float rem = 1.0f - typ->p.attack_ray_rate;
                float dur = gfx->frames * rem + part_b->frames * typ->p.attack_ray_rate2;

                rate = (gfx->frames * rem + part_b->frames * rate) / dur;
            }
            part_a->frame = part_a->frames * rate;
            if (fabsf(part_b->frame - part_b->frames) < FLT_EPSILON) {
                w->attack = 3;
                return 1;
            }
        }
        break;
    case 3:
        {
            ModelPart *gfx = enemy_task_gfx_ptr(obj);
            float frm = gfx->frame;

            frm -= gfx->speed;
            frm = 0.0f <= frm ? frm : 0.0f;
            gfx->frame = frm;
            ulSinCos(frm / gfx->frames * ANG_HALF - ANG_QUARTER, &sc[1][0], &sc[1][1]);
            enemy_task_get_rot(obj, nang);
            yuki_lerp3(nang, nang, w->aim_ang, (sc[1][0] + 1.0f) * 0.5f);
            enemy_task_set_rot(obj, nang);
        }
        break;
    }
    return 0;
}

/* 0x00b06928 */
int ishtar_core_state_move(void *obj, IshtarCoreWork *w)
{
    int n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    Vec3 v;

    enemy_task_get_param_vec(obj, &v);
    v3_add(&w->pos, &v);
    ishtar_core_next_state(obj, w, n, 1);
    return 0;
}

/* 0x00b069e8 */
int ishtar_core_state_open(void *obj, IshtarCoreWork *w)
{
    IshtarCoreVars *typ = w->type;
    int n = typ->p.break_sec * FPS + 1.0f;
    float rate = (float)w->frames / n;
    float dis;
    float ease;
    int pitch;
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[2];

    dis = w->speed * FPS * (typ->p.break_sec * rate) * (1.0f - rate * 0.5f);
    v3_mul(&v, &w->dir, dis);
    v3_sum(&p, &w->from, &v);
    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    v3_mul(&v, &w->up, typ->p.break_up_dis * (sc[0] + 1.0f) * 0.5f);
    v3_add(&p, &v);
    v3_copy(&w->pos, &p);
    ease = safe_powf(1.0f - rate, 2.0f);
    pitch = (typ->p.break_ang + typ->p.start_pitch) * (1.0f - ease);
    ulMatrixPush();
    matrix_unit_set(0);
    ishtar_core_rotate_yxz(w->from_ang);
    vu0_matrix_rotate_x(ulMatrixCur, pitch);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    ishtar_core_next_state(obj, w, n, 2);
    return 0;
}

/* 0x00b06c28 */
int ishtar_core_state_swing(void *obj, IshtarCoreWork *w)
{
    IshtarCoreVars *typ = w->type;
    int n = typ->p.stop_sec * FPS + 1.0f;
    float rate = (float)w->frames / n;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int sang;
    int rx;
    int rz;
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[2][2];

    sang = rate * ANG_HALF - ANG_QUARTER;
    ulSinCos(sang, &sc[0][0], &sc[0][1]);
    sc[0][0] = (sc[0][0] + 1.0f) * 0.5f;
    v3_mul(&v, &w->dir, -sc[0][0] * typ->p.stop_back_dis);
    v3_sum(&p, &w->from, &v);
    ulSinCos(sang, &sc[0][0], &sc[0][1]);
    sc[0][0] = (sc[0][0] + 1.0f) * 0.5f;
    v3_mul(&v, &w->side, -(typ->p.stop_swing_dis * sc[0][0]));
    v3_add(&p, &v);
    sc[0][0] -= 1.0f;
    v3_mul(&v, &w->up, -(typ->p.stop_down_dis * sqrtf(1.0f - sc[0][0] * sc[0][0])));
    v3_add(&p, &v);
    v3_copy(&w->pos, &p);
    ulSinCos(safe_powf(rate, typ->p.stop_pitch_pow) * ANG_HALF - ANG_QUARTER, &sc[1][0], &sc[1][1]);
    sc[1][0] = (sc[1][0] + 1.0f) * 0.5f;
    rx = (typ->p.break_ang + typ->p.stop_pitch_ang) * safe_powf(sc[1][0], typ->p.stop_pitch_pow2);
    ulSinCos(safe_powf(rate, typ->p.stop_roll_pow) * ANG_HALF - ANG_QUARTER, &sc[1][0], &sc[1][1]);
    sc[1][0] = (sc[1][0] + 1.0f) * 0.5f;
    rz = typ->p.stop_roll_ang * safe_powf(sc[1][0], typ->p.stop_roll_pow2);
    if (!(enemy_task_get_param(obj, 4) > 0.0f))
        rz = -rz;
    ulMatrixPush();
    matrix_unit_set(0);
    ishtar_core_rotate_yxz(w->from_ang);
    vu0_matrix_rotate_z(ulMatrixCur, -rz);
    vu0_matrix_rotate_x(ulMatrixCur, -rx);
    model_camera_angles(ang);
    ulMatrixPop();
    enemy_task_set_rot(obj, ang);
    gfx->frame = gfx->frames * rate;
    ishtar_core_next_state(obj, w, n, 3);
    return 0;
}

/* 0x00b07008 */
int ishtar_core_state_attack(void *obj, IshtarCoreWork *w)
{
    IshtarCoreVars *typ = w->type;
    int n = typ->p.turn_sec * FPS + 1.0f;
    float frms = n;
    float rate = (float)w->frames / frms;
    float side = enemy_task_get_param(obj, 4);
    float dis;
    int rz;
    int done;
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[2];

    ulSinCos(rate * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    sc[0] = (sc[0] + 1.0f) * 0.5f;
    v3_mul(&v, &w->up, -(typ->p.turn_down_dis * sc[0]));
    v3_sum(&p, &w->from, &v);
    dis = fabsf(side) * (typ->p.turn_sec * rate) * (typ->p.turn_sec * rate) * 0.5f;
    v3_mul(&v, &w->side, dis);
    v3_add(&p, &v);
    v3_sub(&v, &p, &w->pos);
    enemy_task_set_param_vec(obj, &v);
    v3_copy(&w->pos, &p);
    rz = typ->p.turn_roll_ang * safe_powf(rate, 2.0f);
    if (!(side > 0.0f))
        rz = -rz;
    i3_copy(ang, w->from_ang);
    ang[2] -= rz;
    enemy_task_set_rot(obj, ang);
    done = ishtar_core_attack_motion(obj, w);
    if (ishtar_core_next_state(obj, w, n, 4))
        w->roll_rate = rz - (int)(typ->p.turn_roll_ang * safe_powf((float)(n - 1) / frms, 2.0f));
    return done;
}

/* 0x00b072e0 */
void ishtar_core_state_roll(void *obj, IshtarCoreWork *w)
{
    Vec3 v;
    int ang[3];
    int sgn;

    enemy_task_get_param_vec(obj, &v);
    v3_add(&w->pos, &v);
    sgn = w->roll_rate;
    if (!(enemy_task_get_param(obj, 4) > 0.0f))
        sgn = -sgn;
    i3_copy(ang, w->from_ang);
    ang[2] -= sgn * w->frames;
    enemy_task_set_rot(obj, ang);
    ishtar_core_attack_motion(obj, w);
}

/* 0x00b073b8 */
void ishtar_core_update(void *obj, IshtarCoreWork *w)
{
    int (*fn)(void *obj, IshtarCoreWork *w);
    IshtarCoreVars *typ;
    int fire = 0;
    int v;
    Vec3 p;

    ishtar_core_remove_offset(obj, w);
    fn = ishtar_core_states[w->state];
    if (fn)
        fire = fn(obj, w);
    player_get_frame_pos(&p);
    v3_add(&p, &w->pos);
    enemy_task_set_pos(obj, &p);
    typ = w->type;
    w->roll0 += typ->p.roll0_spd / FPS;
    w->roll1 += typ->p.roll1_spd / FPS;
    ishtar_core_knockback(obj, w);
    ishtar_core_pose_model(obj, w);
    v = w->flash - 1;
    w->flash = v > -1 ? v : 0;
    if (fire)
        ishtar_core_fire_shot(obj, w);
    if (w->arm[0])
        ishtar_arm_held_update(w->arm[0]);
    if (w->arm[1])
        ishtar_arm_held_update(w->arm[1]);
}

/* 0x00b07530 */
void ishtar_core_exec_appear(void *obj, IshtarCoreWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_appear_scale(obj, w);
        w->lock.flags &= ~0x100;
        break;
    case 2:
        enemy_task_appear_scale(obj, w);
        w->lock.flags |= 0x100;
        break;
    default:
        ishtar_core_update(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            w->lock.flags &= ~0x100;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00b07608 */
void ishtar_core_exec_normal(void *obj, IshtarCoreWork *w)
{
    if (enemy_task_get_phase(obj) != 1)
        return;
    ishtar_core_update(obj, w);
    if (w->life > 0 && --w->life <= 0) {
        w->lock.flags &= ~0x100;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00b07680 */
void ishtar_core_exec_leave(void *obj, IshtarCoreWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        w->lock.flags &= ~0x100;
        enemy_task_vanish_scale(obj, w);
        return;
    }
    ishtar_core_update(obj, w);
    enemy_task_vanish_scale(obj, w);
}

/* 0x00b076f8 */
void ishtar_core_exec_destroyed(void *obj, IshtarCoreWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 43;
        desc.models = stage_models;
        desc.lock = &w->lock;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        if (w->arm[0])
            ishtar_arm_detach(w->arm[0]);
        if (w->arm[1])
            ishtar_arm_detach(w->arm[1]);
        FINISH_IF(obj);
    }
}

/* 0x00b077f0 */
void ishtar_core_exec(void *obj, IshtarCoreWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ishtar_core_exec_table[i])
        ishtar_core_exec_table[i](obj, w);
}

/* 0x00b07848 */
void ishtar_core_pre_exec(void *obj, IshtarCoreWork *w)
{
    if (w->arm[0] && (TASK_OF(w->arm[0])->flags & TASK_F_END))
        w->arm[0] = 0;
    if (w->arm[1] && (TASK_OF(w->arm[1])->flags & TASK_F_END))
        w->arm[1] = 0;
}

/* steer(cast) */
/* 0x00b07888 */
void ishtar_core_post_exec(void *obj, IshtarCoreWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 p;
    SpawnDesc desc;
    Vec3 pos;

    if (!enemy_status_get_ticks())
        ishtar_core_pose_model(obj, w);
    if (lock->flags & LOCK_HIT) {
        int hits = lock->life;

        v3_copy(&w->hitv, &lock->imp.v);
        ishtar_core_shake_start(obj, w);
        if (hits >= 2) {
            w->flash = w->type->p.shield_sec * FPS;
        } else if (hits == 1) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.kind = 0x10000;
            desc.models = stage_models;
            desc.type = 45;
            desc.lock = lock;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            enemy_task_get_scl(obj, &desc.scale);
            v3_copy(&desc.v, &w->hitv);
            enemy_shatter_create(obj, &desc, 0, 0);
            w->flash = 0;
        }
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&w->core, (float *)&p);
        project_point(&p, &p);
        switch (hits) {
        case 0:
            SoundRhythmSe_EnemyPos(ishtar_core_init_arg, 1, (float *)&p);
            break;
        case 1:
            SoundRhythmSe_EnemyPos(ishtar_core_init_arg, 2, (float *)&p);
            break;
        case 2:
            SoundRhythmSe_EnemyPos(ishtar_core_init_arg, 0, (float *)&p);
            break;
        }
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2) {
        int mult = lockon_hits_max_shot_cnt(lock);
        IshtarCoreVars *typ = w->type;

        enemy_task_get_pos(obj, &pos);
        enemy_status_score_popup(enemy_task_get_status_slot(obj), &pos, typ->p.score, mult);
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b07ae0 */
int ishtar_core_roll_node_cb(unsigned int *node, int idx, IshtarCoreWork *w)
{
    if (*node & 0x2000000)
        vu0_matrix_rotate_y(ulMatrixCur, w->roll0);
    if (*node & 0x4000000)
        vu0_matrix_rotate_y(ulMatrixCur, -w->roll0 + w->roll1);
    return 1;
}

/* 0x00b07b58 */
void ishtar_core_disp_extras(void *obj, IshtarCoreWork *w, ModelPart *gfx, int *ang)
{
    w->blink++;
    if (w->flash > 0 && (w->blink & 1)) {
        Vec3 *p = &w->flash_pos;

        if (view_sphere_visible(p, gfx->radius)) {
            ulGxSetTextureList(((void **)obj)[1]);
            ulMatrixPush();
            do {
                Vec3 *p_ = p;

                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            ishtar_core_rotate_yxz(ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            dc_object_draw(&w->shield);
            ulMatrixPop();
        }
    }
    if ((unsigned int)(w->attack - 1) < 2) {
        Vec3 *p = &w->muzzle;

        if (view_sphere_visible(p, w->ray.radius)) {
            ulMatrixPush();
            do {
                Vec3 *p_ = p;

                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            ishtar_core_rotate_yxz(ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            model_node_draw_tree2_anim((void *)w->ray.root, w->ray.clip, w->ray.frame);
            ulMatrixPop();
        }
        if (view_sphere_visible(p, w->ray_tip.radius)) {
            ulMatrixPush();
            do {
                Vec3 *p_ = p;

                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            ishtar_core_rotate_yxz(ang);
            ulMatrixUnitRotation((float *)ulMatrixCur);
            dc_object_draw(&w->ray_tip);
            ulMatrixPop();
        }
    }
}

/* steer(cast) */
/* 0x00b07cf0 */
void ishtar_core_disp_body(void *obj, IshtarCoreWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    IshtarCoreVars *typ;
    float alpha, unit;
    int blend;

    blend = 0;
    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    alpha = MIN(scl.x >= 0.0f ? scl.x : 0.0f, 1.0f);
    unit = 1.0f;
    ulGxSetTextureList(((void **)obj)[1]);
    if (alpha < unit) {
        blend = draw_set_override_flags(0x8820);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        heap_bzero(rgba, 16);
        rgba[0] = alpha - unit;
        draw_set_override_color(rgba);
        nj_cnk_set_override_mode(0x2500);
    }
    if (view_sphere_visible(&p, gfx->radius)) {
        do {
            ulMatrixPush();
        } while (0);
        typ = w->type;
        vu0_matrix_translate(ulMatrixCur, p.x, p.y, p.z);
        ishtar_core_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_set_callback(gfx, (int (*)())ishtar_core_roll_node_cb, w);
        dc_object_draw(gfx);
        dc_object_set_callback(gfx, 0, 0);
        if (typ->p.wire) {
            draw_set_color(yuki_beat_color(typ->p.wire_col, typ->p.wire_col2, typ->p.wire_beat, typ->p.wire_linear, alpha, typ->p.wire_near,
                                          typ->p.wire_far, typ->p.wire_near_rate, typ->p.wire_far_rate));
            draw_set_blend(4, 5);
            model_object_draw((struct DcObjectTreeView *)&w->cover, gfx->frame);
        }
        ulMatrixPop();
    }
    ishtar_core_disp_extras(obj, w, gfx, ang);
    if (alpha < 1.0f) {
        draw_pop_strip_flags();
        draw_pop_override_color();
        draw_set_override_flags(blend);
    }
}

/* 0x00b07f50 */
void ishtar_core_disp(void *obj, IshtarCoreWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (ishtar_core_disp_table[i])
        ishtar_core_disp_table[i](obj, w);
}

/* 0x00b07fa8 */
int ishtar_core_create(void *parent, Placement *place)
{
    ObjDesc desc;
    ObjFuncs funcs = ishtar_core_funcs;
    int kind;
    IshtarCoreVars *typ;

    kind = 0;
    typ = ishtar_core_types;
    nj_fill8(&desc, 0, sizeof desc);
    desc.arg = place;
    desc.kind = &kind;
    desc.name = ishtar_core_name;
    desc.funcs = &funcs;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 18;
    desc.model_no = 52;
    desc.model_no2 = -1;
    desc.score = typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = place->param0 * 60.0f / 60.0f;
    desc.size = 688;
    desc.dc_flags = 0;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b080f8 */
void ishtar_core_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
