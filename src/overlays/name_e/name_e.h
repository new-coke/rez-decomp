/* Name entry screen: shared macros and task works. */

#ifndef REZ_NAME_E_H
#define REZ_NAME_E_H

#include "types_common.h"
#include "model.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"



/* suspend (flag 2) a task that has not finished */
#define TASK_SUSPEND(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags |= 2; } while (0)

#define TASK_RESUME(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags &= ~2; } while (0)

#include "color.h"
#include "types_overlays.h"

extern void *ulMatrixCur;

/* 16 bytes; lit when the cursor moves */
typedef struct NeLamp {
    int state;                  /* 1 fading in, 2 fading out */
    char pad_04[4];
    float t;                    /* 0x08 */
    int on;                     /* 0x0c: enabled */
} NeLamp;

/* 408 bytes */
typedef struct NeArrow {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart parts[4];            /* 0x08: two arrows, two glows */
    NeLamp lamp[2];             /* 0x178 */
} NeArrow;

typedef struct NeCharArg {
    unsigned char ch;
    int life;                   /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    float scale;                /* 0x14 */
    unsigned int color;         /* 0x18 */
} NeCharArg;

/* letter built from flying glyph pieces, 52 bytes */
typedef struct NeChar {
    int life;                   /* frames to settle */
    Vec3 pos;                   /* 0x04 */
    float scale;                /* 0x10 */
    unsigned int color;         /* 0x14 */
    int ang[3];                 /* 0x18 */
    Vec3 *verts;                /* 0x24: the glyph's pieces (24 bytes each) */
    int count;                  /* 0x28 */
    Vec3 *pieces;               /* 0x2c */
    int (*angs)[3];             /* 0x30 */
} NeChar;


#define ROT_Z(ang, sc, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#endif /* REZ_NAME_E_H */
