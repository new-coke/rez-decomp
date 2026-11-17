/* TV set-up work (tv_set.c); model record of the yuki_tool.c helpers. */

#ifndef REZ_TVSET_H
#define REZ_TVSET_H

/* TV set-up work (28 bytes, at tvset_work). */
typedef struct TvSet {
    int state;                  /* 0x00: 0 init, 1 select, 500/501 confirm, 1000 done */
    unsigned int choice;        /* 0x04: 0 none yet, 1..4 (2/3/4 chosen with the decide button) */
    int cursor;                 /* 0x08: 0..2 */
    int timer;                  /* 0x0c: frames (the confirm countdown is 300) */
    void *data;                 /* 0x10: loaded texture file */
    void *tex;                  /* 0x14: texture object */
    char pad_18[4];
} TvSet;

#include "types_game.h"

#define TV_TASK(work) ((TvTaskHdr *)(work) - 1)
#define TV_TASK_END(work) do { if (work) TV_TASK(work)->flags |= 1; } while (0)

#endif

#ifndef REZ_TVSET_WIPE
#define REZ_TVSET_WIPE

#endif

#ifndef REZ_TVSET_CAMANG
#define REZ_TVSET_CAMANG

#endif

#ifndef REZ_TVSET_YUKI
#define REZ_TVSET_YUKI
/* yuki_tool.c hierarchy model record (used members only). */

typedef struct YModel {
    char pad_00[4];
    YNode *root;                /* 0x04 */
    int motion;                 /* 0x08: passed to anim_start with time */
    char pad_0c[48];
    float time;               /* 0x3c */
} YModel;

#endif
