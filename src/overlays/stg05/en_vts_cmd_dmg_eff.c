/*
 * Virtues effects, Area 5: enVtsCmdDmgEff, streaks thrown off the Virtues commander (and
 * Seraphim) when hit; enVirtuesLaser, the commander's laser: reaches a point on the path ahead,
 * draws a hexagon there and leaves an enVirtuesBase.
 */

#include "stg05.h"
#include "mathdef.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lens.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_vts_cmd_dmg_eff.h"
#include "en_virtues_base.h"
#include "../common/enemy_vars.h"

extern struct TaskDesc en_vts_cmd_dmg_eff_desc;

/* group name, label, 8 values */
typedef struct EnVirtuesLaserSet {
    char *name;
    char *label;                /* 0x04 */
    int v[8];                   /* 0x08 */
} EnVirtuesLaserSet;

extern EnVirtuesLaserSet en_virtues_laser_types[1];
extern VarDef en_virtues_laser_vardef;
extern VarDef en_virtues_laser_vardefs[8];
extern EnShatterSet en_vts_cmd_dmg_eff_types[3];
extern VarDef en_vts_cmd_dmg_eff_vardef;
extern VarDef en_vts_cmd_dmg_eff_vardefs[15];

/* 28 bytes */
typedef struct EnVtsCmdDmgEffPoint {
    int active;
    Vec3 dir;                   /* 0x04 */
    int frames;                 /* 0x10: left */
    int total;                  /* 0x14 */
    float dist;                 /* 0x18 */
} EnVtsCmdDmgEffPoint;

/* 8 bytes */
typedef struct EnVtsCmdDmgEffArg {
    int idx;
    Vec3 *pos;                  /* 0x04 */
} EnVtsCmdDmgEffArg;

typedef struct EnVtsCmdDmgEffWork {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnVtsCmdDmgEffRec *type;     /* 0x08 */
    EnVtsCmdDmgEffArg arg;              /* 0x0c */
    int frames;                 /* 0x14: left */
    int n;                      /* 0x18: particles */
    EnVtsCmdDmgEffPoint *buf;              /* 0x1c: freed with yuki_free */
} EnVtsCmdDmgEffWork;

/* view of en_vts_cmd_dmg_eff_types */
typedef struct EnVtsCmdDmgEffRec {
    char *name;
    char *label;                /* 0x04 */
    int per;                    /* 0x08: particles started per frame */
    float sec;                  /* 0x0c: seconds of the effect */
    float disp;                 /* 0x10: particle seconds */
    float disp_rnd;             /* 0x14: and their random part */
    float sec_pow;
    int col[3];                 /* 0x1c */
    float rate;
    float len;
    float width;
    float dist;                 /* 0x34 */
    float dist_rnd;             /* 0x38 */
    int angle;                  /* 0x3c: spread in angle units */
    float alpha;
} EnVtsCmdDmgEffRec;

/* 296 bytes */
typedef struct EnVirtuesLaserWork {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnVirtuesLaserRec *type;     /* 0x08 */
    Vec3 pos;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
    Vec3 *target;               /* 0x24: the placement point nearest to it */
    int count;                  /* 0x28: frames */
    int frames;                 /* 0x2c: the time unit of the shot */
    void *owner;                /* 0x30: the creator's work; 0 makes it fade */
    float beam;                 /* 0x34: 0..1, the beam toward the target */
    float ring;                 /* 0x38: 0..1, the hexagon drawn around it */
    float alpha;                /* 0x3c */
    char pad_40[12];
    Vec3 pts[7];                /* 0x4c: the hexagon corners (en_virtues_laser_calc_hexagon) */
    ModelPart body;                /* 0xa0 */
    Vec3 off;                   /* 0xfc */
    EnVirtuesBaseArg arg;               /* 0x108: the base it leaves (en_virtues_base_create) */
} EnVirtuesLaserWork;

/* view of en_virtues_laser_types */
typedef struct EnVirtuesLaserRec {
    char *name;
    char *label;                /* 0x04 */
    float reach;                /* 0x08: share of the time to reach the target */
    float vanish;               /* 0x0c: share of the time when it starts to vanish */
    unsigned int col[3];        /* 0x10 */
    float inner;                /* 0x1c: inner radius ratio */
    float length;               /* 0x20 */
    float width;                /* 0x24 */
} EnVirtuesLaserRec;

/* en_virtues_laser_init's argument */
typedef struct EnVirtuesLaserKind {
    int idx;
    void *owner;                /* 0x04 */
    EnVirtuesBaseArg *arg;              /* 0x08 */
} EnVirtuesLaserKind;

extern struct TaskDesc en_virtues_laser_desc;

/* 0x00b1ed58 */
void en_vts_cmd_dmg_eff_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_vts_cmd_dmg_eff_vardef);
    for (i = 0; i < 3; i++) {
        def = en_vts_cmd_dmg_eff_vardefs;
        val = en_vts_cmd_dmg_eff_types[i].v;
        en_vts_cmd_dmg_eff_vardef.name = en_vts_cmd_dmg_eff_types[i].name;
        en_vts_cmd_dmg_eff_vardef.fmt = en_vts_cmd_dmg_eff_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_vts_cmd_dmg_eff_vardef);
        for (j = 0; j < 15; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_vts_cmd_dmg_eff_vardefs);
    }
}

/* 0x00b1ee70 */
void en_vts_cmd_dmg_eff_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b1ee78 */
void en_vts_cmd_dmg_eff_init(EnVtsCmdDmgEffWork *w, EnVtsCmdDmgEffArg *prm)
{
    EnVtsCmdDmgEffRec *typ = (EnVtsCmdDmgEffRec *)&en_vts_cmd_dmg_eff_types[prm->idx];
    int size;

    w->type = typ;
    w->arg = *prm;
    w->frames = typ->sec * FPS;
    w->n = typ->per * (w->frames + 1);
    size = w->n * 28;
    w->buf = yuki_alloc(size);
    heap_bzero(w->buf, size);
}

/* 0x00b1ef18 */
void en_vts_cmd_dmg_eff_release(EnVtsCmdDmgEffWork *w)
{
    yuki_free(w->buf);
    w->buf = 0;
}

/* 0x00b1ef48 */
void en_vts_cmd_dmg_eff_exec(EnVtsCmdDmgEffWork *w)
{
    EnVtsCmdDmgEffRec *typ = w->type;
    EnVtsCmdDmgEffPoint *p;
    float sc[2][2];
    float x, y, z;
    int i, spawn;

    spawn = 0;
    p = w->buf;
    if ((typ->disp + typ->disp_rnd) * FPS < w->frames)
        spawn = typ->per;
    for (i = 0; i < w->n; i++, p++) {
        if (p->active) {
            if (--p->frames <= 0)
                p->active = 0;
        } else if (spawn-- > 0) {
            x = 0.0f;
            y = 0.0f;
            z = -1.0f;
            p->active = 1;
            ROT_X((unsigned int)((RAND_FLOAT_A() * 2.0f + -1.0f) * typ->angle), sc[0], y, z);
            ROT_Z(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), sc[1], x, y);
            v3_set(&p->dir, x, y, z);
            p->frames = p->total = (typ->disp + RAND_FLOAT_A() * typ->disp_rnd) * FPS;
            p->dist = typ->dist + RAND_FLOAT_A() * typ->dist_rnd;
        }
    }
    if (--w->frames <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00b1f1d0 */
void en_vts_cmd_dmg_eff_disp(EnVtsCmdDmgEffWork *w)
{
    EnVtsCmdDmgEffArg *prm = &w->arg;
    EnVtsCmdDmgEffPoint *p = w->buf;
    EnVtsCmdDmgEffRec *typ = w->type;
    int ang[2];
    Vec3 eye;
    Vec3 to_eye;
    Vec3 pts[2];
    Vec3 v;
    LineStyle lens;
    float sc[2][2];
    int dl;
    float x, y, z, rate, k;
    int i, al;
    unsigned int col;

    dl = RenderSetCurrentDisplayList(6);
    camera_get_eye(&eye);
    v3_sub(&to_eye, &eye, prm->pos);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-to_eye.x, -to_eye.z));
        ang[0] = RAD_TO_ANG(atan2f(to_eye.y, sqrtf(to_eye.x * to_eye.x + to_eye.z * to_eye.z)));
    } while (0);
    draw_set_blend(4, 1);
    for (i = 0; i < w->n; i++, p++) {
        if (p->active) {
            heap_bzero(&lens, 32);
            rate = safe_powf(p->frames / (float)p->total, typ->sec_pow);
            do {
                Vec3 *s_ = &p->dir;
                x = s_->x;
                z = s_->z;
                y = s_->y;
            } while (0);
            ROT_X(ang[0], sc[0], y, z);
            ROT_Y(ang[1], sc[1], x, z);
            v.x = x;
            v.y = y;
            v.z = z;
            v3_mul(&pts[0], &v, p->dist * (1.0f - rate));
            v3_add(&pts[0], prm->pos);
            k = typ->len;
            pts[1].x = v.x * k + pts[0].x;
            pts[1].y = v.y * k + pts[0].y;
            pts[1].z = v.z * k + pts[0].z;
            rate = rate * typ->alpha;
            {
                int r_, g_, b_;

                col = typ->col[0];
                r_ = ((int (*)(unsigned int))color_get_r)(col);
                g_ = color_get_g(col);
                b_ = color_get_b(col);
                al = ((int)(rate * 255.0f) + 1) >> 1 << 24;
                lens.col[0] = al | (((r_ + 1) >> 1 & 0xff) | (((g_ + 1) << 7 & 0xff00) | (((b_ + 1) >> 1 & 0xff) << 16)));
            }
            {
                int r_, g_, b_;

                col = typ->col[1];
                r_ = ((int (*)(unsigned int))color_get_r)(col);
                g_ = color_get_g(col);
                b_ = color_get_b(col);
                lens.col[1] = al | (((r_ + 1) >> 1 & 0xff) | (((g_ + 1) << 7 & 0xff00) | (((b_ + 1) >> 1 & 0xff) << 16)));
            }
            {
                int r_, g_, b_;

                col = typ->col[2];
                r_ = ((int (*)(unsigned int))color_get_r)(col);
                g_ = color_get_g(col);
                b_ = color_get_b(col);
                lens.col[2] = al | (((r_ + 1) >> 1 & 0xff) | (((g_ + 1) << 7 & 0xff00) | (((b_ + 1) >> 1 & 0xff) << 16)));
                lens.width = typ->width;
                lens.rate_in = typ->rate;
            }
            en_virtues_laser_draw_beam(&lens, pts, 1, 1);
        }
    }
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b1f608 */
void *en_vts_cmd_dmg_eff_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &en_vts_cmd_dmg_eff_desc, 32, prm);
}

/* 0x00b1f630 */
void en_virtues_laser_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_virtues_laser_vardef);
    for (i = 0; i < 1; i++) {
        def = en_virtues_laser_vardefs;
        val = en_virtues_laser_types[i].v;
        en_virtues_laser_vardef.name = en_virtues_laser_types[i].name;
        en_virtues_laser_vardef.fmt = en_virtues_laser_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_virtues_laser_vardef);
        for (j = 0; j < 8; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_virtues_laser_vardefs);
    }
}

/* 0x00b1f748 */
void en_virtues_laser_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b1f750 */
void en_virtues_laser_calc_hexagon(EnVirtuesLaserWork *w)
{
    float x, z, sx, cx, sy, cy;
    Vec3 *p = w->pts;
    float y;
    int i;

    for (i = 0; i < 7; i++, p++) {
        ulSinCos((i << 16) / 6 + 5461, &x, &z);
        x *= 11.0f;
        z *= 11.0f;
        y = 0.0f;
        do {
            float ry = y, rz = z;

            ulSinCos(w->ang[0], &sx, &cx);
            y = ry * cx - rz * sx;
            z = ry * sx + rz * cx;
        } while (0);
        do {
            float ry = x, rz = z;

            ulSinCos(w->ang[1], &sy, &cy);
            x = ry * cy + rz * sy;
            z = -ry * sy + rz * cy;
        } while (0);
        p->x = x;
        p->y = y;
        p->z = z;
        v3_add(p, &w->pos);
    }
}

/* 0x00b1f900 */
void en_virtues_laser_place_on_path(Vec3 *pos, int *ang, EnVirtuesBaseArg *prm)
{
    Vec3 p, dir;
    float sc[2][2];
    float x, y, z;

    if (!(prm->h < 0.0f)) {
        player_pass_pos_a(pos, prm->h);
        player_pass_pos_a(&p, prm->h + 1.0f);
        dir.x = pos->x - p.x;
        dir.y = 0.0f;
        dir.z = pos->z - p.z;
        do {
            ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        x = prm->off.x;
        z = prm->off.z;
        y = prm->off.y;
        ROT_X(ang[0], sc[0], y, z);
        ROT_Y(ang[1] + ANG_180, sc[1], x, z);
        ang[0] = 0;
        ang[2] = 0;
        pos->x += x;
        pos->y += y;
        pos->z += z;
    }
}

/* 0x00b1fae8 */
void en_virtues_laser_place(EnVirtuesLaserWork *w)
{
    en_virtues_laser_place_on_path(&w->pos, w->ang, &w->arg);
    en_virtues_laser_calc_hexagon(w);
}

/* steer(cast) */
/* 0x00b1fb20 */
void en_virtues_laser_init(EnVirtuesLaserWork *w, EnVirtuesLaserKind *k)
{
    EnVirtuesBaseArg *prm = k->arg;
    Vec3 *p;
    float best, dist_sq;
    int i;

    w->type = (EnVirtuesLaserRec *)&en_virtues_laser_types[k->idx];
    w->arg = *prm;
    v3_set(&w->off, prm->off.x, prm->off.y, prm->off.z);
    en_virtues_laser_place(w);
    p = prm->pts;
    best = vec3_distance_squared(p, &w->pos);
    w->target = p;
    for (i = 1, p++; i < prm->n; i++, p++) {
        dist_sq = vec3_distance_squared(p, &w->pos);
        if (dist_sq < best) {
            w->target = p;
            best = dist_sq;
        }
    }
    w->count = 0;
    w->frames = prm->frames;
    w->owner = k->owner;
    w->beam = 0.0f;
    w->ring = 0.0f;
    w->alpha = 1.0f;
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 23)), *((struct DcClip **)pnb_get_entry(stage_models, 24)), 0);
    w->body.radius = 10.0f;
}

/* 0x00b1fcf0 */
void en_virtues_laser_release(EnVirtuesLaserWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b1fcf8 */
void en_virtues_laser_exec(EnVirtuesLaserWork *w)
{
    EnVirtuesBaseArg *prm = &w->arg;
    EnVirtuesLaserRec *typ;
    int n;

    n = enemy_status_get_ticks();
    typ = w->type;
    while (n--) {
        w->count++;
        dc_object_advance_time(&w->body);
        if (w->body.frame < w->body.frames)
            continue;
        if (fabsf(w->ring - 1.0f) < FLT_EPSILON) {
            w->alpha -= 1.0f / (w->frames * (1.0f - typ->vanish));
            w->beam -= 1.0f / (w->frames * (1.0f - typ->vanish));
            if (w->alpha < 0.0f) {
                w->alpha = 0.0f;
                FINISH_IF(w);
                break;
            }
        } else if (!w->owner) {
            w->alpha -= 1.0f / (w->frames * (1.0f - typ->vanish));
            if (w->alpha < 0.0f) {
                w->alpha = 0.0f;
                FINISH_IF(w);
                break;
            }
        } else if (fabsf(w->beam - 1.0f) < FLT_EPSILON) {
            w->ring += 1.0f / (w->frames * (typ->vanish - typ->reach));
            w->ring = MIN(w->ring, 1.0f);
            if (fabsf(w->ring - 1.0f) < FLT_EPSILON)
                en_virtues_base_create(w, prm);
        } else {
            w->beam += 1.0f / (w->frames * typ->reach);
            w->beam = MIN(w->beam, 1.0f);
        }
    }
    prm->h -= game_pass_count_spd_get() * prm->fall * n;
    en_virtues_laser_place(w);
}

/* steer(cast) */
/* 0x00b1ff68 */
void en_virtues_laser_disp(EnVirtuesLaserWork *w)
{
    LineStyle lens;
    Vec3 pts[8];
    Vec3 dir;
    Vec3 back;
    EnVirtuesLaserRec *typ = w->type;
    int dl, n, nring, i, j;
    unsigned int col;
    float k;

    dl = RenderSetCurrentDisplayList(6);
    col = typ->col[0];
    lens.col[0] = ((int)(w->alpha * 255.0f) + 1) >> 1 << 24 | (((int (*)(unsigned int))color_get_r)(col) & 0xff | ((color_get_g(col) & 0xff) << 8 | (color_get_b(col) & 0xff) << 16));
    col = typ->col[1];
    lens.col[1] = ((int)(w->alpha * 255.0f) + 1) >> 1 << 24 | (((int (*)(unsigned int))color_get_r)(col) & 0xff | ((color_get_g(col) & 0xff) << 8 | (color_get_b(col) & 0xff) << 16));
    col = typ->col[2];
    lens.col[2] = ((int)(w->alpha * 255.0f) + 1) >> 1 << 24 | (((int (*)(unsigned int))color_get_r)(col) & 0xff | ((color_get_g(col) & 0xff) << 8 | (color_get_b(col) & 0xff) << 16));
    lens.width = typ->width;
    lens.rate_in = typ->inner;
    if (fabsf(w->ring - 1.0f) < FLT_EPSILON) {
        {
            int j;

            for (j = 0; j < 6; j++)
                v3_copy(&pts[j], &w->pts[j]);
        }
        n = 6;
        v3_copy(&pts[6], &w->pts[0]);
        if (!(fabsf(w->beam) < FLT_EPSILON)) {
            n = 7;
            v3_sub(&dir, w->target, &w->pts[0]);
            v3_scale(&dir, safe_powf(w->beam, 10.0f));
            v3_sum_to(&w->pts[0], &dir, &pts[7]);
        }
    } else if (fabsf(w->beam - 1.0f) < FLT_EPSILON) {
        float ring = w->ring;

        nring = ring * 6.0f;
        for (j = 0; j <= nring; j++)
            v3_copy(&pts[j], &w->pts[j]);
        n = nring + 2;
        k = ((float (*)(float))float_frac)(ring * 6.0f);

        v3_sub(&dir, &w->pts[nring + 1], &w->pts[nring]);
        v3_scale(&dir, k);
        v3_sum_to(&w->pts[nring], &dir, &pts[nring + 1]);
        v3_copy(&pts[nring + 2], w->target);
    } else {
        n = 1;
        v3_copy(&pts[0], w->target);
        v3_sub(&back, &w->pts[0], w->target);
        v3_scale(&back, w->beam);
        v3_sum(&pts[1], w->target, &back);
    }
    draw_set_blend(4, 1);
    for (i = 0; i < n; i++)
        en_virtues_laser_draw_beam(&lens, &pts[i], 1, 1);
    if (view_sphere_visible(w->target, w->body.radius)) {
        ulGxSetTextureList(w->tex);
        ulMatrixPush();
        do {
            Vec3 *p_ = w->target;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        dc_object_draw(&w->body);
        ulMatrixPop();
    }
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b204b8 */
void *en_virtues_laser_create(void *w, EnVirtuesBaseArg *prm)
{
    EnVirtuesLaserKind k;

    k.arg = prm;
    k.idx = 0;
    k.owner = w;
    return task_create_sibling_inherit_arg(w, &en_virtues_laser_desc, 296, &k);
}

/* 0x00b204f0 */
void en_virtues_laser_detach(EnVirtuesLaserTarget *w)
{
    if (w->latch) {
        Vec3 *target = w->target;

        w->target = &w->held;
        w->latch = 0;
        v3_copy(&w->held, target);
    }
}

/* 0x00b20528 */
void en_virtues_laser_draw_beam(LineStyle *lens, Vec3 *pts, int n, int lit)
{
    float prev = polyline_set_width_min(0.1f);

    if (lit)
        draw_polyline_wide_capped(lens, pts, n);
    else
        draw_polyline_wide_nocap(lens, pts, n);
    polyline_set_width_min(prev);
}
