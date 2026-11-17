/* Beyond menu: shared macros and task works. */

#ifndef REZ_EXTRA_SHARED_H
#define REZ_EXTRA_SHARED_H

#include "types_common.h"
#include "billboard.h"
#include "float_bits.h"
#include "module.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"


/* suspend (flag 2) a task that has not finished */
#define TASK_SUSPEND(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags |= 2; } while (0)

#define TASK_RESUME(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags &= ~2; } while (0)

#include "color.h"

extern GameState game_state;
extern void *ulMatrixCur;
extern void font_set(int id);

/* 92 bytes; pages at ex_pages */
typedef struct ExItem {
    float x, y;                 /* label position */
    float x0, y0, x1, y1;       /* 0x08: the value box */
    float bx0, by0, bx1, by1;   /* 0x18: the mark box */
    unsigned char *name[6];     /* 0x28: per language */
    void (*adjust)(void *m, struct ExItem *it);         /* 0x40: left/right */
    void (*draw)(void *m, struct ExItem *it, float z);  /* 0x44 */
    void (*accept)(void *m);                            /* 0x48 */
    int (*marked)(void *m);                             /* 0x4c */
    int (*enabled)(void *m);                            /* 0x50 */
    void (*commit)(void *m);                            /* 0x54: on release */
    int hint;                   /* 0x58: the hint text, -1 = none */
} ExItem;

/* 36 bytes */
typedef struct ExPage {
    unsigned char *name[6];
    ExItem *items;              /* 0x18 */
    int count;                  /* 0x1c */
    int sel;                    /* 0x20 */
} ExPage;

/* 36 bytes */
typedef struct ExMenu {
    char pad_00[8];
    int id;                     /* 0x08: the page */
    ExPage *page;               /* 0x0c */
    int state;                  /* 0x10: 0 navigate, 1 chosen, 2 left */
    int result;                 /* 0x14 */
    int sel;                    /* 0x18 */
    void *hint;                 /* 0x1c: the hint task */
    void *frame;                /* 0x20: the frame task */
} ExMenu;

/* 20 bytes */
typedef struct ExExplain {
    char pad_00[4];
    void *textures;             /* 0x04 */
    int hint;                   /* 0x08: < 0 = none */
    int adjust;                 /* 0x0c: show the left/right marks */
    int accept;                 /* 0x10: show the accept mark */
} ExExplain;

/* 12 bytes */
typedef struct ExMenuWire {
    int state;                  /* 0 open, 1 close */
    float t;                    /* 0x04 */
    int id;                     /* 0x08 */
} ExMenuWire;

#endif /* REZ_EXTRA_SHARED_H */
