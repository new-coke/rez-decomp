/*
 * Particle effects: "Ptcl" tasks emitting, moving and drawing sprite particles per ptcl.dat
 * (emitter shape, rate, speed, life, size/colour curves, gravity, drag, spin, attractor), in
 * emitter or world space, one-shot or looping. Also loads and indexes ptcl.dat.
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "ptcl.h"
#include "debug.h"
#include "file.h"
#include "fog.h"
#include "heap.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "yuki_tool.h"
#include "system.h"

typedef struct PtclCfgBlock { int word[43]; } PtclCfgBlock;

/* steer(alias): the code that matched reads PtclWork through these member types; one view
 * shared by those sites (src/game/ptcl.c). */
typedef struct PtclWorkMaxView {
    int count;
    int max;
    void *part;
    PtclCfgBlock cfg;
    void (*cb)(int);
    int cbarg;
    float *eye_src;
    int *rot_src;
    float *move;
    int loop;
    int rotate;
    int drift_mode;
} PtclWorkMaxView;

typedef struct PtclArg { PtclCfgBlock *cfg; int loop; int rotate; int drift_mode; } PtclArg;

/* steer(alias): the code that matched reads PtclWork through these member types; one view
 * shared by those sites (src/game/ptcl.c). */
typedef struct PtclWorkPartView {
    char pad_00[8];
    void *part;
    char pad_0c[44];
    void *sub;
    char pad_3c[12];
    Vec3 eye;
    IVec3 rot;
    char pad_60[88];
    void (*cb)(void *, int);
    int cbarg;
} PtclWorkPartView;

/* Rotations of a 3-vector in place. */
#define PTCL_ROT_X(v, ang)                                                                   \
    do {                                                                                     \
        float rot_s, rot_c;                                                                  \
        float y = (v)[1];                                                                    \
        float z = (v)[2];                                                                    \
                                                                                             \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        (v)[1] = y * rot_c - z * rot_s;                                                      \
        (v)[2] = y * rot_s + z * rot_c;                                                      \
    } while (0)

#define PTCL_ROT_Y(v, ang)                                                                   \
    do {                                                                                     \
        float rot_s, rot_c;                                                                  \
        float x = (v)[0];                                                                    \
        float z = (v)[2];                                                                    \
                                                                                             \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        (v)[0] = x * rot_c + z * rot_s;                                                      \
        (v)[2] = -x * rot_s + z * rot_c;                                                     \
    } while (0)

#define PTCL_ROT_Z(v, ang)                                                                   \
    do {                                                                                     \
        float rot_s, rot_c;                                                                  \
        float x = (v)[0];                                                                    \
        float y = (v)[1];                                                                    \
                                                                                             \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        (v)[0] = x * rot_c - y * rot_s;                                                      \
        (v)[1] = x * rot_s + y * rot_c;                                                      \
    } while (0)

/* steer(alias): the code that matched reads PtclWork through these member types; one view
 * shared by those sites (src/game/ptcl.c). */
typedef struct PtclWorkView {
    int count;
    char pad_04[4];
    struct PtclPart *vtx;
    char pad_0c[72];
    int ang[3];
    char pad_60[56];
    int remain;
    char pad_9c[8];
    Vec3 center;
    char pad_b0[28];
    int loop;
    int rotate;
} PtclWorkView;

#define ROT_Y(ang, x, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = z;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        x = rot_u * rot_c + rot_v * rot_s;                                                   \
        z = -rot_u * rot_s + rot_v * rot_c;                                                  \
    } while (0)

#define ROT_X(ang, y, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = y, rot_v = z;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        y = rot_u * rot_c - rot_v * rot_s;                                                   \
        z = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

#define ROT_Z(ang, x, y)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = y;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        x = rot_u * rot_c - rot_v * rot_s;                                                   \
        y = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

/* steer(alias): the code that matched reads PtclPart through these member types; one view
 * shared by those sites (src/game/ptcl.c). */
typedef struct PtclPartView {
    int age;
    int life;
    float size;
    float v[3];
    char pad_18[16];
} PtclPartView;

#define FREE(p) do { yuki_free(p); (p) = 0; } while (0)

extern int ptcl_texture;

extern void ulMatrixPush(void);
extern struct TaskDesc ptcl_task_desc;
extern struct TaskDesc ptcl_task_loop_desc;
extern struct TaskDesc ptcl_task_world_desc;
extern struct TaskDesc ptcl_task_world_loop_desc;
extern char ptcl_dat_file[];
extern PtclData ptcl_data;

static int ptcl_data_load(char *file, PtclData *data);
static void ptcl_rotate_yxz(int *ang);

/*
 * t in 0..1. Mode 0 constant 1, 1 rising, 2 falling, 3 up-down, 4 down-up, 5 half sine, 6 up,
 * hold, down; else 0.
 */
/* 0x00157070 */
static float ptcl_shape_curve(unsigned int mode, float t)
{
    float val = 0.0f;
    float sc[2];

    switch (mode) {
    case 0:
        val = 1.0f;
        break;
    case 1:
        val = t;
        break;
    case 2:
        val = 1.0f - t;
        break;
    case 3:
        if (t < 0.5f)
            val = t + t;
        else
            val = (1.0f - t) + (1.0f - t);
        break;
    case 4:
        if (t < 0.5f)
            val = (0.5f - t) + (0.5f - t);
        else
            val = (t - 0.5f) + (t - 0.5f);
        break;
    case 5:
        ulSinCos((int)(t * ANG_HALF), &sc[0], &sc[1]);
        val = sc[0];
        break;
    case 6:
        if (t < 0.25f)
            val = t * 4.0f;
        else if (t > 0.75f)
            val = (1.0f - t) * 4.0f;
        else
            val = 1.0f;
        break;
    }
    return val;
}

/*
 * Own copy of the description and sprite table; particle array sized for the most alive at once;
 * display list 6.
 */
/* 0x001571a0 */
static void ptcl_task_init(PtclWorkMaxView *w, PtclArg *prm)
{
    int *param = w->cfg.word;
    PtclCfgBlock *src = prm->cfg;
    int *em = (int *)w + 17;
    int val;
    int n;
    int cnt;
    int rotate;
    int drift;

    w->cfg = *src;
    param[11] = (int)yuki_alloc(param[10] * 20);
    nj_copy8((void *)param[11], (void *)src->word[11], param[10] * 20);
    n = (int)((float)param[0] + fabsf((float)param[1]));
    w->max = n;
    w->loop = prm->loop;
    if (prm->loop == 0) {
        w->max = em[19] < n ? em[19] : n;
    }
    cnt = (w->max + em[18]) / (1 + em[18]) * em[16];
    w->part = 0;
    w->max = cnt;
    if (cnt > 0)
        w->part = yuki_alloc(40 * cnt);
    w->count = 0;
    em[20] = 0;
    drift = prm->drift_mode;
    em[21] = em[19];
    w->eye_src = 0;
    rotate = prm->rotate;
    w->rot_src = 0;
    w->move = 0;
    w->drift_mode = drift;
    w->rotate = rotate;
    task_set_dlist(w, 6);
}

/* Calls the end callback, if any. */
/* 0x001573a8 */
static void ptcl_task_release(PtclWorkPartView *w)
{
    if (w->cb)
        w->cb(w, w->cbarg);
    yuki_free(w->sub);
    w->sub = 0;
    do {
        if (w->part) {
            yuki_free(w->part);
            w->part = 0;
        }
    } while (0);
}

static inline void ptcl_vscale(float *vec, float scale)
{
    vec[0] *= scale;
    vec[1] *= scale;
    vec[2] *= scale;
}

static inline void ptcl_vset(float *dst, float x, float y, float z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

/*
 * Emitter space. Particles: velocity, drag, move, spin, ring Y orbit, gravity, attractor pull over
 * progress. Emitter counts down (holds if looping); every interval frames emits rate x rate curve.
 */
/* 0x00157408 */
static void ptcl_task_exec(PtclWork *w)
{
    float drift[3];
    float ofs[3];
    PtclCfg *cfg = &w->cfg;
    PtclEmit *em = &cfg->em;
    PtclAttr *attr = &cfg->at;
    float *grav = &em->gravity;
    PtclPart *p = w->part;
    float prog;
    int i;
    {
        float rate = (float)(em->duration - em->remain) / (float)em->duration;
        prog = (1.0f < rate) ? 1.0f : rate;
    }
    if (w->eye_src) {
        float *s_ = w->eye_src;
        float *d_ = em->eye;

        em->eye[0] = s_[0];
        d_[1] = s_[1];
        d_[2] = s_[2];
    }
    if (w->rot_src) {
        int *s_ = w->rot_src;
        int *d_ = em->rot;

        em->rot[0] = s_[0];
        d_[1] = s_[1];
        d_[2] = s_[2];
    }
    if (w->move == 0) {
        *(int *)&drift[0] = 0;
        *(int *)&drift[1] = 0;
        *(int *)&drift[2] = 0;
    } else {
        float x = w->move[0];
        float y = w->move[1];
        float z = w->move[2];
        do {
            float s, co, old_x = x, old_z = z;
            ulSinCos(-em->rot[1], &s, &co);
            x = old_x * co + old_z * s;
            z = -old_x * s + old_z * co;
        } while (0);
        do {
            float s, co, old_y = y, old_z = z;
            ulSinCos(-em->rot[0], &s, &co);
            y = old_y * co - old_z * s;
            z = old_y * s + old_z * co;
        } while (0);
        do {
            float s, co, old_x = x, old_y = y;
            ulSinCos(-em->rot[2], &s, &co);
            x = old_x * co - old_y * s;
            y = old_x * s + old_y * co;
        } while (0);
        drift[0] = x;
        drift[1] = y;
        drift[2] = z;
    }
    for (i = 0; i < w->count; i++, p++) {
        p->pos[0] += p->vel[0];
        p->pos[1] += p->vel[1];
        p->pos[2] += p->vel[2];
        ptcl_vscale(p->vel, em->damp);
        p->pos[0] += drift[0];
        p->pos[1] += drift[1];
        p->pos[2] += drift[2];
        *(void **)&p->ang = (void *)((int)((float)p->ang + (float)cfg->spin * vec3_length(p->vel)));
        if (em->shape == 1) {
            float s, co;
            int a;
            a = (int)((float)em->orbit * ptcl_shape_curve(em->orbit_curve, (float)p->age / (float)p->life));
            do {
                float old_x = p->pos[0], old_z = p->pos[2];
                ulSinCos(a, &s, &co);
                p->pos[0] = old_x * co + old_z * s;
                p->pos[2] = -old_x * s + old_z * co;
            } while (0);
        }
        p->vel[1] -= *grav;
        p->age++;
        {
            float pull = attr->pull1 * prog + attr->pull0 * (1.0f - prog);
            ofs[0] = p->pos[0] - attr->center[0];
            ofs[1] = p->pos[1] - attr->center[1];
            ofs[2] = p->pos[2] - attr->center[2];
            ofs[0] *= pull;
            ofs[1] *= pull;
            ofs[2] *= pull;
            p->pos[0] = attr->center[0] + ofs[0];
            p->pos[1] = attr->center[1] + ofs[1];
            p->pos[2] = attr->center[2] + ofs[2];
        }
        if (!(p->age < p->life)) {
            *p = w->part[w->count - 1];
            i--;
            p--;
            w->count--;
        }
    }
    em->remain--;
    if (w->loop == 0) {
        if ((float)em->remain < -((float)cfg->life + fabsf((float)cfg->life_rnd))) {
            TASK_OF(w)->flags |= TASK_F_END;
            return;
        }
    } else
        em->remain = em->remain > 0 ? em->remain : 1;
    if (em->remain > 0 && em->wait-- <= 0) {
        float spd, vz, r;
        int n;

        p = &w->part[w->count];
        spd = em->speed * ptcl_shape_curve(em->speed_curve, prog);
        r = ptcl_shape_curve(em->rate_curve, prog);
        em->wait = em->interval;
        n = (int)((float)em->rate * r);
        if (!(n < w->max - w->count))
            n = w->max - w->count;
        for (i = 0; i < n; ) {
            float a, b, sin_th, cos_th, sin_ph, cos_ph;

            vz = em->vz + RAND_FLOAT_B() * em->vz_rnd;
            a = spd * (RAND_FLOAT_B() * (em->spread - 1.0f) + 1.0f);
            b = ptcl_shape_curve(em->vz_curve, prog);
            p->age = 0;
            vz *= b;
            p->life = (int)((float)cfg->life + (RAND_FLOAT_B() * 2.0f + -1.0f) * (float)cfg->life_rnd);
            p->size = cfg->size + (RAND_FLOAT_B() * 2.0f + -1.0f) * cfg->size_rnd;
            switch (cfg->ang_mode) {
            case 0:
                p->ang = 0;
                break;
            case 1:
                p->ang = (int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f);
                break;
            }
            switch ((unsigned int)em->shape) {
            case 0:
                r = RAND_FLOAT_B();
                ptcl_vset(p->pos, r * (spd + spd) - spd, 0.0f, 0.0f);
                ptcl_vset(p->vel, 0.0f, 0.0f, vz);
                break;
            case 1:
                ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_th, &cos_th);
                ptcl_vset(p->pos, cos_th * a, sin_th * a, 0.0f);
                ptcl_vset(p->vel, cos_th * em->radius, sin_th * em->radius, vz);
                break;
            case 2:
                ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_th, &cos_th);
                ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_ph, &cos_ph);
                ptcl_vset(p->pos, sin_th * cos_ph * a, sin_th * sin_ph * a, cos_th * a);
                ptcl_vset(p->vel, sin_th * cos_ph * vz, sin_th * sin_ph * vz, cos_th * vz);
                break;
            }
            i++;
            p++;
        }
        w->count += n;
    }
}

/*
 * Fog off, effect blend. Sprite by age, size by curve, colour birth -> middle -> end (halved to
 * the GS range 0..128); only in front of the camera.
 */
/* 0x00157cc0 */
static void ptcl_task_disp(PtclWork *w)
{
    float view[3];
    float scl[3];
    int fog;
    PtclCfg *param = &w->cfg;
    PtclPart *p = w->part;
    int i;

    fog = fog_set_enable(0);
    ulGxSetTextureList(ptcl_texture);
    draw_set_blend(param->blend0, param->blend1);
    ((int (*)(void))ulMatrixPush)(); /* steer(cast) */
    do {
        float *eye = w->cfg.em.eye;

        vu0_matrix_translate(ulMatrixCur, eye[0], eye[1], eye[2]);
    } while (0);
    ptcl_rotate_yxz(param->em.rot);
    for (i = 0; i < w->count; i++, p++) {
        float t = (float)p->age / (float)p->life;
        float sz = p->size * ptcl_shape_curve(param->size_curve, t);
        int n = param->nsub - 1;
        int k = (int)((float)param->nsub * t);
        int *sub;

        if (k < n)
            n = k;
        sub = param->sub + n * 5;
        ulMatrixApplyPoint(ulMatrixCur, p->pos, view);
        if (view[2] < -1.0f) {
            unsigned char *to, *from;
            float *qp = scl;
            float rate, inv_rate;
            int r, gr, bl, al;

            scl[2] = view[2];
            scl[0] = 1.0f;
            *(int *)&scl[1] = 0;
            project_point(view, view);
            project_point_abs(qp, qp);
            from = param->col[0];
            to = param->col[2];
            if (t < 0.5f) {
                rate = t + t;
                to = param->col[1];
            } else {
                from = param->col[1];
                rate = (t - 0.5f) + (t - 0.5f);
            }
            inv_rate = 1.0f - rate;
            al = (int)(to[3] * rate + from[3] * inv_rate);
            bl = (int)(to[2] * rate + from[2] * inv_rate);
            gr = (int)(to[1] * rate + from[1] * inv_rate);
            r = (int)(to[0] * rate + from[0] * inv_rate);
            if (r < 0)
                r = 0;
            if (bl < 0)
                bl = 0;
            if (al < 0)
                al = 0;
            if (gr < 0)
                gr = 0;
            if (r >= 256)
                r = 255;
            if (al >= 256)
                al = 255;
            if (bl >= 256)
                bl = 255;
            if (gr >= 256)
                gr = 255;
            yuki_sprite_draw(p->ang,
                         ((unsigned)((al + 1) >> 1) << 24 | (((bl + 1) >> 1) & 0xff)) |
                             ((((gr + 1) >> 1) & 0xff) << 8 | (((r + 1) >> 1) & 0xff) << 16),
                         sub[0], view[0], view[1], 1.0f / view[2], scl[0] * sz, scl[0] * sz);
        }
    }
    ulMatrixPop();
    fog_set_enable(fog);
}

/*
 * World space: drift with the player (drift_mode 1 movement, 2 frame velocity) plus move vector;
 * attractor and ring orbit in emitter space; new particles placed by emitter position, angles.
 */
/* 0x00158018 */
static void ptcl_task_exec_world(PtclWork *w)
{
    float cw[3];
    float drift[3];
    float ofs[3];
    PtclCfg *cfg = &w->cfg;
    PtclEmit *em = &cfg->em;
    PtclAttr *attr = &cfg->at;
    float *eye = em->eye;
    int *rot = em->rot;
    PtclPart *p = w->part;
    float *grav = &em->gravity;
    float prog;
    int i;

    {
        float rate = (float)(em->duration - em->remain) / (float)em->duration;
        prog = (1.0f < rate) ? 1.0f : rate;
    }
    if (w->eye_src) {
        eye[0] = w->eye_src[0];
        eye[1] = w->eye_src[1];
        eye[2] = w->eye_src[2];
    }
    if (w->rot_src) {
        rot[0] = w->rot_src[0];
        rot[1] = w->rot_src[1];
        rot[2] = w->rot_src[2];
    }
    switch (w->drift_mode) {
    case 1:
        player_get_vec(drift);
        break;
    case 2:
        player_get_frame_vel(drift);
        break;
    default:
        drift[0] = 0.0f;
        drift[1] = 0.0f;
        drift[2] = 0.0f;
        break;
    }
    if (w->move) {
        drift[0] += w->move[0];
        drift[1] += w->move[1];
        drift[2] += w->move[2];
    }
    {
        float x = attr->center[0];
        float y = attr->center[1];
        float z = attr->center[2];

        do {
            float s, co, old_x = x, old_y = y;

            ulSinCos(rot[2], &s, &co);
            x = old_x * co - old_y * s;
            y = old_x * s + old_y * co;
        } while (0);
        do {
            float s, co, old_y = y, old_z = z;

            ulSinCos(rot[0], &s, &co);
            y = old_y * co - old_z * s;
            z = old_y * s + old_z * co;
        } while (0);
        do {
            float s, co, old_x = x, old_z = z;

            ulSinCos(rot[1], &s, &co);
            x = old_x * co + old_z * s;
            z = -old_x * s + old_z * co;
        } while (0);
        cw[0] = x + eye[0];
        cw[1] = y + eye[1];
        cw[2] = z + eye[2];
    }
    for (i = 0; i < w->count; i++, p++) {
        p->pos[0] += p->vel[0];
        p->pos[1] += p->vel[1];
        p->pos[2] += p->vel[2];
        ptcl_vscale(p->vel, em->damp);
        p->pos[0] += drift[0];
        p->pos[1] += drift[1];
        p->pos[2] += drift[2];
        *(void **)&p->ang = (void *)((int)((float)p->ang + (float)cfg->spin * vec3_length(p->vel)));
        if (em->shape == 1) {
            int a = (int)((float)em->orbit *
                          ptcl_shape_curve(em->orbit_curve, (float)p->age / (float)p->life));

            p->pos[0] -= eye[0];
            p->pos[1] -= eye[1];
            p->pos[2] -= eye[2];
            PTCL_ROT_Y(p->pos, -rot[1]);
            PTCL_ROT_X(p->pos, -rot[0]);
            PTCL_ROT_Z(p->pos, -rot[2]);
            PTCL_ROT_Y(p->pos, a);
            PTCL_ROT_Z(p->pos, rot[2]);
            PTCL_ROT_X(p->pos, rot[0]);
            PTCL_ROT_Y(p->pos, rot[1]);
            p->pos[0] += eye[0];
            p->pos[1] += eye[1];
            p->pos[2] += eye[2];
        }
        p->vel[1] -= *grav;
        p->age++;
        {
            float pull = attr->pull1 * prog + attr->pull0 * (1.0f - prog);

            ofs[0] = p->pos[0] - cw[0];
            ofs[1] = p->pos[1] - cw[1];
            ofs[2] = p->pos[2] - cw[2];
            ofs[0] *= pull;
            ofs[1] *= pull;
            ofs[2] *= pull;
            p->pos[0] = cw[0] + ofs[0];
            p->pos[1] = cw[1] + ofs[1];
            p->pos[2] = cw[2] + ofs[2];
        }
        if (!(p->age < p->life)) {
            *p = w->part[w->count - 1];
            i--;
            p--;
            w->count--;
        }
    }
    em->remain--;
    if (w->loop == 0) {
        if ((float)em->remain < -((float)cfg->life + fabsf((float)cfg->life_rnd))) {
            TASK_OF(w)->flags |= TASK_F_END;
            return;
        }
    } else
        do {
            em->remain = em->remain > 0 ? em->remain : 1;
        } while (0);
    if (em->remain > 0 && em->wait-- <= 0) {
        float spd, vz, r;
        int n;
        PtclPart *p;

        p = &w->part[w->count];
        spd = em->speed * ptcl_shape_curve(em->speed_curve, prog);
        r = ptcl_shape_curve(em->rate_curve, prog);
        em->wait = em->interval;
        n = (int)((float)em->rate * r);
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            vu0_matrix_translate(ulMatrixCur, eye[0], eye[1], eye[2]);
        } while (0);
        ptcl_rotate_yxz(rot);
        if (!(n < w->max - w->count))
            n = w->max - w->count;
        for (i = 0; i < n; ) {
            {
                float a, b, sin_th, cos_th, sin_ph, cos_ph;

                vz = em->vz + RAND_FLOAT_B() * em->vz_rnd;
                a = spd * (RAND_FLOAT_B() * (em->spread - 1.0f) + 1.0f);
                b = ptcl_shape_curve(em->vz_curve, prog);
                p->age = 0;
                vz *= b;
                p->life = (int)((float)cfg->life + (RAND_FLOAT_B() * 2.0f + -1.0f) * (float)cfg->life_rnd);
                p->size = cfg->size + (RAND_FLOAT_B() * 2.0f + -1.0f) * cfg->size_rnd;
                switch (cfg->ang_mode) {
                case 0:
                    p->ang = 0;
                    break;
                case 1:
                    p->ang = (int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f);
                    break;
                }
                switch ((unsigned int)em->shape) {
                case 0:
                    r = RAND_FLOAT_B();
                    ptcl_vset(p->pos, r * (spd + spd) - spd, 0.0f, 0.0f);
                    ptcl_vset(p->vel, 0.0f, 0.0f, vz);
                    break;
                case 1:
                    ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_th, &cos_th);
                    ptcl_vset(p->pos, cos_th * a, sin_th * a, 0.0f);
                    ptcl_vset(p->vel, cos_th * em->radius, sin_th * em->radius, vz);
                    break;
                case 2:
                    ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_th, &cos_th);
                    ulSinCos((int)DEG_TO_ANG(RAND_FLOAT_B() * 360.0f), &sin_ph, &cos_ph);
                    ptcl_vset(p->pos, sin_th * cos_ph * a, sin_th * sin_ph * a, cos_th * a);
                    ptcl_vset(p->vel, sin_th * cos_ph * vz, sin_th * sin_ph * vz, cos_th * vz);
                    break;
                default:
                    printf("Illigal switch ptcl.c : Exit2()\n");
                    break;
                }
                ulMatrixApplyPoint(ulMatrixCur, p->pos, p->pos);
                ulMatrixApplyVector(ulMatrixCur, p->vel, p->vel);
                i++;
                p++;
            }
        }
        w->count += n;
        ulMatrixPop();
    }
}

/* No emitter placement; culled against the view by particle size. */
/* 0x00158bd8 */
static void ptcl_task_disp_world(PtclWork *w)
{
    float view[3];
    float scl[3];
    int fog;
    PtclCfg *param = &w->cfg;
    PtclPart *p = w->part;
    int i;

    fog = fog_set_enable(0);
    ulGxSetTextureList(ptcl_texture);
    draw_set_blend(param->blend0, param->blend1);
    for (i = 0; i < w->count; i++, p++) {
        float t = (float)p->age / (float)p->life;
        float sz = p->size * ptcl_shape_curve(param->size_curve, t);

        if (view_sphere_visible(p->pos, sz)) {
            int n = param->nsub - 1;
            int k = (int)((float)param->nsub * t);
            int *sub;
            unsigned char *to, *from;
            float *qp = scl;
            float rate, inv_rate;
            int r, gr, bl, al;

            if (k < n)
                n = k;
            sub = param->sub + n * 5;
            ulMatrixApplyPoint(ulMatrixCur, p->pos, view);
            scl[2] = view[2];
            scl[0] = 1.0f;
            *(int *)&scl[1] = 0;
            project_point(view, view);
            project_point_abs(qp, qp);
            from = param->col[0];
            to = param->col[2];
            if (t < 0.5f) {
                rate = t + t;
                to = param->col[1];
            } else {
                from = param->col[1];
                rate = (t - 0.5f) + (t - 0.5f);
            }
            inv_rate = 1.0f - rate;
            al = (int)(to[3] * rate + from[3] * inv_rate);
            bl = (int)(to[2] * rate + from[2] * inv_rate);
            gr = (int)(to[1] * rate + from[1] * inv_rate);
            r = (int)(to[0] * rate + from[0] * inv_rate);
            if (r < 0)
                r = 0;
            if (bl < 0)
                bl = 0;
            if (al < 0)
                al = 0;
            if (gr < 0)
                gr = 0;
            if (r >= 256)
                r = 255;
            if (al >= 256)
                al = 255;
            if (bl >= 256)
                bl = 255;
            if (gr >= 256)
                gr = 255;
            yuki_sprite_draw(p->ang,
                         ((unsigned)((al + 1) >> 1) << 24 | (((bl + 1) >> 1) & 0xff)) |
                             ((((gr + 1) >> 1) & 0xff) << 8 | (((r + 1) >> 1) & 0xff) << 16),
                         sub[0], view[0], view[1], 1.0f / view[2], scl[0] * sz, scl[0] * sz);
        }
    }
    fog_set_enable(fog);
}

/* One-shot, emitter space; cb(cbarg) when it ends. */
/* 0x00158f08 */
PtclWorkMaxView *ptcl_task_create(void *parent, int cfg, void (*cb)(int), int cbarg)
{
    PtclArg args;
    PtclWorkMaxView *w;

    args.cfg = cfg;
    args.loop = 0;
    args.rotate = 1;
    args.drift_mode = 0;
    w = task_create_arg(parent, &ptcl_task_desc, 216, &args);
    w->cb = cb;
    w->cbarg = cbarg;
    return w;
}

/* Looping (ended with ptcl_task_stop). */
/* 0x00158f68 */
PtclWorkMaxView *ptcl_task_create_loop(void *parent, int cfg, void (*cb)(int), int cbarg)
{
    PtclArg args;
    PtclWorkMaxView *w;

    args.cfg = cfg;
    args.loop = 1;
    args.rotate = 1;
    args.drift_mode = 0;
    w = task_create_arg(parent, &ptcl_task_loop_desc, 216, &args);
    w->cb = cb;
    w->cbarg = cbarg;
    return w;
}

/* World space; drift as in ptcl_task_exec_world. */
/* 0x00158fc8 */
PtclWorkMaxView *ptcl_task_create_world(void *parent, int cfg, int drift, void (*cb)(int), int cbarg)
{
    PtclArg args;
    PtclWorkMaxView *w;

    args.cfg = cfg;
    args.loop = 0;
    args.rotate = 0;
    args.drift_mode = drift;
    w = task_create_arg(parent, &ptcl_task_world_desc, 216, &args);
    w->cb = cb;
    w->cbarg = cbarg;
    return w;
}

/* Looping. */
/* 0x00159028 */
static PtclWorkMaxView *ptcl_task_create_world_loop(void *parent, int cfg, int drift, void (*cb)(int), int cbarg)
{
    PtclArg args;
    PtclWorkMaxView *w;

    args.cfg = cfg;
    args.loop = 1;
    args.rotate = 0;
    args.drift_mode = drift;
    w = task_create_arg(parent, &ptcl_task_world_loop_desc, 216, &args);
    w->cb = cb;
    w->cbarg = cbarg;
    return w;
}

/* Effect ends once its particles are gone. */
/* 0x00159088 */
void ptcl_task_stop(PtclWorkView *w)
{
    w->loop = 0;
    w->remain = 0;
}

/* Each optional. */
/* 0x00159098 */
static void ptcl_task_set_pos_rot(PtclWorkPartView *w, Vec3 *pos, IVec3 *rot)
{
    if (pos)
        c13_v3copy(&w->eye, pos);
    if (rot)
        c13_i3copy(&w->rot, rot);
}

/* World vector, taken into emitter space when the effect has one. */
/* 0x001590e8 */
static void ptcl_task_offset_elems(PtclWorkView *w, float *in)
{
    PtclPart *vtx = w->vtx;
    float ofs[3];
    int n;
    float *pos;

    ofs[0] = in[0];
    ofs[1] = in[1];
    ofs[2] = in[2];
    if (w->rotate) {
        int *ang = w->ang;

        ROT_Y(ang[1], ofs[0], ofs[2]);
        ROT_X(ang[0], ofs[1], ofs[2]);
        ROT_Z(ang[2], ofs[0], ofs[1]);
    }
    n = w->count;
    if (n > 0) {
        pos = vtx[0].pos;
        do {
            pos[0] += ofs[0];
            pos[1] += ofs[1];
            pos[2] += ofs[2];
            pos += 10;
        } while (--n);
    }
}

/* 0x00159268 */
static void ptcl_task_set_center(PtclWorkView *w, Vec3 *center)
{
    c13_v3copy(&w->center, center);
}

/* Each optional. */
/* 0x00159288 */
void ptcl_task_set_src(PtclWorkMaxView *w, int eye_src, int rot_src)
{
    w->eye_src = eye_src;
    w->rot_src = rot_src;
}

/* Added to every particle each frame. */
/* 0x00159298 */
static void ptcl_task_set_move(PtclWorkMaxView *w, int move)
{
    w->move = move;
}

/* 0x001592a0 */
static int ptcl_task_count_live(PtclWorkMaxView *w)
{
    int n = 0;
    int i;
    PtclPart *part = w->part;

    for (i = 0; i < w->count; i++, part++)
        n += part->age >= 0;
    return n;
}

/* Position in out; returns its size, 0 when there is none. */
/* 0x001592e0 */
static float ptcl_task_get_live_elem(PtclWorkMaxView *w, int k, float *out)
{
    int i;
    int n = w->count;
    PtclPartView *ent = w->part;
    PtclCfg *param;

    for (i = 0; i < n; i++) {
        if (ent[i].age >= 0 && k-- == 0) {
            float *pos = ent[i].v;
            param = &w->cfg;
            out[0] = pos[0];
            out[1] = pos[1];
            out[2] = pos[2];
            return ent[i].size * ptcl_shape_curve(param->size_curve, (float)ent[i].age / (float)ent[i].life);
        }
    }
    return 0.0f;
}

/* 0x00159388 */
static void ptcl_task_get_draw_data(PtclTask *w, float *out_pos, float *out_size)
{
    int i;
    PtclElem *elem = w->elem;
    int n = w->count;

    for (i = 0; i < n; i++, elem++) {
        if (elem->age >= 0) {
            float *pos = elem->pos;
            PtclEntData *ed = &w->ed;
            out_pos[0] = pos[0];
            out_pos[1] = pos[1];
            out_pos[2] = pos[2];
            *out_size = elem->size * ptcl_shape_curve(ed->curve, (float)elem->age / (float)elem->life);
            out_pos += 3;
            out_size++;
        }
    }
}

static inline void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* World pos, taken into emitter space when the effect has one. */
/* 0x00159450 */
static void ptcl_task_set_elem_pos(PtclTask *w, int idx, float *pos)
{
    PtclElem *elem = w->elem + idx;
    float rel[3];

    if (w->rotate == 0) {
        vcopy(elem->pos, pos);
        return;
    }
    {
        float *eye = w->ed.eye;
        int *rot = w->ed.rot;

        rel[0] = pos[0] - eye[0];
        rel[1] = pos[1] - eye[1];
        rel[2] = pos[2] - eye[2];
        ROT_Y(rot[1], rel[0], rel[2]);
        ROT_X(rot[0], rel[1], rel[2]);
        ROT_Z(rot[2], rel[0], rel[1]);
    }
    vcopy(elem->pos, rel);
}

/* 0x001595e0 */
static void *ptcl_task_get_cfg(char *w)
{
    return w + 12;
}

/* 0x001595e8 */
int ptcl_data_load_default(void)
{
    return ptcl_data_load(ptcl_dat_file, &ptcl_data);
}

static inline unsigned char *cpy(void *dst, unsigned char *src, unsigned n)
{
    unsigned char *out = dst;
    unsigned i;

    for (i = 0; i < n; i++)
        *out++ = *src++;
    return src;
}

/* Effect descriptions with their sprite tables, in heap 0. Returns the effect count. */
/* 0x00159610 */
static int ptcl_data_load(char *file, PtclData *data)
{
    unsigned char *buf;
    unsigned char *src;
    PtclEntData *ent;
    int old_heap;
    int i;
    int k;

    if (file == 0)
        file = ptcl_dat_file;
    buf = file_load_sync(file, 0, 0);
    src = cpy(data, buf, 4);
    old_heap = heap_set_current(0);
    ent = yuki_alloc(data->count * 172);
    data->ent = ent;
    for (i = 0; i < data->count; i++) {
        src = cpy(ent, src, 172);
        ent->sub = yuki_alloc(ent->nsub * 20);
        for (k = 0; k < ent->nsub; k++) {
            PtclSub *dst = ent->sub;
            dst += k;
            src = cpy(dst, src, 20);
        }
        ent++;
    }
    heap_set_current(old_heap);
    file_heap_free(buf);
    return data->count;
}

/* Null: ptcl_data. */
/* 0x001597c0 */
void ptcl_data_free(PtclData *data)
{
    int i;
    PtclEntData *ent;

    if (data == 0)
        data = &ptcl_data;
    for (i = 0, ent = data->ent; i < data->count; i++, ent++)
        FREE(ent->sub);
    yuki_free(data->ent);
    data->count = 0;
    data->ent = 0;
}

/* Null: ptcl_data. */
/* 0x00159840 */
int ptcl_data_get_count(PtclData *data)
{
    data = data ? data : &ptcl_data;
    return data->count;
}

/* idx wraps; null: ptcl_data. */
/* 0x00159858 */
PtclEntData *ptcl_data_get_entry(PtclData *data, int idx)
{
    data = data ? data : &ptcl_data;
    return &data->ent[idx % data->count];
}

/* Null: ptcl_data. */
/* 0x00159898 */
void ptcl_data_set(PtclData *data, void *ent, int count)
{
    data = data ? data : &ptcl_data;
    data->ent = ent;
    data->count = count;
}

/* 0x001598b8 */
static void ptcl_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
