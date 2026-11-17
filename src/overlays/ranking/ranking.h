/* Score ranking screen: shared task works. */

#ifndef REZ_RANKING_H
#define REZ_RANKING_H

#include "types_common.h"
#include "types_overlays.h"
#include "camera.h"
#include "model.h"
#include "pnb.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"


#define MTX_TRANS(p) \
    do { \
        Vec3 *p_ = (p); \
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z); \
    } while (0)

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* board row, 412 bytes */
typedef struct Bar {
    void *models;               /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    int rank;                   /* 0x08: record index, -1 = the header row */
    RankRec *rec;               /* 0x0c */
    ModelPart body;                     /* 0x10 */
    ModelPart wire_obj;                     /* 0x6c */
    Vec3 pos;                   /* 0xc8 */
    int state;                  /* 0xd4: last value given to bar_play */
    Vec3 lab[6];                /* 0xd8: text positions (hook bar_node_cb) */
    char pad_120[20];
    char score[40];             /* 0x134 */
    char val[3][20];            /* 0x15c */
    int mode;                   /* 0x198: 2 = header row (no texts) */
} Bar;

/* board, 180 bytes */
typedef struct Box {
    void *models;               /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    ModelPart part;                  /* 0x08 */
    Vec3 pos;                   /* 0x64 */
    Bar *rows[11];              /* 0x70: [0] the header row, [1..10] the records */
    int shown;                  /* 0x9c: rows started so far */
    int timer;                  /* 0xa0 */
    int table;                  /* 0xa4: ranking table shown */
    Vec3 title;                 /* 0xa8: title position (hook box_node_cb) */
} Box;

/* paging camera, 100 bytes; node hooks in demo_camera_hooks */
typedef struct PageCam {
    void *models;               /* 0x00: inherited */
    char pad_04[4];
    Camera cam;                 /* 0x08 */
    void *node;                 /* 0x30 */
    void *motion;               /* 0x34 */
    void *target;               /* 0x38 */
    float t;                    /* 0x3c */
    float end;                  /* 0x40 */
    float dt;                   /* 0x44 */
    int done;                   /* 0x48 */
    Vec3 off;                   /* 0x4c: x = page * 1200 */
    int page;                   /* 0x58 */
    int dir;                    /* 0x5c: page step requested, +1 / -1 */
    int state;                  /* 0x60 */
} PageCam;

extern void *ulMatrixCur;
extern int game_lang;

extern void ulMatrixPush(void);
extern void font_set(int id);

#endif /* REZ_RANKING_H */
