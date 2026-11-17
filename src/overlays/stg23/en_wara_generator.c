/*
 * Trance Mission wara generator: keeps up to its limit alive, replacing each that goes until its
 * count is spent; mode bits pick what is randomised (position, direction, roll, wobble).
 */

#include "stg23.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "nj_system.h"
#include "player.h"
#include "en_wara_generator.h"
#include "en_wara.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_wara_generator_params;
extern VarDef en_wara_generator_vardefs[14];
extern struct TaskDesc en_wara_generator_desc;
#define EN_WARA_GENERATOR_REC ((EnWaraGeneratorParams *)&en_wara_generator_params)

/* 14 words after name and label */
typedef struct EnWaraGeneratorParams {
    char *name;
    char *label;                /* 0x04 */
    float rnd_pos_min;          /* 0x08 */
    float rnd_pos_max;          /* 0x0c */
    float rnd_vec_min;          /* 0x10 */
    float rnd_vec_max;          /* 0x14 */
    float spd_swing_min;        /* 0x18 */
    float spd_swing_max;        /* 0x1c */
    float r_swing_min;          /* 0x20 */
    float r_swing_max;          /* 0x24 */
    float rad_swing_min;        /* 0x28 */
    float rad_swing_max;        /* 0x2c */
    int test;                   /* 0x30: the two below override the placement's */
    int generate_num;           /* 0x34 */
    unsigned int generate_pattern; /* 0x38 */
    int sally_num_once;         /* 0x3c */
} EnWaraGeneratorParams;

typedef struct EnWaraGeneratorWork {
    EnPlacement self;
    EnPlacement arg;             /* 0x40: the next object's placement */
    int count;                  /* 0x80: objects left */
    int limit;                  /* 0x84: objects alive at once */
    unsigned int mode;          /* 0x88 */
} EnWaraGeneratorWork;

/* 0x00aff9c8 */
void en_wara_generator_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_wara_generator_params.name;
    def.fmt = en_wara_generator_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_wara_generator_vardefs;
    fld = en_wara_generator_params.v;
    for (i = 0; i < 14; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_wara_generator_vardefs);
}

/* 0x00affa78 */
void en_wara_generator_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00affa80 */
void en_wara_generator_rnd_pos(EnPlacement *base, EnPlacement *out)
{
    Vec3 v;

    util_rand_vec3_len(&v, RAND_FLOAT_A() * (EN_WARA_GENERATOR_REC->rnd_pos_max - EN_WARA_GENERATOR_REC->rnd_pos_min)
                         + EN_WARA_GENERATOR_REC->rnd_pos_min);
    v3_sum(&out->pos, &base->pos, &v);
}

/* 0x00affb30 */
void en_wara_generator_rnd_vec(EnWaraGeneratorWork *w, EnPlacement *place)
{
    int ang[3];
    Vec3 fwd;
    float deg = ANG_PER_DEG;
    float rnd_max = 32769.0f;
    int pitch;

    v3_copy(&fwd, &w->self.dir);
    pitch = rand_lcg_next_a() / rnd_max * ((int)(EN_WARA_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_WARA_GENERATOR_REC->rnd_vec_min * deg))
        + (int)(EN_WARA_GENERATOR_REC->rnd_vec_min * deg);
    i3_set(ang, pitch, rand_lcg_next_a() / rnd_max * ((int)(EN_WARA_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_WARA_GENERATOR_REC->rnd_vec_min * deg))
                   + (int)(EN_WARA_GENERATOR_REC->rnd_vec_min * deg), 0);
    if (0.5f < rand_lcg_next_a() / rnd_max) {
        ang[0] = -ang[0];
        ang[1] = -ang[1];
        ang[2] = -ang[2];
    }
    util_rotate_vec3(&fwd, ang, &place->dir);
}

/* 0x00affcd8 */
void en_wara_generator_rnd_rot(void *gen, EnPlacement *place)
{
    i3_set(place->ang, 0, 0, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
}

/* 0x00affd58 */
void en_wara_generator_rnd_swing(EnWaraGeneratorWork *w, EnPlacement *place)
{
    float rnd_max = 32769.0f;

    place->param2 = (int)(rand_lcg_next_a() / rnd_max * (EN_WARA_GENERATOR_REC->r_swing_max - EN_WARA_GENERATOR_REC->r_swing_min) + EN_WARA_GENERATOR_REC->r_swing_min);
    place->param2 += rand_lcg_next_a() / rnd_max * (EN_WARA_GENERATOR_REC->spd_swing_max - EN_WARA_GENERATOR_REC->spd_swing_min) + EN_WARA_GENERATOR_REC->spd_swing_min;
    place->param5 = rand_lcg_next_a() / rnd_max * (EN_WARA_GENERATOR_REC->rad_swing_max - EN_WARA_GENERATOR_REC->rad_swing_min) + EN_WARA_GENERATOR_REC->rad_swing_min;
}

/* 0x00affe48 */
void en_wara_generator_fill_arg(EnWaraGeneratorWork *w)
{
    unsigned int bits = w->mode;
    EnPlacement *place = &w->arg;

    if ((bits & 1) || (EN_WARA_GENERATOR_REC->test && (EN_WARA_GENERATOR_REC->generate_pattern & 1)))
        en_wara_generator_rnd_pos(&w->self, place);
    if ((bits & 4) || (EN_WARA_GENERATOR_REC->test && (EN_WARA_GENERATOR_REC->generate_pattern & 4)))
        en_wara_generator_rnd_rot(w, place);
    if ((bits & 2) || (EN_WARA_GENERATOR_REC->test && (EN_WARA_GENERATOR_REC->generate_pattern & 2)))
        en_wara_generator_rnd_vec(w, place);
    if ((bits & 8) || (EN_WARA_GENERATOR_REC->test && (EN_WARA_GENERATOR_REC->generate_pattern & 8)))
        en_wara_generator_rnd_swing(w, place);
}

/* 0x00afff70 */
int en_wara_generator_count_children(void *w)
{
    char *child = (char *)TASK_OF(w)->child;
    int n = 0;

    child = child ? child + 64 : 0;
    if (child) {
        do {
            child = (char *)TASK_OF(child)->next;
            n++;
        } while ((child = child ? child + 64 : 0) != 0);
    }
    return n;
}

/* 0x00afffb0 */
void en_wara_generator_exec(EnWaraGeneratorWork *w)
{
    Vec3 scr;
    int n;

    player_get_frame_vel(&scr);
    v3_add(&w->self.pos, &scr);
    if (w->count > 0) {
        n = w->limit - en_wara_generator_count_children(w);
        if (n > 0) {
            v3_copy(&w->arg.pos, &w->self.pos);
            if (!(n < w->count))
                n = w->count;
            w->count -= n;
            do {
                en_wara_generator_fill_arg(w);
                n--;
                en_wara_create(w, (TypeRec *)&w->arg);
            } while (n);
        }
    } else if (!en_wara_generator_count_children(w))
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00b000c8 */
void en_wara_generator_init(EnWaraGeneratorWork *w, EnPlacement *place)
{
    Vec3 scr;
    float num;

    nj_copy8(&w->self, place, sizeof *place);
    nj_copy8(&w->arg, place, sizeof *place);
    player_get_frame_vel(&scr);
    v3_sub(&w->self.pos, &w->self.pos, &scr);
    if (EN_WARA_GENERATOR_REC->test) {
        w->count = EN_WARA_GENERATOR_REC->generate_num;
        w->limit = EN_WARA_GENERATOR_REC->sally_num_once;
        w->mode = EN_WARA_GENERATOR_REC->generate_pattern;
    } else {
        num = place->param3;
        w->count = num >= 1.0f ? (int)num : 1;
        w->limit = place->param4 >= 1.0f ? (int)place->param4 : 1;
        w->mode = (int)(((float (*)(float))float_frac)(num) * 1000.0f + 0.5f);
    }
}

/* 0x00b001e0 */
void en_wara_generator_release(void)
{
}

/* 0x00b001e8 */
void *en_wara_generator_create(void *parent, void *place)
{
    return task_create_arg(parent, &en_wara_generator_desc, 144, place);
}
