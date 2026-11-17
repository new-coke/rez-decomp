/* Score attack area select: shared macros and task works. */

#ifndef REZ_AREASEL_H
#define REZ_AREASEL_H

#include "types_common.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"


/* suspend (flag 2) a task that has not finished */
#define TASK_SUSPEND(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags |= 2; } while (0)

#define TASK_RESUME(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags &= ~2; } while (0)

#include "color.h"

#include "types_overlays.h"

extern GameState game_state;

/* 384 bytes */
typedef struct AsBg {
    void *models;               /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    ModelPart base;                /* 0x08 */
    Anim anim;                  /* 0x64 */
    int state;                  /* 0xc0 */
    Part *parts;                /* 0xc4: the shown area's objects */
    int count;                  /* 0xc8 */
    float rate;                 /* 0xcc: anim's motion rate */
    Vec3 pos[7];                /* 0xd0: label positions (from the hook as_bg_node_cb) */
    Vec3 dir[7];                /* 0x124: label directions */
    int req;                    /* 0x178: area requested */
    int area;                   /* 0x17c: area shown */
} AsBg;

typedef struct AsVtx { float x, y, z, u, v; unsigned int color; } AsVtx;

static inline void as_vtx(AsVtx *v, float x, float y, float z, float u, float t, unsigned int col)
{
    v->x = x;
    v->y = y;
    v->z = z;
    v->u = u;
    v->v = t;
    v->color = col;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

#endif /* REZ_AREASEL_H */
