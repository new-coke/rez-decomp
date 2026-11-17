/* Title screen and option menu: shared macros and task works. */

#ifndef REZ_TITLE_SHARED_H
#define REZ_TITLE_SHARED_H

#include "types_common.h"
#include "camera.h"
#include "config.h"
#include "debug.h"
#include "fade.h"
#include "fog.h"
#include "heap.h"
#include "libc.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"



/* suspend (flag 2) a task that has not finished */
#define TASK_SUSPEND(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags |= 2; } while (0)

#define TASK_RESUME(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags &= ~2; } while (0)

/*
 * Demo screen camera (72 bytes; hooks demo_camera_hooks). The options camera (68 bytes) shares
 * the layout and plays once per option_camera_play (dt is the play flag).
 */
typedef struct CameraWork {
    void *models;               /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    Camera cam;                 /* 0x08 */
    void *node;                 /* 0x30 */
    void *motion;               /* 0x34 */
    void *target;               /* 0x38 */
    float t;                    /* 0x3c */
    float dt;                   /* 0x40 */
    float end;                  /* 0x44 */
} CameraWork;

#include "types_overlays.h"

typedef struct OptionWork {
    char pad_00[8];
    void *caller;               /* 0x08: the task that opened the menu */
    void *bg;                   /* 0x0c: the options "Bg" task */
    void *camera;               /* 0x10: the options "Camera" task */
    void *box;                  /* 0x14: the selected item's box task */
    char pad_18[4];
    void *module;               /* 0x1c: the module task started from the menu */
    int active;                 /* 0x20 */
    int mode;                   /* 0x24: 0 open, 1 navigate, 2 autosave, 3 sub-module */
    int state;                  /* 0x28 */
    int sel;                    /* 0x2c: selected item, 0..16 */
    int fog_mode;               /* 0x30: fog saved at open, restored at close */
    int fog_color;              /* 0x34 */
    int fog_back;               /* 0x38 */
    float fog_near;             /* 0x3c */
    float fog_far;              /* 0x40 */
    int stereo;                 /* 0x44 */
#ifdef VERSION_US
    int card;                   /* 0x48: card_changed seen during the configuration load */
#endif
} OptionWork;

typedef struct OptionBgWork {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    ModelPart *parts;              /* 0x08 */
    int count;                  /* 0x0c */
    Vec3 caption;               /* 0x10 */
    Vec3 anchor;                /* 0x1c: where the menu items are drawn */
    Vec3 icons;                 /* 0x28 */
    char pad_34[12];
    int icon12;                 /* 0x40 */
    int icon14;                 /* 0x44 */
} OptionBgWork;

/* BoxSelect and BoxVolume, 160 bytes */
typedef struct BoxWork {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int item;                   /* 0x08 */
    Anim anim;                  /* 0x0c */
    Vec3 pos[4];                /* 0x68: name, icons, value label */
    int ready;                  /* 0x98 */
    int state;                  /* 0x9c */
} BoxWork;

typedef struct TitleWork {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int step;                   /* 0x08: the current step (title_exec switch) */
    int next_step;              /* 0x0c: the step to take next (10 = START timeout) */
    int phase;                  /* 0x10: sub-state of the current step */
    int blink;                  /* 0x14: frame counter; bit 5 blinks the text lines */
    int idle;                   /* 0x18: idle timer of the START wait */
    int next_seq;               /* 0x1c: the sequence sound_sequence_set_next starts */
    int item;                   /* 0x20: the selected title menu item */
    int demo_no;                /* 0x24: the selected demo replay, 0..4 */
    void *demo;                 /* 0x28: the "TitleDemo" task (title_get_tasks returns this pair) */
    void *bg;                   /* 0x2c: the "BG" task */
    int unk_30;                 /* 0x30: set to 0 by title_init, never read */
} TitleWork;

/* fade-in picture, 12 bytes */
typedef struct BgWork {
    int models;                 /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    int alpha;                  /* 0x08: 0..255 */
} BgWork;

/* from title_get_tasks */
typedef struct TitleTasks {
    int demo;                   /* 0x00: the "TitleDemo" task */
    void *bg;                   /* 0x04: the "BG" task, if any */
} TitleTasks;

extern GameState game_state;

extern PadSystem pad_system;

extern unsigned int pad_cancel_mask;
extern unsigned int pad_decide_mask;
extern int score_attack_unlocked;
extern void *ulMatrixCur;
typedef struct BoxIcon { int pos, sprite, halign, valign; } BoxIcon;
extern int game_lang;

extern Pad *pad_main;

extern int game_menu_color;
extern void font_set(int id);

#endif /* REZ_TITLE_SHARED_H */
