/*
 * Trance Mission uni generator: spawns its count one every frm_interval seconds or all at once,
 * moving with the player's frame; mode bits pick what is randomised (position, direction, roll).
 */

#include "stg23.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "nj_system.h"
#include "player.h"
#include "en_uni_generator.h"
#include "en_uni.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_uni_generator_params;
extern VarDef en_uni_generator_vardefs[8];
extern struct TaskDesc en_uni_generator_desc;
#define EN_UNI_GENERATOR_REC ((EnUniGeneratorParams *)&en_uni_generator_params)

/* 8 words after name and label */
typedef struct EnUniGeneratorParams {
    char *name;
    char *label;                /* 0x04 */
    float frm_interval;         /* 0x08 */
    float rnd_pos_min;          /* 0x0c */
    float rnd_pos_max;          /* 0x10 */
    float rnd_vec_min;          /* 0x14 */
    float rnd_vec_max;          /* 0x18 */
    int test;                   /* 0x1c: the two below override the placement's */
    int generate_num;           /* 0x20 */
    unsigned int generate_pattern; /* 0x24 */
} EnUniGeneratorParams;

/* 0x00afe2d8 */
void en_uni_generator_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_uni_generator_params.name;
    def.fmt = en_uni_generator_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_uni_generator_vardefs;
    fld = en_uni_generator_params.v;
    for (i = 0; i < 8; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_uni_generator_vardefs);
}

/* 0x00afe388 */
void en_uni_generator_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afe390 */
void en_uni_generator_rnd_pos(EnPlacement *base, EnPlacement *out)
{
    Vec3 v;

    util_rand_vec3_len(&v, RAND_FLOAT_A() * (EN_UNI_GENERATOR_REC->rnd_pos_max - EN_UNI_GENERATOR_REC->rnd_pos_min)
                         + EN_UNI_GENERATOR_REC->rnd_pos_min);
    v3_sum(&out->pos, &base->pos, &v);
}

/* 0x00afe440 */
void en_uni_generator_rnd_vec(EnGeneratorWork *w, EnPlacement *place)
{
    int ang[3];
    Vec3 fwd;
    float deg = ANG_PER_DEG;
    float rnd_max = 32769.0f;
    int pitch;

    v3_copy(&fwd, &w->self.dir);
    pitch = rand_lcg_next_a() / rnd_max * ((int)(EN_UNI_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_UNI_GENERATOR_REC->rnd_vec_min * deg))
        + (int)(EN_UNI_GENERATOR_REC->rnd_vec_min * deg);
    i3_set(ang, pitch, rand_lcg_next_a() / rnd_max * ((int)(EN_UNI_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_UNI_GENERATOR_REC->rnd_vec_min * deg))
                   + (int)(EN_UNI_GENERATOR_REC->rnd_vec_min * deg), 0);
    if (0.5f < rand_lcg_next_a() / rnd_max) {
        ang[0] = -ang[0];
        ang[1] = -ang[1];
        ang[2] = -ang[2];
    }
    util_rotate_vec3(&fwd, ang, &place->dir);
}

/* 0x00afe5e8 */
void en_uni_generator_rnd_rot(void *gen, EnPlacement *place)
{
    i3_set(place->ang, 0, 0, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
}

/* 0x00afe668 */
void en_uni_generator_fill_arg(EnGeneratorWork *w)
{
    unsigned int bits = w->mode;
    EnPlacement *place = &w->arg;

    if ((bits & 1) || (EN_UNI_GENERATOR_REC->test && (EN_UNI_GENERATOR_REC->generate_pattern & 1)))
        en_uni_generator_rnd_pos(&w->self, place);
    if ((bits & 4) || (EN_UNI_GENERATOR_REC->test && (EN_UNI_GENERATOR_REC->generate_pattern & 4)))
        en_uni_generator_rnd_rot(w, place);
    if ((bits & 2) || (EN_UNI_GENERATOR_REC->test && (EN_UNI_GENERATOR_REC->generate_pattern & 2)))
        en_uni_generator_rnd_vec(w, place);
}

/* 0x00afe758 */
void en_uni_generator_exec(EnGeneratorWork *w)
{
    Vec3 scr;

    player_get_frame_vel(&scr);
    v3_add(&w->self.pos, &scr);
    w->timer -= 1.0f;
    if (w->timer < 0.0f) {
        v3_copy(&w->arg.pos, &w->self.pos);
        en_uni_generator_fill_arg(w);
        en_uni_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, (TypeRec *)&w->arg);
        if (--w->count > 0)
            w->timer = EN_UNI_GENERATOR_REC->frm_interval * FPS;
        else
            FINISH_IF(w);
    }
}

/* Count in the value's whole part, mode bits in its thousandths; bit 8: all at once. */
/* steer(cast) */
/* 0x00afe850 */
void en_uni_generator_init(EnGeneratorWork *w, EnPlacement *place)
{
    Vec3 scr;
    float num;
    int k;

    nj_copy8(&w->self, place, sizeof *place);
    nj_copy8(&w->arg, place, sizeof *place);
    player_get_frame_vel(&scr);
    v3_sub(&w->self.pos, &w->self.pos, &scr);
    if (EN_UNI_GENERATOR_REC->test) {
        w->count = EN_UNI_GENERATOR_REC->generate_num;
        w->mode = EN_UNI_GENERATOR_REC->generate_pattern;
    } else {
        num = place->param4;
        w->count = num >= 1.0f ? (int)num : 1;
        w->mode = (int)(((float (*)(float))float_frac)(num) * 1000.0f + 0.5f);
    }
    if ((w->mode & 8) || (EN_UNI_GENERATOR_REC->test && (EN_UNI_GENERATOR_REC->generate_pattern & 8))) {
        k = w->count;
        do {
            en_uni_generator_fill_arg(w);
            k--;
            en_uni_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, (TypeRec *)&w->arg);
        } while (k);
        FINISH_IF(w);
    }
}

/* 0x00afe9c8 */
void en_uni_generator_release(void)
{
}

/* 0x00afe9d0 */
void *en_uni_generator_create(void *parent, void *place)
{
    return task_create_arg(parent, &en_uni_generator_desc, 140, place);
}
