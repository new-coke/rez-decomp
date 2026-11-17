/*
 * Types shared by several overlays: boss common tasks (music, SE, loads, screen effects, intro
 * console), enemy parameter records named by their VarDef tables, shared enemy and effect works.
 */

#ifndef REZ_TYPES_OVERLAYS_H
#define REZ_TYPES_OVERLAYS_H

struct ShotHit;

#include "types_common.h"

/* Animated object instance (96 bytes, game type C08Part). */
typedef struct Part {
    char pad_00[60];
    float t;                    /* 0x3c */
    float end;                  /* 0x40 */
    float rate;                 /* 0x44 */
    char pad_48[20];
    int color;                  /* 0x5c */
} Part;

/* Animated object (92 bytes). */
typedef struct Anim {
    char pad_00[24];
    unsigned int flags;         /* 0x18: bit 0 = motion finished */
    char pad_1c[40];
    float rate;                 /* 0x44 */
    char pad_48[20];
} Anim;

/* Ranking record (32 bytes; ConfigGetRankRecord). */
typedef struct RankRec {
    char name[4];
    int score; /* 0x04 */
    float val[3]; /* 0x08: printed with "%19.2f" */
    char pad_14[12];
} RankRec;

/* boss_wire.c: wire model task work as wire_edit_exec sees it */
typedef struct WireEditWorkView {
    char pad_00[8];
    void *model;                /* 0x08 */
    int c;                      /* 0x0c */
    unsigned int *motion;       /* 0x10 */
    unsigned char *flags;       /* 0x14: one byte per edge, bit 0 drawn, bit 1 selected */
    int nflags;                 /* 0x18 */
    int sel;                    /* 0x1c: the selected edge */
    float yaw;                  /* 0x20: viewer angle (degrees) */
    float yaw_spd;              /* 0x24 */
    float dist;                 /* 0x28 */
    void *data;                 /* 0x2c: the built edge image */
    int size;                   /* 0x30 */
    float frame;                /* 0x34 */
    float frames;               /* 0x38 */
    char *src;                  /* 0x3c */
} WireEditWorkView;

/* edge list block walked by wire_list */
typedef struct WireEdgeBlk {
    struct WireEdgeBlk *next;
    int n;
    unsigned short e[2];        /* 0x08: vertex index pairs */
} WireEdgeBlk;

/* boss_texture_anime.c: "cmTexAnime" work (192 bytes): frames streamed from a file */
typedef struct CmTexAnimeWork {
    char pad_00[4];
    int tex_list;               /* 0x04: texture list (ulGxSetTextureList) */
    int arg;                    /* 0x08: the creation argument (boss_get_arg0) */
    char *buf1;                 /* 0x0c: two frame buffers */
    char *buf2;                 /* 0x10 */
    int playing;                /* 0x14: frames are played from a loaded buffer */
    int buf_idx;                /* 0x18: the buffer read into next */
    int play_pos[2];            /* 0x1c: the buffer played and the frame in it */
    int wait, interval;         /* 0x24: frames to the next upload, frames per upload */
    int state;                  /* 0x2c: 1 starting, 2 streaming */
    int err;                    /* 0x30 */
    int fsize;                  /* 0x34: bytes per frame */
    int nbuf, nbuf_full;        /* 0x38: frames in the current buffer, in a full one */
    char name[32];              /* 0x40 */
    int vol;                    /* 0x60: the file to open: volume, name */
    char *namep;                /* 0x64 */
    int sector;                 /* 0x68: the next sector read */
    int sectors;                /* 0x6c: of the file */
    int fsec, fsec2;            /* 0x70: sectors per frame */
    int frame_no;               /* 0x78: frames uploaded from the file */
    int pvr;                    /* 0x7c */
    int nframes;                /* 0x80 */
    int limit, limit_arg, play; /* 0x84: end after limit uploads (limit_on); frames advance while play */
    int have_frame;             /* 0x90: a frame was uploaded */
    int visible;                /* 0x94: cm_tex_anime_disp draws */
    int at_end;                 /* 0x98: the last buffer of the file is in */
    int once, limit_on;         /* 0x9c: end at the end of the file; use limit */
    void *handle;               /* 0xa4: the open file while streaming */
    unsigned int color;         /* 0xa8 */
    float x, y, width, h;           /* 0xac: the screen rectangle */
    int close_wait;
} CmTexAnimeWork;

/* boss_texture_anime.c: colour fade task work (24 bytes) */
typedef struct CmTexAnimeColorWork {
    int delay;                  /* frames before the fade */
    int count;                  /* 0x04: steps left */
    int total;                  /* 0x08 */
    unsigned int from, to;      /* 0x0c */
    unsigned int *dst;          /* 0x14: the colour faded */
} CmTexAnimeColorWork;

/* boss_texture_anime.c: frame sprite vertex (24 bytes) */
typedef struct TaVtx {
    float x, y, q;
    float u, v;
    unsigned int color;         /* 0x14 */
} TaVtx;

/* boss_common.c: file or sound the boss loads (16 bytes) */
typedef struct BossEnt {
    void *file;
    int pcm;                    /* 0x04: the stream entry */
    int data;
    int busy;                   /* 0x0c */
} BossEnt;

/* boss_common.c: "cmBGM_Sound_Man" work (564 bytes, cm_bgm_sound_man): music, files, log */
typedef struct BgmMan {
    char pad_00[28];
    int debug;                  /* 0x1c: sound debug display on (sound_debug_toggle) */
    int release_cb;             /* 0x20: called by cm_bgm_sound_man_release */
    BossEnt ent[32];            /* 0x24 */
    int nreq;                   /* 0x224 */
    int nent;                   /* 0x228 */
    int loaded;                 /* 0x22c */
    int pcm;                    /* 0x230: the stream stop steps (2, 3, 4) */
} BgmMan;

/* boss_common.c: sound load task work (44 bytes) */
typedef struct CmBgmSoundSubWork {
    int state;
    int cb, cb_arg;                   /* 0x04 */
    BgmMan *boss;                /* 0x0c */
    BossEnt *ent;               /* 0x10 */
    int phase;
    int n;                      /* 0x18 */
    int started;                /* 0x1c */
    int on;                     /* 0x20 */
    int idx, seq_idx;               /* 0x24 */
} CmBgmSoundSubWork;

/* boss_common.c: file load task work */
typedef struct CmSeLaserWork {
    int unk_00; /* 3 in cm_se_laser_create; never read */
    int state;                  /* 0x04 */
    int step;                   /* 0x08 */
    void *file;                 /* 0x0c */
    BgmMan *boss;                /* 0x10 */
    void (*done)(int arg);      /* 0x14 */
    int arg;                    /* 0x18 */
} CmSeLaserWork;

/* boss_common.c: load log entry (24 bytes) */
typedef struct BossLogEnt {
    char pad_00[4];
    int a, unk_08; /* 0x04; unk_08: printed after a by sound_log_entry */
    char pad_0c[8];
    char *name;                 /* 0x14 */
} BossLogEnt;

/* boss_common.c: tempo ramp task work (28 bytes) */
typedef struct CmSoundBpmWork {
    int unk_00; /* 0 in cm_sound_bpm_create; never read */
    float from, to;             /* 0x04: beats per minute */
    float cur;                  /* 0x0c */
    int count, total;           /* 0x10: frames */
    int curve;                  /* 0x18: 1 sine, 2 square, 3 inverse square, else linear */
} CmSoundBpmWork;

/* boss_common.c: boss sound event (24 bytes; table ends with id -1) */
typedef struct BossSe {
    int id;
    int ch;                     /* 0x04 */
    int note;                   /* 0x08 */
    int kind;                   /* 0x0c: 0 play, 1 play on its grid, 2 start once, 3 stop, 4 random note */
    int arg;                    /* 0x10: grid, count, the entry to stop, or the note range hi << 16 | lo */
    char *name;                 /* 0x14 */
} BossSe;

/* boss_common.c: "VOL" task work (40 bytes; kind 1 in the debug list) */
typedef struct CmBgmSoundWork {
    int kind;
    int phase;                  /* 0x04 */
    int count, total;           /* 0x08 */
    int value;                  /* 0x10 */
    int vol_last;
    int vol_from, vol_to;               /* 0x18 */
    int *def;                   /* 0x20 */
    int arg;                    /* 0x24 */
} CmBgmSoundWork;

/* boss_common.c: VOL table entry (36 bytes; ends with id -1) */
typedef struct BossVolDef {
    int id;
    int kind;                   /* 0x04: 0 mute request, 1 and 3 a VOL task */
    int arg;                    /* 0x08: the creation argument (boss_get_arg0) */
    float secs;                 /* 0x0c */
    char pad_10[12];
    int vol_from, vol_to;               /* 0x1c */
} BossVolDef;

/* boss_common.c: raises boss_width (12 bytes) */
typedef struct BossGageWork {
    int target;
    int count, total;           /* 0x04 */
} BossGageWork;

/* boss_common.c: delayed player command (12 bytes) */
typedef struct BossPlDiWork {
    int delay;
    int cmd;                    /* 0x04 */
    int *states;                /* 0x08: the values for player states 1-3/7 and 4 */
} BossPlDiWork;

/* boss_common.c: screen margin and fade task (28 bytes) */
typedef struct CmScBlurWork {
    float margin, alpha;        /* scales */
    int count, total;           /* 0x08 */
    int state;                  /* 0x10: 0 waiting, 1 running */
    int delay;                  /* 0x14 */
    int curve;                  /* 0x18: 0 rise then square fall, 1 linear down */
} CmScBlurWork;

/* boss_common.c: delayed screen wipe (20 bytes) */
typedef struct CmScCrossWork {
    int delay;
    int state;                  /* 0x04: 0 waiting, 1 covering, 2 uncovering */
    int n;                      /* 0x08: frames to uncover */
    int arg;                    /* 0x0c */
    void *wipe;                 /* 0x10 */
} CmScCrossWork;

/* boss_common.c: light colour task (28 bytes) */
typedef struct CmLightChangeWork {
    int delay;
    int state;
    int count, total;           /* 0x08 */
    int idx;                    /* 0x10 */
    int from;                   /* 0x14: the light colour at the start */
    int to;                     /* 0x18 */
} CmLightChangeWork;

/* boss_common.c: boss lock-on table entry (12 bytes) */
typedef struct BossLockDef {
    void *type;                  /* lockon_obj_init's f18 */
    int lock_max;                     /* 0x04 */
    int score;                  /* 0x08 */
} BossLockDef;

/* boss_common.c: "BossDemoMes2" types message lines to the console (28 bytes) */
typedef struct BossDemoMes2Work {
    int idx;                    /* next line */
    int delay;                  /* 0x04 */
    int level;                  /* 0x08: indexes the name tags boss_level_tags */
    int state;                  /* 0x0c */
    char **lines;               /* 0x10 */
    int n;                      /* 0x14 */
    int quit;                   /* 0x18: end after the last line instead of scrolling them away */
} BossDemoMes2Work;

/* boss_common.c: boss intro console window task (28 bytes) */
typedef struct BossDemoMesWork {
    void *con;
    int width, h;                   /* 0x04: the window size in cells */
    int line;                   /* 0x0c: the next line of boss_demo_mes_lines */
    int delay;                  /* 0x10 */
    int level;                  /* 0x14 */
    void *parent;               /* 0x18 */
} BossDemoMesWork;

/* boss_common.c: model build table entry (24 bytes; ends with idx -1) */
typedef struct BossModelDef {
    int idx;                    /* slot in the output list and the instance pool */
    int kind;                   /* 0x04 */
    unsigned int node;          /* 0x08: entries of the model bank (pnb_get_entry) */
    unsigned int node2;         /* 0x0c */
    unsigned int clip;          /* 0x10: kind 3: a pointer to three entries */
    unsigned int flags;         /* 0x14 */
} BossModelDef;

/* boss_common.c: spark emitter parameters */
typedef struct BossSparkDef {
    float psize;                /* spark lift scale */
    float speed;                /* 0x04 */
    float dscale;               /* 0x08 */
    float secs;                 /* 0x0c */
    float fade;                 /* 0x10: the fraction of the life left when it stops emitting */
    float k1;                   /* 0x14 */
    float size;                 /* 0x18 */
    float k2;                   /* 0x1c */
    int max;                    /* 0x20 */
    int tex;                    /* 0x24 */
    float life_max;             /* 0x28: seconds */
    float life_min;             /* 0x2c */
    char pad_30[4];
    float spread;               /* 0x34: degrees */
} BossSparkDef;

/* boss_common.c: ribbon trail parameters */
typedef struct BossRibbonDef {
    int tex;
    unsigned int color;         /* 0x04 */
    int n;                      /* 0x08 */
    float secs;                 /* 0x0c: the fade-out */
    float rate;                 /* 0x10: samples per frame */
    float width_a;                   /* 0x14 */
    float width_b;                   /* 0x18 */
} BossRibbonDef;

/* boss_common.c: scrolls the UVs of a model strip chunk (32 bytes) */
typedef struct ModelUvWork {
    short **ptrs;               /* UVs in the model */
    unsigned short *base;       /* 0x04: their original values */
    int flags;                  /* 0x08: 1 scroll u, 2 scroll v */
    int n;                      /* 0x0c */
    float u, du;                /* 0x10 */
    float v, dv;                /* 0x18 */
} ModelUvWork;

/* boss_common.c: steps a word through a table at a rate (32 bytes) */
typedef struct CmCchangeWork {
    int *keys;
    char pad_04[4];
    int *dst;                   /* 0x08 */
    int n;                      /* 0x0c */
    int idx;                    /* 0x10 */
    int count;                  /* 0x14: frames to live, 0 = forever */
    float t;                    /* 0x18 */
    float speed;                /* 0x1c: keys per second */
} CmCchangeWork;

/* boss_common.c: 16-byte key */
typedef struct BossCfKey {
    int rgba[4];
} BossCfKey;

/* boss_common.c: steps a 16-byte record through a table at a rate (28 bytes) */
typedef struct BossCfKeyWork {
    BossCfKey *keys;
    BossCfKey *dst;             /* 0x04 */
    int n;                      /* 0x08 */
    int idx;                    /* 0x0c */
    int count;                  /* 0x10 */
    float t;                    /* 0x14 */
    float speed;                /* 0x18 */
} BossCfKeyWork;

/* boss_common.c: task_tree_walk node (child at 0x10, sibling at 0x0c) */
typedef struct BossTNode {
    char pad_00[12];
    struct BossTNode *sibling;  /* 0x0c */
    struct BossTNode *child;    /* 0x10 */
} BossTNode;

typedef struct BossFog {
    unsigned int col;           /* fog_set */
    unsigned int back;          /* BackColReq */
    float near, far;
} BossFog;

typedef struct BossColPair {
    unsigned int c[2];
} BossColPair;

typedef struct SVtxQuad { float v[4]; } __attribute__((aligned(16))) SVtxQuad;

typedef struct BossV4 { float x, y, z, w; } BossV4;

/* CmTexAnimeWork buf1, buf2 as an array. */
typedef struct TexAnimBufs {
    char pad_00[12];
    char *buf[2];               /* 0x0c */
} TexAnimBufs;

/* libm error record (struct exception) */
typedef struct LibmException {
    int type;
    char *name;                 /* 0x04 */
    double arg1;                /* 0x08 */
    double arg2;                /* 0x10 */
    double retval;              /* 0x18 */
    int err;                    /* 0x20 */
} LibmException;

/* libm ieee_double_shape_type */
typedef union LibmDoubleShape {
    double value;
    struct {
        unsigned int lsw;
        unsigned int msw;
    } parts;
} LibmDoubleShape;

/* Debug variable entry (gw_varedit_add, 40 bytes). */
typedef struct VarDef {
    char pad_00[8];
    char *name;                 /* 0x08 */
    void *fmt;                  /* 0x0c */
    void *ptr;                  /* 0x10: the variable */
    char pad_14[20];
} VarDef;

/* Break effect motion parameters, named by the 'EnemyShatter' VarDefs enemy_shatter_vardefs. */
typedef struct ShatterParams {
    float Gravity;              /* per 3600 */
    float PowVec;               /* piece speed */
    float PowAng;               /* piece spin (degrees) */
    float RtoVecDec, RtoAngDec, RotSpdPlayer, TimeAlphaStart, TimeAlphaEnd;
    int force_wire;
    unsigned int pcol_wire;
    int b_acc;                  /* ease the first frames instead of the decays */
    float frm_acc;
} ShatterParams;

typedef struct ListenerDef {
    int idx;
    void (*fn)(int);
} ListenerDef;

/* Phase task (8 bytes). */
typedef struct PhaseWork {
    int phase;
    int unk_04; /* -1 in every stage's phase_init; never read */
} PhaseWork;

/* 'attack_ray' argument (attack_ray_create, 16 bytes). */
typedef struct AttackRayArg {
    void *pos;
    int *ang;                   /* 0x04 */
    void *scale;               /* 0x08 */
    float dur;                  /* 0x0c */
} AttackRayArg;

/* Object kind functions: init, release, exec, disp, pre_exec, post_exec. */
typedef struct ObjFuncs {
    void *fn[6];
} ObjFuncs;

/* Type enemy placement. */
typedef struct Placement {
    char pad_00[4];
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    char pad_1c[12];
    float param0;                  /* 0x28 */
    float param1, param2;             /* 0x2c */
    float param3;                  /* 0x34: the Sphinx sweep length (frames) */
} Placement;

/* 'KhufuXL' parameters (224 bytes), named by khufu_xl_vardefs. */
typedef struct KhufuXLParams {
    int hp;                         /* 0x08 */
    int same_lock;                  /* 0x0c */
    int score;                      /* 0x10 */
    float hp_reduce;                /* 0x14 */
    float crash_r;                  /* 0x18 */
    float crash_damage;             /* 0x1c */
    float attack_ray_scl;           /* 0x20 */
    float attack_ray_sec;           /* 0x24 */
    float attack_ray_sec_dif;       /* 0x28 */
    float backfire_scl;             /* 0x2c */
    float break_sec;                /* 0x30 */
    float break_pow;                /* 0x34 */
    float break_ang_pow;            /* 0x38 */
    float break_dis;                /* 0x3c */
    float break_dis_pow;            /* 0x40 */
    float break_backfire_scl;       /* 0x44 */
    float break_backfire_pow;       /* 0x48 */
    float break_backfire_pow2;      /* 0x4c */
    float shot_sec;                 /* 0x50 */
    float shot_motion_rate;         /* 0x54 */
    float shot_back_dis;            /* 0x58 */
    float shot_back_pow;            /* 0x5c */
    float stop_swing_pow;           /* 0x60 */
    float stop_swing_scl;           /* 0x64 */
    float stop_backfire_sec;        /* 0x68 */
    float stop_backfire_pow;        /* 0x6c */
    float back_sec;                 /* 0x70 */
    float back_dis;                 /* 0x74 */
    float back_pow;                 /* 0x78 */
    float back_backfire_scl;        /* 0x7c */
    float back_backfire_pow;        /* 0x80 */
    int back_pitch;                 /* 0x84 */
    float back_pitch_pow;           /* 0x88 */
    float accel_sec;                /* 0x8c */
    float accel_pow;                /* 0x90 */
    float accel_ang_pow;            /* 0x94 */
    float accel_ang_pow2;           /* 0x98 */
    float accel_ang_over_rate;      /* 0x9c */
    float accel_spd_over_rate;      /* 0xa0 */
    float accel_backfire_scl;       /* 0xa4 */
    float accel_backfire_pow;       /* 0xa8 */
    float accel_motion_spd_den;     /* 0xac */
    float accel_motion_spd_den2;    /* 0xb0 */
    float accel_pitch_pow;          /* 0xb4 */
    float away_sec;                 /* 0xb8 */
    int wire;                       /* 0xbc */
    int wire_linear;                /* 0xc0 */
    int wire_col;                   /* 0xc4 */
    int wire_col2;                  /* 0xc8 */
    float wire_near;                /* 0xcc */
    float wire_far;                 /* 0xd0 */
    float wire_near_rate;           /* 0xd4 */
    float wire_far_rate;            /* 0xd8 */
    int wire_beat;                  /* 0xdc */
} KhufuXLParams;

typedef struct KhufuXLVars {
    char *name;
    char *label;                /* 0x04 */
    KhufuXLParams p;            /* 0x08 */
} KhufuXLVars;

/* Ramses parameters (204 bytes), named by ramses_vardefs. */
typedef struct RamsesParams {
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_reduce;            /* 0x14: "HP Recover" */
    float crash_r, crash_damage;        /* 0x18 */
    float attack_ray_scl;       /* 0x20 */
    float attack_ray_sec, attack_ray_sec_dif;   /* 0x24 */
    float backfire_scl;         /* 0x2c */
    float break_sec, break_pow, break_ang_pow;  /* 0x30 */
    float break_dis, break_dis_pow;     /* 0x3c */
    float break_backfire_scl, break_backfire_pow, break_backfire_pow2;  /* 0x44 */
    float shot_sec, shot_motion_rate;   /* 0x50 */
    float shot_back_dis, shot_back_pow; /* 0x58 */
    float stop_swing_pow, stop_swing_scl;       /* 0x60 */
    float stop_backfire_sec, stop_backfire_pow; /* 0x68 */
    float back_sec, back_dis, back_pow; /* 0x70 */
    float back_backfire_scl, back_backfire_pow; /* 0x7c */
    float accel_sec, accel_pow, accel_ang_pow, accel_ang_pow2;  /* 0x84 */
    float accel_ang_over_rate, accel_spd_over_rate;     /* 0x94 */
    float accel_backfire_scl, accel_backfire_pow;       /* 0x9c */
    float away_sec;             /* 0xa4 */
    int wire;                   /* 0xa8: "Wire Use ?" */
    int wire_linear;            /* 0xac */
    unsigned int wire_col, wire_col2;   /* 0xb0 */
    float wire_near, wire_far;  /* 0xb8: (m) */
    float wire_near_rate, wire_far_rate;        /* 0xc0 */
    int wire_beat;              /* 0xc8 */
} RamsesParams;

typedef struct RamsesVars {
    char *name;
    char *label;                /* 0x04 */
    RamsesParams p;            /* 0x08 */
} RamsesVars;

/* Variable editor group name and format. */
typedef struct VarName {
    char *name;
    void *fmt;
} VarName;

/* shot description bend words: signed, -1 = no curve */
typedef enum { SHOT_CURVE_NONE = -1 } ShotCurve;

/* textured strip vertex (24 bytes) */
typedef struct UvVtx {
    float x, y, q;
    float t, s;
    unsigned int c;
} UvVtx;

/*
 * "ScrFilter" work (84 bytes): colour disc over the screen around the point DIST units ahead
 * of the camera, faded towards its target over FRAMES frames.
 */
typedef struct ScrFilterWork {
    int ang[3];          /* 0x00 camera angles (player_get_frame_ang) */
    float dist, size;    /* 0x0c */
    int inner, outer;    /* 0x14 alpha at the centre and at the rim */
    unsigned int color;  /* 0x1c */
    float tdist, tsize;  /* 0x20 target values */
    int tinner, touter;  /* 0x28 */
    unsigned int tcolor; /* 0x30 */
    int frames;          /* 0x34 */
    char pad_38[12];
    float sx, sy, depth; /* 0x44 projected centre */
    int rot;             /* 0x50 */
} ScrFilterWork;

typedef struct ScrFilterTo { float dist, size; int inner, outer; unsigned int color; } ScrFilterTo;

typedef struct HitSpan {
    char pad_00[12];
    unsigned short lo;          /* 0x0c */
    unsigned short hi;          /* 0x0e */
} HitSpan;

/* enemy kind parameter record as the variable editor shows it (name, label, words) */
typedef struct ParamRec {
    char *name;
    char *label;                /* 0x04 */
    int v[1];                   /* 0x08: as many words as the kind has */
} ParamRec;


typedef struct MapLink {
    void *data;
    struct MapLink *next;       /* 0x04 */
} MapLink;

/* en_hanuman_params record: name, label, 17 words; names from en_hanuman_vardefs */
typedef struct EnHanumanRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float frm_motion;           /* 0x10 */
    float frm_drift;            /* 0x14 */
    float spd_move;             /* 0x18 */
    float spd_escape;           /* 0x1c */
    float spd_ang_rotate;       /* 0x20 */
    float shield_z;             /* 0x24 */
    float ang_rotate_z;         /* 0x28 */
    float pow_stop;             /* 0x2c */
    float pow_stop_ang;         /* 0x30 */
    float frm_anticipate;       /* 0x34 */
    float spd_anticipate;       /* 0x38 */
    float frm_bounce;           /* 0x3c */
    float spd_bounce;           /* 0x40 */
    float pow_bounce;           /* 0x44 */
    float dst_clip;             /* 0x48 */
} EnHanumanRec;

/* en_hanuman_shield_params record: name, label, 14 words; names from en_hanuman_shield_vardefs */
typedef struct EnHanumanShieldRec {
    char *name;
    char *label;                /* 0x04 */
    float spd_ang_uvscroll;     /* 0x08 */
    float rng_uvscroll;         /* 0x0c */
    float frm_mirror_start;     /* 0x10 */
    float frm_mirroring;        /* 0x14 */
    float frm_attack_start;     /* 0x18 */
    float spd_attack_max;       /* 0x1c */
    float acc_attack;           /* 0x20 */
    float rto_ang_rotate;       /* 0x24 */
    float collision_r;          /* 0x28 */
    float collision_z;          /* 0x2c */
    float ang_rotate_x_limit;   /* 0x30 */
    float ang_rotate_y_limit;   /* 0x34 */
    float clip_r;               /* 0x38 */
    float away_z;               /* 0x3c */
} EnHanumanShieldRec;

/* scrolling map texture phase list (yuki_alloc) */
typedef struct MapPulseBuf {
    int n;
    float *v;                   /* 0x04: n phases in 0..1 */
} MapPulseBuf;

/* scrolling map texture (24 bytes) */
typedef struct MapPulse {
    int period;                 /* 0x00: frames per cycle */
    float step;                 /* 0x04 */
    int ofs;                    /* 0x08: first float of the source rows */
    int frame;                  /* 0x0c */
    int ref;                    /* 0x10: users */
    MapPulseBuf *buf;           /* 0x14 */
} MapPulse;

/* Kind record tiamat_body_and_tail_create hands to TiamatBodyAndTail. */
typedef struct TiamatBodyAndTailKind {
    int idx;
    void *a;                    /* 0x04 */
    int n;                      /* 0x08: the segment number */
    int time;                   /* 0x0c */
    void *head;                 /* 0x10: the creating object */
    float phase;                  /* 0x14 */
    float phase_step;                  /* 0x18 */
} TiamatBodyAndTailKind;

/* hunter variables (52 bytes: 11 words) */
typedef struct EnbHunterSet {
    char *name;
    char *label;                /* 0x04 */
    int v[11];                  /* 0x08 */
} EnbHunterSet;

/* the sets' floats as an array member (stg22 view) */
typedef struct EnbHunterSetF { char *name; char *label; float v[11]; } EnbHunterSetF;

typedef struct EnbHunterSets { EnbHunterSetF r[2]; } EnbHunterSets;

/* enb_hunter kind: 0 enb_hunter, 1 enb_chaser */
typedef enum { ENB_HUNTER_KIND_NONE = -1 } EnbHunterKind;

/*
 * en_perfectwall_manager_params record: name, label, 14 words; names from
 * en_perfectwall_manager_vardefs.
 */
typedef struct EnPerfectWallManagerRec {
    char *name;
    char *label;                /* 0x04 */
    int score_unite;            /* 0x08 */
    int score_a;                /* 0x0c */
    int score_b;                /* 0x10 */
    float spd_swing;            /* 0x14 */
    float r_swing;              /* 0x18 */
    float dst_wall;             /* 0x1c */
    float pow_unite;            /* 0x20 */
    float frm_rotate;           /* 0x24 */
    float acc_charge;           /* 0x28 */
    float dst_lightning_joint;  /* 0x2c */
    float frm_life;             /* 0x30 */
    float collision_r;          /* 0x34 */
    float collision_z;          /* 0x38 */
    int test;                   /* 0x3c */
} EnPerfectWallManagerRec;

/* en_perfectwall_params record: name, label, 13 words; names from en_perfectwall_vardefs */
typedef struct EnPerfectWallRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    float frm_attack_ready;     /* 0x0c */
    float frm_escape_motion;    /* 0x10 */
    float frm_float_motion;     /* 0x14 */
    float dst_escape_anticipate; /* 0x18 */
    float spd_escape1_max;      /* 0x1c */
    float spd_escape2_max;      /* 0x20 */
    float acc_escape1;          /* 0x24 */
    float acc_escape2;          /* 0x28 */
    float dst_away_z;           /* 0x2c */
    float frm_flash;            /* 0x30 */
    unsigned int col_flash;     /* 0x34 */
    int test;                   /* 0x38 */
} EnPerfectWallRec;

typedef struct PartAnim {       /* ModelPart animation fields: w[15], w[16] as floats */
    int w[15];
    float frame;                /* 0x3c */
    float frame_end;            /* 0x40 */
} PartAnim;

/* Variable editor record: group name, label, 12 values. */
typedef struct ThronesChildSet {
    char *name;
    char *label;                /* 0x04 */
    int v[12];                  /* 0x08 */
} ThronesChildSet;

/* Variable editor record: group name, label, 15 values. */
typedef struct EnShatterSet {
    char *name;
    char *label;                /* 0x04 */
    int v[15];                  /* 0x08 */
} EnShatterSet;

/* Variable editor record: group name, label, 31 values. */
typedef struct ThronesCoreSet {
    char *name;
    char *label;                /* 0x04 */
    int v[31];                  /* 0x08 */
} ThronesCoreSet;

/* enSort draw list entry, added by en_sort_add (20 bytes). */
typedef struct EnArmorDispArg {
    char pad_00[4];
    void *owner;                /* 0x04 */
    void (*fn)(void *w);        /* 0x08 */
    void *work;                    /* 0x0c */
    void *pos;                  /* 0x10 */
} EnArmorDispArg;

/* enSort work (en_sort_desc, 652 bytes): draw callbacks sorted back to front */
typedef struct EnSortWork {
    char pad_00[8];
    int n;                      /* 0x08 */
    EnArmorDispArg e[32];              /* 0x0c */
} EnSortWork;

/* Work with a frame count, as the Thrones and Powers exec states see it. */
typedef struct ObjLifeWork {
    char pad_00[4];
    int state;                  /* 0x04 */
    char pad_08[16];
    int life;                   /* 0x18: frames left */
} ObjLifeWork;

/* "enArmorDisp" work (12 bytes, en_armor_disp_desc). */
typedef struct EnArmorDispWork {
    char pad_00[8];
    void *obj;                  /* 0x08 */
} EnArmorDispWork;

/* Variable editor record: group name, label, 30 values. */
typedef struct ThronesSet {
    char *name;
    char *label;                /* 0x04 */
    int v[30];                  /* 0x08 */
} ThronesSet;

/* thrones_core_types entry with fields named from its VarDef table. */
typedef struct ThronesCoreRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_recover;           /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float shot_spd;             /* 0x20 */
    float tail_sec;             /* 0x24 */
    float tail_width;           /* 0x28 */
    int tail_skip;              /* 0x2c */
    float shot_sec;             /* 0x30: the launch */
    float shot_spd0;            /* 0x34: distance per second at its start */
    float shot_spd1;            /* 0x38: at its end */
    int sway_spd;               /* 0x3c */
    float sway;                 /* 0x40 */
    float sway_pow;             /* 0x44 */
    float back_sec;             /* 0x48: the backing off */
    float back_dist;            /* 0x4c */
    float back_pow;             /* 0x50 */
    float fire_pow;             /* 0x54 */
    float fire_pow2;            /* 0x58 */
    float fire_size;            /* 0x5c */
    float accel_spd;            /* 0x60 */
    int roll_spd;               /* 0x64: degrees per second */
    float accel_sec;            /* 0x68 */
    float accel_pow;            /* 0x6c */
    float finish_sec;           /* 0x70 */
    float finish_spd;           /* 0x74 */
    float finish_pow;           /* 0x78 */
    float away_sec;             /* 0x7c */
    int bonus_score;            /* 0x80 */
} ThronesCoreRec;

typedef struct ThronesCoreAng {
    int a[3];
} ThronesCoreAng;

/* ThronesCoreManager argument (copied by thrones_core_manager_init). */
typedef struct ThronesCoreManagerArg {
    void *objs[4];
    int fx;                     /* 0x10: the effect slot kind */
} ThronesCoreManagerArg;

/* thrones_child_types entry with fields named from its VarDef table. */
typedef struct ThronesChildRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same_lock;              /* 0x0c */
    int score;                  /* 0x10 */
    float hp_recover;           /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float die_sec;              /* 0x20 */
    int die_rot_x;              /* 0x24: degrees per second */
    int die_rot_z;              /* 0x28 */
    float die_accel;            /* 0x2c: falling acceleration */
    float scroll_out;           /* 0x30: distance per second */
    float motion_density;       /* 0x34 */
} ThronesChildRec;

/* thrones_child_create's second argument, read by thrones_child_init. */
typedef struct ThronesChildArg {
    char pad_00[40];
    float t0;                   /* 0x28 */
    float t1;                   /* 0x2c: their sum is the life in frames */
    char pad_30[12];
    float motion;               /* 0x3c */
} ThronesChildArg;

/* ThronesChild creation record (ObjDesc.kind points to it). */
typedef struct ThronesChildKind {
    int idx;
    int life;                   /* 0x04 */
} ThronesChildKind;

/* thrones_swing_value evaluates cnt / total clamped to 0..1; thrones_swing_reset resets */
typedef struct ThronesSwing {
    int cnt;
    int total;                  /* 0x04 */
    float val;                  /* 0x08: thrones_swing_value's result when thrones_swing_reset ran */
    float base;                 /* 0x0c */
    float a;                    /* 0x10 */
    float b;                    /* 0x14 */
    int ang;                    /* 0x18 */
} ThronesSwing;

typedef struct ThronesPartAngRec {
    int old[3];
    int cur[3];                 /* 0x0c */
    char pad_18[112];
} ThronesPartAngRec;

/* thrones_types entry (128 bytes) with fields named from its VarDef table. */
typedef struct ThronesRec {
    char *name;
    char *label;                /* 0x04 */
    float crash_r;              /* 0x08 */
    float crash_damage;         /* 0x0c */
    int armor_hp;               /* 0x10 */
    float motion_sec;           /* 0x14 */
    float motion_dif_sec;       /* 0x18 */
    float motion_wait_sec;      /* 0x1c */
    float release_dif_sec;      /* 0x20 */
    float die_sec;              /* 0x24 */
    int die_rot_x;              /* 0x28: die_ang_spd_x, degrees per second */
    int die_rot_z;              /* 0x2c: die_ang_spd_z */
    float die_accel;            /* 0x30: die_fall_acc */
    float die_away_spd;         /* 0x34 */
    float explode_scl;          /* 0x38 */
    float explode_dis;          /* 0x3c */
    float core_shot_wait_sec;   /* 0x40 */
    float blow_back_sec;        /* 0x44 */
    float blow_back_rate;       /* 0x48 */
    int blow_back_spd;          /* 0x4c */
    float blow_back_scl;        /* 0x50 */
    float motion_density;       /* 0x54 */
    float away_wait_sec;        /* 0x58 */
    int wire;                   /* 0x5c */
    int wire_linear;            /* 0x60 */
    unsigned int wire_col;      /* 0x64 */
    unsigned int wire_col2;     /* 0x68 */
    float wire_near;            /* 0x6c */
    float wire_far;             /* 0x70 */
    float wire_near_rate;       /* 0x74 */
    float wire_far_rate;        /* 0x78 */
    int wire_beat;              /* 0x7c */
} ThronesRec;

/* trail point ribbon (28 bytes) */
typedef struct Ribbon {
    int n;
    int head; /* 0x04 */
    int count; /* 0x08: 2..200 */
    int seg; /* 0x0c: points per sample */
    int step; /* 0x10 */
    struct RibPt *buf; /* 0x14: count points per segment row */
    float len; /* 0x18: the length of the newest segment row */
} Ribbon;

typedef struct ArchiverArg {
    int kind;
    Vec3 pos; /* 0x04 */
    int ang[3]; /* 0x10 */
    Vec3 dir; /* 0x1c */
    float side;
    float up;
    float drag;
    float delay;
    float maxspd;
    float life;
} ArchiverArg;

/* What 'enBackFire' follows (52 bytes; en_backfire_create_with_cb, _beam_create_with_cb). */
typedef struct BackFireLink {
    void *gfx;
    int flags; /* 0x04 */
    Vec3 *pos; /* 0x08 */
    int *ang; /* 0x0c */
    Vec3 *obj_scale; /* 0x10 */
    int use_pose; /* 0x14: one record per model node, posed */
    float *scale; /* 0x18: the sprites' size factor */
    float *alpha; /* 0x1c */
    float len; /* 0x20: the beam's length */
    float width; /* 0x24: the beam's width */
    float *width_scale; /* 0x28 */
    float *len_scale; /* 0x2c */
    int cb; /* 0x30: called with the node records */
} BackFireLink;

typedef struct BossBeam {
    Vec3 vel;
    Vec3 dir; /* 0x0c: half the beam, spun by rot each frame */
    Vec3 pos; /* 0x18: its middle */
    int rot[3]; /* 0x24 */
    float len; /* 0x30 */
    float width; /* 0x34 */
    unsigned int color; /* 0x38 */
} BossBeam;

typedef struct BossBeamDef {
    Vec3 dir;
    unsigned int color; /* 0x0c */
    float width; /* 0x10 */
} BossBeamDef;

typedef struct BossChain {
    struct BossChain *prev; /* 0x00 */
    struct BossChain *next; /* 0x04 */
    float stiff; /* 0x08: pull back toward the straight pose */
    float damp; /* 0x0c: share of the velocity kept per step */
    float len; /* 0x10: link length */
    Vec3 pos; /* 0x14 */
    Vec3 dir; /* 0x20: offset from the previous link */
    Vec3 vel; /* 0x2c */
    void (*cb)(struct BossChain *, struct BossChain *); /* 0x38: adds forces to a link */
} BossChain;

/* Earth main task sweeping hand, one of two (48 bytes, kuri_hand_step) */
typedef struct BossKuriHand {
    int ang[3]; /* x turns by b[0]; y and z swing by b[1..2] */
    int b[3]; /* 0x0c: x speed (limited to +0x1f9c0), y/z amplitudes */
    int c[3]; /* 0x18: x acceleration, y/z periods */
    int d[3]; /* 0x24: y/z frames left */
} BossKuriHand;

/* Packet object tree node (model_packet_draw_normal / _simple). */
typedef struct BossPktNode {
    char pad_00[12];
    struct BossPktNode *sibling; /* 0x0c */
    struct BossPktNode *child; /* 0x10 */
} BossPktNode;

typedef struct BossShard {
    char pad_00[4];
    int total; /* 0x04 */
    int count; /* 0x08: frames left, 0 = gone */
    char pad_0c[4];
    float k; /* 0x10: count / total */
    float radius; /* 0x14 */
    Vec3 pos; /* 0x18 */
    Vec3 vel; /* 0x24 */
    int ang[3]; /* 0x30 */
    int spin[3]; /* 0x3c */
    void *tex; /* 0x48: its texture list for the wire draw */
    void *wire; /* 0x4c: a wire object to draw instead */
    void *model; /* 0x50 */
} BossShard;

typedef struct BossSpark {
    Vec3 pos;
    Vec3 vel; /* 0x0c: the offset from the emitter at the start of its life */
    int ang; /* 0x18: its roll */
    int spin; /* 0x1c */
    int count;
    int total;
    int dead; /* 0x28 */
    float size; /* 0x2c: eases to the emitter's k1 */
    float lift; /* 0x30: the rolled offset at the start of its life */
    char pad_34[12];
} BossSpark;

/* "effStar" star (36 bytes) */
typedef struct Star {
    Vec3 pos;
    Vec3 prev; /* 0x0c: drawn as a line to pos */
    Vec3 vel; /* 0x18 */
} Star;

/* "effStar" work (88 bytes, eff_star_class; eff_star.c): stars streaking past the camera */
typedef struct EffStarWork {
    int n; /* drawn stars */
    int max; /* 0x04 */
    Star *buf; /* 0x08 */
    unsigned int color; /* 0x0c */
    Vec3 center; /* 0x10: the camera direction times dist */
    Vec3 eye; /* 0x1c */
    Vec3 eye_prev; /* 0x28 */
    Vec3 delta; /* 0x34 */
    Vec3 acc; /* 0x40: the scroll since the last draw */
    float dist; /* 0x4c */
    float range; /* 0x50 */
    float limit; /* 0x54: squared camera step above which nothing is drawn */
} EffStarWork;

typedef struct BossStarCtl {
    EffStarWork *star;
    Vec3 eye; /* 0x04: the camera eye last frame */
    float speed[4]; /* 0x10: the camera's last four steps */
    float limit; /* 0x20: above this step the stars are hidden */
    int idx; /* 0x24 */
    int count;
    int total;
    unsigned int color; /* 0x30 */
    unsigned int to; /* 0x34 */
    unsigned int from; /* 0x38 */
} BossStarCtl;

typedef struct BossTrailEnt {
    Vec3 pos;
    int ang[3]; /* 0x0c */
} BossTrailEnt;

typedef struct BossVtxBlk {
    char pad_00[6];
    short n; /* 0x06 */
    struct { Vec3 pos; Vec3 nrm; } v[1]; /* 0x08 */
} BossVtxBlk;

typedef struct CmBtailWork {
    char pad_00[4];
    void *texlist; /* 0x04 */
    int tex; /* 0x08 */
    void *owner; /* 0x0c: stop sampling when this task ends */
    Vec3 *src; /* 0x10 */
    Vec3 *pts; /* 0x14 */
    int head; /* 0x18 */
    unsigned int flags; /* 0x1c: 1 stopped, 2 fading */
    int count;
    int total;
    float acc; /* 0x28 */
    float rate; /* 0x2c */
    float alpha; /* 0x30 */
    float width_a; /* 0x34 */
    float width_b; /* 0x38 */
    unsigned int color; /* 0x3c */
    unsigned int color0; /* 0x40 */
    int n; /* 0x44 */
} CmBtailWork;

typedef struct CmCircle2Work {
    char pad_00[8];
    Vec3 pos; /* 0x08 */
    int count; /* 0x14 */
    int total2; /* 0x18 */
    int total; /* 0x1c */
    unsigned int color; /* 0x20 */
    float size; /* 0x24 */
    float k; /* 0x28: eases from 0 to 1 */
    float width; /* 0x2c */
    float rate; /* 0x30 */
    int segs; /* 0x34 */
    int shrink; /* 0x38: 3D, shrinking and fading in instead */
    int ang[3]; /* 0x3c */
} CmCircle2Work;

typedef struct CmCircleWork {
    char pad_00[8];
    Vec3 pos; /* 0x08 */
    int total; /* 0x14 */
    int count; /* 0x18 */
    unsigned int color; /* 0x1c */
    float size; /* 0x20 */
    float fade; /* 0x24: the k from which it fades out */
    float k; /* 0x28: 0 to 1 over its life */
} CmCircleWork;

typedef struct CmPtclWork {
    char pad_00[4];
    void *texlist; /* 0x04 */
    int tex; /* 0x08 */
    int state; /* 0x0c */
    int track; /* 0x10: re-aim along dirsrc each frame */
    int count;
    int total;
    int stop; /* 0x1c: stop emitting at this count */
    int timer; /* 0x20 */
    int interval; /* 0x24 */
    int life_min;
    int life_max;
    int hide; /* 0x30: only shrink the sparks */
    Vec3 pos; /* 0x34 */
    Vec3 *follow; /* 0x40 */
    Vec3 dir; /* 0x44 */
    Vec3 vel; /* 0x50 */
    Vec3 *dirsrc; /* 0x5c */
    int ang_y; /* 0x60 */
    int ang_x; /* 0x64 */
    char pad_68[4];
    int spread; /* 0x6c */
    float k1; /* 0x70 */
    float k2; /* 0x74 */
    float size; /* 0x78 */
    float psize; /* 0x7c */
    float speed; /* 0x80 */
    float dscale; /* 0x84 */
    unsigned int color; /* 0x88 */
    int n; /* 0x8c */
    int max; /* 0x90 */
    BossSpark *p; /* 0x94 */
} CmPtclWork;

typedef struct CmWireBreakWork {
    char pad_00[4];
    void *texlist; /* 0x04 */
    BossShard *s; /* 0x08 */
    void *parent; /* 0x0c */
    int n; /* 0x10 */
    unsigned int flags;
    unsigned int color; /* 0x18 */
    void (*cb)(void *parent, Vec3 *pos, Vec3 *vel); /* 0x1c: called per shard and frame */
    float decay; /* 0x20 */
    Vec3 scale; /* 0x24 */
} CmWireBreakWork;

/* change_symbol_create argument */
typedef struct CsArg {
    Vec3 pos;
    int ang[3];                 /* 0x0c */
    float scl[3];               /* 0x18 */
    float scale; /* 0x24 */
    int skip;                   /* 0x28: as ChangeSymbolArg.skip */
} CsArg;

typedef struct EffCircleArg {
    Vec3 *pos;
    unsigned int color;
    float radius;
    float width;
    int frames;
    int fade;
    int n;
} EffCircleArg;

/* "eff_Circle" work (52 bytes, eff_circle_class): two expanding rings */
typedef struct EffCircleWork {
    Vec3 pos;
    int count; /* 0x0c: frames left */
    int frames; /* 0x10 */
    int fade; /* 0x14: frames of the fade-out */
    unsigned int color; /* 0x18 */
    float radius; /* 0x1c */
    float k1;
    float k2;
    float width; /* 0x28 */
    int seg_a;
    int seg_b;
} EffCircleWork;

/* "effStarCol" work (20 bytes, eff_star_col_class): fades the star colour */
typedef struct EffStarColWork {
    EffStarWork *star;
    unsigned int from;
    unsigned int to;
    int frames;
    int count;
} EffStarColWork;

/* node record in a link task buffer (36 bytes) */
typedef struct NodeRec {
    Vec3 pos; /* ulMatrixGetTranslation */
    Vec3 scale; /* 0x0c (yuki_matrix_get_scale) */
    Vec3 fwd; /* 0x18: the point (0, 0, -1) under the node's matrix */
} NodeRec;

/* Follows an object via its link record (en_backfire_class, _beam_class; 84 bytes). */
typedef struct EnBackFireWork {
    char pad_00[4];
    int tex_list; /* 0x04: texture list (ulGxSetTextureList) */
    int angle; /* 0x08: the sprites' angle */
    BackFireLink link; /* 0x0c */
    int counter; /* 0x40 */
    int n; /* 0x44: nodes of the linked model */
    NodeRec *buf; /* 0x48 */
    int tex; /* 0x4c: the sprite texture (set at creation) */
    int beam_tex; /* 0x50: the beam texture (set at creation) */
} EnBackFireWork;

/* en_hanuman placement */
typedef struct EnHanumanArg {
    char pad_00[16];
    int mark[3]; /* 0x10: marker offset (x 360/65536) */
    Vec3 dir; /* 0x1c: the first move target */
    Vec3 mark2; /* 0x28: second marker offset, from the player */
    Vec3 mark3; /* 0x34: third marker offset */
} EnHanumanArg;

/* en_hanuman_spawn_shield -> en_hanuman_shield_create argument (64 bytes, cleared first) */
typedef struct EnHanumanShieldArg {
    char pad_00[4];
    Vec3 pos; /* 0x04 */
    int ang[3]; /* 0x10 */
    char pad_1c[36];
} EnHanumanShieldArg;

/* en_lightning_create -> en_lightning init argument */
typedef struct EnLightningArg {
    Vec3 *a;
    Vec3 *to; /* 0x04 */
    int n; /* 0x08 */
    int m; /* 0x0c */
} EnLightningArg;

/* en_lightning bolt segment (24 bytes) */
typedef struct EnLightningSeg {
    Vec3 d; /* offset from the straight line */
    float len; /* 0x0c: the jag's length */
    int ang; /* 0x10: its roll */
    int spin; /* 0x14: roll per frame */
} EnLightningSeg;

/* en_lightning work (en_lightning_desc, 28 bytes) */
typedef struct EnLightningWork {
    Vec3 *a; /* the two ends */
    Vec3 *to; /* 0x04 */
    EnLightningSeg *seg; /* 0x08: n * m segments, then the points (one heap block) */
    Vec3 *pts; /* 0x0c: the polyline, (n + 1) * lines + 1 points */
    int n; /* 0x10: segments per line */
    int npts; /* 0x14 */
    int m; /* 0x18 */
} EnLightningWork;

/* "perfectwall_manager" work (212 bytes) */
typedef struct EnPerfectWallManagerWork {
    int phase;
    int next; /* 0x04 */
    int cnt; /* 0x08 */
    int cnt_next; /* 0x0c */
    int sstep; /* 0x10: sub-step of en_perfectwall_manager_state_close */
    int result; /* 0x14: the frame's result (1 hit, 2 timed out, 4, 6) */
    Vec3 pos; /* 0x18 */
    int ang[3]; /* 0x24 */
    Vec3 pts[2]; /* 0x30: the two walls */
    int wall_ang_z; /* 0x48 */
    void *sub[2]; /* 0x4c */
    Vec3 seg[2][2]; /* 0x54 */
    float gap; /* 0x84: half the distance between pts */
    float dgap_unite; /* 0x88: en_perfectwall_manager_state_unite's gap change */
    float dgap; /* 0x8c: the eased part of radius */
    int dang; /* 0x90: the turn of wall_ang_z */
    int rot; /* 0x94: the eased part of the turn */
    float dist; /* 0x98: advanced so far */
    int yaw; /* 0x9c */
    float timer; /* 0xa0 */
    Vec3 wob; /* 0xa4: swing offset added to pos */
    float swing_t; /* 0xb0 */
    float speed; /* 0xb4: from the placement, per second */
    float gap_min; /* 0xb8 */
    float turn; /* 0xbc */
    float gap_spd; /* 0xc0: a placement value / 60 */
    float dist_max; /* 0xc4 */
    int flags; /* 0xc8 */
    float rt; /* 0xcc: en_perfectwall_manager_state_rotate's frame */
    float rt_max; /* 0xd0 */
} EnPerfectWallManagerWork;

/* Armor-break task argument (en_thrones_armor_die_init, en_thrones_armor_die2_init). */
typedef struct EnThronesArmorDieArg {
    Vec3 *pos;
    int *ang; /* 0x04 */
    int i; /* 0x08: row of the armor model table */
    unsigned int color; /* 0x0c */
} EnThronesArmorDieArg;

typedef struct FogArg {
    float range;
    float k;
    int count;
    int color;
} FogArg;

/* event 11 data (lock-on hit; main executable listeners read the same name member) */
typedef struct HitObj {
    char b[6];
    short hits; /* 0x06 */
    char pad_08[6];
    short id; /* 0x0e: the texture force id */
    int flags; /* 0x10: bits 3..5 the kind */
    char pad_14[4];
    char *name; /* 0x18 */
    Vec3 *pos; /* 0x1c */
} HitObj;

typedef struct HitEv {
    HitObj *obj;
    HitSpan *span;
} HitEv;

/* Land wave placement. */
typedef struct LandWaveArg {
    Vec3 pos;
    int ang[3]; /* 0x0c */
    int scl[2];
} LandWaveArg;

/* Land wave work: level visualiser along the path. */
typedef struct LandWaveWork {
    char pad_00[8];
    Vec3 pos; /* 0x08: relative to the player */
    int ang[3]; /* 0x14 */
    LandWaveArg src; /* 0x20 */
    LandWaveArg *arg; /* 0x40 */
    float level[28]; /* 0x44 */
} LandWaveWork;

/* object-shot task argument (enb_normal_class, 440-byte work) */
typedef struct ObjShotArg {
    void *kind; /* object kind ('E_Shot') */
    Vec3 *pos; /* 0x04 */
    Vec3 *dir; /* 0x08 */
    int n; /* 0x0c */
    int *blk; /* 0x10: the 188-byte shot parameters */
} ObjShotArg;

/* Break effect piece (76 bytes). */
typedef struct Piece {
    int alive;
    Vec3 pos; /* 0x04 */
    int ang[3]; /* 0x10 */
    Vec3 scale; /* 0x1c */
    Vec3 vel; /* 0x28 */
    int spin[3]; /* 0x34 */
    unsigned int *src; /* 0x40: the model node it came from */
    void *model; /* 0x44 */
    void *node; /* 0x48 */
} Piece;

/* Ring effect position and angle source (ramses_circle_create, end_circle_create). */
typedef struct RingArg {
    Vec3 *pos;
    int *ang;
} RingArg;

/* "RamsesCircle" and "EndCircle" work (36 bytes): expanding rings that drift with the scroll */
typedef struct RingWork {
    char pad_00[8];
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    int count;                  /* 0x20: frames */
} RingWork;

/* ribbon point (24 bytes) */
typedef struct RibPt {
    Vec3 pos;
    float seg; /* 0x0c: distance to the previous point */
    float acc; /* 0x10: length back to the step boundary */
    float wid; /* 0x14 */
} RibPt;

/* Break effect work: pieces and their motion. */
typedef struct Shatter {
    char pad_00[4];
    void *textures; /* 0x04 */
    unsigned int *root; /* 0x08: the model tree that breaks */
    void *models; /* 0x0c: its wire data */
    void *tree; /* 0x10: the wire data compiled for the tree */
    unsigned int flags; /* 0x14: 0x10000 = pieces without models */
    float time; /* 0x18: frames since the break */
    int count; /* 0x1c */
    Piece *pieces; /* 0x20 */
    float color[4]; /* 0x24: [0] fades from 0 to -1 */
    float inherit; /* 0x34: share of the source velocity */
    float Gravity; /* 0x38 */
    float PowVec; /* 0x3c */
    float PowAng; /* 0x40 */
    float RtoVecDec; /* 0x44 */
    float RtoAngDec; /* 0x48 */
    float RotSpdPlayer; /* 0x4c: share of the player's scroll the pieces keep */
    float TimeAlphaStart; /* 0x50 */
    float TimeAlphaEnd; /* 0x54 */
    int force_wire; /* 0x58 */
    unsigned int pcol_wire; /* 0x5c */
    int b_acc; /* 0x60: ease the first frames instead of the decays */
    float frm_acc; /* 0x64 */
    int fog; /* 0x68: draw with fog setting fog_v */
    void *fog_v; /* 0x6c */
    int blend_a;
    int blend_b;
} Shatter;

typedef struct ShotArg {
    unsigned char kind;
    char unk_01; /* 0 in missile_fire; never read */
    short fx_arg;
    Vec3 pos; /* 0x04 */
    int ang[3]; /* 0x10 */
    Vec3 vel; /* 0x1c */
    float move; /* 0x28: launchers only */
    float delay; /* 0x2c: launchers only */
    int params[4];
} ShotArg;

/* Node position and angles recorded by a ThronesChild node callback (24 bytes). */
typedef struct ThronesChildNode {
    Vec3 pos;
    int ang[3]; /* 0x0c */
} ThronesChildNode;

/* ThronesCore placement (thrones_core_create argument, copied into the work). */
typedef struct ThronesCoreArg {
    Vec3 *pos;
    int *ang; /* 0x04 */
    int time; /* 0x08: frames until it launches */
} ThronesCoreArg;

/* ThronesCore creation record (ObjDesc.kind points to it). */
typedef struct ThronesCoreKind {
    int idx;
    char pad_04[4];
    ThronesCoreArg *arg; /* 0x08 */
} ThronesCoreKind;

/* ThronesCoreManager work (44 bytes; thrones_core_manager_init, _fire, _release). */
typedef struct ThronesCoreManagerWork {
    void *objs[4]; /* ThronesCore objects; effect position candidates */
    int killed; /* 0x10: cores destroyed (thrones_core_set_counters) */
    int escaped; /* 0x14: cores that left */
    int fx; /* 0x18 */
    Vec3 pos; /* 0x1c */
    int op; /* 0x28 */
} ThronesCoreManagerWork;

/* ThronesCore object kind work (thrones_core_types records). */
typedef struct ThronesCoreWork {
    ThronesCoreRec *type;
    int state; /* 0x04: indexes thrones_core_exec_table; 0 until launched */
    int count; /* 0x08: frames */
    Vec3 hitv; /* 0x0c: the lock-on record's vector when hit */
    int time; /* 0x18: frames until it launches */
    ThronesCoreArg arg; /* 0x1c */
    Vec3 move; /* 0x28 */
    float speed; /* 0x34 */
    char pad_38[4];
    int unk_3c; /* 0 in thrones_core_init; never read */
    char pad_40[12];
    float fire; /* 0x4c: the back fire size */
    void *tail; /* 0x50 */
    int roll; /* 0x54 */
    void *link[4]; /* 0x58: the cores placed together */
    int lost; /* 0x68: linked cores destroyed */
    int downed; /* 0x6c: linked cores destroyed after launching */
    int *killed; /* 0x70: counted up when it is destroyed */
    int *escaped; /* 0x74: counted up when it leaves */
} ThronesCoreWork;

/* Thrones core slot (36 bytes): node pose and its ThronesCore. */
typedef struct ThronesSlot {
    Vec3 pos;
    int ang[3]; /* 0x0c */
    void *obj; /* 0x18: the ThronesCore (thrones_core_create) */
    int live; /* 0x1c: the armor is still on */
    int wait; /* 0x20: frames until the core is released */
} ThronesSlot;

typedef struct TiamatHeadArg {
    int w[16];
} TiamatHeadArg;

/* tiamat_laser_attack_ray_create argument (16 bytes). */
typedef struct TiamatLaserAttackRayArg {
    Vec3 *pos;
    int *ang; /* 0x04 */
    int frames; /* 0x08 */
    void *owner; /* 0x0c */
} TiamatLaserAttackRayArg;

/* Trail strip vertex. */
typedef struct TrailVtx {
    float x;
    float y;
    float q;
    unsigned int c;
} TrailVtx;

typedef struct VenusZxybWork {
    Vec3 pos;
    Vec3 vel; /* 0x0c */
    int ang[3]; /* 0x18 */
    char pad_24[4];
    int count;
    int total;
    BossBeam beam[6]; /* 0x30 */
} VenusZxybWork;

/* Arm segment chain node (216 bytes, an arm_slots slot) */
typedef struct BossArmNode {
    struct BossArmNode *prev;
    struct BossArmNode *next; /* 0x04 */
    Vec3 spread; /* 0x08: the first node's z is the segment length */
    Vec3 pos; /* 0x14 */
    Vec3 dir; /* 0x20: the step to the next node */
    Vec3 unk_2c; /* 0x2c: cleared by arm_seg_append / arm_exec; never read */
    void *unk_38; /* 0x38: only ever cleared */
    Vec3 unk_3c; /* 0x3c: cleared by arm_seg_append; never read */
    int ang[3]; /* 0x48: x, y, z turns (arm_seg_turn_nodes) */
    Vec3 scale; /* 0x54: the beat wobble (arm_seg_apply_wobble) */
    Vec3 tip; /* 0x60: pos - dir / 2 (arm_seg_node_tips) */
    LockObj lock; /* 0x6c */
    float wave_phase; /* 0xac: a phase (0x1000) */
    float wave_step; /* 0xb0: pi / the segment's wave_cfg[2] */
    float shake;
    float shake_v;
    Vec3 hit_vec; /* 0xbc */
    int flags; /* 0xc8: 0x400 first node, 0x800 lock-on, 0x2000 hit */
    int drawflags; /* 0xcc: flags & 0x1800, plus 2 when lit */
    unsigned int *color_cell; /* 0xd0: a colour cell, from arm_attach_cube */
    struct BossCube *ent; /* 0xd4: a carried entry */
} BossArmNode;

typedef struct BossBake {
    int nodes;
    int frames; /* 0x04 */
    WireObj **tree; /* 0x08: per frame, nodes in walk order */
} BossBake;

/*
 * Shell cube (304 bytes, part 0 node). State: 0 in shell, 1 back to ofs, 2 held, 3 homing to an
 * Arm node, 4/5 carried, 6 dropped, 7..9 kuri hand pickup, 10 on hand, 11 released, 12 charging
 * the player (kuri_*), 13/14 scattered and flying back (cubes_scatter).
 */
typedef struct BossCube {
    int state; /* 7..12: busy (kuri_hit_check) */
    int stage; /* 0x04: the main task's stage when it broke off */
    int poly; /* 0x08: index into cube_poly_flags (cube_face_check) */
    Vec3 vel; /* 0x0c */
    Vec3 pos; /* 0x18: from the model node */
    Vec3 prev; /* 0x24 */
    float pull; /* 0x30: homing_step's pull, the pick-up delay in state 7 */
    int turn[3]; /* 0x34: the main task's turn step when it broke off */
    int rot[3]; /* 0x40: from the model node's angles */
    int spin[3]; /* 0x4c: random */
    char pad_58[4];
    WireObj *obj; /* 0x5c: the model node */
    WireObj *obj2[3]; /* 0x60: the matching nodes of three other trees (cubes_build_tree4) */
    char pad_6c[4];
    int hand_ang[3]; /* 0x70: its angle around the kuri hand */
    int tilt[3]; /* 0x7c: random tilts of the release, the charge's roll */
    Vec3 ofs; /* 0x88: its place: grid cell (cube_place_on_grid) or hand radius */
    Vec3 ofs2; /* 0x94: ofs saved by kuri_hand_arm; the charge's vector */
    float drift_x;
    float spin_y;
    char pad_a8[4];
    int node_pos;
    int node_ang;
    int node_scale;
    int node_flags;
    int color;
    short sign; /* 0xc0: +1 or -1 */
    short locked; /* 0xc2: its lock-on is registered */
    int frames; /* 0xc4: frames */
    LockObj lock; /* 0xc8 */
    float gscl; /* 0x108: eased towards 'satr'/'satr2' ('G_scl') */
    float fade; /* 0x10c: 0..1, by mode */
    float anim_t; /* 0x110: part 20's motion frame while charging */
    float anim_end; /* 0x114 */
    struct BossShellGrp *grp; /* 0x118 */
    int timer; /* 0x11c: the fade's frames left */
    int fade_total;
    int fade_in;
    int mode; /* 0x128: cube_set_mode */
    int hand; /* 0x12c: the hand (kuri_hand_arm) */
} BossCube;

/* Earth main task group (40 bytes): entries below one model node */
typedef struct BossShellGrp {
    int done;
    int n; /* 0x04 */
    int present; /* 0x08: its cubes in the shell */
    BossCube *ent[6]; /* 0x0c */
    LockObj *lock; /* 0x24: the model entry (model_get_part) */
} BossShellGrp;

/* Earth main task entry set (in its work) */
typedef struct BossCubeSet {
    BossCube ents[400];
    BossShellGrp grp[100]; /* 0x1db00: one per entry of part 0's model (model_get_part) */
    int nents; /* 0x1eaa0 */
    int ngrp; /* 0x1eaa4 */
    int nslot; /* 0x1eaa8: entries in the current group */
} BossCubeSet;

/* en_hanuman work */
typedef struct EnHanumanWork {
    Vec3 *pos; /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec; /* 0x04 */
    int *ang; /* 0x08 */
    Vec3 *scale; /* 0x0c */
    int phase; /* 0x10: the state table's index (en_hanuman_states) */
    int next; /* 0x14 */
    unsigned int step; /* 0x18 */
    int next_step; /* 0x1c */
    int sel; /* 0x20: the move target, 0 = the object's v32 */
    int cur; /* 0x24 */
    Vec3 tgt[2]; /* 0x28 */
    Vec3 rel; /* 0x40: the position relative to the player */
    Vec3 to; /* 0x4c: the target minus rel */
    Vec3 bf_pos; /* 0x58: spawn point 1 (en_hanuman_get_spawn_points) */
    Vec3 scl; /* 0x64: the backfire's scale */
    Vec3 vel; /* 0x70: damped by 0.97 each frame */
    Vec3 last; /* 0x7c: the last move step (en_hanuman_state_move) */
    int aoff[3]; /* 0x88: angle offset of the turn (en_hanuman_state_anticipate) */
    int atot[3]; /* 0x94: the whole turn (en_hanuman_state_stop) */
    int atgt[3]; /* 0xa0: full angle offset of the turn */
    LockObj lock; /* 0xac */
    Vec3 vec; /* 0xec: spawn point 0 */
    char pad_f8[4];
    float frame; /* 0xfc */
    float frames; /* 0x100 */
    int dir_set; /* 0x104 */
    Vec3 moved; /* 0x108: the distance moved in the turn */
} EnHanumanWork;

/* en_hunter_params record: name, label, 20 words; names from en_hunter_vardefs */
typedef struct EnHunterRec {
    char *name;
    char *label; /* 0x04 */
    int hp; /* 0x08 */
    int score; /* 0x0c */
    float r_swing; /* 0x10: wobble amplitude */
    float frm_attack_motion; /* 0x14 */
    float frm_escape_motion; /* 0x18 */
    float frm_float_motion; /* 0x1c */
    float dst_escape_anticipate; /* 0x20: sideways sway while beaming */
    float spd_escape1_max; /* 0x24: leaving speed (per second) */
    float spd_escape2_max; /* 0x28: retreat speed */
    float acc_escape1; /* 0x2c: per second squared */
    float acc_escape2; /* 0x30 */
    float dst_away_z; /* 0x34: util_ahead_of_player's radius */
    float pow_shot; /* 0x38 */
    float pow_laser; /* 0x3c */
    float frm_stop_motion; /* 0x40: animation stop after a hit */
    float sca_fire_max; /* 0x44: beam scale */
    float sca_fire_min; /* 0x48 */
    float collision_r; /* 0x4c: the object's f38 */
    int col_wire; /* 0x50 */
    int test; /* 0x54 */
} EnHunterRec;

/* en_hunter object work (312 bytes past the object; en_hunter_calc_points updates it) */
typedef struct EnHunterWork {
    Vec3 *pos; /* 0x00: the object's (enemy_task_pos_ptr) */
    Vec3 *vel; /* 0x04 */
    int *ang; /* 0x08: enemy_task_rot_ptr */
    Vec3 *scale; /* 0x0c: enemy_task_scl_ptr */
    unsigned int phase; /* 0x10: the state table's index */
    int next; /* 0x14: the phase for the next frame */
    unsigned int step; /* 0x18: 0 starts the phase */
    int next_step; /* 0x1c */
    Vec3 off; /* 0x20: the swing offset en_hunter_swing adds to the position */
    float t; /* 0x2c: wobble time */
    Vec3 knock; /* 0x30 */
    int aknock[3]; /* 0x3c: angular knock (decays by 0.95) */
    Vec3 drift; /* 0x48: sway while beaming */
    Vec3 pts[8]; /* 0x54: the eight spawn points (en_hunter_calc_spawn_points) */
    void *flash; /* 0xb4: the last task en_hunter_fire_flashes created */
    Vec3 muzzle; /* 0xb8 */
    Vec3 beam; /* 0xc4: the fire effect's scale */
    int wire; /* 0xd0 */
    float alpha; /* 0xd4: en_hunter_state_fade_in's ramp, 0..1 */
    int type; /* 0xd8: parameter 0 clamped to 1..3 */
    int spin; /* 0xdc: parameter 4 (degrees per second) as an angle per frame */
    float speed; /* 0xe0 */
    char pad_e4[4];
    LockObj lock; /* 0xe8 */
    int skip; /* 0x128: bits 0/1 = en_hunter_calc_tip/en_hunter_calc_spawn_points ran this frame; bits 7..10 set by en_hunter_break_part */
    float cnt; /* 0x12c */
    float cnt_end; /* 0x130 */
    float wait; /* 0x134: animation stop after a hit */
} EnHunterWork;

/* Named by the 'TIAMAT LASER' VarDefs. */
typedef struct TiamatLaserParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float hp_reduce;            /* 0x0c */
    float collision_r;          /* 0x10 */
    float damage;               /* 0x14 */
    float width;                /* 0x18 */
    float width_add;            /* 0x1c */
    float width_density;        /* 0x20 */
    float width_add_density;    /* 0x24 */
    float alpha_density;        /* 0x28 */
    float len;                  /* 0x2c */
    float speed;                /* 0x30 */
    float speed_away;           /* 0x34 */
    float away_sec;             /* 0x38 */
    float turn_sec;             /* 0x3c */
} TiamatLaserParams;

typedef struct TiamatLaserVars {
    char *name;
    char *label;                /* 0x04 */
    TiamatLaserParams p;        /* 0x08 */
} TiamatLaserVars;

/* Tiamat's laser (tiamat_laser_desc, 204 bytes): a beam from the head to the tail, both kept
 * relative to the player. */
typedef struct TiamatLaserWork {
    void *models;
    void *textures;             /* 0x04 */
    TiamatLaserVars *type;      /* 0x08 */
    int state;                  /* 0x0c: 0 move, 1 away, 2 hit, 3 turn */
    Vec3 pos;                   /* 0x10: head */
    Vec3 bend;                  /* 0x1c: the point where it was shot down */
    Vec3 tail;                  /* 0x28 */
    Vec3 tail_ofs;              /* 0x34: tail - player */
    Vec3 ofs;                   /* 0x40: head - player */
    Vec3 bend_ofs;              /* 0x4c */
    Vec3 dir0;                  /* 0x58: the starting direction */
    Vec3 dir;                   /* 0x64 */
    int fx;                     /* 0x70: effect slot */
    int fx_op;                  /* 0x74 */
    LockObj lock;               /* 0x78 */
    int life;                   /* 0xb8: frames left before it hits */
    int frames;                 /* 0xbc: frames in the state */
    float width;                /* 0xc0 */
    float width_add;            /* 0xc4 */
    float alpha;                /* 0xc8 */
} TiamatLaserWork;

/*
 * Title screen camera task (demo_camera_desc, 72 bytes; hooks "Demo Camera" at
 * demo_camera_hooks): plays a camera motion from the screen's models over its length.
 */
typedef struct Cam {
    void *models;               /* 0x00: inherited */
    char pad_04[4];
    Camera cam;                 /* 0x08 */
    void *node;                 /* 0x30 */
    void *motion;               /* 0x34 */
    void *target;               /* 0x38 */
    float t;                    /* 0x3c */
    float dt;                   /* 0x40 */
    float end;                  /* 0x44 */
} Cam;

/* map_hook_draw object (some stage callbacks set its color). */
typedef struct DrawObj {
    char pad_00[12];
    unsigned int color;         /* 0x0c */
    char pad_10[16];
    int unk_20; /* 0x20: 0 in draw_cb_list3; never read */
} DrawObj;

/* "attack_ray" work (108 bytes, descriptor copied from attack_ray_desc) */
typedef struct AttackRayWork {
    Vec3 *pos;                  /* 0x00 */
    Vec3 *scale;                /* 0x04 */
    ModelPart body;                /* 0x08: models attack_ray_model / attack_ray_motion */
    float t;                    /* 0x64: frames played */
    float dur;                  /* 0x68 */
} AttackRayWork;

/* Arm task segment, one of eight (320 bytes) */
typedef struct BossArmSeg {
    BossArmNode *head;
    BossArmNode *tail;          /* 0x04 */
    struct BossArmNode *target; /* 0x08: a node whose lock-on is held */
    int pitch;                  /* 0x0c */
    int yaw;                    /* 0x10: an angle */
    int swing_yaw;              /* 0x14 */
    int pitch_add[3];           /* 0x18: added to the x turn */
    Vec3 start;                 /* 0x24 */
    Vec3 dir;                   /* 0x30: the first node's step */
    Vec3 spread;                /* 0x3c */
    int flags;                  /* 0x48: 0x10 = empty */
    int bubble_cell;            /* 0x4c */
    float sweep_phase;          /* 0x50: a phase */
    float sweep_step, sweep_end, sweep_wait, sweep_r; /* 0x54: arm_seg_sweep_start */
    int sweep_n, sweep_i;       /* 0x64 */
    int sweep_ang;              /* 0x6c: an angle */
    Vec3 sweep_center;          /* 0x70 */
    float blend;                /* 0x7c */
    float thrust_phase, thrust_step; /* 0x80: a phase and its step (flag 4) */
    float thrust_speed, recoil, thrust_len; /* 0x88 */
    float wave_cfg[4];          /* 0x94: wave timings and node scale amplitudes (arm_init, arm_exec) */
    int wave_time[2];           /* 0xa4: frames to the next wave and the period (arm_seg_wave_start) */
    int waves_left;             /* 0xac: waves still to start */
    int wave_n[2];              /* 0xb0: waves running and done */
    float bubble_scale;         /* 0xb8: a bubble's scale (bubble_spawn_carried) */
    float wave_phase, wave_step; /* 0xbc: the wave's phase and its step */
    float swing_phase, swing_step; /* 0xc4: a phase and its step (flag 2) */
    float swing_amp, swing_turns; /* 0xcc: degrees added per swing, half turns of the swing */
    int unk_d4; /* 0 in arm_init; never read */
    int counter;                /* 0xd8: lock hits left (arm_hit_check), then frames to the next drop (arm_seg_retire) */
    int count;                  /* 0xdc */
    int active;                 /* 0xe0 */
    ModelPart part;                /* 0xe4 */
} BossArmSeg;

/* "Arm" (16896 bytes, arm_desc) */
typedef struct BossArmWork {
    char pad_00[44];
    int launch_state;           /* 0x2c */
    char pad_30[12];
    Vec3 *pos;                  /* 0x3c: from the creation argument */
    Vec3 *ofs;                  /* 0x40 */
    Vec3 *dir;                  /* 0x44 */
    int *ang;                   /* 0x48 */
    int *roty;                  /* 0x4c */
    int *cmd;                   /* 0x50 */
    int prev_roty;              /* 0x54: *roty last frame (arm_segs_update) */
    BossArmSeg seg[8];          /* 0x58 */
    int chain_buf[3456];        /* 0xa58: chain_step_both's scratch links */
    float yaw_limit;            /* 0x4058 */
    ModelPart *part;               /* 0x405c */
    NjCnkModel *models[32];      /* 0x4060: part's child chain (arm_collect_models) */
    int nmodels;                /* 0x40e0 */
    char pad_40e4[12];
    float mtx[16];              /* 0x40f0: turned towards the player (arm_exec) */
    int period;                 /* 0x4130: frames between launches */
    int timer;                  /* 0x4134 */
    int left;                   /* 0x4138: launches left */
    int order[4];               /* 0x413c: a shuffle of the four directions */
    int spin[3];                /* 0x414c */
    void (*disp_cb)(void *w);   /* 0x4158: the argument's disp_cb, run by arm_disp */
    void (*update_cb)(void *w); /* 0x415c: the argument's update_cb, run by arm_exec */
    void *cb_arg;               /* 0x4160: the argument's cb_arg (the main task) */
    float wave[32];             /* 0x4164: the beat wobble per node (arm_wobble_update) */
    int beat_toggle;            /* 0x41e4: toggled on beats */
    void *parent;               /* 0x41e8 */
    int enabled;                /* 0x41ec: set by arm_enable; gates the callbacks */
    int launch_started;
    int wobble_timer;           /* 0x41f4: frames to the next wobble shift */
    char pad_41f8[8];
} BossArmWork;

typedef struct CmObjMotWork {
    char pad_00[4];
    void *texlist;              /* 0x04 */
    union {
        ModelPart part;            /* 0x08 */
        struct {
            int w[15];
            float t;            /* 0x44: the instance's time */
            float len;          /* 0x48 */
            float speed;        /* 0x4c */
        } anim;
    } m;
    Vec3 pos;                   /* 0x64: its own position when not following */
    Vec3 *ppos;                 /* 0x70 */
    Vec3 scale;                 /* 0x74 */
    int ang[3];                 /* 0x80 */
    int *pang;                  /* 0x8c */
    int follow;                 /* 0x90: draw at the caller's position and angles */
    int state;                  /* 0x94 */
    int count, total;           /* 0x98 */
    char pad_a0[4];
} CmObjMotWork;

typedef struct CmObjectBlurWork {
    char pad_00[4];
    void *texlist;              /* 0x04 */
    ModelPart *model;              /* 0x08 */
    Vec3 *pos;                  /* 0x0c: followed */
    int *ang;                   /* 0x10 */
    int step;                   /* 0x14: frames to the next sample */
    int interval;               /* 0x18 */
    int count, total;           /* 0x1c: the fade-out, 0 = none */
    char pad_24[4];
    int n;                      /* 0x28 */
    int head;                   /* 0x2c */
    int fog;                    /* 0x30: passed to fog_set_enable while drawing */
    float alpha;                /* 0x34 */
    float t;                    /* 0x38: animation time */
    float speed;                /* 0x3c */
    float end;                  /* 0x40 */
    void *clip;                 /* 0x44: animate when set */
    BossTrailEnt *ring;         /* 0x48 */
} CmObjectBlurWork;

/* en_hanuman_shield_break work */
typedef struct EnHanumanShieldBreakWork {
    Vec3 pos;                   /* relative to the player until the exec adds it back */
    int ang[3];                 /* 0x0c */
    int spin[3];                /* 0x18: per frame */
    Vec3 vel;                   /* 0x24 */
    ModelPart body;                /* 0x30 */
    float alpha;                /* 0x8c */
    int part;                   /* 0x90 */
    float t;                    /* 0x94: frames */
} EnHanumanShieldBreakWork;

/* Object with a model part and seven lock-on records. */
typedef struct EnHanumanShieldWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: en_hanuman_shield's state table index (en_hanuman_shield_states) */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    int tang[3];                /* 0x20: target angles */
    float spd;                  /* 0x2c */
    int flags;                  /* 0x30: bit i = part i present, bit 14 + i = part i broken */
    float u;                    /* 0x34: texture scroll */
    float v;                    /* 0x38 */
    ModelPart body;                /* 0x3c */
    LockObj lock[7];               /* 0x98 */
    Vec3 pts[7];                /* 0x258 */
    int angs[7][3];             /* 0x2ac: the parts' angles */
    int k;                      /* 0x300: from en_hanuman_shield_create (en_hanuman's dir_set) */
    float timer;                /* 0x304: frames */
    void *owner;                /* 0x308: the en_hanuman object (en_hanuman_shield_create) */
} EnHanumanShieldWork;

/* en_perfectwall work (800 bytes) */
typedef struct EnPerfectWallWork {
    char pad_00[16];
    int kind;                   /* 0x10: 0 or 1, picks the point tables */
    int n;                      /* 0x14: points in pts */
    int phase;                  /* 0x18 */
    int next;
    int step;                   /* 0x20 */
    int next_step;
    ModelPart kind_part;                 /* 0x28 */
    ModelPart wire_part;                 /* 0x84 */
    int src;                    /* 0xe0 */
    ModelPart body;                /* 0xe4 */
    ModelPart glow;                /* 0x140 */
    Vec3 tip;                   /* 0x19c: en_perfectwall_calc_tip's point */
    int ang[3];                 /* 0x1a8: from en_perfectwall_manager_get_angles */
    float flash;                /* 0x1b4 */
    float flash_t;              /* 0x1b8 */
    ModelPart part;                /* 0x1bc */
    Vec3 pts[3];                /* 0x218: en_perfectwall_calc_points's points */
    Vec3 ends[2];               /* 0x23c: en_perfectwall_calc_ends's points */
    LockObj lock[3];               /* 0x254 */
    int skip;                   /* 0x314: bits 0..2 skip en_perfectwall_calc_points, en_perfectwall_calc_ends, en_perfectwall_calc_tip (they set them) */
    float timer;                /* 0x318 */
    float timer_max;            /* 0x31c */
} EnPerfectWallWork;

/* enThronesArmor piece (232 bytes). */
typedef struct EnThronesArmorPiece {
    ModelPart model;
    Vec3 pos;                   /* 0x5c */
    int ang[3];                 /* 0x68 */
    Vec3 vel;                   /* 0x74 */
    int spin[3];                /* 0x80 */
    ModelPart body2;                /* 0x8c: drawn in colour 0x2cc of the work */
} EnThronesArmorPiece;

/* enb_hunter / enb_chaser work (340 bytes; released by enb_hunter_release) */
typedef struct EnbHunterWork {
    char pad_00[8];
    int state;                  /* 0x08: index into enb_hunter_states */
    int next;                   /* 0x0c */
    unsigned int step;          /* 0x10 */
    int next_step;              /* 0x14 */
    EnbHunterKind kind;         /* 0x18: 0 enb_hunter, 1 enb_chaser (enb_hunter_sets[kind]) */
    ModelPart a;                   /* 0x1c */
    ModelPart ring;                   /* 0x78 */
    int roll;                   /* 0xd4 */
    Vec3 fx_pos;                /* 0xd8: the position */
    int ang[3];                 /* 0xe4 */
    Vec3 dir;                   /* 0xf0 */
    float spd;                  /* 0xfc */
    float size;                 /* 0x100: grows to enb_hunter_max_sizes[kind] */
    LockObj lock;                  /* 0x104 */
    int fx_op;                  /* 0x144 */
    int fx;                     /* 0x148 */
    int flags;                  /* 0x14c: 1 released, 2 near warning given */
    float life;                 /* 0x150: frames left */
} EnbHunterWork;

/* "enbNormal" work (440 bytes): enemy shot */
typedef struct EnbNormalWork {
    int unk_00; /* 0 in enb_normal_init; never read */
    void *tex;                  /* 0x004: texture list (ShotDesc.tex) */
    int fx;                     /* 0x008: effect slot (enemy_status_ent_alloc) */
    int fx_op;                  /* 0x00c: the slot's closing event, 0 once closed */
    LockObj lock;                  /* 0x010 */
    Vec3 lpos;                  /* 0x050: the lock-on point */
    int mask;                   /* 0x05c: the model nodes it follows (ShotDesc.mask) */
    int score;                    /* 0x060: ShotDesc.score */
    float damage;                  /* 0x064: ShotDesc.damage */
    int done;                   /* 0x068: ends after the next draw */
    unsigned int type;          /* 0x06c: 0 model, 1 sprite */
    void *fog;                  /* 0x070 */
    float radius;               /* 0x074 */
    int has_model;              /* 0x078 */
    ModelPart body;                /* 0x07c */
    int ang[3];                 /* 0x0d8 */
    void *tex2;                 /* 0x0e4 */
    int spr_tex;                /* 0x0e8: from the ShotDesc */
    float spr_w;                /* 0x0ec */
    float spr_h;                /* 0x0f0 */
    int spr_ang;                /* 0x0f4 */
    unsigned int spr_col;       /* 0x0f8: from the ShotDesc */
    int st_fc;                  /* 0x0fc */
    int st_a, st_b;             /* 0x100 */
    int spr2;                   /* 0x108: two crossed strips */
    int frames;                 /* 0x10c: ShotDesc.motion_sec * 60 */
    float t;                    /* 0x110 */
    float spd;                 /* 0x114: speed per frame (ShotDesc.spd_start / 60) */
    float spd_chg_rate;                 /* 0x118: speed factor near the target */
    float spd_chg_dis_start, spd_chg_dis_finish;           /* 0x11c: far and near distances */
    int curve_time;                   /* 0x124: bend amount */
    int curve_type;                   /* 0x128: bend shape */
    float unk_12c; /* must be 0 for enb_normal_move's contact damage; never written */
    Vec3 target;                /* 0x130 */
    Vec3 pos;                   /* 0x13c */
    Vec3 d;                     /* 0x148 */
    float cur_spd, dis;           /* 0x154 */
    float len;                  /* 0x15c */
    Vec3 dir;                   /* 0x160 */
    Vec3 vel;                   /* 0x16c */
    int trail;                  /* 0x178: ShotDesc.use_tail */
    int ang2[3];                /* 0x17c */
    int roll, roll_spd;             /* 0x188 */
    void (*cb)(struct ShotHit *h); /* 0x190: from the ShotDesc */
    int level;                  /* 0x194 */
    int spin;                   /* 0x198 */
    int flip;                   /* 0x19c */
    int started;                   /* 0x1a0 */
    int life;                   /* 0x1a4: ShotDesc.frames */
    int face_target;                   /* 0x1a8: from the ShotDesc */
    int clip_ang;                   /* 0x1ac */
    int dead;                   /* 0x1b0 */
    int unk_1b4;                   /* 0x1b4: ShotDesc.unk_b8 */
} EnbNormalWork;

/* KhufuXL enemy work. */
typedef struct KhufuXLWork {
    KhufuXLVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    int shot_set;               /* 0x18: its 'E_Shot' set (-1: none, aims at the player) */
    Vec3 pos;                   /* 0x1c */
    LockObj lock;                  /* 0x28 */
    Vec3 tip;                   /* 0x68 */
    int aim[3];                 /* 0x74: the facing of a shot of its set (e_shot_initial_angles) */
    int fire;                   /* 0x80: fires an 'E_Shot' */
    Vec3 scale;                 /* 0x84 */
    int ang0[3];                /* 0x90: the angles when its state last changed */
    float speed;                /* 0x9c */
    Vec3 dir;                   /* 0xa0: towards its exit point */
    int dang[3];                /* 0xac: its angles */
    float alpha;                /* 0xb8 */
    int life;
    ModelPart body;                /* 0xc0 */
    ModelPart ring;                /* 0x11c */
    float backfire_scl;         /* 0x178: linked as the 'enBackFire' sprites' size */
    Vec3 sway;                  /* 0x17c: the offset added last frame */
    int turn[3];                /* 0x188 */
    int end;                    /* 0x194: frames to the shot */
    int flag;                   /* 0x198: shot */
} KhufuXLWork;

/* 'LandWaveMng' work (land_wave_mng, 248 bytes): area path model, waves spawned along it. */
typedef struct LandWaveMngWork {
    void *models;               /* 0x00 */
    char pad_04[4];
    ModelPart part;                /* 0x08: the path model */
    char pad_64[4];
    int last;                   /* 0x68: the last node a wave was spawned at */
    float bias[28];             /* 0x6c: raises the waves */
    unsigned int flags;         /* 0xdc: button history (bit 0 = this frame), 2 = paused */
    int nrec;                   /* 0xe0 */
    int *recs;                  /* 0xe4: 32-byte node records */
    int fade;                   /* 0xe8 */
    int fade_len;               /* 0xec */
    int variant;                /* 0xf0: path model 1-5, 0 = none */
    int restart;                /* 0xf4 */
} LandWaveMngWork;

/* Ramses enemy work (580 bytes). */
typedef struct RamsesWork {
    RamsesVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    int shot_set;               /* 0x18: its 'E_Shot' set (-1: none, aims at the player) */
    Vec3 pos;                   /* 0x1c */
    LockObj lock;                  /* 0x28 */
    Vec3 local_pos;             /* 0x68: the model's offset, turned by the object's angles */
    int local_ang[3];           /* 0x74 */
    Vec3 dir;                   /* 0x80 */
    float speed;                /* 0x8c */
    Vec3 pos2;                  /* 0x90: the second pose (enemy_task_set_status_pos) */
    int ang2[3];                /* 0x9c */
    Vec3 halo_pos;              /* 0xa8: node flag 0x1000000 */
    Vec3 wing_pos;              /* 0xb4: node flag 0x1000000 of the first pose */
    int aim[3];                 /* 0xc0: the facing of a shot of its set (e_shot_initial_angles), in its own frame */
    Vec3 tip;                   /* 0xcc */
    int shot;                   /* 0xd8 */
    Vec3 scale;                 /* 0xdc */
    int ang0[3];                /* 0xe8: its angles when its state last changed */
    int end_time;               /* 0xf4: the placement's end time */
    ModelPart body;                /* 0xf8 */
    ModelPart ring;                /* 0x154 */
    ModelPart wing;                /* 0x1b0 */
    Vec3 sway;                  /* 0x20c: the offset added last frame */
    int turn[3];                /* 0x218 */
    float ground_y;             /* 0x224: the height of the cut plane */
    float rise;                 /* 0x228: 0..1 out of the ground */
    float halo_scale;           /* 0x22c */
    int end;                    /* 0x230: frames to the shot */
    int flag;                   /* 0x234: shot */
    int count;                  /* 0x238: frames, for the rings */
    float radius0, wing_radius0;        /* 0x23c: the object's and the wing's first radii */
} RamsesWork;

/* shot callback argument (e_shot_event and its two handlers) */
typedef struct ShotHit {
    int op;                     /* 0x00: the event */
    void *owner;                /* 0x04 */
    int level;                  /* 0x08: the E_Shot variable set */
    int *spin;                  /* 0x0c */
    int *flip;                  /* 0x10: alternates the glow colour */
    Vec3 *pos;                  /* 0x14 */
    int *ang;                   /* 0x18 */
    ModelPart *model;              /* 0x1c */
    int spr_ang;                /* 0x20 */
    LockObj *lock;                 /* 0x24 */
    float t;                    /* 0x28: the shot's age */
    Vec3 *lpos;                 /* 0x2c: its lock-on point */
} ShotHit;

typedef struct TrailArg {
    ModelPart *body;
    Vec3 *pos;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    int tail_seg;               /* 0x0c */
    int frames;                 /* 0x10 */
    int tail_tex, tail_mask;    /* 0x14 */
    float tail_width;           /* 0x1c */
    int one;                    /* 0x20 */
    int tail_skip;              /* 0x24 */
} TrailArg;

typedef struct AirArg { void *owner; TrailArg *t; } AirArg;

/* "AirMng" work (120 bytes, air_mng_class): trail behind a shot */
typedef struct AirMngWork {
    char pad_00[4];
    void *tex;                  /* 0x04 */
    void *owner;                /* 0x08: the shot task, cleared when it ends */
    TrailArg t;                 /* 0x0c */
    Ribbon rib;                 /* 0x34 */
    void *mem;                  /* 0x50 */
    int life;                   /* 0x54: frames left after the owner ended */
    int frames;                 /* 0x58 */
    float fade;                 /* 0x5c */
    Vec3 last;                  /* 0x60 */
    int has_last;               /* 0x6c */
    float width;                /* 0x70 */
    int stop;                   /* 0x74 */
} AirMngWork;

typedef struct BossCameraWork {
    void *blk;                  /* 0x00: looked up by pnb_get_entry */
    char pad_04[4];
    int camsrc;                 /* 0x08 */
    unsigned int *mot;          /* 0x0c: [1] is its frame count */
    int path_mot;                 /* 0x10 */
    float t;                    /* 0x14: the motion time, cleared on reset */
    float speed;               /* 0x18 */
    float frames;               /* 0x1c */
    Camera cam;                /* 0x20 */
    Camera prev;               /* 0x48 */
    void *node;                 /* 0x70: camera_node_add's */
    int state;                  /* 0x74 */
    Vec3 eye;                   /* 0x78: the current camera's eye when last seen */
    Vec3 *target;               /* 0x84 */
    Vec3 vel;                   /* 0x88: the eye's last move */
    Vec3 ofs0;                  /* 0x94 */
    Vec3 ofs;                   /* 0xa0 */
    int count;                  /* 0xac */
    int total;                  /* 0xb0 */
    int pv[3];                  /* 0xb4: a vector of the player's work (player_set_eye) */
    int pv0[3];                 /* 0xc0 */
    unsigned int flags;         /* 0xcc */
    int hold;                 /* 0xd0 */
    int ease_out;                 /* 0xd4 */
    int ease_in;                 /* 0xd8 */
    int fov;                    /* 0xdc */
    void (*cb)(int, Vec3 *);    /* 0xe0: told the camera's move each update */
    int cb_arg;                 /* 0xe4 */
    Vec3 mv;                    /* 0xe8 */
    float path_dist;               /* 0xf4 */
    int timer;                  /* 0xf8 */
    int next;                   /* 0xfc: the state when timer runs out */
#ifdef VERSION_US
    int node_on;                /* 0x100: node is registered */
#endif
} BossCameraWork;

typedef struct BossCamAngWork {
    int from;
    int to;                     /* 0x04 */
    int wait;                   /* 0x08 */
    int dur;                    /* 0x0c */
    int phase;                  /* 0x10 */
    BossCameraWork *cam;              /* 0x14 */
} BossCamAngWork;

/* Stage core object work. */
typedef struct ArchiverWork {
    void *models;               /* 0x00 */
    int textures;
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 dir;                   /* 0x20 */
    Vec3 acc;                   /* 0x2c */
    Vec3 tgt;                   /* 0x38 */
    Vec3 vel;                   /* 0x44 */
    Vec3 spos;                  /* 0x50: a spring trailing the position */
    int ang2[3];                /* 0x5c: tilt from the spring's lag */
    Vec3 svel;                  /* 0x68 */
    Vec3 sacc;                  /* 0x74 */
    int motion;                 /* 0x80 */
    ModelPart body;                /* 0x84 */
    ModelPart ring;                /* 0xe0 */
    LockObj lock;                  /* 0x13c */
    int fx;                     /* 0x17c: effect slot */
    char pad_180[4];
    int fx_op;                  /* 0x184 */
    int state;                  /* 0x188 */
    int timer;                  /* 0x18c */
    float side;                 /* 0x190 */
    float up;                   /* 0x194 */
    float drag;                 /* 0x198 */
    float delay;                /* 0x19c: frames before it starts */
    float maxspd;               /* 0x1a0 */
    float life;                 /* 0x1a4: frames of the final flight */
    char pad_1a8[8];
    float mtx[16];              /* 0x1b0 */
} ArchiverWork;

/* enemy shot description (188 bytes, e_shot_fire and similar); member notes give word index */
typedef struct ShotDesc {
    int kind;                   /* 0: effect kind (enemy_status_ent_alloc) */
    int slot;                   /* 1: set to -1, never read */
    unsigned int type;          /* 2: 0 model, 1 sprite */
    void *fog;                  /* 3 */
    float radius;               /* 4: below 0 -> the model's or the sprite's */
    int *root;                  /* 5 */
    int clip;                   /* 6 */
    void *tex;                  /* 7 */
    int mflags;                 /* 8 */
    int spr_tex;                /* 9 */
    float spr_w, spr_h;         /* 10, 11 */
    unsigned int spr_col;       /* 12 */
    int st_fc, st_a, st_b;      /* 13-15 */
    int spr2;                   /* 16 */
    float motion_sec;                 /* 17: seconds */
    float spd_start, spd_chg_dis_start, spd_chg_dis_finish, spd_chg_rate;   /* 18-21 */
    float curve_rate;                  /* 22: speed factor when no direction is given */
    ShotCurve curve_type, curve_time;         /* 23, 24 */
    float pursuit;                 /* 25: aim lead on the player */
    int use_tail;                  /* 26 */
    int tail_seg, tail_tex;               /* 27, 28 */
    float tail_sec;              /* 29: seconds */
    float tail_roll_spd;        /* 30 */
    int tail_mask;              /* 31 */
    float tail_width;                  /* 32 */
    int tail_skip;                    /* 33 */
    int hp, same_lock;                 /* 34, 35: lock-on */
    int score;                    /* 36 */
    float damage;                  /* 37 */
    int frames;                 /* 38 */
    void (*cb)(struct ShotHit *h); /* 39 */
    int level, spin, flip;      /* 40-42 */
    int face_target;                    /* 43 */
    int clip_ang;                   /* 44 */
    int mask;                   /* 45 */
    int unk_b8; /* 46: copied to EnbNormalWork.unk_1b4 by enb_normal_init; never read */
} ShotDesc;

/*
 * enemy_shatter_create break effect source (84 bytes, cleared first). kind: bits 0-1 enemy type,
 * 0x20000 one piece (idx), 0x40000 posed by motion at time, 0x80000 no transform.
 */
typedef struct SpawnDesc {
    unsigned int kind;
    void *models;               /* 0x04 */
    int type;                   /* 0x08: the model's index in models */
    int idx;                    /* 0x0c */
    int motion;                 /* 0x10 */
    float time;                 /* 0x14 */
    LockObj *lock;                 /* 0x18 */
    Vec3 pos;                   /* 0x1c */
    int ang[3];                 /* 0x28 */
    Vec3 scale;                 /* 0x34 */
    Vec3 v;                     /* 0x40 */
    float inherit;              /* 0x4c */
    int unk_50; /* 0 in the destroyed / post-exec spawn descriptors; never read */
} SpawnDesc;

/* "EnKawasemiFlash" work (two flashing model parts). */
typedef struct EnKawasemiFlashWork {
    unsigned int kind;          /* 0 follows a position, 1..2 fixed (en_kawasemi_flash_scales) */
    Vec3 pos;                   /* 0x04 */
    Vec3 *follow;               /* 0x10 */
    ModelPart part[2];          /* 0x14 */
    unsigned int *motion[2];    /* 0xcc: model entries ([1] is the frame count) */
    float alpha[2];             /* 0xd4 */
} EnKawasemiFlashWork;

/* chain link (152 bytes): BossChain with a larger record, as in b_eden */
typedef struct BossJointLink {
    struct BossJointLink *prev;    /* 0x00 */
    struct BossJointLink *next;    /* 0x04 */
    float stiff;                /* 0x08 */
    float damp;                 /* 0x0c */
    float len;                  /* 0x10 */
    float roll;
    float up_rate;               /* 0x18 */
    Vec3 pos;                   /* 0x1c */
    Vec3 dir;                   /* 0x28: offset from the previous link */
    int prev_dir[3];
    Vec3 vel;                   /* 0x40 */
    int up_dir[3];
    int ang[3];                 /* 0x58 */
    int flags;
    void (*cb)(struct BossJointLink *, struct BossJointLink *); /* 0x68: called with (new state, link) */
    int arg[2];                 /* 0x6c: set with cb */
    Vec3 fwd;                   /* 0x74 */
    Vec3 up;                    /* 0x80 */
    int rel_ang[3];              /* 0x8c */
} BossJointLink;

typedef struct EnLightningRec {
    char *name;
    char *label;                /* 0x04 */
    float len_min;              /* 0x08 */
    float len_max;              /* 0x0c */
    float ang_spd_min;          /* 0x10: degrees per second */
    float ang_spd_max;          /* 0x14 */
    float rto_complement;       /* 0x18 */
    int use_custom_laser;       /* 0x1c */
    int pcol_center;            /* 0x20 */
    int pcol_out1;              /* 0x24 */
    int pcol_out2;              /* 0x28 */
    float r_all;                /* 0x2c */
    float rate_in;              /* 0x30 */
} EnLightningRec;

typedef struct BossMainWork {
    void *file0;                /* 0x00: the model file (model_pnb_load) */
    void *file1;                /* 0x04: the motion file (model_load_timed) */
    int arg0;                   /* 0x08: from the creation argument */
    int level;                   /* 0x0c */
    int state;                  /* 0x10: 0 = loading, 1 = loaded, 2 = running */
    int wait;
    Vec3 pos;                   /* 0x18 */
    Vec3 prev;                  /* 0x24: the position last frame */
    int ang[3];                 /* 0x30 */
    int turn[3];              /* 0x3c */
    int roty;                   /* 0x48 */
    Vec3 ofs;                   /* 0x4c: added to the orbit position */
    Vec3 vel;                   /* 0x58: pos - prev */
    Vec3 vel0;                  /* 0x64: the velocity last frame */
    int orbit_state, orbit_side;
    int kuri_state;                    /* 0x78: kuri_attack_step's state */
    int swings;                 /* 0x7c: swings left (cubes_swing) */
    unsigned int swing_side;        /* 0x80 */
    int wobble_phase;
    float wobble_speed;
    float turn_count;               /* 0x8c: frames (earth_orbit_step) */
    float approach;               /* 0x90: a phase (earth_phase_step) */
    float dist_count;               /* 0x94: frames (earth_orbit_step) */
    float swing_count;               /* 0x98: frames, cubes_swing */
    char pad_9c[16];
    LockObj lock[4];               /* 0xac */
    ModelPart pool[32];            /* 0x1ac */
    ModelPart *parts[26];          /* 0xd2c: the 25 model instances of model_defs, 0-terminated */
    char pad_d94[24];
    BossCubeSet set;             /* 0xdac */
    Vec3 target;                /* 0x1f858: the orbit position */
    Vec3 dir;                   /* 0x1f864 */
    float amp_x, amp_y, amp_z;
    int back_ofs[3];
    int rot[3];                 /* 0x1f888: the orbit angles */
    int ang2[3];                /* 0x1f894 */
    int fin_ang[9];
    float dist;                 /* 0x1f8c4: the orbit distance */
    float dist_back;
    int nloose;
    BossKuriHand hand[2];           /* 0x1f8d0: the second used when kuri_levels[arg1].two */
    char pad_1f930[144];
    int hand_speed_max;
    int kuri_count_vol;
    int kuri_count;
    int phase;                  /* 0x1f9cc */
    int turns, shell_hits, kuri_hits;
    int hp;                     /* 0x1f9dc */
    int unk_1f9e0, kuri_timer; /* unk_1f9e0: 0 in earth_start; never read */
    char pad_1f9e8[4];
    int fin_count;
    int fin_total;
    int arm_cmd;
    int arm_count;
    char pad_1f9fc[4];
    void *arm;                  /* 0x1fa00: the BossArmWork */
    int nerr;
    char msg[64];               /* 0x1fa08 */
    int flags2;                 /* 0x1fa48 */
    unsigned int drawflags;     /* 0x1fa4c: parts drawn by earth_display */
    int animflags;              /* 0x1fa50: parts animated by earth_exec_battle */
    int flags;                  /* 0x1fa54: 1 = model entries registered, 4 = lock-ons registered */
    int bubbles_end, attack_end;
    int thr[4];                 /* 0x1fa60: 3/4, 2/4 and 1/4 of the hit points CMD_CORE_LIFE, then 0 */
    char pad_1fa70[112];
    int stage;                  /* 0x1fae0: the index into thr */
    int shell_timer, kuri_round;
    Vec3 lockdir[4];            /* 0x1faec */
    Vec3 lockpos[4];            /* 0x1fb1c */
    int unk_1fb4c; /* 0 in earth_phase_step; never read */
    float rgba[4];              /* 0x1fb50: draw_set_override_color */
    struct BossCfKeyWork *fade;    /* 0x1fb60: steps rgba (cm_cfchange_create) */
    int save[4];                /* 0x1fb64: material values replaced by earth_materials_save */
    short *mat[4];              /* 0x1fb74 */
    BgmMan *bgm;                /* 0x1fb84 */
    int loops, far;
    int clearing;
} BossMainWork;

/* tiamat_head_funcs object work. */
/* Named by the 'tiamat_head' VarDefs tiamat_head_vardefs. */
typedef struct TiamatHeadParams {
    int hp;
    int hp_short;               /* 0x04 */
    int same_lock;              /* 0x08 */
    int score;                  /* 0x0c */
    int score_short;            /* 0x10 */
    float hp_reduce;            /* 0x14 */
    float crash_r;              /* 0x18 */
    float crash_damage;         /* 0x1c */
    float bullet_spd;           /* 0x20 */
    float appear_sec;           /* 0x24 */
    float appear_dis;           /* 0x28 */
    float break_sec;            /* 0x2c */
    float pose_sec;             /* 0x30 */
    float stop_sec;             /* 0x34 */
    float accel_sec;            /* 0x38 */
    float disappear_sec;        /* 0x3c */
    float disappear_dis;        /* 0x40 */
    float spike_rate;           /* 0x44 */
    float damage_swing_rate;    /* 0x48 */
    int damage_swing_size;      /* 0x4c */
    int damage_swing_ang;       /* 0x50 */
    float damage_swing_sec;     /* 0x54 */
    int bonus_score;            /* 0x58 */
    float attack_ray_rate;      /* 0x5c */
} TiamatHeadParams;

typedef struct TiamatHeadVars {
    char *name;
    char *label;                /* 0x04 */
    TiamatHeadParams p;         /* 0x08 */
} TiamatHeadVars;

typedef struct TiamatHeadWork {
    struct TiamatHeadVars *type;
    int frames;                 /* 0x04: frames in the state */
    Vec3 hitv;                  /* 0x08: the lock-on record's vector when hit */
    int state;                  /* 0x14 */
    int life;                   /* 0x18: frames left; 0 = no limit */
    int path;                   /* 0x1c */
    float prog;                 /* 0x20: 0 to 1 along the path */
    float prog_from;            /* 0x24 */
    Vec3 base;                  /* 0x28: start position, player relative */
    int base_ang[3];            /* 0x34 */
    Vec3 cur;                   /* 0x40: position on the path */
    Vec3 prev;                  /* 0x4c */
    Vec3 from;                  /* 0x58: position at the state's start */
    Vec3 dir;                   /* 0x64 */
    int look_from[3];           /* 0x70: rotation when it turned to the player */
    float speed;                /* 0x7c */
    ModelPart body;             /* 0x80 */
    ModelPart ring;             /* 0xdc */
    Vec3 neck;                  /* 0x138: where the chain attaches */
    void *chain;                /* 0x144: the first body-and-tail segment */
    int chain_wait;             /* 0x148: frames before the chain is created */
    Vec3 seg[14];               /* 0x14c: the chain's joint offsets */
    float look;                 /* 0x1f4: blend towards facing the player */
    int segments;               /* 0x1f8 */
    Vec3 mouth;                 /* 0x1fc */
    int fired;                  /* 0x208 */
    TiamatHeadArg arg;          /* 0x20c */
    int swing_phase;            /* 0x24c */
    int swing_count;            /* 0x250: frames of damage swing left */
    int swing_ang[3];           /* 0x254 */
    int swing_base[3];          /* 0x260 */
    char pad_26c[12];
    int hits;                   /* 0x278: segments shot, as the chain counts them */
} TiamatHeadWork;

/* Named by the 'tiamat_body_and_tail' VarDefs tiamat_body_and_tail_vardefs. */
typedef struct TiamatBodyAndTailParams {
    int hp;
    int hp_short;               /* 0x04 */
    int same_lock;              /* 0x08 */
    int score;                  /* 0x0c */
    int score_short;            /* 0x10 */
    float hp_reduce;            /* 0x14 */
    float crash_r_body;         /* 0x18 */
    float crash_r_tail;         /* 0x1c */
    float crash_damage;         /* 0x20 */
    int wire_col;               /* 0x24 */
    float rate;                 /* 0x28 */
    float ang_rate;             /* 0x2c */
    float pose_sec;             /* 0x30 */
    float die_vec;              /* 0x34 */
    float damage_swing_rate;    /* 0x38 */
    float damage_swing_rate2;   /* 0x3c */
    int damage_swing_ang;       /* 0x40 */
    float damage_swing_sec;     /* 0x44 */
    int damage_swing_add;       /* 0x48 */
} TiamatBodyAndTailParams;

typedef struct TiamatBodyAndTailVars {
    char *name;
    char *label;                /* 0x04 */
    TiamatBodyAndTailParams p;  /* 0x08 */
} TiamatBodyAndTailVars;

/* What a Tiamat chain segment follows (tiamat_body_and_tail_create's lnk). */
typedef struct TiamatChainLink {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    int *hits;                  /* 0x08: the head's count of segments shot */
    int big;                    /* 0x0c */
} TiamatChainLink;

/* tiamat_body_and_tail_funcs object work. */
typedef struct TiamatBodyAndTailWork {
    TiamatBodyAndTailVars *type;
    Vec3 hitv;                  /* 0x04: the lock-on record's vector when hit */
    TiamatChainLink link;       /* 0x10 */
    LockObj lock;               /* 0x20 */
    void *head;                 /* 0x60: the segment or head in front */
    void *last;                 /* 0x64: the segment behind */
    ModelPart body;             /* 0x68 */
    ModelPart ring;             /* 0xc4 */
    Vec3 tip;                   /* 0x120: where the next segment follows */
    Vec3 anchor;                /* 0x12c: the link target, player relative */
    Vec3 steer_to;              /* 0x138: the head's pose point for this joint */
    int steering;               /* 0x144 */
    float steer_rate;           /* 0x148 */
    int fall;                   /* 0x14c: frames of the destroyed fall */
    int life;                   /* 0x150: frames left; 0 = no limit */
    float phase;                /* 0x154: beat offset of the ring flash */
    char pad_158[4];
    int swing_phase;            /* 0x15c */
    int swing_count;            /* 0x160: frames of damage swing left */
    Vec3 swing_dir;             /* 0x164 */
    Vec3 swing;                 /* 0x170 */
    Vec3 swing_base;            /* 0x17c */
    float pull;                 /* 0x188 */
    float swing_rate;           /* 0x18c */
    int kind;                   /* 0x190: segments left behind; 2 or more is body */
    int big;                    /* 0x194 */
} TiamatBodyAndTailWork;

/* TiamatLaserAttackRay work (tiamat_laser_attack_ray_desc, 244 bytes) */
typedef struct TiamatLaserAttackRayWork {
    void *models;
    void *textures;             /* 0x04 */
    TiamatLaserAttackRayArg arg; /* 0x08: the creation argument */
    int t;                      /* 0x18: frames */
    ModelPart body;             /* 0x1c */
    ModelPart ring;             /* 0x78 */
    int laser_state;            /* 0xd4: 1 = create the laser */
    Vec3 pos;                   /* 0xd8: kept once the owner is gone */
    int ang[3];                 /* 0xe4 */
    int gone_frm;               /* 0xf0: frames since the owner was gone */
} TiamatLaserAttackRayWork;

/* TiamatLaserTurnEff work (tiamat_laser_turn_eff_desc, 108 bytes) */
typedef struct TiamatLaserTurnEffWork {
    void *models;
    void *tex;                  /* 0x04 */
    int t;                      /* 0x08: frames */
    Vec3 *pos;                  /* 0x0c */
    ModelPart body;             /* 0x10 */
} TiamatLaserTurnEffWork;

/* Plane-cut and wire drawer vertex (64 bytes): position, screen coords, colour, 1/depth. */
typedef struct SVtx {
    float x, y, z;
    float scr[4];               /* 0x0c */
    float q;                    /* 0x1c */
    float col[4];               /* 0x20 */
    char pad_30[8];
    float rq;                   /* 0x38 */
    char pad_3c[4];
} SVtx;

/* thrones_child_funcs object part (76 bytes). */
typedef struct ThronesChildPart {
    Vec3 pos;
    LockObj lock;                  /* 0x0c */
} ThronesChildPart;

/* Thrones armor plate (208 bytes): model, node pose, wire model. */
typedef struct ThronesArm {
    ModelPart m;
    Vec3 pos;                   /* 0x5c */
    int ang[3];                 /* 0x68 */
    ModelPart wire;                /* 0x74 */
} ThronesArm;

/* Thrones ring (136 bytes). */
typedef struct ThronesRing {
    int state;                     /* 0 shown, 1 with halo, 2 gone */
    ModelPart m;                   /* 0x04 */
    Vec3 pos;                   /* 0x60 */
    int ang[3];                 /* 0x6c */
    int cur[3];                 /* 0x78 */
    float scale;                /* 0x84: halo scale */
} ThronesRing;

/* "Thrones" work (2184 bytes, thrones_types records). */
typedef struct ThronesWork {
    struct ThronesRec *type;
    int state;                  /* 0x04: indexes thrones_exec_table */
    int count;                  /* 0x08: frames */
    Vec3 hitv;                   /* 0x0c */
    int life;                   /* 0x18: frames */
    Vec3 pos;                   /* 0x1c: on the path */
    int ang[3];                 /* 0x28 */
    float blend;                /* 0x34 */
    ModelPart body;                /* 0x38 */
    ThronesArm arm[4];               /* 0x94 */
    ThronesSlot x[4];                /* 0x3d4 */
    ThronesRing y[6];                /* 0x464 */
    ModelPart core;                /* 0x794 */
    char pad_7f0[24];
    ThronesSwing swing;                /* 0x808 */
    int level;                  /* 0x824 */
    ModelPart halo;                /* 0x828 */
    void *task;                 /* 0x884: the enArmorDisp task (en_armor_disp_init) */
} ThronesWork;

/* ThronesChild work (552 bytes; thrones_child_funcs, thrones_child_types). */
typedef struct ThronesChildWork {
    struct ThronesChildRec *type;
    int state;                  /* 0x04: indexes thrones_child_exec_table */
    int count;                  /* 0x08: frames */
    Vec3 hitv;                   /* 0x0c: the velocity given to its debris */
    int life;                   /* 0x18: frames */
    Vec3 pos;                   /* 0x1c: on the path */
    int ang[3];                 /* 0x28 */
    float blend;                /* 0x34: weight of the path position against the object position */
    ModelPart body;                /* 0x38 */
    ThronesChildPart part[4];           /* 0x94: four lock-on parts */
    ThronesChildNode nodes[4];       /* 0x1c4 */
    int lost;                   /* 0x224: parts destroyed */
} ThronesChildWork;

/* 'Khafra' parameters (288 bytes), named by khafra_vardefs. */
typedef struct KhafraParams {
    int hp;                         /* 0x08 */
    int same_lock;                  /* 0x0c */
    int score;                      /* 0x10 */
    float hp_reduce;                /* 0x14 */
    float crash_r;                  /* 0x18 */
    float crash_damage;             /* 0x1c */
    float backfire_scl;             /* 0x20 */
    float backfire2_scl;            /* 0x24 */
    float backfire_len;             /* 0x28 */
    float backfire_width;           /* 0x2c */
    float break_spd;                /* 0x30 */
    float break_pow;                /* 0x34 */
    float break_r_over_rate;        /* 0x38 */
    float break_r_pow;              /* 0x3c */
    float break_r_pow2;             /* 0x40 */
    float break_ang_over_rate;      /* 0x44 */
    float break_ang_pow;            /* 0x48 */
    float break_ang_pow2;           /* 0x4c */
    float break_backfire_pow;       /* 0x50 */
    float break_backfire_pow2;      /* 0x54 */
    float break_backfire_scl;       /* 0x58 */
    float break_backfire2_pow;      /* 0x5c */
    float break_backfire2_pow2;     /* 0x60 */
    float break_backfire2_scl;      /* 0x64 */
    float break_dis;                /* 0x68 */
    float break_dis_pow;            /* 0x6c */
    float break_dis_pow2;           /* 0x70 */
    float accel_sec;                /* 0x74 */
    float accel_pow;                /* 0x78 */
    float accel_spd_over_rate;      /* 0x7c */
    float accel_r_pow;              /* 0x80 */
    float accel_r_pow2;             /* 0x84 */
    float accel_ang_pow;            /* 0x88 */
    float accel_ang_pow2;           /* 0x8c */
    float accel_ang_rate;           /* 0x90 */
    float accel_ang_pow3;           /* 0x94 */
    float accel_ang_pow4;           /* 0x98 */
    float accel_backfire_pow;       /* 0x9c */
    float accel_backfire2_pow;      /* 0xa0 */
    float accel_backfire_pow2;      /* 0xa4 */
    float accel_backfire2_pow2;     /* 0xa8 */
    float finish_sec;               /* 0xac */
    float shot_motion_sec;          /* 0xb0 */
    float shot_motion_sec_dif;      /* 0xb4 */
    float break2_sec;               /* 0xb8 */
    float break2_pow;               /* 0xbc */
    float break2_backfire_pow;      /* 0xc0 */
    float break2_backfire2_pow;     /* 0xc4 */
    float shot2_sec;                /* 0xc8 */
    float accel2_sec;               /* 0xcc */
    float accel2_pow;               /* 0xd0 */
    float accel2_spd_over_rate;     /* 0xd4 */
    float accel2_backfire_scl;      /* 0xd8 */
    float accel2_backfire_pow;      /* 0xdc */
    float accel2_backfire2_scl;     /* 0xe0 */
    float accel2_backfire2_pow;     /* 0xe4 */
    float accel2_roll_spd_rate;     /* 0xe8 */
    float accel2_roll_pow;          /* 0xec */
    float finish2_sec;              /* 0xf0 */
    int wire;                       /* 0xf4 */
    int wire_linear;                /* 0xf8 */
    int wire_col;                   /* 0xfc */
    int wire_col2;                  /* 0x100 */
    float wire_near;                /* 0x104 */
    float wire_far;                 /* 0x108 */
    float wire_near_rate;           /* 0x10c */
    float wire_far_rate;            /* 0x110 */
    int wire_beat;                  /* 0x114 */
    int parts_beat;                 /* 0x118 */
    float attack_ray_scl;           /* 0x11c */
} KhafraParams;

typedef struct KhafraVars {
    char *name;
    char *label;                /* 0x04 */
    KhafraParams p;            /* 0x08 */
} KhafraVars;

/* Type table enemy work. */
typedef struct KhafraWork {
    KhafraVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    Vec3 up;                    /* 0x18 */
    float backfire_scl;         /* 0x24: linked as the 'enBackFire' sprites' size */
    float backfire2_scl;        /* 0x28: linked as its beams' scale */
    int roll;                   /* 0x2c */
    int ang[3];                 /* 0x30 */
    float speed;                /* 0x3c */
    Vec3 sway;                  /* 0x40: the offset added last frame */
    int shot;                   /* 0x4c: the shot's progress (placement 'Shot' > 0) */
    int start;                  /* 0x50: frames */
    int end;                    /* 0x54 */
    Vec3 pos;                   /* 0x58 */
    LockObj lock;                  /* 0x64 */
    Vec3 tip;                   /* 0xa4: a mount point of the model */
    ModelPart body;                /* 0xb0 */
    ModelPart ring;                /* 0x10c */
    float alpha;                /* 0x168 */
    Vec3 scale;                 /* 0x16c */
    int fire;                   /* 0x178: fires an 'E_Shot' */
} KhafraWork;

/* 'Anu' parameters (69 words), named by anu_vardefs. */
typedef struct AnuParams {
    int hp;                         /* 0x08 */
    int same_lock;                  /* 0x0c */
    int score;                      /* 0x10 */
    float hp_reduce;                /* 0x14 */
    float crash_r;                  /* 0x18 */
    float crash_damage;             /* 0x1c */
    float backfire_scl;         /* 0x20 */
    float backfire2_scl;        /* 0x24 */
    float backfire_len;         /* 0x28 */
    float backfire_width;       /* 0x2c */
    float break_spd;            /* 0x30 */
    float break_pow;            /* 0x34 */
    float break_r_over_rate;    /* 0x38 */
    float break_r_pow;          /* 0x3c */
    float break_r_pow2;         /* 0x40 */
    float break_ang_over_rate;  /* 0x44 */
    float break_ang_pow;        /* 0x48 */
    float break_ang_pow2;       /* 0x4c */
    float break_backfire_pow;   /* 0x50 */
    float break_backfire_pow2;  /* 0x54 */
    float break_backfire_scl;   /* 0x58 */
    float break_backfire2_pow;  /* 0x5c */
    float break_backfire2_pow2; /* 0x60 */
    float break_backfire2_scl;  /* 0x64 */
    float break_dis;            /* 0x68 */
    float break_dis_pow;        /* 0x6c */
    float break_dis_pow2;       /* 0x70 */
    float accel_sec;            /* 0x74 */
    float accel_pow;            /* 0x78 */
    float accel_spd_over_rate;  /* 0x7c */
    float accel_r_pow;          /* 0x80 */
    float accel_r_pow2;         /* 0x84 */
    float accel_ang_pow;        /* 0x88 */
    float accel_ang_pow2;       /* 0x8c */
    float accel_ang_rate;       /* 0x90 */
    float accel_ang_pow3;       /* 0x94 */
    float accel_ang_pow4;       /* 0x98 */
    float accel_backfire_pow;   /* 0x9c */
    float accel_backfire2_pow;  /* 0xa0 */
    float accel_backfire_pow2;  /* 0xa4 */
    float accel_backfire2_pow2; /* 0xa8 */
    float finish_sec;           /* 0xac */
    float shot_motion_sec;      /* 0xb0 */
    float shot_motion_sec_dif;  /* 0xb4 */
    float break2_sec;           /* 0xb8 */
    float break2_pow;           /* 0xbc */
    float break2_backfire_pow;  /* 0xc0 */
    float break2_backfire2_pow; /* 0xc4 */
    float shot2_sec;            /* 0xc8 */
    float accel2_sec;           /* 0xcc */
    float accel2_pow;           /* 0xd0 */
    float accel2_spd_over_rate; /* 0xd4 */
    float accel2_backfire_scl;  /* 0xd8 */
    float accel2_backfire_pow;  /* 0xdc */
    float accel2_backfire2_scl; /* 0xe0 */
    float accel2_backfire2_pow; /* 0xe4 */
    float accel2_roll_spd_rate; /* 0xe8 */
    float accel2_roll_pow;      /* 0xec */
    float finish2_sec;          /* 0xf0 */
    int wire;                   /* 0xf4 */
    int wire_linear;            /* 0xf8 */
    int wire_col;               /* 0xfc */
    int wire_col2;              /* 0x100 */
    float wire_near;            /* 0x104 */
    float wire_far;             /* 0x108 */
    float wire_near_rate;       /* 0x10c */
    float wire_far_rate;        /* 0x110 */
    int wire_beat;              /* 0x114 */
    float attack_ray_scl;       /* 0x118 */
} AnuParams;

typedef struct AnuVars {
    char *name;
    char *label;                /* 0x04 */
    AnuParams p;                /* 0x08 */
} AnuVars;

/* Type table enemy over the Anu record layout (stg02 Anu, stg03 type enemies). */
typedef struct AnuTypeEnemyWork {
    AnuVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    Vec3 up;                    /* 0x18 */
    float backfire_scl;         /* 0x24: linked as the 'enBackFire' sprites' size */
    float backfire2_scl;        /* 0x28: linked as its beams' scale */
    int roll;                   /* 0x2c */
    int ang[3];                 /* 0x30 */
    float speed;                /* 0x3c */
    Vec3 sway;                  /* 0x40: the offset added last frame */
    int shot;                   /* 0x4c: the shot's progress */
    int start;                  /* 0x50: frames */
    int end;                    /* 0x54 */
    Vec3 pos;                   /* 0x58 */
    LockObj lock;                  /* 0x64 */
    Vec3 tip;                   /* 0xa4: a mount point of the model */
    ModelPart body;                /* 0xb0 */
    ModelPart ring;                /* 0x10c */
    float alpha;                /* 0x168 */
    Vec3 scale;                 /* 0x16c */
    int fire;                   /* 0x178: fires a shot */
} AnuTypeEnemyWork;

/* Type table enemy without the ring model. */
typedef struct AnuWork {
    AnuVars *type;
    int state;                  /* 0x04 */
    int frames;                 /* 0x08: frames in the state */
    Vec3 hitv;                  /* 0x0c */
    Vec3 up;                    /* 0x18 */
    float backfire_scl;         /* 0x24: linked as the 'enBackFire' sprites' size */
    float backfire2_scl;        /* 0x28: linked as its beams' scale */
    int roll;                   /* 0x2c */
    int ang[3];                 /* 0x30 */
    float speed;                /* 0x3c */
    Vec3 sway;                  /* 0x40: the offset added last frame */
    int shot;                   /* 0x4c: the shot's progress */
    int start;                  /* 0x50: frames */
    int end;                    /* 0x54 */
    Vec3 pos;                   /* 0x58 */
    LockObj lock;                  /* 0x64 */
    Vec3 tip;                   /* 0xa4: a mount point of the model */
    ModelPart body;                /* 0xb0 */
    float alpha;                /* 0x10c */
    Vec3 scale;                 /* 0x110 */
    int fire;                   /* 0x11c: fires a shot (e_shot_fire, by anu_update) */
} AnuWork;

/* Variable editor record: group name, label, 24 values. */
typedef struct EShotSet {
    char *name;
    char *label;                /* 0x04 */
    int v[24];                  /* 0x08 */
} EShotSet;

/* An enemy shot kind, named by the 'E_Shot' VarDefs e_shot_vardefs. */
typedef struct EShotParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float damage;               /* 0x0c */
    float collision_r;          /* 0x10 */
    int disp_type;              /* 0x14: model or sprite */
    float sprite_r;             /* 0x18 */
    float sprite_r_limit;       /* 0x1c */
    float motion_sec;           /* 0x20 */
    float spd_start;            /* 0x24 */
    float spd_chg_dis_start;    /* 0x28 */
    float spd_chg_dis_finish;   /* 0x2c */
    float spd_chg_rate;         /* 0x30 */
    int use_tail;               /* 0x34 */
    float tail_sec;             /* 0x38 */
    int tail_roll_spd;          /* 0x3c */
    int tail_skip;              /* 0x40 */
    float curve_rate;           /* 0x44 */
    int curve_type;             /* 0x48 */
    int curve_time;             /* 0x4c */
    float pursuit;              /* 0x50 */
    float life_time;            /* 0x54 */
    int test;                   /* 0x58 */
    int clip_ang;               /* 0x5c */
} EShotParams;

typedef struct EShotParamSet {
    char *name;
    char *label;                /* 0x04 */
    EShotParams p;              /* 0x08 */
} EShotParamSet;

/* A needle shot kind, named by the 'CR_Needle' VarDefs cr_needle_vardefs. */
typedef struct CrNeedleParams {
    int hp;
    int same_lock;              /* 0x04 */
    int score;                  /* 0x08 */
    float damage;               /* 0x0c */
    float collision_r;          /* 0x10 */
    float sprite_r;             /* 0x14 */
    float sprite_r_limit;       /* 0x18 */
    float motion_sec;           /* 0x1c */
    float spd_start;            /* 0x20 */
    float spd_chg_dis_start;    /* 0x24 */
    float spd_chg_dis_finish;   /* 0x28 */
    float spd_chg_rate;         /* 0x2c */
    int use_tail;               /* 0x30 */
    float tail_sec;             /* 0x34 */
    int tail_roll_spd;          /* 0x38 */
    int tail_skip;              /* 0x3c */
    float curve_rate;           /* 0x40 */
    int curve_type;             /* 0x44 */
    int curve_time;             /* 0x48 */
    float pursuit;              /* 0x4c */
    float life_time;            /* 0x50 */
    int test;                   /* 0x54 */
    int clip_ang;               /* 0x58 */
    float shot_spd;             /* 0x5c */
} CrNeedleParams;

typedef struct CrNeedleSet {
    char *name;
    char *label;                /* 0x04 */
    CrNeedleParams p;           /* 0x08 */
} CrNeedleSet;

/* mesh packet vertex (28 bytes) */
typedef struct MVtx {
    float x, y, z;
    char pad_0c[12];
    unsigned int col;           /* 0x18 */
} MVtx;

/* "Earth_1stDemo" (104 bytes) */
typedef struct Earth1stDemoWork {
    int state;                  /* 0x00: -2 loading .. 7 */
    int timer, timer0;          /* 0x04 */
    int frames;                 /* 0x0c: counts from state 4 on */
    int fog_n, fog_frames;      /* 0x10: fog fade, frames left and total */
    int wave_n, wave_frames;    /* 0x18: screen wave, frames left and total */
    int started;                /* 0x20: the camera motion was started on a beat */
    void *sub;                  /* 0x24: the wipe */
    int (*cb)();                /* 0x28: (arg) for Earth, (arg, state) for the other bosses */
    void *arg;                  /* 0x2c: the boss main task */
    BossCameraWork *cam;        /* 0x30 */
    BossMainWork *parent;       /* 0x34 */
    void *mpb[2];               /* 0x38: the two sequence loads */
    float speed;                /* 0x40: the map level's scale, the pass speed */
    char pad_44[4];
    BossFog fog_to;             /* 0x48 */
    BossFog fog_from;           /* 0x58 */
} Earth1stDemoWork;

/* Polyline style (32 bytes): centre and two outer colours, width, fade rate. */
typedef struct LineStyle {
    int col[3];                 /* 0x00: centre, outer 1, outer 2 */
    int unk_0c; /* 0 in the custom line styles; never read */
    float width;                /* 0x10 */
    float rate_in;              /* 0x14 */
    float unk_18[2]; /* 0x18: 0 in the custom line styles; never read */
} LineStyle;

/* boss_wire.c: wire model task work (128 bytes) */
typedef struct WireEditWork {
    char pad_00[8];
    void *model;                /* 0x08 */
    int c;                      /* 0x0c: passed to the edge builder (wire_list) */
    unsigned int *motion;       /* 0x10: [1] is the frame count */
    char *flags;                /* 0x14: one byte per edge, 1 = drawn */
    int nflags;                 /* 0x18 */
    char pad_1c[16];
    int buf;                    /* 0x2c: heap block freed by wire_edit_release */
    char pad_30[4];
    float frame;                /* 0x34: the motion frame shown */
    float frames;               /* 0x38 */
    char *src;                  /* 0x3c: the name given */
    char name[64];              /* 0x40: the .njw file name */
} WireEditWork;

/* Map part as stage draw callbacks see it (head of MapPart). */
typedef struct MapDrawPart {
    char pad_00[4];
    MapLink *link;              /* 0x04 */
    char pad_08[52];
    float time;                 /* 0x3c: the animation time */
} MapDrawPart;

/* Map object as stage draw callbacks see it (MapObj). */
typedef struct MapDrawObj {
    char pad_00[4];
    MapDrawPart *part;          /* 0x04 */
    char pad_08[56];
    int busy;                   /* 0x40 */
    int idx;                    /* 0x44 */
} MapDrawObj;


/* Overlay types shared by several stage and front-end units. */
typedef void (*ObjStateFn)(void *o, void *w);

/* draw_polyline_wide_capped line style */
typedef struct WideLineStyle {
    int col[3];
    char pad_0c[4];
    float width;                /* 0x10 */
    float k;                    /* 0x14 */
} WideLineStyle;

/* Player camera data, 64 bytes (player_camera_get_preset, player_camera_set_data). */
typedef struct PlayerCameraPreset {
    float cam_under_height;
    float cam_top_height;       /* 0x04 */
    float cam_under_dist;       /* 0x08 */
    float cam_top_dist;         /* 0x0c */
    int p_ang[3];               /* 0x10 */
    float cam_ang_rate_x;       /* 0x1c */
    float cam_ang_rate_y;       /* 0x20 */
    float cam_rate_x;           /* 0x24 */
    float cam_rate_y;           /* 0x28 */
    int cam_margin_dx;          /* 0x2c */
    int cam_margin_dy;          /* 0x30 */
    int eye_limit_y;            /* 0x34 */
    int eye_limit_x_top;        /* 0x38 */
    int eye_limit_x_und;        /* 0x3c */
} PlayerCameraPreset;

typedef struct DemoCamera {
    void *models;
    char pad_04[4];
    Camera cam;               /* 0x08 */
    void *node;                 /* 0x30 */
    void *motion;               /* 0x34 */
    void *target;               /* 0x38 */
    float t;                    /* 0x3c */
    float dt;                   /* 0x40 */
    float end;                  /* 0x44 */
    int done;                   /* 0x48 */
} DemoCamera;

typedef struct DominionsBodyAndTailSet {
    char *name;
    char *label;                /* 0x04 */
    int v[11];                  /* 0x08 */
} DominionsBodyAndTailSet;

typedef struct EnPerfectWallPlace {
    char pad_00[4];
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    char pad_1c[12];
    float param0;                  /* 0x28: per wall: 0 or 1 */
    float param1, param2, param3, param4;   /* 0x2c */
    char pad_3c[4];
} EnPerfectWallPlace;

typedef struct TypeVars {
    char *name;
    char *label;                /* 0x04 */
    union {
        int v[70];              /* 0x08 */
    } u;
} TypeVars;

typedef struct TypeEnemyWork {
    TypeVars *type;
    int state;                  /* 0x04 */
    int frames;
    Vec3 hitv;                  /* 0x0c: the lock-on record's vector when hit */
    Vec3 up;                    /* 0x18 */
    float unk_24;                  /* 0x24 */
    float unk_28;                  /* 0x28 */
    int unk_2c;
    int ang[3];                 /* 0x30 */
    float speed;                /* 0x3c */
    Vec3 v40;                   /* 0x40 */
    int flag;                   /* 0x4c */
    int start;                  /* 0x50: frames */
    int end;                    /* 0x54 */
    Vec3 pos;                   /* 0x58 */
    LockObj lock;                  /* 0x64 */
    Vec3 tip;                   /* 0xa4: a mount point of the model */
    ModelPart body;                /* 0xb0 */
    ModelPart ring;                /* 0x10c */
    float alpha;                /* 0x168 */
    Vec3 scale;                 /* 0x16c */
    int unk_178;
} TypeEnemyWork;

#endif /* REZ_TYPES_OVERLAYS_H */
