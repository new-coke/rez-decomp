/*
 * Trance Mission gufu generator: spawns its count one every frm_interval seconds or all at once,
 * moving with the player's frame; mode bits pick what is randomised (position, direction, roll,
 * escape direction, timing).
 */

#include "stg23.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "nj_system.h"
#include "player.h"
#include "en_gufu_generator.h"
#include "en_gufu.h"
#include "../common/enemy_vars.h"
#include "../common/util.h"

extern ParamRec en_gufu_generator_params;
extern VarDef en_gufu_generator_vardefs[14];
extern struct TaskDesc en_gufu_generator_desc;

/* name, label, then floats */
typedef struct EnGufuGeneratorParams {
    char *name;
    char *label;                /* 0x04 */
    float frm_interval;         /* 0x08 */
    float rnd_pos_min;          /* 0x0c */
    float rnd_pos_max;          /* 0x10 */
    float rnd_vec_min;          /* 0x14 */
    float rnd_vec_max;          /* 0x18 */
    float rnd_vec_esc_min;      /* 0x1c */
    float rnd_vec_esc_max;      /* 0x20 */
    float rnd_frm_start_min;    /* 0x24 */
    float rnd_frm_start_max;    /* 0x28 */
    float rnd_frm_drift_min;    /* 0x2c */
    float rnd_frm_drift_max;    /* 0x30 */
    int test;                   /* 0x34: the two below override the placement's */
    int generate_num;           /* 0x38 */
    unsigned int generate_pattern; /* 0x3c */
} EnGufuGeneratorParams;

#define EN_GUFU_GENERATOR_REC ((EnGufuGeneratorParams *)&en_gufu_generator_params)

/* 0x00afc6b0 */
void en_gufu_generator_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_gufu_generator_params.name;
    def.fmt = en_gufu_generator_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_gufu_generator_vardefs;
    fld = en_gufu_generator_params.v;
    for (i = 0; i < 14; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_gufu_generator_vardefs);
}

/* 0x00afc760 */
void en_gufu_generator_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00afc768 */
void en_gufu_generator_rnd_pos(EnPlacement *base, EnPlacement *out)
{
    Vec3 v;

    util_rand_vec3_len(&v, RAND_FLOAT_A() * (EN_GUFU_GENERATOR_REC->rnd_pos_max - EN_GUFU_GENERATOR_REC->rnd_pos_min)
                         + EN_GUFU_GENERATOR_REC->rnd_pos_min);
    v3_sum(&out->pos, &base->pos, &v);
}

/* 0x00afc818 */
void en_gufu_generator_rnd_vec(EnGeneratorWork *w, EnPlacement *place)
{
    int ang[3];
    Vec3 fwd;
    float deg = ANG_PER_DEG;
    float rnd_max = 32769.0f;
    int pitch;

    v3_copy(&fwd, &w->self.dir);
    pitch = rand_lcg_next_a() / rnd_max * ((int)(EN_GUFU_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_GUFU_GENERATOR_REC->rnd_vec_min * deg))
        + (int)(EN_GUFU_GENERATOR_REC->rnd_vec_min * deg);
    i3_set(ang, pitch, rand_lcg_next_a() / rnd_max * ((int)(EN_GUFU_GENERATOR_REC->rnd_vec_max * deg) - (int)(EN_GUFU_GENERATOR_REC->rnd_vec_min * deg))
                   + (int)(EN_GUFU_GENERATOR_REC->rnd_vec_min * deg), 0);
    if (0.5f < rand_lcg_next_a() / rnd_max) {
        ang[0] = -ang[0];
        ang[1] = -ang[1];
        ang[2] = -ang[2];
    }
    util_rotate_vec3(&fwd, ang, &place->dir);
}

/* 0x00afc9c0 */
void en_gufu_generator_rnd_rot(void *gen, EnPlacement *place)
{
    i3_set(place->ang, 0, 0, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
}

/* 0x00afca40 */
void en_gufu_generator_rnd_escape(EnGeneratorWork *w, EnPlacement *place)
{
    int ang[3];
    Vec3 esc;
    Vec3 *p;
    float deg = ANG_PER_DEG;
    int pitch;

    esc.x = w->self.param2;
    esc.y = w->self.param3;
    esc.z = w->self.param4;
    pitch = RAND_FLOAT_A() * ((int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_max * deg) - (int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_min * deg))
        + (int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_min * deg);
    i3_set(ang, pitch, RAND_FLOAT_A() * ((int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_max * deg) - (int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_min * deg))
                   + (int)(EN_GUFU_GENERATOR_REC->rnd_vec_esc_min * deg), 0);
    if (0.5f < RAND_FLOAT_A()) {
        ang[0] = -ang[0];
        ang[1] = -ang[1];
        ang[2] = -ang[2];
    }
    p = &esc;
    util_rotate_vec3(p, ang, p);
    do {
        place->param2 = p->x;
        place->param3 = p->y;
        place->param4 = p->z;
    } while (0);
    place->time = (int)(RAND_FLOAT_A() * (EN_GUFU_GENERATOR_REC->rnd_frm_start_max * FPS - EN_GUFU_GENERATOR_REC->rnd_frm_start_min * FPS)
                    + EN_GUFU_GENERATOR_REC->rnd_frm_start_min * FPS);
    place->time += (int)(RAND_FLOAT_A() * (EN_GUFU_GENERATOR_REC->rnd_frm_drift_max * FPS - EN_GUFU_GENERATOR_REC->rnd_frm_drift_min * FPS)
                     + EN_GUFU_GENERATOR_REC->rnd_frm_drift_min * FPS) * 0.001f;
}

/* 0x00afccb0 */
void en_gufu_generator_fill_arg(EnGeneratorWork *w)
{
    unsigned int bits = w->mode;
    EnPlacement *place = &w->arg;

    if ((bits & 1) || (EN_GUFU_GENERATOR_REC->test && (EN_GUFU_GENERATOR_REC->generate_pattern & 1)))
        en_gufu_generator_rnd_pos(&w->self, place);
    if ((bits & 4) || (EN_GUFU_GENERATOR_REC->test && (EN_GUFU_GENERATOR_REC->generate_pattern & 4)))
        en_gufu_generator_rnd_rot(w, place);
    if ((bits & 2) || (EN_GUFU_GENERATOR_REC->test && (EN_GUFU_GENERATOR_REC->generate_pattern & 2)))
        en_gufu_generator_rnd_vec(w, place);
    if ((bits & 8) || (EN_GUFU_GENERATOR_REC->test && (EN_GUFU_GENERATOR_REC->generate_pattern & 8)))
        en_gufu_generator_rnd_escape(w, place);
}

/* 0x00afcdd8 */
void en_gufu_generator_exec(EnGeneratorWork *w)
{
    Vec3 scr;

    player_get_frame_vel(&scr);
    v3_add(&w->self.pos, &scr);
    w->timer -= 1.0f;
    if (w->timer < 0.0f) {
        v3_copy(&w->arg.pos, &w->self.pos);
        en_gufu_generator_fill_arg(w);
        en_gufu_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, &w->arg);
        if (--w->count > 0)
            w->timer = EN_GUFU_GENERATOR_REC->frm_interval * FPS;
        else
            FINISH_IF(w);
    }
}

/* steer(cast) */
/* 0x00afced0 */
void en_gufu_generator_init(EnGeneratorWork *w, EnPlacement *place)
{
    Vec3 scr;
    float num;
    int k;

    nj_copy8(&w->self, place, sizeof *place);
    nj_copy8(&w->arg, place, sizeof *place);
    player_get_frame_vel(&scr);
    v3_sub(&w->self.pos, &w->self.pos, &scr);
    if (EN_GUFU_GENERATOR_REC->test) {
        w->count = EN_GUFU_GENERATOR_REC->generate_num;
        w->mode = EN_GUFU_GENERATOR_REC->generate_pattern;
    } else {
        num = place->param5;
        w->count = num >= 1.0f ? (int)num : 1;
        w->mode = (int)(((float (*)(float))float_frac)(num) * 1000.0f + 0.5f);
    }
    if ((w->mode & 0x10) || (EN_GUFU_GENERATOR_REC->test && (EN_GUFU_GENERATOR_REC->generate_pattern & 0x10))) {
        k = w->count;
        do {
            en_gufu_generator_fill_arg(w);
            k--;
            en_gufu_create(TASK_OF(w)->parent ? TASK_OF(w)->parent + 1 : 0, &w->arg);
        } while (k);
        FINISH_IF(w);
    }
}

/* 0x00afd048 */
void en_gufu_generator_release(void)
{
}

/* 0x00afd050 */
void *en_gufu_generator_create(void *parent, void *place)
{
    return task_create_arg(parent, &en_gufu_generator_desc, 140, place);
}
