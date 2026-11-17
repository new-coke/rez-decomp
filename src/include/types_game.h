/* Game types of the main executable: task works, menus, wipes, cameras, enemies, maps, player. */

#ifndef REZ_TYPES_GAME_H
#define REZ_TYPES_GAME_H

#include "types_main.h"
#include "types_common.h"

typedef enum McSlotState { MC_SLOT_STATE_0 } McSlotState;

typedef struct FormatWork {
    int phase;
    unsigned int saved_flags; /* 0x04 */
    int code;                 /* 0x08 */
} FormatWork;

typedef struct CheckVmsWork {
    char buf[1024];
    int phase;
    unsigned int saved_flags; /* 0x404 */
} CheckVmsWork;

/* Listener list node (listener_lists[0..24]). */
typedef struct Listener {
    void (*fn)(int arg);
    struct Listener *next;
} Listener;

typedef struct { float pos[3]; int pitch; int yaw; int roll; } CamView;

typedef struct {
    float pos;
    float vel;
    float k;
    float damp;
    int handle;
} Ramp;

typedef struct { char *name; int help; int run; } CmdDef;

typedef struct { char name[4]; int score; float analyze; float shotdown; float item; int one; } ResultRecord;

typedef struct {
    char pad_00[24];
    float left;
    float right;
    float top;
    float bottom;
} ScrInfo;

typedef struct {
    int level;
    int state;
    float x;
    float y;
    float in_x;
    float out_x;
    float in_y;
    float out_y;
} ConWin;

typedef struct ConEvent {
    int id;
    void (*fn)();
} ConEvent;

typedef struct CharParam {
    int ch;
    int time;
    float x;
    float y;
    float persp;
    float z;
    float scl;
    int col;
} CharParam;

typedef struct NumRec {
    int *obj;
    float x;
    float y;
} NumRec;

/* 56 bytes */
typedef struct CharWork {
    int time;
    float x;
    float y;
    float persp;
    float z;
    float scl;
    int col;
    int rot[3];
    int verts;
    int cnt;
    int *pos;
    int *ang;
} CharWork;

typedef struct NumParam {
    float x;
    float y;
    float persp;
    int value;
    float scl;
    int color;
} NumParam;

/* 80 bytes */
typedef struct NumWork {
    char digits[4];
    int digit_cnt;
    int color;
    int color_cur;
    int color_to;
    float scale;
    int rot_x;
    int rot_y;
    int rot_z;
    NumRec rec[2];
    char pad_3c[12];
    int timer;                  /* 0x48 */
    unsigned int state;
} NumWork;

typedef struct NumInit {
    int x;
    int y;
    float persp;
    int value;
    float scl;
    int col;
} NumInit;

typedef struct ContWork {
    int state;
    int sub;
    int lives;
    int timer;
    int num;
    unsigned anim;
} ContWork;

typedef struct DebugCommand {
    int name;
    int help;
    int run;
} DebugCommand;

typedef struct DVtx {
    float x;
    float y;
    float z;
    int color;
} DVtx;

typedef struct EnemyPassVars {
    char pad_00[16];
    float spd_pass;
    int force_pass;
    int pass_no;
    float move_x;
    float move_y;
    float move_z;
    int ang[3];
    float scl_x;
    float scl_y;
    float scl_z;
} EnemyPassVars;

typedef struct {
    unsigned char b[5];
    unsigned char key5;
    unsigned char key;
    unsigned char mod;
} KbdState;

typedef struct {
    int kind;
    int count;
    float time;
} FlowCtrl;

typedef struct { char pad_00[92]; } ItemModel;

typedef struct {
    float pos[3];
    float target[3];
    int state;
} ItemOrigin;

/* 192 bytes */
typedef struct ItemWork {
    ItemOrigin org;
    float pos[3];
    int rot[3];
    float vel[3];
    LockObj lock;
    int color;
    int rot_spd_y;
    int rot_spd_z;
    float decay;
    float scale;
    int life;
    float lock_used;
    int lock_left;
    int lock_time;
    unsigned int flags;
    int kind;
    struct ConsoleCom *con;
    unsigned int cur;
    unsigned int max;
    char pad_b8[4];
    int approach;               /* 0xbc */
} ItemWork;

/* 24 bytes */
typedef struct ItemManageWork {
    float pos[3];
    struct Listener *count_listener;
    struct Listener *spawn_listener;
    struct Listener *got_listener;
} ItemManageWork;

typedef struct ResultDef {
    int files;                  /* result model pack, its textures 8 bytes on */
    int msg;                    /* clear message, typed by LogIO */
    int *world_ents;            /* 6 words each */
    int world_cnt;
    int *numbers;
    int player_motion;
    int *player_models;
    int cam_motion;
    int cam_hdr;
    float fog_near;
    float fog_far;
    int fog_color;              /* also the background */
    int fog_mode;
    int fade_col;
} ResultDef;

typedef struct LockIntervalEditWork {
    int col;
    int row;                    /* -1: none */
} LockIntervalEditWork;

typedef enum { GS_NONE = -1 } GState;

/* 44 bytes */
typedef struct GameWork {
    int unk_00;                 /* 0 in GameStart; never read */
    int state;                  /* 0 play, 1 demo */
    int unk_08;                 /* 0 in GameStart; never read */
    int player_status;
    int enemy_status;
    int enemy_status2;
    int formation;
    int stage;
    int sound_task;
    int console;
    int paused;
} GameWork;

/* 132 bytes */
typedef struct ResultWork {
    void *pnb;
    struct ModelTimedLoad *models;
    float analyze;
    float shotdown;
    float item;
    int stage_score;
    int boss_score;
    int total_score;
    void *bg_task;
    void *log_task;
    void *score_task;
    void *number_task;
    void *motion_task;
    void *shell_task;
    int phase;
    int stage;
    int next_stage;
    int go_next;                /* play-through: next_stage follows */
    int col;
    float rgba[4];
    ResultDef *def;
    int timing;
    float time;                 /* result time: 1 per frame, 4 with decide held */
    float step;
    float best_analyze[4];
    float new_analyze;
    void *message_task;
} ResultWork;

/* 108 bytes */
typedef struct ResultMotionWork {
    void *pnb;
    struct ModelTimedLoad *models;
    void *node;
    void *motion;
    float *cam_motion;
    int cam_hdr;
    Camera cam;
    struct CamNode *cam_node;
    float pos[3];
    int rot[3];
    float scale;
    float time;
    float time_max;
    ResultWork *res;
} ResultMotionWork;

/* fog and background fade at stage clear; 0x20 bytes */
typedef struct BgDeleteWork {
    float fade;                 /* 1 down to 0 */
    float fade_step;
    int phase;
    int capturing;
    void *owner;
    float fog_near;
    int fog_col;
    int back_col;
} BgDeleteWork;

/* placed where its model node lands */
typedef struct ResultRate {
    float pos[3];
    int node_mask;
    float val;
} ResultRate;

/* 348 bytes */
typedef struct ResultNumberWork {
    void *pnb;
    struct ModelTimedLoad *models;
    ResultWork *res;
    char parts[3][92];
    ResultRate rates[3];        /* 0x120 */
} ResultNumberWork;

/* 12 bytes */
typedef struct ResultMsgDispWork {
    struct ConsoleCom *con;
    int queued;
    int alt_pos;
} ResultMsgDispWork;

/* 12 bytes */
typedef struct ResultMsgWork {
    char *text;
    int phase;
    int timer;
} ResultMsgWork;

typedef struct ResultMsgBlinkWork {
    int frame;
} ResultMsgBlinkWork;

/* 224 bytes */
typedef struct EnemyPassWork {
    int data;                   /* inherited model data base */
    char pad_04[4];
    float pos[3];               /* 0x08: the flight point */
    int ang[3];
    float move[3];
    ModelPart model;
    char pad_88[8];
    float mtx[16];              /* 0x90 */
    int shot;                   /* bit 0 shot frame passed, bit 1 this frame */
    char pad_d4[12];
} EnemyPassWork;

/* 104 bytes */
typedef struct WorldPart {
    ModelPart model;
    int col;
    int highlight;              /* in the result colour */
    int hide_at_end;            /* hidden once its animation ends */
} WorldPart;

/* 20 bytes */
typedef struct WorldDrawWork {
    void *pnb;
    struct ModelTimedLoad *models;
    int cnt;
    WorldPart *parts;
    ResultWork *res;
} WorldDrawWork;

/* 100 bytes */
typedef struct ShellPart {
    ModelPart model;
    int breaks;                 /* this play reached 100% */
    int shown;
} ShellPart;

/* 412 bytes */
typedef struct ResultShellWork {
    void *pnb;
    struct ModelTimedLoad *models;
    ResultWork *res;
    ShellPart parts[4];
} ResultShellWork;

/* 40 bytes */
typedef struct ResultScoreWork {
    void *pnb;
    struct ModelTimedLoad *models;
    ResultWork *res;
    int score[4];               /* stage, boss, total counting up, total */
    int rows;
    int row_timer;
    int count_timer;
} ResultScoreWork;

/* 32 bytes */
typedef struct FormMsg {
    float pos[3];
    char pad_0c[4];
    int idx;                    /* 0x10 */
    char pad_14[12];
} FormMsg;

typedef struct CamAngWork {
    int step;
    int total;
    int start;
    int end;
} CamAngWork;

typedef struct DcNodeObj { char pad_00[8]; int center[3]; float radius; } DcNodeObj;

typedef struct ModelColor4 { float v[4]; } ModelColor4;

typedef struct ModelLineVtx { float x, y, z; unsigned int col; } ModelLineVtx;

typedef struct PathRec { float time; float pos[3]; float col[4]; } PathRec;

typedef struct ModelListNode { char pad_00[24]; struct ModelListNode *next; } ModelListNode;

typedef struct ModelKindEnt { struct ModelKindEnt *next; } ModelKindEnt;

typedef struct WireBreakSeg {
    char pad_00[24];
    float v[3];
    int a;
    int b;
} WireBreakSeg;

typedef struct WireBreakChain { char pad_00[4]; int edge_cnt; unsigned short idx[2]; } WireBreakChain;

typedef struct DrawColor4 { float v[4]; } DrawColor4;

typedef struct {
    char pad_00[16];
    unsigned int cur;
    unsigned int trig;
    unsigned int release;
    unsigned int repeat;
    unsigned int dclick_on;
    unsigned int dclick;
} PadButtonsView;

typedef struct {
    int w;
    int bpr;
    int h;
    unsigned char *color;
    unsigned char *mask;
} PadLcdImage;

typedef struct {
    char pad_00[304];
    int act[1];
    char pad_134[176];
} PeriActRecView;

typedef struct PauseItem {
    float x, y;
    char **names;
    void (*enter)(void *w);
    void (*input)(void *w);
    void (*draw)(void *w, float x, float y, float z);
    void (*decide)(void *w);
} PauseItem;

typedef struct Pause2Work {
    char pad_00[4];
    int state;
} Pause2Work;

typedef struct PlayerV3 {
    float x, y, z;
} PlayerV3;

typedef struct PlayerMotion {
    char pad_00[4];
    unsigned int frames;    /* 4 */
} PlayerMotion;

typedef struct { float x, y, z; } PlayerCamVec;

typedef struct PlayerStatusView {
    unsigned int flags;
    unsigned int chain_hit;
    char pad_08[4];
    int level;
    int next_level;
    int shot_cnt;
    int hit_cnt;
    int cap_cnt;
    int cap_end_cnt;
    int shot_add;
    int hit_add;
    int hit_dmg_add;
    int cap_add;
    int cap_end_add;
    int cap_dmg_add;
    int shot_frm;
    int hit_frm;
    int hit_dmg_frm;
    int cap_frm;
    int cap_end_frm;
    int cap_dmg_frm;
    float damage;
    int items_add;
    int items;
    int cur;
    int max;
    int best_chain;
    int chain;
    int chain_timer;
    int score;
    int score_add;
    int score_stage;
    int score_boss;
} PlayerStatusView;

typedef struct PlayerStatusDispScoreArg { int pos; int score; int mul; } PlayerStatusDispScoreArg;

typedef struct PsSubAddItemWork {
    int mode;
    int idx;
    int delay;
    float x;
    float y;
    float scl;
    int alpha;
    int ang;
    int spd;
} PsSubAddItemWork;

typedef struct {
    int zmask;
    int mode;
} DlMode;

typedef struct { float x; float y; float z; unsigned c; } BgVtx;

typedef struct PcmEntry {
    int dir;
    const char *name;
} PcmEntry;

typedef struct DebugMenu {
    char pad_00[24];
    float x;                    /* 0x18 */
    char pad_1c[4];
    float y;                    /* 0x20 */
} DebugMenu;

typedef struct StageInfo {
    int module;
    char *file; /* 0x04: "stg01.bin" */
    int frame_col;
    int console_col;
    int laser_col0;
    int laser_col1;
    int laser_col2;
    int music_no;
    int analyze_total;
    float tempo; /* 0x24 */
} StageInfo;

typedef struct StageDef {
    void *model;                /* 0x00: loaded by model_pnb_load */
    void *tex_file;               /* 0x04: with tex_vram, loaded by model_load_timed */
    int tex_vram;
    int unk_0c;                 /* 0x0c-0x14: arguments of GameFlowInit */
    int enemy_table;
    int func_table;
    void *(*create)(void *w);   /* 0x18: result kept in StageWork.child */
    void *(*create2)(void *w);  /* 0x1c: result kept in StageWork.task_10 */
    void (*start)(void);        /* 0x20 */
    void (*exit)(void);         /* 0x24 */
    int work_size;              /* 0x28: size of the "StageWork" buffer */
} StageDef;

typedef struct LogIo {
    char *text;                 /* 0x00 */
    int font;                   /* 0x04: passed to font_begin_current */
    int state;                  /* 0x08: 0 type in, 1 hold, 2 erase */
    int count;                  /* 0x0c */
    int shown;                  /* 0x10: characters shown */
    int len;                    /* 0x14 */
    int t_in;                   /* 0x18 */
    int t_hold;                 /* 0x1c */
    int t_out;                  /* 0x20 */
    int blink;                  /* 0x24: cursor blink counter */
} LogIo;

typedef struct SysState {
    char pad_00[4];
    int unk_04; /* cleared by system_init; never read */
    int unk_08; /* 0 in system_init, 3 in system_info_print; never read */
    int stamp;                  /* 0x0c: perform_timer_read at the frame start */
    char pad_10[8];
    float dt0;                  /* 0x18: frame delta times (main.c) */
    float dt1;                  /* 0x1c */
    float dt2;                  /* 0x20 */
    float dt3;                  /* 0x24: time rate shown by debug_hud */
    int unk_28; /* 0x28: set to 6 at init; never read */
    int unk_2c; /* 0x2c: set to 3 at init; never read */
} SysState;

typedef struct Time3 {
    int v[3];
} Time3;

typedef struct VarEditSave {
    void *buf;                  /* 0x00 */
    int size;                   /* 0x04 */
    int state;                  /* 0x08 */
} VarEditSave;

/* "TexForce" task work (texforce_desc, 0x20 bytes). */
typedef struct TexForce {
    void *end;                  /* 0x00: work of its "TexForceEnd" task */
    int fbp;                    /* 0x04: 0xf800 */
    int handle;                 /* 0x08: from fog_set_enable(0), restored on exit */
    char pad_0c[4];
    int size;                   /* 0x10: from the create argument (256) */
    float x;                    /* 0x14 */
    float y;                    /* 0x18 */
    int active;                 /* 0x1c: texforce_b_eff & texforce_b_eff2 */
} TexForce;

typedef struct TfCtrl {
    TexForce *tf;               /* 0x00 */
    void **ctrl2;               /* 0x04: "TfCtrl2" work */
    char pad_08[4];
} TfCtrl;

typedef struct DlBuf {
    char pad_00[4];
    unsigned int *buf;          /* 4 */
    int words;                  /* 8 */
} DlBuf;

typedef struct MeshObj {
    int valid;                  /* 0 */
    int count;                  /* 4 */
    unsigned short idx[1];      /* 8: index pairs */
} MeshObj;

typedef struct MeshVtx {
    int pos[3];
    char pad_0c[52];
} MeshVtx;

typedef struct MeshCtx {
    char pad_00[128];
    char mtx128[576];           /* 128 */
    char mtx704[908];           /* 704 */
    MeshVtx *vtx;               /* 1612 */
} MeshCtx;

typedef struct CmdRec {
    int head;
    unsigned int ent;
    int payload[1];             /* 8 */
} CmdRec;

typedef struct PktObj {
    int words;                  /* 0 */
    void *data;                 /* 4 */
    unsigned int size;          /* 8 */
} PktObj;

typedef struct PnbHandler {
    int tag;
    void (*fn)(char *, int);
} PnbHandler;

typedef struct TexSource {
    char pad_00[10];
    unsigned short count;
} TexSource;

typedef struct LinePoint {
    float x;
    float y;
    float z;
    int w;
} LinePoint;

/* line_mode batch mode */
typedef enum { LINE_MODE_NONE, LINE_MODE_1, LINE_MODE_2 } LineMode;

typedef struct BillboardVtx {
    float x;
    float y;
    float z;
    float u;
    float v;
    int tex;
} BillboardVtx;

typedef struct AnimCursorSet {
    char *in0;                  /* 0 */
    char *in1;                  /* 4 */
    int *out;                   /* 8 */
} AnimCursorSet;

typedef struct {
    int list[14];
} DlSlot;

typedef struct PadState {
    char pad_00[64];
    unsigned int edge; /* 64 */
    char pad_44[40];
    int buttons; /* 0x6c */
} PadState;

/* game_state part used by the "Warning" box. */
typedef struct TvGame {
    char pad_00[16];
    int lang; /* 0x010: language (0..3) */
    char pad_14[3044];
    int msg; /* 0xbf8: 0 = draw the warning lines, else the select text */
    char pad_bfc[28];
    int tex; /* 0xc18: frame texture for rect_draw_outline */
    int font; /* 0xc1c: font/colour word for font_print and tex_rect_draw_uv */
} TvGame;

/* Pad state via pad_system.pad. */
typedef struct TvPad {
    char pad_00[108];
    unsigned int trig; /* 0x6c: newly pressed buttons */
} TvPad;

typedef struct TvSys {
    char pad_00[48];
    TvPad *port_rec; /* 0x30 */
} TvSys;

/* 0x40-byte task header (task.h), used part. */
typedef struct TvTaskHdr {
    char pad_00[28];
    int flags; /* 0x1c: bit 0 = end the task, bit 3 = set on create */
    char pad_20[32];
} TvTaskHdr;

typedef struct TvVtx {
    float x;
    float y;
    int z;
    unsigned int col;
} TvVtx;

/* Door strip vertex: position, z, UV, colour. */
typedef struct TvVtxUV {
    float x;
    float y;
    float z;
    float u;
    float v;
    unsigned int col;
} TvVtxUV;

typedef struct {
    float x;
    float y;
    float z;
} V3;

/* "Warning" task work (warning_desc, 8 bytes). */
typedef struct Warning {
    unsigned int id; /* 0x00: < 16 warning message, >= 16 select message id+16 */
    int sel; /* 0x04: current choice (0/1), result on confirm */
} Warning;

/* Camera for wfcamang_camera_exec, filled from camera_get_main_cam. */
typedef struct WfCam {
    float pos[3]; /* 0x00: eye */
    float target[3]; /* 0x0c: look-at point */
    char pad_18[12];
    int fov; /* 0x24: angle, 65536 = full turn */
} WfCam;

/*
 * "WFCamAng" task work (wfcamang_desc, 24 bytes): screen transition done by a camera callback
 * (wfcamang_camera_exec) registered with camera_node_add.
 */
typedef struct WfCamAng {
    char pad_00[8];
    unsigned int state; /* 0x08: 0 idle, 1 in, 2 covered, 3 out, 4 done */
    int count; /* 0x0c */
    int frames; /* 0x10 */
    int handle; /* 0x14: from camera_node_add, released with camera_node_remove */
} WfCamAng;

/* camera_node_add registration. */
typedef struct WfCamDesc {
    const char *name;
    int unk_04; /* 0 in every descriptor initialiser; never read */
    void (*func)(void *w, void *cam);
    int unk_0c; /* 0 in every descriptor initialiser; never read */
} WfCamDesc;

/* "WhiteOut" task work (whiteout_desc, 20 bytes). */
typedef struct WhiteOut {
    char pad_00[8];
    unsigned int state; /* 0x08: 0 idle, 1 fading in, 2 white, 3 fading out */
    int count; /* 0x0c */
    int frames; /* 0x10 */
} WhiteOut;

/* "WipeDoor" task work (wipedoor_desc, 20 bytes; wipefade_camang.c). */
typedef struct WipeDoor {
    int state; /* 0x00: 0 idle, 1 closing, 2 closed, 3 opening */
    int count; /* 0x04: opening frames left */
    int frames; /* 0x08: opening length */
    int timer; /* 0x0c: minimum closed time, counted down every frame */
    int handle; /* 0x10: from screen_swap_render(1), passed back when opening */
} WipeDoor;

/* wipe_types entry. */
typedef struct WipeType {
    void *(*create)(void *parent);
    void (*start)(void *w, int a, int b);
    int (*is_covered)(void *w);
    void (*finish)(void *w, int n);
    int (*is_done)(void *w);
} WipeType;


typedef struct FlagName {
    const char *name;
    unsigned int bit;
} FlagName;

/* Stage best results (16 bytes; config_stage_records): analyze, shot-down, item percentages. */
typedef struct StageRecord {
    float analyze;
    float shotdown;             /* 0x04 */
    float item;                 /* 0x08 */
    char pad_0c[4];
} StageRecord;

/* boss_start_task_init and peri_act_play_if_less argument. */
typedef struct Pair {
    int a, b;
} Pair;

/* Strip drawer textured vertex (24 bytes): position, UV, colour. */
typedef struct Vtx {
    float x, y, z;
    float u, v;                 /* 0x0c */
    unsigned int col;           /* 0x14 */
} Vtx;

/* Line and colour-strip vertex (16 bytes): position, colour. */
typedef struct LineVtx {
    float x, y, z;
    int col;                    /* 0x0c */
} LineVtx;

typedef enum WbMode { WB_MODE_0 } WbMode;

/* WireBreak creation source (64 bytes; wire_break_init copies it into the work). */
typedef struct WireBreakArg {
    void *model;                /* 0x00 */
    int w;                      /* 0x04 */
    float v[3];                 /* 0x08 */
    float drag;                  /* 0x14 */
    float spd;               /* 0x18 */
    float rot_range;               /* 0x1c */
    unsigned int flags;         /* 0x20 */
    int h;                      /* 0x24 */
    WbMode mode0;               /* 0x28: enum words (4 and 5 in the descriptors) */
    WbMode mode1;               /* 0x2c */
    float spd_min;                  /* 0x30 */
    char pad_34[12];
} WireBreakArg;

/* Flow enemy event (64 bytes): type, flags, formation, position. */
typedef struct FlowEnemyEvent {
    unsigned char type;
    unsigned char flags;
    short formation;            /* 0x02 */
    Vec3 pos;                   /* 0x04 */
    int ang[3];
    char pad_1c[36];
} FlowEnemyEvent;

/* kbd_state (14 bytes). */
typedef struct PadKeyRecView {
    char key_hold;
    char key_trig;
    char key;
    char mods;
    char pad_04[1];
    unsigned char k5;           /* 0x05 */
    unsigned char k6;           /* 0x06 */
    char chr;
    char keys[6];                  /* 0x08 */
} PadKeyRecView;

/* screen_layout: screen size, view origin, view rectangle. */
typedef struct ScreenView {
    float scr_w, scr_h;
    float view_x, view_y;       /* 0x08 */
    char pad_10[8];
    float left;                 /* 0x18 */
    float right;                /* 0x1c */
    float top;                  /* 0x20 */
    float bottom;               /* 0x24 */
} ScreenView;

typedef enum LoadPhase { LOAD_PHASE_0 } LoadPhase;

typedef struct LoadWork {
    void *owner;              /* 0x00 */
    void *data;                /* 0x04 */
    int size;                 /* 0x08 */
    LoadPhase phase;          /* 0x0c */
    unsigned int saved_flags; /* 0x10 */
    int code;                 /* 0x14 */
    int invalid;              /* 0x18 */
} LoadWork;

typedef enum SavePhase { SAVE_PHASE_0 } SavePhase;

typedef struct SaveWork {
    void *buf;                /* 0x00 */
    SavePhase phase;          /* 0x04 */
    unsigned int saved_flags; /* 0x08 */
    int code;                 /* 0x0c */
} SaveWork;


/* Player task work, partial view (0x504 bytes). */
typedef struct PlayerWorkView {
    char pad_00[4];
    void *obj;                  /* 0x04 */
    float x, y, z;              /* 0x08 */
    char pad_14[4];
    int no_damage;
    int level;
    float blend;
    char pad_24[1056];
    float turn;
    char pad_448[12];
    int neck_ang;
    char pad_458[132];
    int draw_blend;
    char pad_4e0[12];
    int next_mode;
    int phase;
    char pad_4f4[8];
    struct PlayerWorkView *prev; /* 0x4fc */
    void *sub;                  /* 0x500 */
} PlayerWorkView;

/* Camera list node hooks; a node copies one in. */
typedef struct CamHooks {
    const char *name;                       /* 0x00 */
    void (*init)(struct Camera *c, int user);     /* 0x04 */
    void (*update)(struct Camera *c, int user);   /* 0x08 */
    void (*release)(struct Camera *c, int user);  /* 0x0c */
} CamHooks;

/* Camera scene list node (0xd0 bytes, next at 0xcc). */
typedef struct CamNode {
    int id;             /* 0x00 */
    char pad_04[12];
    float m1[16];       /* 0x10: matrix, initialised by matrix_unit_set */
    float m2[16];       /* 0x50: matrix, initialised by matrix_unit_set */
    int user;           /* 0x90: passed to the hooks */
    Camera cam;         /* 0x94 */
    CamHooks hooks;     /* 0xbc */
    struct CamNode *next;   /* 0xcc */
} CamNode;

/* camera_scene manager. */
typedef struct CamScene {
    char mtx[0x40];
    CamNode *head;      /* 0x40: points at root */
    char pad_44[12];
    CamNode root;       /* 0x50 */
    int mode;           /* 0x120 */
    void *sel;          /* 0x124 */
    int view_mtx;           /* 0x128 */
    int inv_mtx;           /* 0x12c */
    int main_cam;           /* 0x130 */
    int main_view_mtx;           /* 0x134 */
} CamScene;

/* Console grid cell (4 bytes). */
typedef struct ConsoleCell {
    short ch;               /* 0 = empty */
    short age;              /* 0..10; new characters draw larger */
} ConsoleCell;

/* console_fonts metrics (12 bytes per mode). */
typedef struct ConsoleFontInfo {
    int font;               /* font_set() argument */
    float adv_x;            /* character advance */
    float adv_y;            /* line advance */
} ConsoleFontInfo;

/* Text console (console_com_init); setters fall back to console_cur when passed NULL. */
typedef struct ConsoleCom {
    ConsoleCell *cells;     /* 0x00 cols * rows cells */
    int *rows;              /* 0x04 per-row word array */
    int mode;               /* 0x08 */
    int cols;               /* 0x0c */
    int nrows;              /* 0x10 */
    int cur_x;              /* 0x14 */
    int cur_y;              /* 0x18 */
    float adv_x;           /* 0x1c */
    float adv_y;           /* 0x20 */
    float pos_x;            /* 0x24 */
    float pos_y;            /* 0x28 */
    int size_x;             /* 0x2c */
    int size_y;             /* 0x30 */
    int col;             /* 0x34 */
    int panel_col;             /* 0x38 */
    int panel_col2;             /* 0x3c */
    int panel;             /* 0x40 */
    struct ConsoleCom *next; /* 0x44 child console list */
} ConsoleCom;

/* console_sys: two console lists, mode word. */
typedef struct ConsoleSys {
    ConsoleCom *list_a;     /* 0x00 */
    ConsoleCom *list_b;     /* 0x04 */
    int mode;               /* 0x08 */
} ConsoleSys;

/* enemy_status effect slot (36 bytes). */
typedef struct FxEntry {
    int unk_00; /* cleared by enemy_status_ent_clear; never read */
    int owner;
    int id;                 /* 0x08 effect id (32..45 = bullet sounds) */
    int handle;             /* 0x0c handle returned by enemy_formation_appear etc., -1 = none */
    unsigned state;         /* 0x10 0..3 */
    float pos[3];           /* 0x14 */
    int life;             /* 0x20 -1 when unused */
} FxEntry;

/* Effect table: flag word, counters, 256 slots. */
typedef struct FxTable {
    unsigned flags;         /* 0x00 */
    char pad_04[12];
    int cur;             /* 0x10 */
    int fake;             /* 0x14 */
    char pad_18[24];
    int add_appear;             /* 0x30 */
    int add_die;             /* 0x34 */
    int add_away;             /* 0x38 */
    char pad_3c[12];
    FxEntry slot[256];      /* 0x48 */
} FxTable;

/* enemy_formation_slots entry (64 slots, 36 bytes). */
typedef struct FormSlot {
    int unk_00; /* cleared by enemy_formation_slot_clear; never read */
    float pos[3];           /* 0x04 */
    int assign;
    int dispose;
    int appear;
    int die;
    int away;
} FormSlot;

/* debug_flag_table entry (MESO, TMIT, KODE, YUKI, NODA). */
typedef struct DebugFlag {
    const char *name;
    unsigned mask;          /* bit in debug_flags */
} DebugFlag;

/* screen_layout visible-area rectangle. */
typedef struct ScreenRect {
    char pad_00[24];
    float left;             /* 0x18 */
    float right;            /* 0x1c */
    float top;              /* 0x20 */
    float bottom;           /* 0x24 */
} ScreenRect;

/* Enemy status slot (36 bytes, 256 slots); state 0 = free. */
typedef struct EnemyEnt {
    int ids[4];          /* 0x00 */
    int state;              /* 0x10 */
    int pos[4];          /* 0x14 */
} EnemyEnt;

/* enemy_status (EnemyStatusInitGame / EnemyStatusInitStage). */
typedef struct EnemyStatus {
    unsigned int flags;     /* 0x00 bit 0: reset the phase (VarDef bit flag "no_move") */
    float action_time;      /* 0x04 VarDef "action_time": added to phase every frame */
    float phase;            /* 0x08 fractional part of the accumulated action_time */
    int ticks;              /* 0x0c integer part */
    int cur;                /* 0x10 current enemy count */
    int fake;               /* 0x14 */
    int game[3];            /* 0x18 whole-game enemy: appear, die, away */
    int stage[3];           /* 0x24 per-stage enemy: appear, die, away */
    int add[3];             /* 0x30 pending increments */
    int last[3];            /* 0x3c */
    EnemyEnt ent[256];      /* 0x48 */
} EnemyStatus;

/* Gindows desktop metrics at screen_layout (used fields). */
typedef struct GwDesktop {
    char pad_00[24];
    float left;             /* 0x18 left edge */
    char pad_1c[4];
    float top;             /* 0x20 top edge */
    float bottom;             /* 0x24 bottom edge */
} GwDesktop;

struct ActObj;

/* ActObj.lock: lock-on record (enemy_task_init -> lockon_obj_init; LockObj layout). */
typedef struct ActSub {
    char pad_00[6];
    short life;             /* 0x06 LockObj.life */
    char pad_08[8];
    int flags;              /* 0x10 bit 0x2000 set by lockon_obj_hit, bit 0x100 = hit pending */
    char pad_14[32];
} ActSub;

/* Callbacks copied into init_fn .. post_fn (24 bytes). */
typedef struct ActVtbl {
    void (*init_fn)();
    void (*release_fn)(struct ActObj *o, void *arg);
    void (*state_fn)(struct ActObj *o, void *arg);
    void (*disp_fn)(struct ActObj *o, void *arg);
    void (*exec_fn)(struct ActObj *o, void *arg);
    void (*post_fn)(struct ActObj *o, void *arg);
} ActVtbl;

/* ActObj.gfx (68 bytes). */
typedef struct ActGfx {
    char pad_00[4];
    int root;                /* 0x04 */
    int clip;                /* 0x08 */
    char pad_0c[48];
    float time;              /* 0x3c */
    float length;              /* 0x40 */
} ActGfx;

/* Enemy/actor object of the enemy task methods; >= 0x17c bytes, own work follows. */
typedef struct ActObj {
    int res[2];          /* 0x00 */
    Vec3 pos;           /* 0x08 */
    IVec3 rot;              /* 0x14 rotation (enemy_task_init: model rot) */
    Vec3 param_vec;           /* 0x20 */
    Vec3 scale;             /* 0x2c (enemy_task_init: 1, 1, 1; ActObjScalePulse*) */
    int pstate;                /* 0x38 */
    int phase;                /* 0x3c */
    int state;              /* 0x40 */
    int count;              /* 0x44 */
    unsigned int flags;     /* 0x48 */
    float time;              /* 0x4c */
    float state_time;              /* 0x50 */
    ActGfx gfx;             /* 0x54 */
    char pad_98[12];
    float radius;             /* 0xa4 cull radius */
    char pad_a8[8];
    void (*init_fn)(); /* 0xb0 init */
    void (*release_fn)(struct ActObj *o, void *arg); /* 0xb4 */
    void (*state_fn)(struct ActObj *o, void *arg); /* 0xb8 */
    void (*disp_fn)(struct ActObj *o, void *arg); /* 0xbc */
    void (*exec_fn)(struct ActObj *o, void *arg); /* 0xc0 */
    void (*post_fn)(struct ActObj *o, void *arg); /* 0xc4 */
    unsigned char status_slot;  /* 0xc8 fx slot (enemy_status_ent_create -> enemy_status_ent_alloc) */
    char pad_c9[1];
    short link;             /* 0xca */
    float param[6];          /* 0xcc */
    int score;               /* 0xe4 */
    float time_limit;             /* 0xe8 */
    float out_dist;             /* 0xec */
    Vec3 off;           /* 0xf0 */
    IVec3 ang;             /* 0xfc */
    IVec3 angspd;          /* 0x108 */
    float amp;              /* 0x114 */
    ActSub lock;            /* 0x118 lock-on record (lockon_obj_init) */
    Vec3 lock_imp;          /* 0x14c */
    Vec3 hit_imp;          /* 0x158 */
    int end_event;               /* 0x164 */
    int status_pos;               /* 0x168 */
    int appear_flags;               /* 0x16c */
    int vanish_flags;               /* 0x170 */
    int appear_frm;               /* 0x174 */
    int vanish_frm;               /* 0x178 */
    int work[4];          /* 0x17c (extent unknown) */
} ActObj;

/* One of 12 screen effect control points (16 bytes). */
typedef struct FadePoint {
    float x;                /* 0x00 */
    float y;                /* 0x04 */
    float w;                /* 0x08 (1 / speed after init) */
    int val;                /* 0x0c result of the per-kind callback */
} FadePoint;

/* Fade/wipe overlay task work (296 bytes); current one is fade_work; state 3 = idle. */
typedef struct FadeWork {
    FadePoint v[12];            /* 0x000 */
    int kind[12];           /* 0x0c0 callback index per control point */
    int mode;               /* 0x0f0 */
    int cur;                /* 0x0f4 elapsed frames */
    int len;                /* 0x0f8 duration in frames (0 = none) */
    signed char dir;        /* 0x0fc 1 = counting down */
    char pad_fd[3];
    int color;              /* 0x100 */
    int prev_color;               /* 0x104 */
    float speed;            /* 0x108 */
    int blend_a;               /* 0x10c */
    int blend_b;               /* 0x110 */
    int col_mode;               /* 0x114 */
    int speed_kind;               /* 0x118 */
    char pad_11c[4];
    int state;              /* 0x120 */
    int flag;               /* 0x124 */
} FadeWork;

/*
 * "tf" effect node (56-byte work, fade_shape_create). state/count: requested state and phase;
 * pstate/phase: previous frame's.
 */
typedef struct FadeShapeWork {
    int base;               /* 0x00 base address of the data bank (system_pnb) */
    void *p4;               /* 0x04 */
    int pstate;             /* 0x08 */
    int phase;              /* 0x0c */
    int state;              /* 0x10 */
    int count;              /* 0x14 */
    unsigned char alpha;    /* 0x18 */
    char pad_19[3];
    float frm;              /* 0x1c frame counter */
    float in_len;              /* 0x20 length of mode A */
    float out_len;              /* 0x24 length of mode B */
    int saved;                /* 0x28 */
    float mot_frm;              /* 0x2c */
    int flags;              /* 0x30 bit 0 = busy, bit 1 = hold */
    int kind;               /* 0x34 index into the tables at fade_shape_object_offsets */
} FadeShapeWork;

/* filer_queue slot (1068 bytes, 16 slots). */
typedef struct FileEnt {
    int dir;                 /* 0x000 */
    char *name;             /* 0x004 points to buf */
    char buf[1024];         /* 0x008 */
    int unk_408; /* 0x408: cleared by filer_init / file_request; never read */
    int dest;               /* 0x40c destination buffer */
    int size;               /* 0x410 requested size */
    float elapsed;          /* 0x414 elapsed seconds when reading */
    int state;              /* 0x418 0 = free, 1 = queued, 2 = reading, ... */
    int retry;              /* 0x41c */
    int async;              /* 0x420 */
    int own_buf;              /* 0x424 */
    int prio;               /* 0x428 */
} FileEnt;

/* filer_queue. */
typedef struct FileSys {
    int cur;                /* 0x00 index of the slot served last */
    int busy;               /* 0x04 */
    int prio_next;                 /* 0x08 */
    int handle;             /* 0x0c current file handle */
    FileEnt ent[16];        /* 0x10 */
} FileSys;

/* Fog slot (536 bytes, fog_slots[fog_cur]). */
typedef struct FogEnt {
    unsigned int flags;     /* 0x000 bit 0 = fog dirty, bit 1 = background colour dirty */
    int mode;               /* 0x004 */
    int color;              /* 0x008 packed 0x00bbggrr colour */
    float near;             /* 0x00c */
    float far;              /* 0x010 */
    int table[128];        /* 0x014 */
    int back;               /* 0x214 background colour */
} FogEnt;

/* fog_slots entry as fog_get_current and FogGet* read it (same slots as FogEnt). */
typedef struct FogSlot {
    int flags;              /* 0x000 */
    int mode;               /* 0x004 */
    int color;              /* 0x008 packed fog colour */
    float near;             /* 0x00c */
    float far;              /* 0x010 */
    char table[512];        /* 0x014 fog table (fog_get_table) */
    int back;               /* 0x214 background colour */
} FogSlot;

/* "FogAnim" work (28 bytes): colour fade. */
typedef struct FogAnimWork {
    int cur[3];
    int dst[3];
    int count;
} FogAnimWork;

/* "FogMap" work (fog_map_init caches the current fog slot; FogMapWork layout). */
typedef struct FogMapObj {
    float near;             /* 0x000 */
    float far;              /* 0x004 */
    int a;                 /* 0x008 colour bytes 3..0 of the packed fog colour */
    int r;                 /* 0x00c */
    int g;                 /* 0x010 */
    int b;                 /* 0x014 */
    char table[512];        /* 0x018 base fog table */
    int pt;                 /* 0x218 fog point array (fog_map_set_points) */
    int npt;                /* 0x21c fog point count */
} FogMapObj;

#define HEAP_MAGIC_HEAP 0x50414548 /* "HEAP" */
#define HEAP_MAGIC_FREE 0x45455246 /* "FREE" */
#define HEAP_MAGIC_BASE 0x45534142 /* "BASE" */

typedef struct HeapBlkHdr HeapBlkHdr;
typedef struct HeapNode HeapNode;
typedef struct HeapBase HeapBase;

/* free-list node (4 words) */
struct HeapNode {
    int magic;
    HeapNode *next;
    unsigned int size;
    int ret;
};

/* heap descriptor at heap start; blocks follow its 0x40 bytes */
struct HeapBase {
    int magic;          /* "BASE" */
    HeapNode *free;  /* 4 */
    int unk_08; /* cleared by heap_create; never read */
    const char *name;   /* 0xc */
    int count;          /* 0x10: number of allocations */
    char *end;          /* 0x14 */
    HeapBase *next;  /* 0x18: next heap in the global list */
};

/* 0x40-byte block header before each allocation */
struct HeapBlkHdr {
    int magic;          /* "FREE" or "HEAP" */
    int next;
    int size;           /* 8: including header */
    void *ret;          /* 0xc */
    const char *src;    /* 0x10 */
    int line;           /* 0x14 */
};

/* block header as a free-list node */
typedef struct HeapFreeBlk HeapFreeBlk;
struct HeapFreeBlk {
    int magic;              /* "FREE" or "HEAP" */
    HeapFreeBlk *next;            /* 4 */
    unsigned int size;      /* 8: including the 0x40-byte header */
    int ret;                /* 0xc: caller address of the allocation */
    const char *src;        /* 0x10 */
    int line;               /* 0x14 */
    HeapBase *heap;      /* 0x18: owning heap */
    int pool_next;
    int pool_cnt;
};

/* game-flow event record (16 bytes, copied unaligned) */
typedef struct { int id; int a, b, c; } FlowFuncEv;

/* event list node (20 bytes, calloc) */
typedef struct FlowFuncEvNode FlowFuncEvNode;
struct FlowFuncEvNode { FlowFuncEv ev; FlowFuncEvNode *next; };

/* flow_func_tbl_get entry */
typedef struct {
    const char *name;       /* 0x00 */
    void (*fn4)(int *);     /* 0x04 */
    void (*fn8)(void);      /* 0x08 */
    void (*fnc)(int, FlowFuncEv *); /* 0x0c */
    const char *pname[3];   /* 0x10: names of the three float parameters */
} FlowFuncEnt;

/* FlowFuncObj.tags record; kind 2 carries an event index */
typedef struct FlowTag FlowTag;
struct FlowTag { int f0; int kind; unsigned int idx; FlowTag *next; };

/* "GameFlow FlowFuncData" object */
typedef struct FlowFuncObj {
    unsigned int flags;     /* 0x00: bit 0 = events in linked list, else in array */
    char pad_04[16];
    FlowFuncEnt **table;     /* 0x14 */
    int count;              /* 0x18 */
    char pad_1c[4];
    int arg;                /* 0x20 */
    char pad_24[16];
    FlowFuncEv *arr;           /* 0x34 */
    int arr_count;          /* 0x38 */
    FlowTag *tags;           /* 0x3c */
    char pad_40[12];
    FlowFuncEvNode *head;        /* 0x4c */
    int list_count;         /* 0x50 */
} FlowFuncObj;

typedef struct LockNode LockNode;
typedef struct LockMgr LockMgr;

/* lock list entry (32 bytes) */
struct LockNode {
    LockObj *obj;           /* 0 */
    int radar_next, radar_x, radar_z;
    LockNode *link;         /* 0x10: link of the active list (head: LockMgr.act) */
    LockNode *prev;         /* 0x14 */
    LockNode *next;         /* 0x18 */
    int timer;              /* 0x1c: lock timer set by lockon_confirm_step, counted down by lockon_node_update */
};

/* "Lockon" task work (0x2128 bytes) */
struct LockMgr {
    int pnb, ptm;
    int count;              /* 8: entries in use */
    LockNode slot[256];     /* 0xc */
    int lasers[16][3];           /* 0x200c */
    int radar_count;        /* 0x20cc: entries of the radar list (lockon_radar_list_build) */
    int radar_list;         /* 0x20d0: head of the radar list (nodes linked through radar_next) */
    int act_count;          /* 0x20d4: entries of the active list (lockon_radar_scan) */
    LockNode *act;          /* 0x20d8: head of the active list (linked through link) */
    LockNode head;          /* 0x20dc: list sentinel */
    int free_locks;         /* 0x20fc: locks still available (16 - lockon_get_lock_count) */
    int lock_delay;         /* 0x2100: frames until the next lock (lockon_confirm_step) */
    int state;              /* 0x2104: bit 0/1 disabled (lockon_set_enable / _od), bits 2/3 locking */
    int state_set;          /* 0x2108: bits OR-ed into state by the task exec */
    int state_clear;        /* 0x210c: bits cleared from state by the task exec */
    int request;            /* 0x2110: bit 0 starts the laser manager (lockon_laser_manage_start) */
    LockMgr *prev;          /* 0x2114: value of lockon_work before this task */
    int power;              /* 0x2118: per-lock weight, VarDef "power" (lockon_get_power) */
    int listeners[2];               /* 0x211c: event handler handles */
    int lock_time;          /* 0x2124: lock-on time, counted up to lockon_time_limit (lockon_time_update) */
};

#define TASK_FLAGS(w) (*(int *)((char *)(w) - 36))    /* TASK_* bits */

typedef struct LockonVec3 { float x, y, z; } LockonVec3;

static inline void lockon_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void lockon_vadd(float *dst, float *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void lockon_vadd3(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

static inline void lockon_vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

static inline void lockon_vlerp(float *dst, float *from, float *to, float t)
{
    dst[0] = from[0] + (to[0] - from[0]) * t;
    dst[1] = from[1] + (to[1] - from[1]) * t;
    dst[2] = from[2] + (to[2] - from[2]) * t;
}

static inline void lockon_vscale(float *vec, float scale)
{
    vec[0] *= scale;
    vec[1] *= scale;
    vec[2] *= scale;
}

/* d += (target - d) * t */
static inline void lockon_vstep(float *vec, float *target, float t)
{
    float cur_x = vec[0];
    float cur_y = vec[1];
    float cur_z = vec[2];
    float x = target[0] - cur_x;
    float y = target[1] - cur_y;
    float z = target[2] - cur_z;

    x *= t;
    y *= t;
    z *= t;
    vec[0] = cur_x + x;
    vec[1] = cur_y + y;
    vec[2] = cur_z + z;
}

typedef struct DcClip DcClip;
typedef struct MapObj MapObj;
typedef struct MapPart MapPart;
typedef struct DcNode DcNode;
typedef struct MapSlot MapSlot;
typedef struct MapVec4 MapVec4;
typedef struct MapModelInfo MapModelInfo;
typedef struct MapScene MapScene;
typedef struct MapManager MapManager;
typedef struct MapAnim MapAnim;
typedef struct MapInitDesc MapInitDesc;
typedef struct MapPair MapPair;
typedef struct MapDef MapDef;
typedef struct MapHookCtx MapHookCtx;

struct DcClip {
    char pad_00[4];
    unsigned frames;        /* 4 */
};

typedef struct ModbreakWork {
    int ntri, ncmd, tri_size, vtx_size, cmd_size;
    int tris, vtxs, cmds;
} ModbreakWork;

/* model instance with indexed wire words */
typedef struct DcObjectWires {
    char pad_00[36];
    int wire[1];
} DcObjectWires;

typedef struct DcObjectCompiled {
    char pad_00[44];
    int comp[1];
} DcObjectCompiled;

struct MapVec4 {
    float v[4];
};

/* payload of a tree node: two sub-objects */
typedef struct DcNodeData {
    void *points;
    void *cmds;
    float center[3];
    float radius;
} DcNodeData;

struct DcNode {
    unsigned flags;
    DcNodeData *data;
    float pos[3];           /* 8 */
    float rot[3];           /* 0x14 */
    float scl_x;
    float scl_y;
    float scl_z;
    DcNode *child;         /* 0x2c */
    DcNode *next;          /* 0x30 */
};

/*
 * Map part (140 bytes): the 92-byte "DcObject" of dc_object_init (node tree, clip, animation
 * time), then the Map part fields.
 */
struct MapPart {
    int flags;              /* 0: bit 1 = active, 0x10 = owns record block, 0x20 = heap allocated */
    DcNode *root;          /* 4 */
    DcClip *clip;          /* 8: animation source */
    int clip_b, clip_c;
    int wire_list;
    unsigned flags2;        /* 0x18 */
    void *cb;               /* 0x1c: callback set by dc_object_set_callback (player_part_setup, result_number_init) */
    void *cb_arg;           /* 0x20: its argument */
    int wire0;
    int wire1;
    int comp0;
    int comp1;
    char pad_34[8];
    float time;             /* 0x3c: current animation time */
    float length;           /* 0x40: clip length (frames - 1) */
    float speed;            /* 0x44: time step per frame */
    float unk_48, unk_4c;   /* both 1.0e15 at model setup (model.c); never read */
    float radius;
    void *coll;             /* 0x54: block of 108-byte collision records ('DcObjectColl') */
    short ncoll;            /* 0x58: number of collision records */
    short coll_lock;
    int color_anim;
    int wire;               /* 0x60: wire data (map_part_set_wire) */
    int color, ndef;
    int follow_frame;       /* 0x6c: time follows the Map frame (map_task_exec) */
    union {
        MapVec4 v4;
        void (*hook[4])(MapHookCtx *);
    } user;              /* 0x70: 16-byte vector / per-mode hooks */
    int cull;               /* 0x80: culling flag (map_draw_sorted) */
    int bar_sync;           /* 0x84: time = length * bar position (map_task_exec) */
    int dlist;
};

/* slot record (12 bytes) */
struct MapSlot {
    int radius, culled, clip;
};

/* model animation record (48 bytes) */
struct MapAnim {
    int node_ofs, clip_ofs, vec, clip_b_ofs, clip_c_ofs;
    int has_wire;
    int follow_frame;
    int bar_clip_ofs, bar_clip_b_ofs, bar_clip_c_ofs, color_ofs;
    int color_alt_ofs;
};

struct MapPair {
    int key, val;
};

struct MapInitDesc {
    char pad_00[24];
};

/* part definition (44 bytes) */
struct MapDef {
    int part_idx;
    int slot_idx;
    float pos[3];
    float rot[3];
    float scale[3];
};

/* part hook context, filled by map_part_ctx_fill (88 bytes) */
struct MapHookCtx {
    int idx;
    MapPart *part;
    int wire, color, follow_frame, slot_clip, cull, far_clip, far_fade, color_anim;
    float *pos;              /* 0x28 */
    float *rot;             /* 0x2c */
    float *scale;             /* 0x30 */
    int slot_culled, slot_idx, part_idx, ent_key, ent_val;
    MapPart *tex;           /* 0x48: copy of MapObj.p4 */
    int data;                /* MapObj.data copy */
    MapObj *obj;            /* 0x50 */
    float beat_pos;
};

struct MapModelInfo {
    int ndef;               /* 0: 44-byte definitions */
    int npart;              /* 4: 140-byte parts */
    int nslot;
    MapDef *defs;           /* 0xc: 44-byte definitions */
    MapAnim *anm;            /* 0x10: 48-byte entries */
};

/* "Map" task work (map_task_init); also the lock-on hooks' object */
struct MapObj {
    int data;               /* 0: model data base (animation record offsets) */
    MapPart *p4;
    MapModelInfo *model;        /* 8 */
    MapSlot *slots;         /* 0xc */
    MapPart *parts;         /* 0x10 */
    MapPair *ents;
    float frame;            /* 0x18: "Frame" (map_task_disp), set by map_set_frame */
    int own_wires;          /* 0x1c: creation argument [1]; map_task_release frees the part wires */
    int enable;
    int far_clip;
    int far_fade;
    float beat_pos;         /* 0x2c: position in the 16-beat cycle (map_task_exec) */
    int mode;               /* 0x30: animation record mode (creation argument [0]) */
    char pad_34[8];
    int unk_3c; /* 0 in map_task_init; never read */
    int sort_type;          /* 0x40: "sort_type" (map_task_disp) */
    union {
        float f;
        void *p;
    } sort_keys;              /* 0x44 */
    char pad_48[28];
    int unk_64; /* first argument of map_obj_set_params; never read */
    char pad_68[4];
    int unk_6c; /* second argument of map_obj_set_params; never read */
};

/* entry insertion-sorted by one of two float keys (12 bytes) */
typedef struct MapSortKey {
    int tag;
    float key[2];
} MapSortKey;

/* sequence scene (88 bytes) */
struct MapScene {
    MapModelInfo *model;        /* 0 */
    int fog_mode;           /* 4: fog_set arguments */
    float fog_near;         /* 8 */
    float fog_far;          /* 0xc */
    int fog_color;          /* 0x10 */
    int bg_color;           /* 0x14: BackColReq */
    int color;
    float cam_scale;        /* 0x1c: camera_set_far */
    int fov;                /* 0x20: camera_set_fov_direct (cam_ang_task_create interpolates it) */
    int cell_x, cell_y;     /* 0x24/0x28: map_manager_set_cell */
    unsigned cell_mode;     /* 0x2c */
    float frame_start;      /* 0x30 */
    float frame_end;        /* 0x34 */
    int wipe_type;          /* 0x38: wipe_start arguments */
    int wipe_len;           /* 0x3c */
    int wipe_end_len;
    int wipe_col;
    int wipe_end_arg;
    int auto_wait;          /* 0x4c: auto-change countdown */
    int flags;              /* 0x50 */
    int bg_tex;
};

/* "MapManager" task work (map_manager_task_init) */
struct MapManager {
    int data, tex;
    MapObj *obj;            /* 8: object task work */
    int wipe_task;          /* 0xc: wipe task work area */
    MapModelInfo *model;        /* 0x10 */
    int parent;             /* 0x14: parent task of the Map tasks (creation argument [3]) */
    void *sub_task;
    MapScene *scenes;      /* 0x1c */
    int nscene;             /* 0x20 */
    int cur;                /* 0x24 */
    int cell_x, cell_y;     /* 0x28/0x2c */
    int state;              /* 0x30: 0 idle, 1 advance, 2 wipe cover, 3 wipe end, 4 wait */
    float frame;            /* 0x34: Map frame */
    int next;               /* 0x38: requested scene, -1 none */
    int auto_wait;          /* 0x3c: auto-change countdown */
    int auto_change;        /* 0x40: "AutoChange On/Off" */
    int change_req;         /* 0x44 */
    void (*start_cb)(MapManager *);  /* 0x48: run when a scene starts playing */
    void (*change_cb)(MapManager *); /* 0x4c: run by map_manager_change_scene */
    int parent_cb;          /* 0x50: creation argument [4], called once by map_manager_task_exec; its result becomes parent */
    int work;
    int nwire;              /* 0x58: number of wire table entries */
    MapPair *wires;           /* 0x5c: wire table {key, model_load_wire_data handle} */
    int map_mode;           /* 0x60: creation argument [5]: < 2 creates mode 1 Maps, else mode 2 */
    int title_on;
    int frames;             /* 0x68: frame counter */
    int title_col;
};

/* modbreak_project_vertex output: screen x/y, 1/z, packed colour */
typedef struct ModbreakOut {
    float x, y, w;
    unsigned color;
} ModbreakOut;

/* modbreak_project_vertex_tex output */
typedef struct ModbreakOutTex {
    float x, y, w;
    float u, v;
    unsigned color;
} ModbreakOutTex;

/* source vertex: colour floats, UV */
typedef struct ModbreakVtx {
    char pad_00[32];
    float col_r, col_g, col_b, col_a;
    float u, v;             /* 0x30 texture coordinates */
    char pad_38[8];
} ModbreakVtx;

/* primitive record (40 bytes); alpha = alpha scale */
typedef struct ModbreakTri {
    char pad_00[32];
    float alpha;
    char pad_24[4];
} ModbreakTri;

/* compiled vertex (24 bytes): position, three attribute floats */
typedef struct ModbreakGxVtx {
    float x, y, z;
    float nx, ny, nz;
} ModbreakGxVtx;

/* ModbreakGxVtx layout, output stream */
typedef struct ModbreakVtx6Out {
    float x, y, z;
    float nx, ny, nz;
} ModbreakVtx6Out;

/* compiled triangle (40 bytes) */
typedef struct ModbreakTriRec {
    float center[3];
    float vel_x, vel_y, vel_z;
    int spin_x, spin_y;
    float alpha;
    int unk_24; /* cleared by modbreak_compile_strips(3); never read */
} ModbreakTriRec;

/* walk list node: per-node data parallel to the tree */
typedef struct DcWalkNode DcWalkNode;
struct DcWalkNode {
    char pad_00[12];
    DcWalkNode *next;          /* 0xc */
    DcWalkNode *child;         /* 0x10 */
};

typedef struct DcSceneNode DcSceneNode;
typedef int (*DcSceneNodeCb)(DcSceneNode *, int, int);
typedef int (*DcScenePostCb)(int);

/* scene-graph node of drawable parts */
struct DcSceneNode {
    unsigned int flags;     /* 0x00 */
    int *obj;               /* 0x04 */
    char pad_08[20];
    DcSceneNodeCb cb;           /* 0x1c */
    int user;               /* 0x20 */
    char pad_24[8];
    DcSceneNode *child;         /* 0x2c */
    DcSceneNode *next;          /* 0x30 */
};

/* 0x40-byte task header before a work area (task.h) */
typedef struct C09Task {
    char pad_00[16];
    void (*exec)(void *);   /* 0x10 */
    char pad_14[8];
    unsigned int flags;     /* 0x1c: bit 0 = end, bit 3 = ... */
    char pad_20[32];
} C09Task;

#define C09_TASK(w) ((C09Task *)(w) - 1)

typedef struct NodeTreeNode NodeTreeNode;
struct NodeTreeNode {
    unsigned int flags;     /* 0x00: bit 0x10 = no children; 1/2/4 = no pos/rot/scale */
    int data;
    float pos[3];           /* 0x08 */
    int rot[3];             /* 0x14 */
    float scl[3];           /* 0x20 */
    NodeTreeNode *child;         /* 0x2c */
    NodeTreeNode *next;          /* 0x30 */
};

/* "peri_act.pak" player state: peri_act_rec -> record whose act[] holds action numbers (+1). */
typedef struct PeriActRec {
    char pad_00[464];
    int act[8];
} PeriActRec;

/* kbd_state: 14-byte current key record, saved copy, owner list (PadKeyState). */
typedef struct PadKeyRec {
    char key_hold, key_trig;
    char key;               /* 0x02: key code (index of the translation tables) */
    char mods;              /* 0x03: modifier bits (0x22 shift) */
    char pad_04[3];
    char chr;               /* 0x07: translated character (pad_keyboard_translate) */
    char keys[6];           /* 0x08: pressed key codes (pad_keyboard_key_down) */
} PadKeyRec;

typedef struct PadOwner PadOwner;
struct PadOwner {
    int unk_00; /* cleared by pad_owner_add; never read */
    int key;                /* 0x04 */
    PadOwner *next;      /* 0x08 */
};

typedef struct PadKeyState {
    int id;
    PadKeyRec cur;        /* 0x04 */
    PadKeyRec saved;      /* 0x12 */
    PadOwner *owner;     /* 0x20 */
    char repeat_wait;               /* 0x24 */
    char pad_25[3];
    int idx;                /* 0x28 */
} PadKeyState;

typedef struct PassVec3 {
    float x, y, z;
} PassVec3;

/* pass_line_state (menu item 'PassLineView'). */
typedef struct PassLineState {
    int total;          /* 0  line count total */
    float step;         /* 4  counts per unit step */
    PassVec3 *pts;       /* 8  null: line off */
    int kind;           /* 12 0 pass data, 1 straight line, 2 other */
    PassVec3 dir;        /* 16 straight line direction */
} PassLineState;

/*
 * Pass (camera path): 20-byte entry (pass_table, pass_count) owning 108-byte parts; a part has
 * camera and interest splines and run data (AngZ, count). Buffers via yuki_alloc/yuki_free.
 */
typedef struct PassSpline {
    unsigned flags;     /* 0  bits 0-1: unused/absolute */
    int ref;            /* 4  symbol (relative mode) */
    int type;           /* 8  0 linear, 1 bezier (3 pts/key), 2 b-spline, 3 cardinal, 4 disang */
    int nkey;              /* 12 */
    float *rate;        /* 16 n count rates */
    PassVec3 *pts;       /* 20 n points (3n for type 1) */
    float tension;      /* 24 type 3 */
    void *knots;        /* 28 allocated for type 2 */
    PassVec3 base;       /* 32 type 4: DisAng base */
} PassSpline;           /* 44 bytes */

typedef struct PassRun {
    unsigned flags;     /* 0  0x4000: no second buffer; 0xc000: no symbol reference */
    int ref;            /* 4  symbol */
    int nkey;              /* 8  */
    int *key_count;          /* 12 n words */
    int *key_angz;         /* 16 n words */
} PassRun;              /* 20 bytes */

typedef struct PassPart {
    PassSpline cam;     /* 0   */
    PassSpline in;      /* 44  */
    PassRun run;        /* 88  */
} PassPart;             /* 108 bytes */

typedef struct PassEnt {
    int sym;            /* 0  4-character symbol */
    int nparts;         /* 4  */
    PassPart *parts;    /* 8  */
    int start;          /* 12 */
    int len;            /* 16 in counts */
} PassEnt;

typedef void (*PassEditCb)(void);

/* Field of type T at byte offset o of p (window objects have fields before and after p). */

#define FIELD_AT(p, o, T) (*(T *)((char *)(p) + (o)))
#define FIELD_BEFORE(p, o, T) (*(T *)((char *)(p) - (o)))

/* Pass editor window object (partial). */
typedef struct PassEditor {
    char pad_00[8];
    int gamecnt;            /* 8 */
    int drawmode;           /* 12  off / absolute / relative */
    int npass;              /* 16  */
    PassEnt *tbl;           /* 20  copy being edited */
    int symwin;             /* 24  window id */
    char symtxt[4];         /* 28 */
    int *symptr;            /* 32  where the symbol is stored back */
    PassEditCb symcb1;           /* 36  */
    PassEditCb symcb2;           /* 40  */
    int symcur;             /* 44  0..3 */
    int win;                /* 48  child window id */
    char title[256];        /* 52 */
    int top;                /* 308 first visible row */
    int passIdx;            /* 312 */
    int cellw;              /* 316 */
    int cellh;              /* 320 */
    PassEnt *pass;          /* 324 */
    int part_top;           /* 328 first visible part row */
    int partIdx;            /* 332 */
    int partIdx2;           /* 336 swap partner */
    PassPart *part;         /* 340 */
    int cur;                /* 344 */
    int sub;                /* 348 */
    int run_preview;            /* 352 cycled 0..2 in the run-data window */
    PassRun *run;           /* 356 */
    int sp_top;             /* 360 first visible control-point row */
    int cpidx;              /* 364 key / control point */
    int playpos;            /* 368 */
    int sel;                
    PassSpline *spline;     /* 376 */
    PassVec3 *pt;            /* 380 point being moved */
} PassEditor;

typedef void (*PassEditFn)(void);

/* Pass editor copy of the pass data; a pass is a 20-byte table entry. */
typedef struct PassEditSpline {
    unsigned flags;
    int ref;
    int type;
    int n;
    float *rate;
    void *pts;
    float tension;
    void *knots;
    int base_x, base_y, base_z;
} PassEditSpline;        /* 44 bytes */

typedef struct PassEditRun {
    unsigned flags;
    int ref;
    int n;
    int *angz;
    int *count;
} PassEditRun;           /* 20 bytes */

typedef struct PassEditPart {
    PassEditSpline cam;  /* 0   */
    PassEditSpline in;   /* 44  */
    PassEditRun run;     /* 88  */
} PassEditPart;          /* 108 bytes */

typedef struct PassEditEnt {
    int sym;            /* 0  4-character symbol */
    int nparts;         /* 4  */
    PassEditPart *parts; /* 8  */
    int start;          /* 12 */
    int len;            /* 16 in counts */
} PassEditEnt;

/* Pass editor window object (partial; passed to window callbacks). */
typedef struct PassEditWork {
    int countFlag;          /* 0   saved at open */
    int countSave;          /* 4   game count (int) saved at open */
    int countSave2;         /* 8   */
    int drawmode;                /* 12  */
    int npass;              /* 16  */
    PassEditEnt *tbl;        /* 20  copy being edited */
    int symwin;             /* 24  */
    char symtxt[4];         /* 28  */
    int *symptr;            /* 32  */
    PassEditFn symcb1;           /* 36  */
    PassEditFn symcb2;           /* 40  */
    int symcur;             /* 44  */
    int win;                /* 48  child window id */
    char title[256];        /* 52  */
    int top;               /* 308 */
    int passIdx;            /* 312 */
    int cellw;              /* 316 */
    int cellh;              /* 320 */
    PassEditEnt *pass;       /* 324 */
    char pad_148[4];
    int partIdx;            /* 332 */
    int partIdx2;           /* 336 */
    PassEditPart *part;      /* 340 */
    int cur;                /* 344 */
    int sub;                /* 348 */
    int run_preview;
    PassEditRun *run;        /* 356 */
    char pad_168[12];
    int sel;                /* 372 */
    PassEditSpline *spline;  /* 376 */
} PassEditWork;

/* "PassSim" task work (pass_sim_desc): replays the game count from cur to end. */
typedef struct PassSimWork {
    int saved;              /* 0  game count at creation (truncated) */
    int cur;                /* 4  */
    int end;                /* 8  */
    char *parent;           /* 12 creator's work (its flag bit 1 set meanwhile) */
} PassSimWork;

/* Task header flags of work area w. */
#define TASK_FLAGS_OF(w) (*(unsigned *)((char *)(w) - 36))

/* pcfile_entries: 64-byte entries, at most 512. */
typedef struct PcFileEnt {
    char name[32];
    int done;                /* 32 */
    char *buf;              /* 36 data copy */
    int size;               /* 40 */
    char pad_2c[20];
} PcFileEnt;


static inline void c13_v3copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void c13_i3copy(IVec3 *dst, IVec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* player_status */
typedef struct PlayerStatus {
    int flags;          /* 0: player_flag_names (DEAD 0x1, DAMAGE 0x2, ...) */
    int chain_hit;      /* 4: bit 0 = chain trigger (player_status_chain_check) */
    int no_damage_cnt;  /* 8: NO DAMAGE timer */
    int level;          /* 12: "Level %2d" */
    int next_level;     /* 16: latched into level with flag 0x100 LEVEL CHANGE */
    int shot_cnt;
    int hit_cnt;
    int cap_cnt;
    int cap_end_cnt;
    int shot_add;
    int hit_add;
    int hit_dmg_add;
    int cap_add;
    int cap_end_add;
    int cap_dmg_add;
    int shot_frm;
    int hit_frm;
    int hit_dmg_frm;
    int cap_frm;
    int cap_end_frm;
    int cap_dmg_frm;
    float damage;       /* 84: pending (player_status_add_damage) */
    int items_add;      /* 88: pending */
    int items;          /* 92: towards the next level (8 per level) */
    int overdrive;      /* 96: stock */
    int overdrive_max;  /* 100 */
    int best_chain;     /* 104 */
    int chain;          /* 108: flag 0x40000 CHAIN */
    int chain_timer;    /* 112 */
    int score;          /* 116 */
    int score_add;      /* 120: pending, flag 0x2000 GetSCORE */
    int score_stage;
    int score_boss;     /* 128: gained while boss_start_alive */
    int game_analyze_total; /* 132: ConfigGameAnalyzeRate denominator */
    int stage_analyze_total;/* 136: ConfigStageAnalyzeRate denominator */
    int game_analyzed;      /* 140 */
    int stage_analyzed;     /* 144 */
    int game_item_total;    /* 148: ConfigGameItemRate denominator */
    int stage_item_total;   /* 152 */
    int game_items_got;     /* 156 */
    int stage_items_got;    /* 160 */
    char pad_a4[92];
} PlayerStatus;

/* player_obj */
typedef struct PlayerObj {
    int aim_mode;
    char pad_04[12];
    Vec3 aim_vec;
    char pad_1c[12];
    IVec3 aim_ang;
    Vec3 pos;          /* 52: "Pos" (player_info_window_draw) */
    Vec3 vec;          /* 64: "Vec" */
    Vec3 rot;
    Vec3 vec_old;
    Vec3 accel;
    char pad_70[12];
    Vec3 frame_pos;
    Vec3 frame_vel;
    IVec3 frame_ang;
    Vec3 frame_vel_old;
    Vec3 frame_accel;
    char pad_b8[12];
    Vec3 next_pos;     /* 196: bytes 196..339 copied over pos.. each frame */
    char pad_d0[132];
    Vec3 shot_pos;
    Vec3 shot_rot;
    Vec3 eye;          /* 364: "eye" angles */
    char pad_178[24];
    Vec3 next_rot;
    float radius;       /* 412: player_sphere_test */
    char pad_1a0[32];
} PlayerObj;

/* Camera slots slot_a, slot_b of player_model (252 bytes each). */
typedef struct PlayerModelSlot {
    char pad_00[8];
    int turn_motion;
    int motion;
    char pad_10[220];
    int pos;
    float rate;
    char pad_f4[8];
} PlayerModelSlot;

typedef struct PlayerModel {
    char pad_00[24];
    PlayerModelSlot slot_a;
    PlayerModelSlot slot_b;
} PlayerModel;

/* "Player" task work (player_work, player_get_work). */
typedef struct PlayerWorkSub {
    char pad_00[12];
    int motion;
    char pad_10[220];
    int pos;
} PlayerWorkSub;

typedef struct PlayerPassNode {
    int state;
    float dur;
    float dur2;
    int cam_intro;
    int cam_loop;
    int roll_intro;
    int roll_loop;
    int view_intro;
    int view_loop;
    Vec3 va;
    Vec3 vb;
} PlayerPassNode;

typedef struct PlayerPassCursor {
    int state;
    float dur;
    float dur2;
    void *cam_intro;
    void *cam_loop;
    int roll_intro;
    int roll_loop;
    int view_intro;
    int view_loop;
    int va[3];
    int vb[3];
} PlayerPassCursor;

typedef struct PlayerWork {
    char pad_00[24];
    int no_damage;          /* 24: "NoDamage %4d" (player_task_exec) */
    int level;
    char pad_20[28];
    PlayerWorkSub sub;
    char pad_12c[816];
    int inpass;
    int pending;
    PlayerPassNode pass_a;
    PlayerPassCursor pass_b;
    int visible;
    float pass_time;
    char pad_4e4[4];
    int mode;           /* 1256: "MODE [%s]": NORMAL, DEAD, DEMO MOTION, LEVEL UP, LEVEL DOWN */
    int next_mode;      /* 1260 */
    int phase;              /* 1264: mode handler phase ("MODE [%s]%d") */
    char pad_4f4[4];
    unsigned int cmd_flags; /* 1272: bit 0 holds the NoDamage counter (player_command_start) */
    char pad_4fc[4];
    int camera;
} PlayerWork;

/* replay recorder (replay_work) */
typedef enum { REPLAY_IDLE, REPLAY_REC, REPLAY_PLAY } ReplayMode;

typedef struct ReplayStream {
    int idx;            /* 0: entries consumed / produced */
    int run;            /* 4: repeats within the current entry */
    int cnt;              /* 8: entries */
    void *ptr;          /* 12: current entry */
} ReplayStream;

typedef struct Replay {
    ReplayMode mode;    /* 0 */
    int flag;           /* 4: 1 = suspended */
    int frame;          /* 8 */
    int count;          /* 0xc: number of frames */
    ReplayStream dig;   /* 0x10: digital buttons, 16-bit {bits, repeat} */
    ReplayStream ax;    /* 0x20: analog x, 8-bit {value, repeat} */
    ReplayStream ay;    /* 0x30: analog y */
    char hist0[32];     /* 0x40 */
    char hist1[32];     /* 0x60 */
    int hdr[7];         /* 0x80: file header (magic 1234, counts, ...) */
} Replay;

typedef struct { unsigned short a, b; } ReplayDigEnt;
typedef struct { signed char x, y; } ReplayAnaEnt;
typedef struct { int hdr[7]; } ReplayHdr;       /* 28 bytes */

/* controller state, replay view */
typedef struct ReplayPad {
    char pad_00[12];
    int prev;           /* 0xc */
    int cur;            /* 0x10 */
    int press;          /* 0x14 */
    int release;        /* 0x18 */
} ReplayPad;

/* "ptcl.dat": 172-byte entries, each with 20-byte sub-entries */
typedef struct PtclSub {
    int tex_uv[5];
} PtclSub;                      /* 20 bytes */

typedef struct PtclEntData {
    int life[4];
    int curve;                  /* 0x10: curve id for ptcl_shape_curve */
    int col[5];
    int nsub;                      /* 0x28: number of sub-entries */
    PtclSub *sub;               /* 0x2c */
    int blend_src, blend_dst;
    int shape;                  /* 0x38: 0 axes, 1 line + circle, 2 box (editor gizmo) */
    float eye[3];               /* 0x3c */
    int rot[3];                 /* 0x48 */
    char c[88];
} PtclEntData;                  /* 172 bytes */

typedef struct PtclData {
    int count;
    PtclEntData *ent;
} PtclData;

/* editor list node: entry plus links (188 bytes, yuki_alloc) */
typedef struct PtclEnt PtclEnt;
struct PtclEnt {
    PtclEntData data;
    int flag;                   /* 0xac */
    void *task;                 /* 0xb0: preview task work */
    PtclEnt *prev;              /* 0xb4 */
    PtclEnt *next;              /* 0xb8 */
};

/* "PtclEdit" task work (ptcl_edit_work) */
typedef struct PtclEdit {
    int win;                    /* 0 */
    PtclEnt *head;              /* 4 */
    int flag;                   /* 8: entry selected / enabled */
    PtclEntData ed;             /* 0xc: working copy of the current entry */
    PtclSub sub;                /* 0xb8: working copy of the current sub-entry */
    int count;                  /* 0xcc: number of entries */
    int cur;                    /* 0xd0: current entry */
    int subidx;                 /* 0xd4: current sub-entry */
    int busy;                   /* 0xd8: number of live preview tasks */
} PtclEdit;


/* live "Ptcl" element (40 bytes) */
typedef struct PtclElem {
    int age;                    /* < 0: free */
    int life;
    float size;
    float pos[3];
    char pad_18[16];
} PtclElem;

/* "Ptcl" task work (216 bytes) */
typedef struct PtclTask {
    int count;                  /* 0: elements */
    int max;
    PtclElem *elem;             /* 8 */
    PtclEntData ed;             /* 0xc: copy of the entry being played */
    void (*cb)(int);            /* 0xb8 */
    int cbarg;                  /* 0xbc */
    char pad_c0[16];
    int rotate;                 /* 0xd0: positions are in camera space */
    int drift_mode;
} PtclTask;

/* pad_debug pad state (analogue values). */
typedef struct PadAnalog {
    char pad_00[56];
    unsigned short analog0;
    unsigned short analog1;
} PadAnalog;

/* GS vertex of screen quads and wave meshes (24 bytes). */
typedef struct ScreenVtx {
    float x, y, z;
    float u, v;
    unsigned c;
} ScreenVtx;

/* "S-Wave" task work (1376 bytes): 21x16 height table, phase state. */
typedef struct ScreenWaveWork {
    int grid;
    float tab[336];
    float strength;             /* 1348: (screen_wave_set_strength) */
    int phase_x;                /* 1352: row phase, += speed_x per frame */
    int phase_y;                /* 1356: column phase, += speed_y per frame */
    int step_x;                 /* 1360: per row */
    int step_y;                 /* 1364: per column */
    int speed_x;                /* 1368 */
    int speed_y;                /* 1372 */
} ScreenWaveWork;

/* One of three 32-byte "Fish" task records (task 96 bytes). */
typedef struct FishRec {
    int active;
    int unk_04; /* cleared by fish_spawn; never read */
    float x;
    float y;
    float speed;
    int ang;
    int tail_phase;             /* advanced with the speed; swings the tail */
    int timer;
} FishRec;

/* "ScrWvUV" task argument (scr_wv_uv_arg) and its 32-byte work. */
typedef struct ScrWvUvArg {
    int cols;
    int rows;
    void *height;                /* S-Wave height table (float *) */
    float slope_scale;
    float z;
    ScreenVtx *vtx;
    void *slope_x;                /* float * */
    void *slope_y;                /* float * */
} ScrWvUvArg;

typedef struct ScrWvUvWork {
    ScreenVtx *vtx;
    void *slope_x;                /* float * */
    void *slope_y;                /* float * */
    int cols;
    int rows;
    void *height;                /* S-Wave height table (float *) */
    float slope_scale;
    int saved;
} ScrWvUvWork;

/* "S-Lens" work area. */
typedef struct ScreenLensWork {
    void *grid;
    float y;
} ScreenLensWork;

/* "Sequence" task work (28 bytes), sound_sequence_table. */
typedef struct SoundSequenceWork {
    int count;
    void *child;
    int cur;
    int next;
    int pending;                /* 16: set by sound_sequence_set_pending; becomes next at op 255 */
    int step;                   /* 20: current command index */
    int heap_count;             /* 24: heap_alloc_count() at module start (op 1) */
} SoundSequenceWork;

typedef struct SoundSequenceCmd {
    int op;
    int arg;
    int (*cond)(void);
} SoundSequenceCmd;

typedef struct SoundSequenceEntry {
    char *name;
    SoundSequenceCmd *cmds;
} SoundSequenceEntry;

/* starts with the sd memory block; 0x2c bytes */
typedef struct SoundDmaWork {
    int blk;                    /* sdMemBlkCreate fills it */
    int kind;                   /* 0 multi unit, 1 bank */
    int buf;
    int size;
    int bank;
    int mode;
    int phase;
    int polls;
    char *name;
    int se_muted;
    int created;
} SoundDmaWork;

/* "MpbLoad" task work (1056 bytes). */
typedef struct SoundMpbLoadWork {
    char name[1024];
    int req;
    char *self;
    int state;
    int arg1;
    void *buf;
    int arg2;
    int *child;
    int busy;
} SoundMpbLoadWork;

/* peri_act.c pad vibration records */
typedef struct PeriBlk PeriBlk;
struct PeriBlk {
    char pad_00[8];
    int *data;                  /* 8: vibration table {channel, count, {duration, value} * count} */
    PeriBlk *next;              /* 12 */
};

typedef struct PeriRec {
    int port;                   /* 0 */
    int slot;                   /* 4 */
    unsigned char act[4];       /* 8: actuator levels sent to the pad (act[2] = extra) */
    char pad_0c[4];
    int acc[6];                 /* 16: per-channel levels this frame */
    PeriBlk *list;              /* 40 */
} PeriRec;                      /* 44 bytes, registered in peri_act_records[port][slot] */

typedef struct PeriPack {
    void *pack;                 /* 0: loaded pack */
    int count;                  /* 4 */
    int *entry[1];              /* 8: tables {n, {channel, count, ...} * n} */
} PeriPack;

/* vibration editor tables, peri_act format {channel, count, {duration, value} * count} */
typedef struct VibPair {
    int dur;
    int val;
} VibPair;

typedef struct VibTab {
    int channel;                /* 0 */
    int count;                  /* 4 */
    VibPair pair[5];            /* 8 */
} VibTab;                       /* 48 bytes */

typedef struct VibSet {
    VibPair ch[3][5];           /* 3 x 40 bytes */
} VibSet;                       /* 120 bytes */

/* ulGxState as the command builder sees it: table base, running word total. */
typedef struct CmdCtx {
    char pad_00[1612];
    char *base;                 /* 1612 */
    char pad_650[720];
    int total;                  /* 2336 */
} CmdCtx;

/* anim_records stream (72 bytes); anim_cur_record = current. */
typedef struct AnimStream {
    int pos;                    /* 0 */
    int len;                    /* 4: bytes (4 * n) */
    int stride;                 /* 8: per step (8 * n) */
    int start;                  /* 12: current step start */
    int end;                    /* 16 */
    char pad_14[8];
    int idx;                     /* 28 */
    char pad_20[40];
} AnimStream;                      /* 72 bytes */

/* anim_cursor_table cursor (28 bytes); anim_cur_cursor = current. */
typedef struct AnimCursor {
    int *ptr;                   /* 0: two-word steps */
    float time;                 /* 4: (anim_cursor_start) */
    int user;                   /* 8: start record's second word */
    int nrec;                   /* 12: packed records played by anim_cursor_play */
    int track;                  /* 16: 8-byte steps */
    int frame;                  /* 20: start counter; entries of another frame are refreshed */
    int slot;                   /* 24: current track cache slot (anim_cache_refresh) */
} AnimCursor;                       /* 28 bytes */

/* The 72-byte record as a channel player: key tables and counts, time, channel, type hooks. */
typedef void (*AnimEvalFn)(float, void *, int, void *);
typedef int (*AnimPlayFn)(void *);

typedef struct AnimRecord {
    int pos;                     /* 0 */
    char pad_04[8];
    void **tabs;                /* 12: per channel */
    int *counts;                /* 16: per channel */
    float time;                 /* 20 */
    int unk_18; /* 24: the second header value (anim_load_header); never read */
    int idx;                    /* 28 */
    int flags;                     /* 32 */
    AnimEvalFn eval9;            /* 36 */
    AnimEvalFn eval10;           /* 40 */
    AnimEvalFn eval11;           /* 44 */
    AnimEvalFn eval12;           /* 48 */
    AnimPlayFn play13;           /* 52: per-channel-kind hooks */
    AnimPlayFn play14;           /* 56 */
    AnimPlayFn play15;           /* 60 */
    AnimPlayFn play16;           /* 64 */
    AnimPlayFn play17;           /* 68 */
} AnimRecord;

/* Animation node (56 bytes) walked by anim_eval_node / anim_eval_tree. */
typedef struct AnimNode {
    unsigned int flags;         /* 0: bit 3 / 5 / 6 / 10 select behaviour */
    char pad_04[4];
    float pos[3];               /* 8 */
    int rot[3];                 /* 20: integer angles (floats when bit 10 is set) */
    float scale[3];             /* 32 */
    int child;                    /* 44 */
    int next;                    /* 48: sibling */
    float w;                    /* 52: quaternion w (bit 10) */
} AnimNode;

/* screen_layout: base size, centre, scale to 640x480, debug window visible rectangle. */
typedef struct ScreenGeom {
    float w;                /* 0x00 */
    float h;                /* 0x04 */
    float cx;               /* 0x08 w / 2 */
    float cy;               /* 0x0c h / 2 */
    float sx;               /* 0x10 w / 640 */
    float sy;               /* 0x14 h / 480 */
    float left;             /* 0x18 */
    float right;            /* 0x1c */
    float top;              /* 0x20 */
    float bottom;           /* 0x24 */
} ScreenGeom;

/* "FogMap" fog point (44 bytes): coloured sphere thickening the fog; t0.. = fog_ease record. */
typedef struct FogMapPt {
    char pad_00[4];
    int r, g, b;            /* 0x04 colour */
    float pos[3];           /* 0x10 world position */
    float t0;               /* 0x1c ease base */
    float radius;           /* 0x20 ease end / sphere radius */
    float amp;              /* 0x24 */
    int type;               /* 0x28 ease type */
} FogMapPt;

/* "FogMap" task work (544 bytes, fog_map_desc; FogMapObj layout). */
typedef struct FogMapWork {
    float near;             /* 0x00 */
    float far;              /* 0x04 */
    int col[4];             /* 0x08 base colour (a?, r, g, b) */
    float table[128];       /* 0x18 base fog table */
    FogMapPt *pt;           /* 0x218 */
    int npt;                /* 0x21c */
} FogMapWork;

/* Flow editor enemy event (64 bytes, game_flow_enemy.c). */
typedef struct FlowEnemyEv {
    unsigned char type;     /* 0x00 enemy type index */
    unsigned char flags;    /* 0x01 bit 0: relative to the camera */
    short formation;        /* 0x02 */
    float pos[3];           /* 0x04 */
    int ang[3];             /* 0x10 16-bit angles */
    float dir[3];           /* 0x1c velocity vector */
    float param[6];         /* 0x28 type-specific parameters */
} FlowEnemyEv;

/* flow_enemy_tbl_get enemy type. */
typedef struct FlowEnemyType {
    char *name;             /* 0x00 */
    char pad_04[12];
    void (*disp)(int, FlowEnemyEv *);   /* 0x10 preview draw */
    char *param_name[6];    /* 0x14 */
} FlowEnemyType;

/* "Map" model description (MapModelInfo plus slot spheres). */
typedef struct MapModel {
    int ndef;               /* 0x00 number of 44-byte part definitions */
    int npart;              /* 0x04 number of 140-byte parts */
    int nslot;              /* 0x08 number of 12-byte slots */
    struct C08Def *defs;    /* 0x0c */
    struct C08Anm *anm;     /* 0x10 one 48-byte record per part */
    float (*sphere)[4];     /* 0x14 one per slot */
} MapModel;

/* "Camera" task work (player camera; player_camera_task_init). */
typedef struct PlayerCamWork {
    float ofs_y;               /* 0x00 */
    float ofs_z;               /* 0x04 */
    char pad_08[12];
    int pos[3];             /* 0x14 */
    int ang[3];             /* 0x20 accumulated angles */
    float eye[3];           /* 0x2c transformed origin */
    int aim[3];             /* 0x38 */
    int aim_tgt[3];             /* 0x44 */
    int rot[3];             /* 0x50 per-frame rotation (from the camera record) */
    float fwd[3];           /* 0x5c forward vector */
    char pad_68[12];
    void *prev;             /* 0x74 */
    int listener;           /* 0x78 */
} PlayerCamWork;

/* "Ptcl" particle (40 bytes; PtclElem with velocity and spin named). */
typedef struct PtclPart {
    int age;                /* 0x00 */
    int life;               /* 0x04 */
    float size;             /* 0x08 */
    float pos[3];           /* 0x0c */
    float vel[3];           /* 0x18 */
    int ang;                /* 0x24 */
} PtclPart;

/* PtclCfg.em emitter. */
typedef struct PtclEmit {
    int shape;              /* 0x00 (cfg 0x38) 0: line, 1: ring, 2: sphere */
    float eye[3];           /* 0x04 */
    int rot[3];             /* 0x10 */
    float speed;            /* 0x1c */
    int speed_curve;        /* 0x20 */
    float vz;               /* 0x24 */
    float vz_rnd;           /* 0x28 */
    float damp;             /* 0x2c */
    int vz_curve;           /* 0x30 */
    float radius;           /* 0x34 */
    int orbit;              /* 0x38 orbit angle amplitude */
    int orbit_curve;        /* 0x3c */
    int rate;               /* 0x40 particles per emission */
    int rate_curve;         /* 0x44 */
    int interval;           /* 0x48 frames between emissions */
    int duration;           /* 0x4c */
    int wait;               /* 0x50 emission countdown */
    int remain;             /* 0x54 frames left */
    float spread;           /* 0x58 */
    float gravity;          /* 0x5c */
} PtclEmit;

/* PtclCfg.at attractor. */
typedef struct PtclAttr {
    float center[3];        /* 0x00 */
    float pull0;            /* 0x0c pull at the start */
    float pull1;            /* 0x10 pull at the end */
} PtclAttr;

/* Emitter config (172-byte "ptcl.dat" entry, PtclEntData), named from the Ptcl task exec. */
typedef struct PtclCfg {
    int life;               /* 0x00 */
    int life_rnd;           /* 0x04 */
    float size;             /* 0x08 */
    float size_rnd;         /* 0x0c */
    int size_curve;         /* 0x10 */
    unsigned char col[3][4];    /* 0x14 RGBA at birth, middle and end of life */
    int ang_mode;           /* 0x20 0: zero, 1: random */
    int spin;               /* 0x24 spin per unit of speed */
    int nsub;               /* 0x28 */
    int *sub;               /* 0x2c */
    int blend0, blend1;     /* 0x30 draw_set_blend arguments */
    PtclEmit em;            /* 0x38 */
    PtclAttr at;            /* 0x98 */
} PtclCfg;

/* "Ptcl" task work (216 bytes). */
typedef struct PtclWork {
    int count;              /* 0x00 live particles */
    int max;                /* 0x04 */
    PtclPart *part;         /* 0x08 */
    PtclCfg cfg;            /* 0x0c */
    void (*cb)(int);        /* 0xb8 */
    int cbarg;              /* 0xbc */
    float *eye_src;         /* 0xc0 */
    int *rot_src;           /* 0xc4 */
    float *move;            /* 0xc8 */
    int loop;               /* 0xcc */
    int rotate;             /* 0xd0 */
    int drift_mode;               /* 0xd4 */
} PtclWork;

#endif /* REZ_TYPES_GAME_H */
