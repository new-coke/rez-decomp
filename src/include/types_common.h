/* Types shared by the executable and the overlays. */

#ifndef REZ_TYPES_COMMON_H
#define REZ_TYPES_COMMON_H

typedef struct IVec3 {
    int x;
    int y;
    int z;
} IVec3;

typedef struct Vec3 {
    float x, y, z;
} Vec3;

/* FileReq.state: 0 idle, 1 running, then the task's end code (2..7). */
typedef enum ReqState { REQ_IDLE, REQ_BUSY } ReqState;

/* Memory-card/file request (filereq_work): set by filereq_set*, polled by the game loop. */
typedef struct FileReq {
    ReqState state; /* 0x00 */
    int result; /* 0x04 */
    int cmd; /* 0x08 */
    int drive; /* 0x0c */
    const char *name; /* 0x10 */
    void *buf; /* 0x14 */
    int size; /* 0x18 */
    int flag; /* 0x1c */
} FileReq;

typedef enum McJob { MC_JOB_NONE, MC_JOB_SAVE, MC_JOB_LOAD, MC_JOB_FORMAT } McJob;

/* Memory-card slot, 0x2c bytes, in mc_slots; mc_drive_slots maps port -> slot. */
typedef struct McSlot {
    int port; /* 0x00: from mcslot_init_ports */
    int index; /* 0x04 */
    int id; /* 0x08: pad port the slot is assigned to, -1 none */
    unsigned int flags; /* 0x0c: bit 0 = memory attached, bit 5 = mounted */
    int state; /* 0x10: 3 = card present/ready */
    McJob job; /* 0x14: job running on the slot */
    int card_size; /* 0x18 */
    void *buf; /* 0x1c: memory block */
    int check; /* 0x20 */
    int mode; /* 0x24: memory-check mode (mcslot_update; "slot->mode_memory") */
    int access; /* 0x28 */
} McSlot;

/* cam_far, right after screen_cfg; written by njClipZ. */
typedef struct NjClip {
    float zfar; /* abs */
    float near;
} NjClip;

/* Lock-on target (64 bytes); owner = owning task's work area. */
/* LockObj.flags */
#define LOCK_FACING         0x7         /* index into lock_obj_facing_limits */
#define LOCK_STATE          0x38        /* index into lock_obj_state_masks, << 3 */
#define LOCK_CAM_RELATIVE   0x40        /* pos is relative to the camera */
#define LOCK_SILENT         0x80        /* no hit or destroy sound */
#define LOCK_LOCKABLE       0x100
#define LOCK_ACTIVE         0x200       /* in the active lock list */
#define LOCK_LOCKED         0x400
#define LOCK_SHOT           0x1000      /* lasers on their way */
#define LOCK_HIT            0x2000      /* hit by a laser this frame */
#define LOCK_CAPTURING      0x4000      /* capture beam pulling it in */
#define LOCK_HELD           0x8000      /* beam fired, draining life */
#define LOCK_CAPTURED       0x10000     /* capture done; cleared each frame */
#define LOCK_CAPTURE        0x1c000     /* any capture phase */
#define LOCK_REGISTERED     0x20000
#define LOCK_OK_MARK        0x40000

typedef struct LockObj {
    signed char shots;      /* 0x00: lasers in flight towards the object (lockon_obj_add_shot / lockon_obj_hit) */
    signed char lock_max;   /* 0x01 */
    signed char locks;      /* 0x02: locks placed on the object (lockon_confirm_step) */
    signed char lock_anim;  /* 0x03 */
    short life_max;         /* 0x04 */
    short life;             /* 0x06: reduced by lockon_obj_hit */
    short lockable;         /* 0x08: life - locks * power - shot_dmg (lockon_obj_calc_remaining) */
    short shot_dmg;         /* 0x0a: damage of the lasers in flight */
    short lock_time;        /* 0x0c */
    short fx_slot;          /* 0x0e: fx slot id (enemy_status_ent_get_id; negative = none, lockon_event_sound) */
    int flags;              /* 0x10: LOCK_* */
    void *owner;            /* 0x14: the owning task's work area */
    char *name;             /* 0x18: shown in the debug lists */
    Vec3 *pos;              /* 0x1c: the position (lockon_obj_get_pos; flag 0x40: camera relative) */
    float *dir;             /* 0x20: facing, for the range test (lockon_obj_in_range) */
    void *hit_list;         /* 0x24: the shots that hit the object, linked at +0x18 (lockon_obj_hit) */
    float scr_x, scr_y;     /* 0x28: projected screen position (lockon_project_objects) */
    float key;              /* 0x30: sort key: the projected depth */
    union {
        Vec3 v;             /* 0x34: impulse vector added by lockon_obj_hit */
        int words[3];           /* cleared one int at a time (lockon_clear_all) */
    } imp;
} LockObj;

typedef struct NjVec3 {
    float x, y, z;
} NjVec3;

/* Chunk model (nj_cnk_model_dump): vertex list, polygon list, bounding sphere. */
typedef struct NjCnkModel {
    int *vlist;             /* 0x00 */
    short *plist;           /* 0x04: chunk polygon list (nj_cnk_plist_dump) */
    NjVec3 center;          /* 0x08: "CENTER = (...)" */
    float r;                /* 0x14: "RADIUS = (...)" */
} NjCnkModel;

typedef struct WireObj {
    int flags;
    NjCnkModel *model; /* 0x04 */
    float pos[3]; /* 0x08 */
    int ang[3]; /* 0x14 */
    float scl[3]; /* 0x20 */
    struct WireObj *child; /* 0x2c */
    struct WireObj *sibling; /* 0x30 */
    float qa; /* 0x34 */
} WireObj;

/* Last fog request fog_req (mode, colour, near, far). */
typedef struct FogReq {
    int col;
    int color; /* 0x04 */
    float near; /* 0x08 */
    float far; /* 0x0c */
} FogReq;

/* Per-node mesh data copy, mirrors the YNode tree (yuki_save_create, 16 bytes). */
typedef struct YSave {
    int *data; /* copy of the mesh packets */
    int count; /* 0x04: its length in words */
    struct YSave *child; /* 0x08 */
    struct YSave *next; /* 0x0c */
} YSave;

/* Camera (40 bytes; camera_cur = current): eye, look-at, up, field of view. */
typedef struct Camera {
    Vec3 eye;
    Vec3 center;                /* 0x0c */
    Vec3 up;                    /* 0x18 */
    int fov;                    /* 0x24 */
} Camera;

/*
 * Model instance (92 bytes): the "DcObject" of dc_object_init and head of a MapPart (node tree,
 * animation clip and time, collision records).
 */
typedef struct ModelPart {
    int flags;                  /* 0x00 */
    int root;                   /* 0x04: node tree (a DcNode *) */
    int clip;                   /* 0x08: animation source (a DcClip *) */
    int clip_b, clip_c;         /* 0x0c */
    int wire_list;
    int flags2;                 /* 0x18: bit 0 = motion finished */
    int cb;                     /* 0x1c: dc_object_set_callback */
    int cb_arg;                 /* 0x20 */
    int wire0, wire1;           /* 0x24 */
    int comp0, comp1;           /* 0x2c */
    char pad_34[8];
    float frame;                /* 0x3c: the motion's current frame */
    float frames;               /* 0x40: its length */
    float speed;                /* 0x44: frames per tick */
    char pad_48[8];
    float radius;               /* 0x50 */
    int coll;                   /* 0x54: collision records */
    short ncoll;                /* 0x58 */
    short coll_lock;
} ModelPart;

/* item_create argument (32 bytes). */
typedef struct Spawn {
    int kind;
    float x, y, z;              /* 0x04 */
    int vel_x, vel_y, vel_z;    /* 0x10 */
    float scale;                /* 0x1c */
} Spawn;

/* Model pack (PNB) block entry (16 bytes; pnb_get_entry). */
typedef struct PnbEntry {
    void *src;                    /* 0x00: the entry's data */
    void *buf;                  /* 0x04: a block loaded for it (model_free_block) */
    char pad_08[8];
} PnbEntry;

/* enemy_task_task_create argument (80 bytes). */
typedef struct ObjDesc {
    char *name;
    struct ObjFuncs *funcs;     /* 0x04 */
    void *arg;                  /* 0x08 */
    int *kind;                  /* 0x0c */
    void *models;               /* 0x10 */
    void *tex;                  /* 0x14 */
    int flags;
    int dc_flags, model_no;
    int model_no2;
    int hp;                     /* 0x28 */
    float same_lock;            /* 0x2c */
    int score;                  /* 0x30 */
    float hp_reduce;            /* 0x34 */
    float crash_r;              /* 0x38 */
    float crash_damage;         /* 0x3c */
    float time;                 /* 0x40 */
    int wob_amp, wob_spd;
    int size;                   /* 0x4c: of the work beyond the object's */
} ObjDesc;

/* Double as two words, low first. */
typedef union DWords {
    double d;
    struct {
        unsigned int lo, hi;
    } w;
} DWords;

typedef struct CamArgs {
    Camera *c;
    Vec3 *d;
    short *roll;
    short *fov;
} CamArgs;

/* Button words of one pad data block (40 bytes): sticks, then buttons. */
typedef struct PadBlock {
    short stick[6];             /* 0x00 */
    char pad_0c[4];
    unsigned int on;            /* 0x10: held */
    unsigned int trig;          /* 0x14: newly pressed */
    unsigned int hold;          /* 0x18 */
    unsigned int rep;           /* 0x1c: newly pressed or auto-repeated */
    unsigned int on2;           /* 0x20 */
    char pad_24[4];
} PadBlock;

/* Pad data (196 bytes): pad_main -> first port's; pad_system.recs holds four. */
typedef struct Pad {
    int id;                     /* 0x00: pad type */
    char pad_04[4];
    PadBlock player;                /* 0x08 */
    PadBlock menu;               /* 0x30 */
    PadBlock raw;               /* 0x58 */
    struct PadOwner *owners;    /* 0x80 */
    char t1[32];                /* 0x84 */
    char t2[32];                /* 0xa4 */
} Pad;

/* pad_system (0x378 bytes): four ports. */
typedef struct PadSystem {
    int id[4];                  /* 0x00: pad id per port, < 0 when none */
    int assign[4][2];                /* 0x10: {4, 2} per port */
    Pad *pad[4];                /* 0x30: per-port pad data (one of recs, else pad_null_data) */
    char recs[4][196];          /* 0x40: the ports' Pad records */
    int input_on[4];            /* 0x350: pad_input_enable / pad_input_disable */
    int trig_on[4];             /* 0x360 */
    int flip;                   /* 0x370 */
    char ready;
} PadSystem;

/* Config items in the SystemData block (GameState.cfg). */
typedef struct ConfigItems {
    int stereo;                         /* 0x44 "Stereo" */
    int unk_48; /* 0x48: set to 1 by game_state_init; never read */
    int vol_se;                         /* 0x4c "VolSe" */
    int vol_seq;                        /* 0x50 "VolSeq" */
    int updown;                         /* 0x54 "Updown" */
    int shot_button;                    /* 0x58 "ShotButton" */
    int od_button;                      /* 0x5c "Od  Button" */
    int vibration;                      /* 0x60 "Vibration" */
    int disp_console;                   /* 0x64 "DispConsole" */
    int disp_score;                     /* 0x68 "DispScore" */
} ConfigItems;

/* game_state (0xc44 bytes): system data, config, clears, settings, memory card link. */
typedef struct GameState {
    char pad_00[8];
    int fps;                    /* 0x008: 50 or 60 */
    int autosave_on;            /* 0x00c */
    int vms_slot;               /* 0x010: the memory card drive used */
    int card_changed;           /* 0x014 */
    char sysdata[0x44 - 0x18];  /* 0x018: SystemData block (2944 bytes at 0x18) */
    ConfigItems cfg;            /* 0x044 .. 0x06c */
    char sysdata2[0xb1c - 0x6c]; /* 0x06c: ranking tables etc. */
    int clear_count[13];        /* 0xb1c: clears per stage (ConfigGetClearCount); [11] game clears, [12] score attack clears */
    int setting_seen[5];        /* 0xb50 */
    char pad_b64[16];
    union {
        int all[33];            /* unlocked settings (ConfigGetSetting, ConfigSetSetting) */
        struct {
            int unk_b74[9];
            int area_open[11];  /* 0xb98: areas open */
            int area_table[13]; /* 0xbc4: area tables available */
        } by;
    } setting;                  /* 0xb74 */
    int lang;                   /* 0xbf8: message language */
    int country;                /* 0xbfc */
    int vms_link;               /* 0xc00: a memory card is linked */
    int save_time[3];           /* 0xc04 */
    unsigned int vms_crc;       /* 0xc10 */
    unsigned int data_crc;      /* 0xc14 */
    int line_color;
    int menu_color;             /* 0xc1c */
    int unk_c20; /* 0 in game_state_init; never read */
    int select[8];              /* 0xc24: selected settings (ConfigGetSelect) */
} GameState;

/* screen_cfg (16 bytes): scale and centre. */
typedef struct ScreenCfg {
    float sx, sy;
    float cx, cy;               /* 0x08 */
} ScreenCfg;

/* 64-byte header task_create puts before a work area (overlays call it Task). */
typedef struct TaskHdr {
    unsigned int magic;         /* 0x00: magic 'TASK' */
    struct TaskHdr *next;       /* 0x04: next sibling */
    struct TaskHdr *child;      /* 0x08: first child */
    struct TaskHdr *parent;     /* 0x0c */
    void (*exec)(void *work);   /* 0x10: the current per-frame callback */
    void (*disp)(void *work);   /* 0x14: the draw callback */
    void (*release)(void *work); /* 0x18 */
    unsigned int flags;         /* 0x1c: 1 end, 2 sleep, 4 released, 8 runs while paused */
    const char *name;           /* 0x20 */
    float exec_time;            /* 0x24: profile, last exec */
    float disp_time;            /* 0x28: profile, last disp */
    int dlist;                  /* 0x2c: display list for disp (5) */
    char pad_30[16];
} TaskHdr;                      /* 0x40 bytes; the work area follows */

typedef struct TaskHdr Task;

/* Node mesh (20 bytes; yuki_tool.c, plane cutters): packets, 2nd-pass texture, bound sphere. */
typedef struct YMesh {
    int *data;                  /* 0x00: vertex/packet data, saved by yuki_data_save */
    int tex;                    /* 0x04: second-pass texture (0 = none) */
    Vec3 center;                /* 0x08: bounding sphere */
    float radius;               /* 0x14 */
} YMesh;

/* nj_cnk_vu1_draw parameter block (YMesh layout). */
typedef struct YDrawPrm {
    int *data;
    int tex;                    /* 0x04 */
    Vec3 center;                /* 0x08 */
    float radius;               /* 0x14 */
} YDrawPrm;

/* yuki_tool.c hierarchy node (52 bytes). */
typedef struct YNode {
    unsigned int flags;         /* 0x00: bit 4 = do not descend into the children */
    YMesh *mesh;                /* 0x04 */
    int unk_08; /* decreased by n in yuki_node_sub; never read */
    char pad_0c[32];
    struct YNode *child;        /* 0x2c */
    struct YNode *next;         /* 0x30 */
} YNode;

/* Task descriptor (20 bytes): task_create callbacks and name. */
typedef struct TaskDesc {
    void (*init)(void *work);
    void (*exec)(void *work);       /* 0x04: once per frame */
    void (*disp)(void *work);       /* 0x08: debug display */
    void (*release)(void *work);    /* 0x0c */
    const char *name;               /* 0x10 */
} TaskDesc;

#endif /* REZ_TYPES_COMMON_H */
