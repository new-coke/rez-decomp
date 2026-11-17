/*
 * Ending: butterfly scene after the model scene. One butterfly flies a path while a logo grows,
 * then a swarm of up to 1000 drifts; fades out to the staff roll. Tunables: "EndingButterfly".
 */

#include "ending_shared.h"
#include "mathdef.h"
#include "anim.h"
#include "approach.h"
#include "camera.h"
#include "debug.h"
#include "fade.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "pnb.h"
#include "render.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "end_butterfly.h"
#include "end_staff_roll.h"
#include "ending.h"

/* debug variables "EndingButterfly" */
typedef struct EndButterflyPrm {
    int count;                  /* butterflies */
    Vec3 from;                  /* 0x04 */
    Vec3 to;                    /* 0x10 */
    float radius;               /* 0x1c */
    float rate;                 /* 0x20 */
    float delay;                /* 0x24 */
    float scale, scale2;       /* 0x28 */
    float time;                 /* 0x30 */
    float eye;                  /* 0x34 */
    float chance;               /* 0x38 */
    float ease;                 /* 0x3c */
} EndButterflyPrm;

/* 76 bytes */
typedef struct EndButterflyChild {
    float phase;                /* animation time */
    float t;                    /* 0x04 */
    Vec3 ofs_goal;                    /* 0x08 */
    Vec3 ofs;                    /* 0x14 */
    Vec3 pos;                   /* 0x20 */
    Vec3 prev;                  /* 0x2c */
    int ang[3];                 /* 0x38 */
    int alt;                    /* 0x44: the second flap motion */
    float speed;                /* 0x48 */
} EndButterflyChild;

/* 76436 bytes */
typedef struct EndButterfly {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart body;                /* 0x08 */
    Vec3 cam;                   /* 0x64 */
    int cang[3];                /* 0x70 */
    Vec3 cam0;                  /* 0x7c */
    int cang0[3];               /* 0x88 */
    ModelPart path;                /* 0x94 */
    int count;                  /* 0xf0 */
    ModelPart child_part;               /* 0xf4 */
    EndButterflyChild children[1000]; /* 0x150 */
    int timer;                  /* 0x12a30 */
    ModelPart logo;                /* 0x12a34 */
    float start;                /* 0x12a90 */
} EndButterfly;

extern EndButterflyPrm end_butterfly_prm;
extern int end_butterfly_vars_group;
extern VarDef end_butterfly_vardefs[16];
extern VarDef end_butterfly_vars_group_def;
extern struct TaskDesc end_butterfly_desc;

/* copied as a whole */
typedef struct EndAng3 {
    int x, y, z;
} EndAng3;

/* steer(cast) */
/* 0x00af6700 */
void end_butterfly_vars_register(void)
{
    unsigned int i;
    VarDef *vd;
    int *word;

    end_butterfly_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, game_varedit_get(), (char *)&end_butterfly_vars_group_def);
    vd = end_butterfly_vardefs;
    word = (int *)&end_butterfly_prm;
    for (i = 0; i < 16; i++) {
        vd->ptr = word;
        word++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, end_butterfly_vars_group, (struct GwVarDef *)end_butterfly_vardefs);
}

/* 0x00af6780 */
void end_butterfly_vars_unregister(void)
{
    gw_varedit_remove(&gw_varedit_root, game_varedit_get(), (struct GwVarDef *)&end_butterfly_vars_group_def);
}

/* 0x00af67b8 */
void end_butterfly_vars_init(void)
{
    end_butterfly_vars_register();
}

/* 0x00af67c0 */
void end_butterfly_vars_release(void)
{
    end_butterfly_vars_unregister();
}

/* 0x00af67c8 */
void end_butterfly_child_position(EndButterflyChild *child)
{
    Vec3 base, end, ofs;

    v3_copy(&ofs, &child->ofs_goal);
    vec3_approach(&child->ofs, &ofs, child->t);
    v3_set(&base, end_butterfly_prm.from.x, end_butterfly_prm.from.y, end_butterfly_prm.from.z);
    v3_set(&end, end_butterfly_prm.to.x, end_butterfly_prm.to.y, end_butterfly_prm.to.z);
    vec3_approach(&end, &base, child->t);
    v3_sum(&child->pos, &base, &ofs);
}

/* 0x00af6890 */
void end_butterfly_children_init(EndButterfly *w)
{
    ModelPart *cpart;
    int *root_ent, *clip_ent;
    EndButterflyChild *child;
    int i;
    float sc[4];
    float r, rnd_max, deg, unit;

    cpart = &w->child_part;
    root_ent = pnb_get_entry(w->models, 8);
    clip_ent = pnb_get_entry(w->models, 10);
    dc_object_init(cpart, *root_ent, *clip_ent, 4096);
    cpart->speed *= frame_scale;
    cpart->radius = 30.0f;
    w->count = end_butterfly_prm.count;
    child = w->children;
    for (i = 0; i < w->count; i++, child++) {
        rnd_max = 32769.0f;
        deg = 360.0f;
        unit = ANG_PER_DEG;
        child->t = safe_powf(rand_lcg_next_b() / rnd_max, end_butterfly_prm.ease);
        ulSinCos((int)(rand_lcg_next_b() / rnd_max * deg * unit), &sc[0], &sc[1]);
        ulSinCos((int)(rand_lcg_next_b() / rnd_max * deg * unit), &sc[2], &sc[3]);
        r = rand_lcg_next_b() / rnd_max * end_butterfly_prm.radius;
        v3_set(&child->ofs_goal, r * sc[0] * sc[2], r * sc[0] * sc[3], r * sc[1]);
        ulSinCos((int)(rand_lcg_next_b() / rnd_max * deg * unit), &sc[0], &sc[1]);
        ulSinCos((int)(rand_lcg_next_b() / rnd_max * deg * unit), &sc[2], &sc[3]);
        r = rand_lcg_next_b() / rnd_max * end_butterfly_prm.radius;
        v3_set(&child->ofs, r * sc[0] * sc[2], r * sc[0] * sc[3], r * sc[1]);
        end_butterfly_child_position(child);
        child->phase = rand_lcg_next_b() / rnd_max * cpart->frames;
        child->speed = rand_lcg_next_b() / rnd_max * (1.2f - 0.8f) + 0.8f;
        child->alt = 0;
    }
}

/* steer(cast) */
/* 0x00af6bd0 */
void end_butterfly_children_update(EndButterfly *w)
{
    ModelPart *cpart;
    EndButterflyChild *child;
    int i;
    Vec3 dir;

    cpart = &w->child_part;
    child = w->children;
    for (i = 0; i < w->count; i++, child++) {
        child->t += end_butterfly_prm.rate / (time_scale * FPS);
        if (1.0f < child->t)
            child->t -= 1.0f;
        if (child->alt)
            dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 14)));
        else
            dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 10)));
        child->phase += cpart->speed * child->speed;
        if (cpart->frames < child->phase) {
            child->phase -= cpart->frames;
            child->alt = RAND_FLOAT_B() < end_butterfly_prm.chance;
        }
        v3_copy(&child->prev, &child->pos);
        end_butterfly_child_position(child);
        v3_sub(&dir, &child->pos, &child->prev);
        dir.y = 0.0f;
        do {
            child->ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            child->ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        child->ang[2] = 0;
    }
}

/* 0x00af6e60 */
void end_butterfly_path_eval(EndButterfly *w)
{
    ModelPart *path_part;
    void *res[3];
    Vec3 dummy;

    path_part = &w->path;
    res[0] = &w->cam;
    res[1] = w->cang;
    res[2] = &dummy;
    anim_eval_hierarchy(path_part->root, path_part->clip, res, path_part->frame);
}

/* steer(cast) */
/* 0x00af6ea8 */
void end_butterfly_init(EndButterfly *w, float *start_t)
{
    ModelPart *body_part, *path_part;

    body_part = &w->body;
    path_part = &w->path;
    task_set_dlist(w, 4);
    rand_lcg_seed_b(4660);
    dc_object_init(body_part, *((struct DcNode **)pnb_get_entry(w->models, ending_type_get() == 4 ? 7 : 8)), *((struct DcClip **)pnb_get_entry(w->models, 10)), 0);
    body_part->speed *= frame_scale;
    body_part->radius = 30.0f;
    dc_object_init(path_part, system_pnb_entry_get(0), *((struct DcClip **)pnb_get_entry(w->models, 0)), 0);
    path_part->speed *= frame_scale;
    path_part->frame = *start_t;
    w->start = *start_t;
    end_butterfly_children_init(w);
    dc_object_init(&w->logo, *((struct DcNode **)pnb_get_entry(w->models, 5)), *((struct DcClip **)pnb_get_entry(w->models, 6)), 0);
    w->logo.speed *= frame_scale;
}

/* 0x00af7020 */
void end_butterfly_release(EndButterfly *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->path);
    dc_object_release(&w->child_part);
    dc_object_release(&w->logo);
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void end_ang3_copy(EndAng3 *dst, EndAng3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* 0x00af7068 */
void end_butterfly_exec_end(EndButterfly *w)
{
    Vec3 dir;
    int ang[3];
    float dis, fps, dx, dy, dz;
    Task *ptask;

    dc_object_advance_time(&w->body);
    end_butterfly_children_update(w);
    dx = end_butterfly_prm.to.x - end_butterfly_prm.from.x;
    dy = end_butterfly_prm.to.y - end_butterfly_prm.from.y;
    dz = end_butterfly_prm.to.z - end_butterfly_prm.from.z;
    dis = sqrtf(dx * dx + dy * dy + dz * dz);
    dx = end_butterfly_prm.to.x - w->cam0.x;
    dy = 0.0f;
    dz = end_butterfly_prm.to.z - w->cam0.z;
    v3_set(&dir, dx, dy, dz);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = 0;
    end_ang3_copy((EndAng3 *)w->cang, (EndAng3 *)w->cang0);
    ang3_approach(ang, w->cang, MIN(1.0f, w->timer++ / (end_butterfly_prm.time * time_scale * FPS)));
    fps = FPS;
    dir.x = end_butterfly_prm.to.x - w->cam.x;
    dir.y = end_butterfly_prm.to.y - w->cam.y;
    dir.z = end_butterfly_prm.to.z - w->cam.z;
    vec3_normalize(&dir, &dir);
    v3_scale(&dir, dis * end_butterfly_prm.rate / (time_scale * fps));
    v3_add(&w->cam, &dir);
    if (fade_is_idle() == 1) {
        FINISH_IF(w);
        fade_start_out((int)(time_scale * fps));
        ptask = TASK_OF(w)->parent;
        end_staff_roll_create(ptask ? ptask + 1 : 0);
    }
}

/* 0x00af73a8 */
void end_butterfly_exec_wait(EndButterfly *w)
{
    dc_object_advance_time(&w->body);
    end_butterfly_children_update(w);
    if (++w->timer > end_butterfly_prm.delay * time_scale * FPS) {
        TASK_OF(w)->exec = (void (*)(void *))end_butterfly_exec_end;
        fade_start(0, 0, (int)(time_scale * FPS));
        w->timer = 0;
        v3_copy(&w->cam0, &w->cam);
        i3_copy(w->cang0, w->cang);
    }
}

/* 0x00af7490 */
void end_butterfly_exec_path(EndButterfly *w)
{
    ModelPart *path_part;

    dc_object_advance_time(&w->body);
    path_part = &w->path;
    dc_object_advance_time(path_part);
    end_butterfly_children_update(w);
    end_butterfly_path_eval(w);
    if (fabsf(path_part->frame - path_part->frames) < FLT_EPSILON)
        TASK_OF(w)->exec = (void (*)(void *))end_butterfly_exec_wait;
}

/* steer(cast) */
/* 0x00af7510 */
void end_butterfly_exec(EndButterfly *w)
{
    ModelPart *body_part, *path_part;

    body_part = &w->body;
    path_part = &w->path;
    dc_object_advance_time(body_part);
    dc_object_advance_time(path_part);
    end_butterfly_children_update(w);
    end_butterfly_path_eval(w);
    if (fabsf(body_part->frame - body_part->frames) < FLT_EPSILON) {
        dc_object_set_clip(body_part, *((struct DcClip **)pnb_get_entry(w->models, 10)));
        body_part->frame = 0;
        dc_object_set_flags(body_part, 4096);
        TASK_OF(w)->exec = (void (*)(void *))end_butterfly_exec_path;
    }
}

/* steer(cast) */
/* 0x00af75c8 */
void end_butterfly_disp(EndButterfly *w)
{
    Vec3 eye, logo_pos, dir;
    ModelPart *body_part, *cpart;
    EndButterflyChild *child;
    int i, prev_list;
    float r, logo_end, scl;

    body_part = &w->body;
    cpart = &w->child_part;
    child = w->children;
    r = cpart->radius;
    if (view_sphere_visible(&w->cam, body_part->radius) && 702.0f < w->path.frame) {
        ulGxSetTextureList(w->textures);
        ulMatrixPush();
        MTX_TRANS(&w->cam);
        end_butterfly_rotate_yxz(w->cang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (w->path.frame < 920.0f)
            vu0_matrix_scale(ulMatrixCur, end_butterfly_prm.scale, end_butterfly_prm.scale, end_butterfly_prm.scale);
        else
            vu0_matrix_scale(ulMatrixCur, end_butterfly_prm.scale2, end_butterfly_prm.scale2, end_butterfly_prm.scale2);
        dc_object_draw(body_part);
        ulMatrixPop();
    }
    logo_end = 702.0f;
    if (!(logo_end < w->path.frame)) {
        prev_list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->textures);
        camera_get_eye(&eye);
        v3_sub(&dir, &eye, &w->cam);
        vec3_normalize(&dir, &dir);
        v3_scale(&dir, end_butterfly_prm.eye);
        v3_sum(&logo_pos, &dir, &w->cam);
        ulMatrixPush();
        MTX_TRANS(&logo_pos);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        scl = (w->path.frame - w->start) / (logo_end - w->start);
        vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
        model_object_draw(&w->logo, w->logo.frames * (w->path.frame - w->start) / (logo_end - w->start));
        ulMatrixPop();
        RenderSetCurrentDisplayList(prev_list);
    }
    if (920.0f < w->path.frame) {
        ulGxSetTextureList(w->textures);
        for (i = 0; i < w->count; i++, child++) {
            if (view_sphere_visible(&child->pos, r)) {
                if (child->alt)
                    dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 14)));
                else
                    dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 10)));
                ulMatrixPush();
                MTX_TRANS(&child->pos);
                end_butterfly_rotate_yxz(child->ang);
                vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                model_object_draw(cpart, child->phase);
                ulMatrixPop();
            }
        }
    }
}

/* 0x00af7958 */
void *end_butterfly_create(void *parent, float t)
{
    float prm[1];

    prm[0] = t;
    return task_create_inherit_arg(parent, &end_butterfly_desc, sizeof(EndButterfly), prm);
}

/* 0x00af7990 */
void end_butterfly_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
