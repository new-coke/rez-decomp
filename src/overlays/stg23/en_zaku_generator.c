/*
 * Trance Mission zaku generator: spawns its count one every frm_interval seconds or all at once,
 * moving with the player's frame; mode bits pick what is randomised (position, direction, roll,
 * weave period and width).
 */

#include "stg23.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "nj_system.h"
#include "player.h"
#include "en_zaku_generator.h"
#include "en_zaku.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_zaku_generator_params;
extern VarDef en_zaku_generator_vardefs[12];
extern struct TaskDesc en_zaku_generator_desc;
#define EN_ZAKU_GENERATOR_REC ((EnZakuGeneratorParams *)&en_zaku_generator_params)

/* 12 words after name and label */
typedef struct EnZakuGeneratorParams {
    char *name;
    char *label;                /* 0x04 */
    float frm_interval;         /* 0x08 */
    float rnd_pos_min;          /* 0x0c */
    float rnd_pos_max;          /* 0x10 */
    float rnd_vec_min;          /* 0x14 */
    float rnd_vec_max;          /* 0x18 */
    float rnd_frm_cycle_min;    /* 0x1c */
    float rnd_frm_cycle_max;    /* 0x20 */
    float rnd_dst_cycle_min;    /* 0x24 */
    float rnd_dst_cycle_max;    /* 0x28 */
    int test;                   /* 0x2c: the two below override the placement's */
    int generate_num;           /* 0x30 */
    unsigned int generate_pattern; /* 0x34 */
} EnZakuGeneratorParams;

/* 0x00b01378 */
void en_zaku_generator_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_zaku_generator_params.name;
    def.fmt = en_zaku_generator_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_zaku_generator_vardefs;
    fld = en_zaku_generator_params.v;
    for (i = 0; i < 12; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_zaku_generator_vardefs);
}

/* 0x00b01428 */
void en_zaku_generator_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b01430 */
void en_zaku_generator_rnd_pos(EnPlacement *base, EnPlacement *out)
{
    Vec3 v;

    util_rand_vec3_len(&v, RAND_FLOAT_A() * (EN_ZAKU_GENERATOR_REC->rnd_pos_max - EN_ZAKU_GENERATOR_REC->rnd_pos_min)
                         + EN_ZAKU_GENERATOR_REC->rnd_pos_min);
    v3_sum(&out->pos, &base->pos, &v);
}

/* 0x00b014e0 */
void en_zaku_generator_rnd_vec(EnGeneratorWork *w, EnPlacement *place)
{
    int ang[3];
    Vec3 fwd;
    float deg = ANG_PER_DEG;
    float rnd_max = 32769.0f;
    int pitch;

    v3_copy(&fwd, &w->self.dir);
    pitch = rand_lcg_next_a() / rnd_max * ((int)(EN_ZAKU_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_ZAKU_GENERATOR_REC->rnd_vec_min * deg))
        + (int)(EN_ZAKU_GENERATOR_REC->rnd_vec_min * deg);
    i3_set(ang, pitch, rand_lcg_next_a() / rnd_max * ((int)(EN_ZAKU_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_ZAKU_GENERATOR_REC->rnd_vec_min * deg))
                   + (int)(EN_ZAKU_GENERATOR_REC->rnd_vec_min * deg), 0);
    if (0.5f < rand_lcg_next_a() / rnd_max) {
        ang[0] = -ang[0];
        ang[1] = -ang[1];
        ang[2] = -ang[2];
    }
    util_rotate_vec3(&fwd, ang, &place->dir);
}

/* 0x00b01688 */
void en_zaku_generator_rnd_rot(void *gen, EnPlacement *place)
{
    i3_set(place->ang, 0, 0, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
}

/* 0x00b01708 */
void en_zaku_generator_rnd_cycle(EnGeneratorWork *w, EnPlacement *place)
{
    float rnd_max = 32769.0f;

    place->param2 = rand_lcg_next_a() / rnd_max * (EN_ZAKU_GENERATOR_REC->rnd_frm_cycle_max * FPS - EN_ZAKU_GENERATOR_REC->rnd_frm_cycle_min * FPS)
             + EN_ZAKU_GENERATOR_REC->rnd_frm_cycle_min * FPS;
    place->param3 = rand_lcg_next_a() / rnd_max * (EN_ZAKU_GENERATOR_REC->rnd_dst_cycle_max - EN_ZAKU_GENERATOR_REC->rnd_dst_cycle_min) + EN_ZAKU_GENERATOR_REC->rnd_dst_cycle_min;
}

/* 0x00b017c0 */
void en_zaku_generator_fill_arg(EnGeneratorWork *w)
{
    unsigned int bits = w->mode;
    EnPlacement *place = &w->arg;

    if ((bits & 1) || (EN_ZAKU_GENERATOR_REC->test && (EN_ZAKU_GENERATOR_REC->generate_pattern & 1)))
        en_zaku_generator_rnd_pos(&w->self, place);
    if ((bits & 4) || (EN_ZAKU_GENERATOR_REC->test && (EN_ZAKU_GENERATOR_REC->generate_pattern & 4)))
        en_zaku_generator_rnd_rot(w, place);
    if ((bits & 2) || (EN_ZAKU_GENERATOR_REC->test && (EN_ZAKU_GENERATOR_REC->generate_pattern & 2)))
        en_zaku_generator_rnd_vec(w, place);
    if ((bits & 8) || (EN_ZAKU_GENERATOR_REC->test && (EN_ZAKU_GENERATOR_REC->generate_pattern & 8)))
        en_zaku_generator_rnd_cycle(w, place);
}

/* 0x00b018e8 */
void en_zaku_generator_exec(EnGeneratorWork *w)
{
    Vec3 scr;

    player_get_frame_vel(&scr);
    v3_add(&w->self.pos, &scr);
    w->timer -= 1.0f;
    if (w->timer < 0.0f) {
        v3_copy(&w->arg.pos, &w->self.pos);
        en_zaku_generator_fill_arg(w);
        en_zaku_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, (TypeRec *)&w->arg);
        if (--w->count > 0)
            w->timer = EN_ZAKU_GENERATOR_REC->frm_interval * FPS;
        else
            FINISH_IF(w);
    }
}

/* steer(cast) */
/* 0x00b019e0 */
void en_zaku_generator_init(EnGeneratorWork *w, EnPlacement *place)
{
    Vec3 scr;
    float num;
    int k;

    nj_copy8(&w->self, place, sizeof *place);
    nj_copy8(&w->arg, &w->self, sizeof *place);
    player_get_frame_vel(&scr);
    v3_sub(&w->self.pos, &w->self.pos, &scr);
    if (EN_ZAKU_GENERATOR_REC->test) {
        w->count = EN_ZAKU_GENERATOR_REC->generate_num;
        w->mode = EN_ZAKU_GENERATOR_REC->generate_pattern;
    } else {
        num = place->param4;
        w->count = num >= 1.0f ? (int)num : 1;
        w->mode = (int)(((float (*)(float))float_frac)(num) * 1000.0f + 0.5f);
    }
    if ((w->mode & 0x10) || (EN_ZAKU_GENERATOR_REC->test && (EN_ZAKU_GENERATOR_REC->generate_pattern & 0x10))) {
        k = w->count;
        do {
            en_zaku_generator_fill_arg(w);
            k--;
            en_zaku_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, (TypeRec *)&w->arg);
        } while (k);
        FINISH_IF(w);
    }
}

/* 0x00b01b58 */
void en_zaku_generator_release(void)
{
}

/* 0x00b01b60 */
void *en_zaku_generator_create(void *parent, void *place)
{
    return task_create_arg(parent, &en_zaku_generator_desc, 140, place);
}
