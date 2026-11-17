/*
 * Lock-on: registered objects in the cursor area lock while the button is held (up to
 * lockon_limit); release fires a laser per lock on the beat. Also the lasers (types 2-6, M by
 * evolution level), capture beam, cursor, marks, Overdrive, 3D radar, level text, debug windows.
 */

#include "types_game.h"
#include "lockon.h"
#include "task.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_choicebox.h"
#include "gw_draw.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "nj_draw.h"
#include "nj_strip.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "game.h"
#include "debugmenu.h"
#include "pad.h"
#include "sound_midise.h"

struct LockonShot { int pad[6]; struct LockonShot *next; };
/* steer(alias): the code that matched reads LockonShot through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonShotView {
    struct { int *parent; } *target;
    int dmg;
    int mode;
    unsigned short shot_no;
    unsigned short he;
    int flags;
    char pad_14[4];
    struct LockonShotView *next;
} LockonShotView;
struct LockonShotNextView { char pad_00[4]; int v; int pad[4]; struct LockonShotNextView *next; };

typedef struct LEnt {
    int flags;
    LockNode *node;
    int *work;
} LEnt;

typedef struct RNode {
    LockObj *obj;
    struct RNode *next;
} RNode;

typedef struct { float x, y; } V2;

struct LockonArea {
    char pad_00[4];
    float radius_prev, radius, radius_init;
    V2 cursor;
    V2 cursor_prev;
    float left, top, right, bottom; /* lock-on rectangle */
    int rot, disable_cnt, disable_od_cnt, hide_cnt;
};

typedef struct { LockObj *obj; } LockEv;

struct LockonCaptureWork {
    int pad[5];
    int slot;
    char pad_18[4];
    float dst[3];           /* +0x1c */
    float pos[3];           /* +0x28 */
    float pos2[3];          /* +0x34 */
};

/* steer(alias): the code that matched reads LockonCaptureWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonCaptureWorkView {
    struct { LockObj *obj; } *node;
    int dmg;
    int mode;
    int fc;
    int fired;
    char pad_14[8];
    float dst[3];
    float pos[3];
    float pos2[3];
    float move_t;
    float dt;
    int state;
    int alpha;
    float rate;
    int ang;
    float life;
} LockonCaptureWorkView;

struct VcopyLockonCaptureWork {
    struct { void *obj; } *node;
    int pad[9];
    float pos[3];           /* +0x28 */
    float pos2[3];          /* +0x34 */
};

typedef struct LockonCaptureWorkShotNoView {
    struct { void *obj; } *node;
    int dmg;
    int mode;
    short shot_no, shot_cnt;
    int fired;
    int slot;
    int hit_next;
    float dst[3];           /* +0x1c */
    float pos[3];           /* +0x28 */
    float pos2[3];          /* +0x34 */
    float move_t;                /* +0x40 */
    float dt;               /* +0x44 */
    int state;
    float alpha;
    float rate;
    float ang;
    float life;
} LockonCaptureWorkShotNoView;

typedef struct { void *target; int slot; } LockonLaserArg;

typedef struct LockonCaptureWorkAlphaView {
    char pad_00[76];
    float alpha;
    float rate;
} LockonCaptureWorkAlphaView;

typedef struct LockonCaptureWorkAngleView {
    char pad_00[28];
    float dst[3];             /* +0x1c */
    float pos[3];             /* +0x28 */
    float pos2[3];             /* +0x34 */
    char pad_40[12];
    float alpha;
    float rate;
    int angle;              /* +0x54 */
} LockonCaptureWorkAngleView;

/* steer(alias): the code that matched reads LockonLaserArg through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserArgNodeView {
    LockNode *node;
} LockonLaserArgNodeView;

typedef struct { void **node; int valid; } Ent;

typedef struct LockonLaserManageWork {
    Ent ent[16];
    int count;
    int start;
} LockonLaserManageWork;

typedef struct { int *node; int *work; } LockonEnt;
typedef struct { int *ent; int power; } LockArg;

/* steer(alias): the code that matched reads LockonLaserManageWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserManageWorkView {
    LockonEnt ent[16];
    int count;
    int start;
    int wait;
    int shot_no;
} LockonLaserManageWorkView;

/* steer(alias): the code that matched reads Ent through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct EntView {
    int **node;
    int *work;
} EntView;

typedef struct {
    EntView ent[16];
    int count;              /* +0x80 */
    int start;              /* +0x84 */
    int wait;               /* +0x88 */
    unsigned int shot_no;         /* +0x8c */
} LockonLaserManageWorkEntView;

/* Laser-2 shot effect and its trail. */
typedef struct LockonLaser2Work {
    char pad_00[20];
    int slot;               /* +0x14: player_get_shot_origin slot */
    char pad_18[4];
    float dst[3];           /* +0x1c */
    float pos[3];           /* +0x28 */
    float dir[3];           /* +0x34: randomised launch direction */
    float pos2[3];          /* +0x40 */
    float trail[16][3];     /* +0x4c */
    float zero[16][3];      /* +0x10c: cleared by lockon_laser2_trail_init */
    char pad_1cc[24];
    int idx;                /* +0x1e4 */
} LockonLaser2Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    char pad_34[408];
    int steps;
    char pad_1d0[12];
    float spd;
    float accel;
} LockonLaser2CalcStepsLockonLaser2Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    float delta[3];
    float cur[3];
    char pad_4c[384];
    int cnt;
    char pad_1d0[4];
    float ease;
} LockonLaser2FollowLockonLaser2Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    float delta[3];
    float prev[3];
} LockonLaser2SnapLockonLaser2Work;

typedef struct {
    int target; int dmg; int mode;
    short shot_no; short shot_cnt;
    void *fired; int slot; int hit_next;
    char pad_1c[432];
    int steps;
    int state; int ease; float glow; float spd; float accel;
    int trail_len; int trail_used; int hit;
} LockonLaser2InitLockonLaser2Work;

typedef struct { char pad_00[484]; int trail_len; int trail_used; } LockonLaser2FadeExecLockonLaser2Work;

typedef struct {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    int slot;
    int hit_next;
    float dst[3];
    char pad_28[36];
    float pa[3];
    float pb[3];
    char pad_64[364];
    int state;
    char ease[4];
    float glow;
    char pad_1dc[8];
    int trail_len;
    int trail_used;
    int hit;
} LockonLaser2ExecLockonLaser2Work;

typedef struct { char pad_00[76]; V3 trail[1]; char pad_58[396]; int total; int used; } LockonLaser2DispLockonLaser2Work;
typedef struct { int idx; float sx; float sy; float sz; float len[6]; char pad_28[4]; float total; } LockonLaser3Shape;

#define V3_GET(p, x, y, z) do { (x) = (p)[0]; (y) = (p)[1]; (z) = (p)[2]; } while (0)

typedef struct {
    void **target;
    char pad_04[16];
    int slot;
    char pad_18[88];
    float pos[3];
    float pos2[3];
} LockonLaser3Work;

typedef struct {
    int target; int dmg; int mode;
    short shot_no; short shot_cnt;
    void *fired; int slot; int hit_next;
    char pad_1c[108];
    char shape[68];
    float head_step;
    char pad_d0[20];
    float mid_step;
    char pad_e8[24];
    float alpha;
} LockonLaser3WorkView;

typedef struct {
    char pad_00[28];
    float path[3];
    char pad_28[96];
    float shape[3];
    char pad_94[40];
    float head_pos[3];
    char pad_c8[8];
    float head_t;
    float mid_pos[3];
    char pad_e0[12];
    float tail_pos[3];
    int tail_edge;
    float tail_t;
    float alpha;
} LockonLaser3FadeExecLockonLaser3Work;

typedef struct {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    char pad_14[8];
    float path[3];
    char pad_28[48];
    float last_from[3];
    float last_to[3];
    float pos[3];
    float pos2[3];
    float shape[3];
    char pad_94[36];
    unsigned int state;
    float head_pos[3];
    int head_edge;
    float head_step;
    float head_t;
    float mid_pos[3];
    int mid_edge;
    float mid_step;
    float mid_t;
    float tail_pos[3];
    int tail_edge;
    float tail_t;
} LockonLaser3ExecLockonLaser3Work;

typedef struct {
    char pad_00[28];
    V3 path[1];
    char pad_28[148];
    V3 head_pos[1];
    int head_edge;
    char pad_cc[8];
    V3 mid_pos[1];
    int mid_edge;
    char pad_e4[4];
    float mid_t;
    V3 tail_pos[1];
    int tail_edge;
    char pad_fc[4];
    float alpha;
} LockonLaser3DispLockonLaser3Work;

typedef struct {
    char pad_00[20];
    int slot;
    char pad_18[4];
    float from[3];
    float pos[3];
} LockonLaser4Work;

typedef struct {
    void **target;
    char pad_04[24];
    float from[3];
    float pos[3];
    char pad_34[4];
    int cnt;
    int total;
} LockonLaser4WorkView;

typedef struct { void **target; char pad_04[36]; float pos[3]; } LockonLaser4SnapLockonLaser4Work;

typedef struct {
    int target; int dmg; int mode;
    short shot_no; short shot_cnt;
    int fired; int slot; int hit_next;
    float from[3];
    float pos[3];
    char pad_34[4];
    int cnt; int total;
    int grow; int flash;
    float width; float alpha;
} LockonLaser4InitLockonLaser4Work;

typedef struct { void **target; int slot; } LockonLaser4InitLockonLaserArg;

typedef struct {
    char pad_00[72];
    float width;
    float alpha;
} LockonLaser4FadeExecLockonLaser4Work;

#define MAX_LOCKON_LASER4_FADE_EXEC(a, b) ((a) >= (b) ? (a) : (b))

typedef struct {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    char pad_14[8];
    float from[3];
    float pos[3];
    unsigned int state;
    char pad_38[8];
    float grow;
    float flash;
    float width;
} LockonLaser4ExecLockonLaser4Work;

typedef struct {
    void **target;
    char pad_04[24];
    float from[3];
    float pos[3];
    char pad_34[16];
    float flash;
    float width;
    float alpha;
} LockonLaser4DispLockonLaser4Work;

typedef struct {
    char pad_00[40];
    float pos[3];
    char pad_34[12];
    float pos2[3];
    LockonVec3 pts[16];
} LockonLaser5Work;

typedef struct {
    char pad_00[20];
    int slot;
    char pad_18[4];
    float dst[3];
    float pos[3];
    float vel[3];
    float pos3[3];
    float trail[16][3];
    float zero[16][3];
} LockonLaser5WorkView;

typedef struct {
    char pad_00[28];
    float dst[3];
    float pos[3];
    char pad_34[24];
    float trail[16][3];
    char pad_10c[224];
    int idx;
} LockonLaser5TrailSmoothLockonLaser5Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    char pad_34[408];
    int steps;
    char pad_1d0[12];
    float ease;
    float glow;
    float spd;
    float acc;
} LockonLaser5CalcStepsLockonLaser5Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    float delta[3];
    float cur[3];
    char pad_4c[384];
    int cnt;
    char pad_1d0[12];
    float ease;
} LockonLaser5FollowLockonLaser5Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
    float delta[3];
    float prev[3];
} LockonLaser5SnapLockonLaser5Work;

typedef struct {
    int target; int dmg; int mode;
    short shot_no; short shot_cnt;
    int fired; int slot; int hit_next;
    char pad_1c[432];
    int steps;
    int state; float rate; float wire_rate; int ease; float glow;
    float spd; float acc; int trail_len; int trail_used; int hit;
} LockonLaser5InitLockonLaser5Work;

typedef struct { void *target; int slot; } LockonLaser5InitLockonLaserArg;
typedef struct { char pad_00[468]; float rate; float wire_rate; } LockonLaser5FadeExecLockonLaser5Work;

typedef struct {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    int slot;
    int hit_next;
    float dst[3];
    char pad_28[36];
    float pa[3];
    float pb[3];
    char pad_64[364];
    int state;
    float rate;
    char pad_1d8[8];
    float glow;
    char pad_1e4[8];
    int trail_len;
    int trail_used;
    int hit;
} LockonLaser5ExecLockonLaser5Work;

typedef struct {
    char pad_00[76];
    V3 trail[1];
    char pad_58[168];
    V3 trail_end[1];
    char pad_10c[200];
    float rate;
    float wire_rate;
    char pad_1dc[16];
    int n1;
    int n2;
} LockonLaser5DispLockonLaser5Work;

typedef struct {
    void **target;
    char pad_04[36];
    float pos[3];
} LockonLaser6Work;

typedef struct {
    void *owner; int dmg; int mode;
    short shot_no; short shot_cnt;
    int fired; int slot; int hit_next;
    char pad_1c[24];
    int head_t; int tail_t; int state; int alpha;
} LockonLaser6WorkView;

typedef struct { void *owner; int arg; } Src;

typedef struct {
    char pad_00[28];
    V3 pa;
    V3 pb;
    float head_t;
    float tail_t;
    char pad_3c[4];
    float alpha;
} V3AddLockonLaser6Work;

typedef struct {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    char pad_14[4];
    int hit_next;
    float goal[3];
    float pos[3];
    float head_t;
    float tail_t;
    int state;
    float alpha;
    int wait;
} LockonLaser6ExecLockonLaser6Work;

typedef struct {
    char pad_00[28];
    float goal[3];
    float pos[3];
    float head_t;
    float tail_t;
    int state;
    float alpha;
} LockonLaser6DispLockonLaser6Work;

typedef struct { char pad_00[20]; int slot; char pad_18[16]; float pos[3]; } LockonLaserMWork;

/* steer(alias): the code that matched reads LockonLaserMWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserMWorkView {
    void **target;
    char pad_04[24];
    float goal[3];
    char pad_28[12];
    float move_t;
    int state;
    int cnt;
    int total;
    float alpha;
    float radius;
} LockonLaserMWorkView;

/* steer(alias): the code that matched reads LockonLaserMWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserMWorkModeView {
    int target;
    int dmg;
    int mode;
    short shot_no;
    short shot_cnt;
    int fired;
    int slot;
    int hit_next;
    char pad_1c[12];
    float pos[3];
    int move_t;
    char pad_38[4];
    int cnt;
    int total;
    float alpha;
    float radius;
} LockonLaserMWorkModeView;

/* steer(alias): the code that matched reads LockonLaserArg through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserArgView {
    int target;
    int slot;
} LockonLaserArgView;

/* steer(alias): the code that matched reads LockonLaserMWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LockonLaserMWorkDmgView {
    LockObj **target;
    int dmg;
    char pad_08[8];
    int fired;
    char pad_14[8];
    float goal[3];
    float pos[3];
    float move_t;
    int state;
    char pad_3c[8];
    float alpha;
    float radius;
} LockonLaserMWorkDmgView;

/* unk_08: cleared at init, never read */
typedef struct { int state; int held; int unk_08; int alpha; int x; int y; float scale; } LockCursorWork;

typedef struct {
    char pad_00[12];
    int alpha;
    float x;
    float y;
    float scale;
} LockCursorWorkView;

#define ROT_Z(ang, x, y) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = y; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c - rot_v * rot_s; \
        y = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_X(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; \
        z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; \
        z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

typedef struct { float x, y, z; int col; } LockonVtx;

typedef struct Marker {
    int cnt;
    float pos[3];       /* +4 */
    float shot_pos[3];         /* +16 */
    float shot_dir[3];         /* +28 */
    char pad_28[12];
    float goal[3];
    float vel[3];
    float acc[3];
} Marker;

typedef struct { void *cnt; int pos[3]; char pad_10[80]; int alpha; } OverdriveWork;

typedef struct {
    char pad_00[92];
    float fade;
    float alpha;
    int fan_ang;
} LockonOverdriveWork;

typedef struct {
    int cnt;
    char pad_04[24];
    float pos[3];
    int ang1;
    int ang2;
    int ang3;
    char pad_34[40];
    float fade;
    float alpha;
    int fan_ang;
    int rot;
} OdVsetOverdriveWork;

#define ROT_X_OD_VSET(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y_OD_VSET(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Z_OD_VSET(ang, x, y) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = y; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c - rot_v * rot_s; y = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

/* steer(alias): the code that matched reads OverdriveWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct OverdriveWorkView {
    char pad_00[16];
    float pos[3];
    char pad_1c[68];
    float alpha;
    int fan_ang;
    int rot;
} OverdriveWorkView;

typedef struct { char pad_00[184]; int listener; } LockonRadar3dWork;

typedef struct {
    int fade;
    float letter[24][9];
    int count;
} LuEffWork;

typedef struct { float x; float y; float z; int ang; char pad_10[4]; int step; char pad_18[12]; } LuEffLetter;
typedef struct { char pad_00[8]; LuEffLetter letter[1]; } LockonLuEffWork;

typedef struct {
    int ch;
    char pad_04[12];
    int angle;
    int col;
    int step;
    float sx;
    float sy;
} LockonLuEffLetter;

/* steer(alias): the code that matched reads LuEffWork through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LuEffWorkView {
    int fade;
    LockonLuEffLetter letter[1];
    char pad_28[828];
    int count;
} LuEffWorkView;
typedef struct { int x, y, z; } R3;

/* steer(alias): the code that matched reads LuEffLetter through these member types; one view
 * shared by those sites (src/game/lockon.c). */
typedef struct LuEffLetterView {
    float x;
    float y;
    float z;
    int ang;
    char pad_10[8];
    float sx;
    float sy;
    int tail;
} LuEffLetterView;

typedef struct { char pad_00[8]; LuEffLetterView letter[1]; char pad_2c[824]; int count; } LuEffWorkDrawView;

extern int line_set_translucent(int p0);
extern void line_add_v_rgb(int h, int col0, float f, int col1, int p0, int p1);
extern void line_end(int h);
extern int line_add_seg_rgb(int h, int col0, int col1, float x0, float y0, float z0,
                            float x1, float y1, float z1);
extern char gw_varedit_root[];
extern int lockon_vars_group;
extern int lockon_info_open;
extern GwVarDef lockon_vardef[];
extern GwVarDef lockon_vardefs[];
extern GwVarDef laser_vardef[];
extern GwVarDef laser_vardefs[];
extern GwVarDef mark_vardef[];
extern GwVarDef mark_vardefs[];
extern GwVarDef capture_vardef[];
extern GwVarDef capture_vardefs[];
extern GwVarDef laser4_vardef[];
extern GwVarDef laser4_vardefs[];
extern GwVarDef laser5_vardef[];
extern GwVarDef laser5_vardefs[];
extern GwVarDef laser6_vardef[];
extern GwVarDef laser6_vardefs[];
extern GwVarDef laserm_vardef[];
extern GwVarDef laserm_vardefs[];
extern GwVarDef overdrive_vardef[];
extern GwVarDef overdrive_vardefs[];
extern int lockon_time_limit;
extern void *ulMatrixCur;
extern int camera_get_main_view_mtx(void);
extern int camera_scene_if_active(void);
extern int ulMatrixPush(void);
extern float lockon_far;
extern void matrix_unit_set(int a);
extern void player_get_state_vectors(float *v, int a, int *iv, int b);
extern int camera_get_inv_mtx(void);
extern char lockon_view_axis[];
extern float lock_obj_facing_limits[];
extern int lock_obj_state_masks[];
extern int b_ok_disp;
extern int lockon_free_min;
extern int lockon_interval;
extern int interval_mark;
extern char *lock_obj_debug_labels[];
extern void debug_draw_cross(char *s, float x, float y, float a, float b);
extern int system_flags;
extern int system_ptm;
extern int RenderSetCurrentDisplayList(int a);
extern void draw_set_fc_mode(int a);
extern void draw_set_blend(int a, int b);
extern int game_flags;
extern int lockon_press_cnt;
extern int overdrive_interval;
extern int lockon_press_delay;
extern int *pad_main;
extern int player_status_test_flags(int mask);
extern int lockon2_work;
extern struct TaskDesc lockon_task_desc;
extern void *scr_root;
extern struct TaskDesc lockon2_task_desc;
extern float screen_layout[];
extern int lockon_power;
extern int lockon_limit;
extern int lockon_interval_table;
extern unsigned char kbd_state[];
extern int debug_set_color(int c);
extern int *interval_editor_task;
extern struct TaskDesc interval_editor_desc;
extern int lockon_event_table[];
extern void listener_remove(int id, int h);
extern int enemy_status_ent_get_id(int v);
extern char *lock_obj_state_names[];
extern char *bits_to_string(int bits, int n, const char *on, const char *off);
extern int lockon_info_list_mode;
extern int lockon_info_list_window;
extern int lockon_info_window;
extern int lockon_info_list_choice;
extern void player_get_vec(float *vel);
extern int laser_time_max;
extern float laser_spd;
extern unsigned int capture_col0;
extern unsigned int capture_col1;
extern unsigned int capture_col2;
extern float capture_w;
extern float capture_wr;
extern int color_get_a(unsigned int c);
extern void billboard_draw_rot(int a, unsigned int col, int ang, float *p, float x, float y);
extern struct TaskDesc lockon_capture_desc;
extern int *lockon_laser_create_tables[];
extern int lockon_laser_create_table[];
extern int *lockon_laser_create_cur;
extern int ConfigGetSelect(int a);
extern int player_status_get_level(void);
extern float lockon_laser_dist_max;
extern void player_get_pos(float *player);
extern float vec3_distance_squared(float *a, float *b);
extern int b_l_magni;
extern float beat_clock_get_interval(void);
extern struct TaskDesc lockon_laser_manage_desc;
extern void player_status_count_shot(int v);
extern void player_status_count_hit(int a, int b);
extern void player_status_count_capture(int v);
extern void player_status_count_capture_end(int a, int b);
extern int laser2_cols[3];
extern int rand_lcg_next_a(void);
extern float laser_accel;
extern float laser_accel_ratio;
extern float laser2_joint_rate;
extern int line_set_color(int a);
extern int line_add_v(int h, V3 *a, V3 *b);
extern void line_flush(int h);
extern struct TaskDesc lockon_laser2_desc;
extern int laser3_col0_a;
extern int laser3_col1_a;
extern int laser3_cols[2];
extern int lockon_laser3_edge_axes[];
extern void draw_polyline_capped(int *col, float *pts, int n);
extern void draw_polyline_nocap(int *col, float *pts, int n);

extern int rand_lcg_next_b(void);
extern void *fog_set_enable(void *p);
extern void light_beam_draw_point(int col, V3 *p, int z, float f);
extern struct TaskDesc lockon_laser3_desc;
extern int laser4_col0_a;
extern int laser4_col1_a;
extern int laser4_col2_a;
extern int laser4_col0;
extern int laser4_col1;
extern int laser4_col2;
extern float laser4_wredu;
extern float laser4_w;
extern float laser4_wr;
extern float polyline_set_width_min(float f);
extern struct TaskDesc lockon_laser4_desc;
extern int laser5_col0;
extern int laser5_col1;
extern int laser5_col0_a;
extern int laser5_col1_a;
extern float laser5_sr;
extern float laser5_vr;
extern float laser5_vwr;
extern float laser5_hr;
extern void billboard_draw(int a, int col, V3 *p, float sx, float sy);
extern struct TaskDesc lockon_laser5_desc;
extern int laser6_col_r;
extern int laser6_col_g;
extern int laser6_col_b;
extern int color_get_r(unsigned int a);
extern int color_get_g(unsigned int a);
extern int color_get_b(unsigned int a);
extern float laser6_dist;
extern float *camera_get_current(void);
extern void matrix_from_axis_angle(float *m, float *axis, int angle);
extern float laser6_lw;
extern void project_xyz_abs(float a, float b, float c, float *out);
extern float float_add_bits16(int n, float f);
extern float laser6_ls2;
extern float laser6_va;
extern float laser6_ls0;
extern float laser6_ls1;
extern int laser6_wait;
extern int laser6_scol0;
extern int laser6_scol1;
extern float laser6_sw;
extern float laser6_ll;
extern float laser6_ps;
extern void tex_rect_draw(int a, int col, float x, float y, float z, float w, float h);
#ifdef VERSION_US
extern void tex_rect_draw_rot_pivot(int tex_idx, int tex, float *uv, int ang, float x, float y, float z,
                                    float width, float height, float px, float py);
#else
extern void tex_rect_draw_rot_pivot(float x, float y, float z, float width, float height, float px, float py,
                                    int tex_idx, int tex, float *uv, int ang);
#endif
extern struct TaskDesc lockon_laser6_desc;
extern float laserm_rr_start;
extern float laserm_spdrate;
extern float laserm_rr_end_g;
extern float laserm_ra_end;
extern float laserm_rr_grow;
extern int laserm_col0;
extern int laserm_col1;
extern float laserm_radius;
extern float laserm_folow;
extern struct TaskDesc lockon_laser_m_desc;
extern float screen_cfg[];
extern unsigned char system_fps;
extern int b_l_magni_disp;
extern void tex_rect_draw_uv(int tex, int color, float *uv, float x, float y, float z, float w, float h);

extern void font_set(int font);
extern void font_print(float x, float y, float z, int color, char *s);
extern float lock_cursor_uv[];
extern char str_lock_cursor_count[];
extern int draw_set_test_mode(int a);
extern int draw_set_ztest_mode(int a);
extern struct TaskDesc lock_cursor_desc;
extern float overdrive_tgt_r;
extern float overdrive_tgt_c;
extern void player_camera_get_angles(int *out);
extern int overdrive_a_a0;
extern int overdrive_a_a1;
extern float overdrive_a_r;
extern float overdrive_ka;
extern float overdrive_kv;
extern void spring3_step(float f1, float f2, float *a, float *b, float *c, float *d);
extern void player_set_shot_origin(float *a, float *b);
extern void player_status_set_flags(int a);
extern void player_command_start(int a);
extern void player_status_use_overdrive(void);
extern void *player_status_get_od_time(void);
extern void listener_dispatch(int a, int b);
extern void player_command_end(int a);
extern void player_status_clear_flags(int a);
extern int overdrive_a_r_s;
extern int beat_grid_hit(int grid);
extern float overdrive_size_w;
extern float overdrive_size_h;
extern float overdrive_flare_r;
extern float overdrive_flare_a;
extern void lens_flare_draw_point(float f0, float f1, float *p, int a, int b);
extern struct TaskDesc overdrive_desc;
extern void *player_get_work(void);
extern void *lockon_radar3d_work;
extern int system_pnb;
extern int *pnb_get_entry(int h, int idx);
extern void dc_object_init(void *p, int a, int b, int c);
extern void *model_get_part_cache(int h, int a);
extern void dc_object_set_wire(void *p, void *k);
extern void dc_object_release(void *p);
extern void dc_object_advance_time(void *p);
extern int player_is_drawn(void);
extern int console_get_pcolp(void);
extern void draw_set_color(int a);
extern void dc_object_draw(void *p);
extern struct TaskDesc lockon_radar3d_desc;
extern float float_lerp(float a, float b, float t);
extern float lu_eff_neg_zero;
extern char *lu_eff_texts[];
extern int game_state[];
extern void font_draw_char3d(unsigned char c, V3 *pos, R3 *rot, V3 *scale, int color);
extern struct TaskDesc lu_eff_desc;
extern void draw_polyline_wide_capped(int st, int pts, int n);
extern int listener_add();
extern void lockon_laser2_calc_steps(LockonLaser2CalcStepsLockonLaser2Work *w);
extern void lockon_laser2_fade_exec(LockonLaser2FadeExecLockonLaser2Work *w);
extern int lockon_laser2_follow(LockonLaser2FollowLockonLaser2Work *w);
extern void lockon_laser2_snap(LockonLaser2SnapLockonLaser2Work *w);
extern void lockon_laser3_fade_exec(LockonLaser3FadeExecLockonLaser3Work *w);
extern void lockon_laser3_split_edges(float *out, LockonLaser3Shape *shape, float *from, float *to);
extern void lockon_laser3_update_ends(LockonLaser3Work *w);
extern void lockon_laser4_add_velocity(LockonLaser4Work *w);
extern int lockon_laser4_ease(LockonLaser4WorkView *w);
extern void lockon_laser4_fade_exec(LockonLaser4FadeExecLockonLaser4Work *w);
extern void lockon_laser4_snap(LockonLaser4SnapLockonLaser4Work *w);
extern void lockon_laser5_add_velocity(LockonLaser5Work *w);
extern void lockon_laser5_calc_steps(LockonLaser5CalcStepsLockonLaser5Work *w);
extern void lockon_laser5_fade_exec(LockonLaser5FadeExecLockonLaser5Work *w);
extern int lockon_laser5_follow(LockonLaser5FollowLockonLaser5Work *w);
extern void lockon_laser5_snap(LockonLaser5SnapLockonLaser5Work *w);
extern void lockon_laser5_trail_init(LockonLaser5WorkView *w);
extern void lockon_laser5_trail_smooth(float rate, LockonLaser5TrailSmoothLockonLaser5Work *w);
extern void lockon_laser6_fade_exec(V3AddLockonLaser6Work *w);
extern void lockon_laser6_random_start(LockonLaser6Work *w);
extern int lockon_laser_dispatch(LockonLaserArgNodeView *w);
extern Ent *lockon_laser_find_nearest(LockonLaserManageWork *w);
extern int lockon_laser_m_ease(LockonLaserMWorkView *w);
extern void lockon_laser_m_fade_exec(LockonLaserMWorkView *w);
extern void lockon_laser_m_update_start(LockonLaserMWork *w);
extern void lockon_lock_cursor_draw(LockObj *w);
extern void lockon_lu_eff_place_letters(LockonLuEffWork *w);
extern float lockon_lu_eff_smooth_ring(float x, LuEffWork *w);
extern void lockon_overdrive_fade_exec(LockonOverdriveWork *w);
extern void player_get_rot();
extern void player_get_shot_origin(int idx, void **pos_out, void **rot_out);

extern struct LockonArea lockon_area;
extern int b_cursor_disp;
extern float lockon_digit_widths[];
extern float lockon_cursor_uvs[][4];
extern int camera_get_mode(void);
extern float float_add_bits16(int a, float f);
extern void tex_rect_draw_uv(int a, int col, float *uv, float x, float y, float z, float w, float h);

/* Translucent. */
/* 0x001280e0 */
void lockon_draw_line_3d(int start, int end, int start_col, int end_col)
{
    int line;

    line = line_set_translucent(1);
    line_add_v_rgb(line, start_col, 1.0f, end_col, start, end);
    line_end(line);
}

/* Translucent. */
/* 0x00128158 */
void lockon_draw_line_2d(unsigned int col0, unsigned int col1, float depth, float x0, float y0, float x1, float y1)
{
    float inv = 1.0f / depth;
    int line;

    line = line_set_translucent(1);
    line_add_seg_rgb(line, col0, col1, x0, y0, inv, x1, y1, inv);
    line_end(line);
}

/* Lockon variable group with subgroups, and the Lockon debug menu (Info, Interval Editor). */
/* 0x00128208 */
void lockon_vars_register(void)
{
    int root;

    lockon_vars_group = gw_varedit_add(gw_varedit_root, 0, lockon_vardef);
    gw_varedit_add_table(gw_varedit_root, lockon_vars_group, lockon_vardefs);
    root = lockon_vars_group;
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, laser_vardef), laser_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, mark_vardef), mark_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, capture_vardef), capture_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, laser4_vardef), laser4_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, laser5_vardef), laser5_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, laser6_vardef), laser6_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, laserm_vardef), laserm_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, root, overdrive_vardef), overdrive_vardefs);
    lockon_info_open = 0;
    menu_item_add(0, "Lockon", 0, 0, 1);
    menu_item_add("Lockon", "Info", lockon_info_window_toggle, 0, 0);
    menu_item_add("Lockon", "Interval Editor", lockon_interval_editor_toggle, 0, 0);
}

/* 0x00128418 */
void lockon_vars_unregister(void)
{
    gw_varedit_remove(gw_varedit_root, 0, lockon_vars_group);
    menu_item_remove(0, "Lockon");
}

/* 0x00128450 */
void *LockonInit(void *parent)
{
    DEBUG_PRINT("-----[ LockonInit ]-----\n");
    return lockon_task_create(parent);
}

/* 0x00128488 */
void LockonExit(int *w)
{
    DEBUG_PRINT("-----[ LockonExit ]-----\n");
    if (w)
        TASK_FLAGS(w) |= TASK_F_END;
}

/* Marks the object locked (flag 0x400), restarts its lock time. */
/* 0x001284c8 */
void lockon_list_insert(LockNode *prev, LockNode *node)
{
    LockObj *target;
    LockNode *nx;

    node->prev = prev;
    target = node->obj;
    nx = prev->next;
    node->next = nx;
    target->lock_time = 0;
    target->flags |= LOCK_LOCKED;
    nx->prev = node;
    prev->next = node;
}

/* Clears the locked flag and timer. */
/* 0x001284f8 */
void lockon_list_remove(LockNode *node)
{
    LockObj *target;
    LockNode *prev;

    node->prev->next = node->next;
    target = node->obj;
    target->flags &= ~LOCK_LOCKED;
    prev = node->prev;
    node->next->prev = prev;
    node->timer = 0;
}

/* Life less n per placed lock and the damage of shots in flight. */
/* 0x00128530 */
void lockon_obj_calc_remaining(LockObj *target, int n)
{
    target->lockable = target->life - target->locks * n - target->shot_dmg;
}

/* Shots in flight. */
/* 0x00128550 */
int lockon_hits_count(LockObj *target)
{
    struct LockonShot *shot;
    int cnt = 0;

    shot = target->hit_list;
    if (shot) {
        do {
            shot = shot->next;
            cnt++;
        } while (shot);
    }
    return cnt;
}

/* Among shots in flight. */
/* 0x00128588 */
int lockon_hits_max_shot_cnt(LockObj *target)
{
    struct LockonShotView *shot;
    int max_cnt = 0;

    for (shot = target->hit_list; shot; shot = shot->next) {
        int cnt = shot->he;

        if (max_cnt < cnt)
            max_cnt = cnt;
    }
    return max_cnt;
}

/* Of shots in flight. */
/* 0x001285c0 */
int lockon_hits_damage(LockObj *target)
{
    struct LockonShotNextView *shot;
    int sum = 0;

    for (shot = target->hit_list; shot; shot = shot->next)
        sum += shot->v;
    return sum;
}

/* 16 laser slots. */
/* 0x001285f8 */
void lockon_clear_entries(LockMgr *w)
{
    int *p = &w->lasers[0][0];
    int i;

    for (i = 16; i != 0; i--) {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p += 3;
    }
}

/*
 * Frees slots whose target, owner, lockable flag or laser task is gone; lock states 8 and 24 get
 * the lock time. Each lock without a slot gets a waiting laser (lockon_laser_dispatch).
 */
/* 0x00128628 */
void lockon_laser2_ready_check(LockMgr *w)
{
    LEnt *ent;
    LEnt *base;
    LockNode *head = &w->head;
    LockNode *node;
    LEnt *end;
    int i;

    ent = (LEnt *)&w->lasers[0][0];
    base = ent;
    for (i = 16; i != 0; ent++, i--) {
        int fl = ent->flags;

        if (fl & 2) {
            LockObj *target = ent->node->obj;

            if (target == 0 || target->owner == 0 || (TASK_FLAGS(target->owner) & TASK_F_END) || !(target->flags & 0x100) || ent->work == 0
                || (TASK_FLAGS(ent->work) & TASK_F_END)) {
                ent->node = 0;
                ent->flags = fl & 0xfffffffd;
                ent->work = 0;
            } else {
                switch (target->flags & LOCK_STATE) {
                case 16:
                    break;
                case 8:
                case 24:
                    target->lock_time = w->lock_time;
                    break;
                }
            }
        }
    }
    if (head->next != head) {
        node = head->next;
        end = (LEnt *)&w->radar_count;
        do {
            int cnt = node->obj->locks;
            int j;
            int k;

            ent = base;
            for (k = 16; k != 0; k--, ent++) {
                if (ent->flags & 2) {
                    if (ent->node == node)
                        cnt--;
                }
            }
            if (cnt < 0) {
                ERROR_PRINTF("Trap !!!!!!!!!!!!!\n");
                ERROR_PRINTF("laser2_ready_check()\n");
                ERROR_PRINTF("ready_num Minus!\n");
            }
            j = 0;
            ent = base;
            while (cnt > 0) {
                if (ent >= end)
                    for (;;)
                        ;
                if (!(ent->flags & 2)) {
                    struct {
                        LockNode *n;
                        int j;
                    } arg;
                    int *laser;

                    arg.n = node;
                    arg.j = j;
                    laser = lockon_laser_dispatch(&arg);
                    if (laser) {
                        ent->flags |= 2;
                        cnt--;
                        ent->work = laser;
                        ent->node = node;
                    }
                }
                ent++;
                j++;
            }
            node = node->next;
        } while (node != head);
    }
}

/* Slots and shot power go to a new laser manager task; slots cleared. */
/* 0x00128878 */
void lockon_laser_manage_start(LockMgr *w)
{
    int arg[2];
    int *p = &w->lasers[0][0];
    int i;

    arg[0] = (int)p;
    arg[1] = w->power;
    lockon_laser_manage_create(w, arg);
    for (i = 16; i != 0; i--) {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p += 3;
    }
}

/* Lock time counts up to lockon_time_limit once a lock is placed, else resets. Fires on request. */
/* 0x001288d0 */
void lockon_time_update(LockMgr *w)
{
    int request;

    if (w->state & 0xc) {
        if (w->free_locks < 16 && w->lock_time < lockon_time_limit)
            w->lock_time++;
    } else if (w->lock_time != 0) {
        w->lock_time = 0;
    }
    lockon_laser2_ready_check(w);
    request = w->request;
    if (request & 1) {
        w->request = request & ~1;
        lockon_laser_manage_start(w);
    }
}

/* All 256 objects: lock state and counts cleared, remaining life recomputed. */
/* 0x00128968 */
void lockon_clear_all(LockMgr *w)
{
    LockNode *p = w->slot;
    int i;
    int n = w->power;

    for (i = 256; i; p++, i--) {
        LockObj *target = p->obj;

        if (target) {
            int *imp = target->imp.words;

            target->flags &= ~(LOCK_CAPTURED | LOCK_HIT | LOCK_ACTIVE);
            imp[0] = 0;
            target->hit_list = 0;
            imp[1] = 0;
            imp[2] = 0;
            lockon_obj_calc_remaining(target, n);
        }
    }
}

/* View-space objects (flag 0x40) as is, or via the scene matrix while a scene is active. */
/* 0x001289f8 */
void lockon_project_objects(LockMgr *w)
{
    float pos[4];
    LockNode *p;
    LockNode *base;
    int cam, scene, i;

    cam = camera_get_main_view_mtx();
    scene = camera_scene_if_active();
    ulMatrixPush();
    if (cam)
        ulMatrixLoad(cam);
    p = w->slot;
    base = p;
    for (i = 256; i != 0; i--) {
        LockObj *target = p->obj;

        if (target) {
            if (target->flags & LOCK_CAM_RELATIVE) {
                if (scene == 0)
                    project_point(target->pos, (struct Vec3 *)&target->scr_x);
            } else {
                ulMatrixApplyPoint(ulMatrixCur, &target->pos->x, pos);
                project_point(pos, (struct Vec3 *)&target->scr_x);
            }
        }
        p++;
    }
    if (scene) {
        ulMatrixLoad(scene);
        p = base;
    for (i = 256; i != 0; i--) {
            LockObj *target = p->obj;

            if (target) {
                if (target->flags & LOCK_CAM_RELATIVE) {
                    ulMatrixApplyPoint(ulMatrixCur, &target->pos->x, pos);
                    project_point(pos, (struct Vec3 *)&target->scr_x);
                }
            }
            p++;
        }
    }
    ulMatrixPop();
}

/*
 * Lockable (0x100) within lockon_far on the ground plane, in the player frame scaled to the unit
 * circle. No longer lockable -> OK mark flag (0x40000) cleared.
 */
/* 0x00128b40 */
void lockon_radar_list_build(LockMgr *w)
{
    float cam[4];
    int iv[4];
    float pos[4];
    int view;
    int n = 0;
    LockNode **link;
    LockNode *p;
    int i = 256;
    float inv_far = 1.0f / lockon_far;

    link = (LockNode **)&w->radar_list;
    p = w->slot;
    w->radar_list = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    player_get_state_vectors(cam, 0, iv, 0);
    vu0_matrix_rotate_y(ulMatrixCur, -iv[1]);
    vu0_matrix_rotate_x(ulMatrixCur, -iv[0]);
    vu0_matrix_rotate_z(ulMatrixCur, -iv[2]);
    vu0_matrix_translate(ulMatrixCur, -cam[0], -cam[1], -cam[2]);
    view = camera_get_inv_mtx();
    do {
        LockObj *target = p->obj;

        if (target) {
            int fl = target->flags;

            if (fl & 0x100) {
                float x, z;
                float *src;

                if (fl & 0x40) {
                    ulMatrixApplyPoint(view, &target->pos->x, pos);
                } else {
                    src = &target->pos->x;
                    pos[0] = src[0];
                    pos[1] = src[1];
                    pos[2] = src[2];
                }
                ulMatrixApplyPoint(ulMatrixCur, pos, pos);
                x = pos[0] * inv_far;
                z = pos[2] * inv_far;
                if (x * x + z * z < 1.0f) {
                    *link = p;
                    n++;
                    *(float *)&p->radar_x = x;
                    link = (LockNode **)&p->radar_next;
                    *(float *)&p->radar_z = z;
                    p->radar_next = 0;
                }
            } else if (fl & 0x40000) {
                target->flags = fl & 0xfffbffff;
            }
        }
        p++;
    } while (--i);
    ulMatrixPop();
    w->radar_count = n;
}

/* Ascending depth key. */
/* 0x00128d50 */
void lockon_sorted_insert(LockNode **head, LockNode *node)
{
    LockNode *cur, *next;
    float key;

    node->link = 0;
    cur = *head;
    if (cur == 0) {
        *head = node;
        return;
    }
    key = node->obj->key;
    goto loop;
front:
    node->link = cur;
    *head = node;
    return;
append:
    cur->link = node;
    return;
loop:
    if (key < cur->obj->key)
        goto front;
    next = cur->link;
    if (next == 0)
        goto append;
    head = &cur->link;
    cur = next;
    if (head)
        goto loop;
}

/* Facing . view axis below lock_obj_facing_limits[flags & 7]. */
/* 0x00128db8 */
int lockon_obj_in_range(LockObj *target)
{
    float dir[3];
    float dot;

    if (target->flags & LOCK_CAM_RELATIVE) {
        float *p = target->dir;

        dir[0] = p[0];
        dir[1] = p[1];
        dir[2] = p[2];
    } else {
        ulMatrixApplyVector(ulMatrixCur, target->dir, dir);
    }
    dot = ulVec3Dot(lockon_view_axis, dir);
    if (lock_obj_facing_limits[target->flags & LOCK_FACING] <= dot)
        return 0;
    return 1;
}

/* In the lock-on rectangle, or anywhere with a 32 px margin while player status flag 0x10000. */
/* 0x00128e58 */
int lockon_point_on_screen(float x, float y)
{
    struct LockonArea *la = &lockon_area;

    if (player_status_test_flags(0x10000)) {
        if (x <= -32.0f || 672.0f <= x || y <= -32.0f)
            return 0;
        if (512.0f <= y)
            return 0;
        return 1;
    }
    if (x <= la->left || la->right <= x || y <= la->top)
        return 0;
    if (la->bottom <= y)
        return 0;
    return 1;
}

/*
 * Candidates: ahead, in the rectangle, life left, under lock limit, not blocked (0x1c000), facing;
 * depth sorted, marked 0x200; OK mark on entry. Targets no longer allowed give their locks back.
 */
/* 0x00128f50 */
void lockon_radar_scan(LockMgr *w)
{
    int i;
    RNode *node;

    i = w->radar_count;
    w->act = 0;
    w->act_count = 0;
    i--;
    node = (RNode *)w->radar_list;
    if (i == -1)
        return;
    do {
        LockObj *target = node->obj;
        int vis = (1.0f <= target->key);
        int ok = lockon_point_on_screen(target->scr_x, target->scr_y) & vis;
        int mask = lock_obj_state_masks[(target->flags & LOCK_STATE) >> 3];

        if (vis && b_ok_disp) {
            if (!(target->flags & LOCK_OK_MARK)) {
                target->flags |= LOCK_OK_MARK;
                ((void (*)(LockMgr *, RNode *))lockon_ok_mark_create)(w, node); /* steer(cast) */
            }
        }
        if (ok && target->lockable > 0 && target->locks < target->lock_max && !(target->flags & 0x1c000)) {
            if (!((target->flags & LOCK_FACING) && target->dir) || lockon_obj_in_range(target)) {
                lockon_sorted_insert(&w->act, node);
                target->flags |= LOCK_ACTIVE;
                w->act_count++;
            }
        }
        if (target->flags & LOCK_LOCKED) {
            if (!(w->state & mask)) {
                w->free_locks += target->locks;
                target->locks = 0;
                target->lockable = target->life;
                lockon_list_remove(node);
            }
        }
        i--;
        node = node->next;
    } while (i != -1);
}

/* Nearest allowed candidate with its interval run out, or 0. Hangs on a corrupt list. */
/* 0x00129120 */
LockNode *lockon_find_target(LockMgr *w)
{
    int count = w->act_count;
    int i;
    LockNode *node;
    LockObj *target;
    int mask;

    for (i = 0, node = w->act; i < count && node; i++, node = node->link) {
        target = node->obj;
        mask = lock_obj_state_masks[(target->flags & LOCK_STATE) >> 3];
        if (node >= &w->slot[256])
            for (;;)
                ;
        if (target == 0)
            for (;;)
                ;
        if ((w->state & mask) && node->timer == 0)
            return node;
    }
    return 0;
}

/*
 * Button held: a lock every lockon_interval frames while free locks stay above lockon_free_min.
 * Sends message 12 with the target and lock number.
 */
/* 0x001291e8 */
void lockon_confirm_step(LockMgr *w)
{
    struct {
        LockObj *o;
        int pad[4];
        int count;
    } ev;
    LockNode *node;
    LockNode *head;
    LockNode *tail;
    LockObj *target;

    if (!(w->state & 0xc)) {
        w->lock_delay = 0;
        return;
    }
    if (w->lock_delay) {
        w->lock_delay--;
        return;
    }
    if (lockon_free_min < w->free_locks) {
        node = lockon_find_target(w);
        if (node) {
            target = node->obj;
            head = &w->head;
            tail = head;
            while (tail->next != head)
                tail = tail->next;
            if (!(target->flags & LOCK_LOCKED))
                lockon_list_insert(tail, node);
            target->locks++;
            target->lockable--;
            w->free_locks--;
            target->lock_anim = 0;
            node->timer = interval_mark;
            w->lock_delay = lockon_interval;
            ev.o = target;
            ev.count = lockon_get_lock_count() - 1;
            listener_dispatch(12, &ev);
        }
    }
}

/* Lock time up to life in lock state 16; no longer lockable -> locks given back, off the list. */
/* 0x00129330 */
void lockon_node_update(LockMgr *w, LockNode *node)
{
    LockObj *target = node->obj;
    int fl;

    if (node->timer)
        node->timer--;
    fl = target->flags;
    if ((fl & 0x38) == 0x10) {
        if (target->lock_time < target->life)
            target->lock_time++;
    }
    if ((fl & 0x100) == 0) {
        w->free_locks += target->locks;
        target->lockable = target->life;
        target->locks = 0;
        lockon_list_remove(node);
    }
}

/* Debug: labelled cross at each object ahead. */
/* 0x001293b0 */
void lockon_debug_labels(LockMgr *w)
{
    LockNode *p = w->slot;
    int i;

    for (i = 256; i != 0; i--) {
        LockObj *target = p->obj;

        p++;
        if (target) {
            if (target->key >= 1.0f) {
                debug_draw_cross(lock_obj_debug_labels[(target->flags & 0x7ffff) >> 4], target->scr_x, target->scr_y, 0.5f, 5.0f);
            }
        }
    }
}

/* Debug. */
/* 0x00129468 */
void lockon_debug_area(void)
{
    struct LockonArea *la = &lockon_area;
    float left, top, right, bottom;

    left = floorf(la->left);
    top = floorf(la->top);
    right = floorf(la->right);
    bottom = floorf(la->bottom);
    lockon_draw_line_2d(0x80800000, 0x80800000, 0.95f, left, top, left, bottom);
    lockon_draw_line_2d(0x80800000, 0x80800000, 0.95f, left, bottom, right, bottom);
    lockon_draw_line_2d(0x80800000, 0x80800000, 0.95f, right, bottom, right, top);
    lockon_draw_line_2d(0x80800000, 0x80800000, 0.95f, right, top, left, top);
}

/* Player camera mode, unless hidden. Frame squashed while hiding; full mark (digit 10) at limit. */
/* 0x00129570 */
void lockon_cursor_disp(LockMgr *w)
{
    struct LockonArea *la = &lockon_area;
    float *scr;
    float *uv;
    int st, saved, n;
    float half, sx, sy, scale_y, scale_x, z, tw, th, x, y, hw, hh, ty, fx, fy;

    if (b_cursor_disp == 0)
        return;
    if (camera_get_mode() != 1)
        return;
    if (w->state & 1)
        return;
    half = 0.5f;
    st = RenderSetCurrentDisplayList(13);
    ulGxSetTextureList(system_ptm);
    draw_set_blend(4, 5);
    scr = screen_cfg;
    scale_x = scr[7];
    sx = la->cursor.x;
    sy = la->cursor.y;
    scale_y = scr[8] * (float)(30 - la->hide_cnt) / 30.0f;

    saved = draw_set_test_mode(0x18000);
    z = float_add_bits16(3, 1.0526316f);
    uv = lockon_cursor_uvs[11];
    th = scr[8] * 13.0f;
    tw = scale_x * 13.0f;
    x = floorf(sx);
    hh = th * half;
    hw = tw * half;
    y = floorf(sy);
#ifdef VERSION_US
    tex_rect_draw_rot_pivot(0, 0x80808080, uv, la->rot,
                 x, y, z, tw, th, hw, hh);
#else
    tex_rect_draw_rot_pivot(x, y, z, tw, th, hw, hh,
                 0, 0x80808080, uv, la->rot);
#endif
    z = float_add_bits16(-1, z);
    tw = scale_x * 128.0f;
    th = scale_y * 128.0f;
    tex_rect_draw(11, 0x80808080, sx - tw * half, sy - th * half, z, tw, th);
    z = float_add_bits16(1, z);
    n = lockon_get_lock_count();
    if (n > 0) {
        th = scale_y * 40.0f;
        if (n == lockon_limit)
            n = 10;
        tw = lockon_digit_widths[n] * scale_y;
        ty = sy - th * half;
        fx = floorf(sx - tw * half);
        fy = floorf(ty);
        tex_rect_draw_uv(0, 0x4d808080, lockon_cursor_uvs[n], fx, fy, z, tw, th);
    }
    draw_set_test_mode(saved);
    RenderSetCurrentDisplayList(st);
}

/* Lock states 8 or 24, no fog, unless hidden. Re-projects first while system_flags & 3. */
/* 0x00129890 */
void lockon_disp_targets(LockMgr *w)
{
    LockNode *p;
    int saved, state;
    int i;

    if (w->state & 1)
        return;
    saved = fog_set_enable(0);
    if (system_flags & 3)
        lockon_project_objects(w);
    state = RenderSetCurrentDisplayList(13);
    p = w->slot;
    ulGxSetTextureList(system_ptm);
    draw_set_fc_mode(0x2000);
    draw_set_blend(4, 5);
    for (i = 256; i != 0; i--) {
        LockObj *target = p->obj;

        if (target) {
            if (target->flags & LOCK_LOCKABLE) {
                switch (target->flags & LOCK_STATE) {
                case 16:
                    break;
                case 8:
                case 24:
                    lockon_lock_cursor_draw(target);
                    break;
                }
            }
        }
        p++;
    }
    fog_set_enable(saved);
    RenderSetCurrentDisplayList(state);
}

/* Lock state 4; cursor and special sound 1 when enabled and shown. Sends message 13. */
/* 0x001299e8 */
int lockon_lock_press(LockMgr *w)
{
    int state = w->state;

    if (state & 8)
        return;
    if (state & 4)
        return;
    w->state_set |= 4;
    if ((state & 3) == 0) {
        lockon_lock_cursor_create(w);
        se_special_trigger(1);
    }
    return ((int (*)(int, void *))listener_dispatch)(13, 0); /* steer(cast) */
}

/* Sends message 14 with the lock list. */
/* 0x00129a48 */
void lockon_lock_release(LockMgr *w)
{
    struct {
        int a[4];
        LockNode *n;
    } ev;

    if (w->state & 4) {
        ev.n = &w->head;
        w->state_clear |= 4;
        listener_dispatch(14, &ev);
    }
}

/*
 * game_flags 0x10000 holds locking off. Player status flag 0x10000: auto lock and fire every
 * overdrive_interval frames. Else held lockon_press_delay frames starts, release fires.
 */
/* 0x00129a90 */
void lockon_input_update(LockMgr *w)
{
    int cnt;

    if (game_flags & 0x10000) {
        w->state_clear |= 0xc;
    } else if (player_status_test_flags(0x10000)) {
        lockon_press_cnt = lockon_press_cnt - 1;
        if (lockon_press_cnt > 0)
            goto call88;
        lockon_lock_release(w);
        w->request |= 1;
        lockon_press_cnt = overdrive_interval;
    } else if (pad_main[6] & 4) {
        int lim;

        cnt = lockon_press_cnt + 1;
        lim = lockon_press_delay;
        lockon_press_cnt = cnt;
        if (cnt < lim)
            return;
        lockon_press_cnt = lim;
call88:
        lockon_lock_press(w);
    } else {
        if (lockon_press_cnt != 0) {
            lockon_press_cnt = 0;
            w->request |= 1;
        }
        lockon_lock_release(w);
    }
}

/* Becomes lockon_work (previous kept); 16 free locks; Lockon2 reference; radar; lock messages. */
/* 0x00129b90 */
void lockon_task_init(LockMgr *w)
{
    int *res;

    DEBUG_PRINTF("-----[ lockon_task_init ]----- %08x\n", w);
    do {
        w->prev = lockon_work;
    } while (0);
    lockon_work = w;
    do {
        w->pnb = system_pnb;
        w->ptm = system_ptm;
    } while (0);
    w->count = 0;
    w->act = 0;
    w->act_count = 0;
    w->head.obj = 0;
    w->head.link = 0;
    w->head.timer = 0;
    w->state = 0;
    w->state_set = 0;
    w->state_clear = 0;
    w->request = 0;
    w->head.prev = &w->head;
    w->head.next = &w->head;
    w->free_locks = 16;
    res = &lockon2_work;
    *res = lockon_shared_acquire(w);
    lockon_clear_entries(w);
    lockon_lock_cursor_setup();
    lockon_area_init(&lockon_area);
    lockon_event_register(w);
    lockon_radar3d_create(w);
}

/* Restores the previous lock-on. */
/* 0x00129c68 */
void lockon_task_exit(LockMgr *w)
{
    DEBUG_PRINTF("-----[ lockon_task_exit ]----- %08x\n", w);
    lockon_event_unregister(w);
    lockon_shared_release(lockon2_work);
    lockon_work = w->prev;
}

/* Order: power, area, radar, candidates, lock, targets, button, fire. Hide counter 0..30. */
/* 0x00129cb8 */
void lockon_task_exec(LockMgr *w)
{
    LockNode *head = &w->head;
    LockNode *node;
    struct LockonArea *la;
    int bits;

    lockon_get_lock_count();
    w->power = lockon_power;
    lockon_area_update(w);
    lockon_clear_all(w);
    lockon_project_objects(w);
    lockon_radar_list_build(w);
    lockon_radar_scan(w);
    lockon_confirm_step(w);
    node = w->head.next;
    if (node != head) {
        do {
            lockon_node_update(w, node);
            node = node->next;
        } while (node != head);
    }
    lockon_input_update(w);
    lockon_time_update(w);
    la = &lockon_area;
    if (w->state & 2) {
        if (la->hide_cnt < 30)
            la->hide_cnt++;
    } else {
        if (la->hide_cnt >= 0)
            la->hide_cnt--;
    }
    bits = w->state_set;
    if (bits) {
        w->state_set = 0;
        w->state |= bits;
    }
    bits = w->state_clear;
    if (bits) {
        w->state_clear = 0;
        w->state &= ~bits;
    }
}

/* Debug labels and rectangle with the Info window open. */
/* 0x00129dd8 */
void lockon_task_disp(void *w)
{
    lockon_disp_targets(w);
    lockon_cursor_disp(w);
    if (lockon_info_open) {
        lockon_debug_labels(w);
        lockon_debug_area();
    }
}

/* Work is a LockMgr. */
/* 0x00129e28 */
void *lockon_task_create(void *parent)
{
    return task_create(parent, &lockon_task_desc, 8488);
}

/* 0x00129e50 */
void lockon2_task_disp(void)
{
}

/* Created on first use. Returns its work (the reference count). */
/* 0x00129e58 */
int lockon_shared_acquire(LockMgr *m)
{
    int work;
    int *ref;

    if (lockon2_work == 0) {
        work = task_create(scr_root, &lockon2_task_desc, 4);
    } else {
        work = lockon2_work;
    }
    ref = (int *)work;
    ref[0]++;
    return (int)ref;
}

/* The last one ends it. */
/* 0x00129ea8 */
void lockon_shared_release(int *w)
{
    if (--w[0] == 0) {
        do {
            TASK_FLAGS(w) |= TASK_F_END;
        } while (0);
        lockon2_work = 0;
    }
}

/* Radius 35, screen centre, enabled, shown, unrotated. */
/* 0x00129ed8 */
void lockon_area_init(struct LockonArea *area)
{
    area->radius_init = area->radius = area->radius_prev = 35.0f;
    area->cursor.x = screen_layout[2];
    area->cursor.y = screen_layout[3];
    area->cursor_prev = area->cursor;
    area->hide_cnt = area->disable_od_cnt = area->disable_cnt = area->rot = 0;
}

/* Rectangle from the screen-scaled radius; ring turns two degrees per frame. */
/* 0x00129f28 */
void lockon_area_update(LockMgr *m)
{
    struct LockonArea *la = &lockon_area;
    float *scr = screen_cfg;
    float c = scr[7];
    float s = scr[8];
    float radius = la->radius;
    float x = la->cursor.x;
    float y = la->cursor.y;

    la->cursor_prev.x = x;
    la->cursor_prev.y = y;
    la->radius_prev = radius;
    la->left = x - radius * c;
    la->top = y - radius * s;
    la->right = x + radius * c;
    la->bottom = y + radius * s;
    la->rot = (la->rot + 364) & 0xffff;
}

/* Damage per lock. */
/* 0x00129f98 */
int lockon_get_power(void)
{
    return lockon_power;
}

/* 0x00129fa8 */
void lockon_obj_get_screen_pos(float *target, float *out)
{
    float *p = target + 10;

    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
}

/* View-space positions (flag 0x40) turned back through the camera. */
/* 0x00129fc8 */
void lockon_obj_get_pos(LockObj *target, float *out)
{
    if (target->flags & LOCK_CAM_RELATIVE) {
        ulMatrixApplyPoint(camera_get_inv_mtx(), &target->pos->x, out);
        return;
    }
    out[0] = target->pos->x;
    out[1] = target->pos->y;
    out[2] = target->pos->z;
}

/* Radar and lock-on range. */
/* 0x0012a048 */
void lockon_set_far(float v)
{
    lockon_far = v;
}

/* 0x0012a058 */
float lockon_get_far(void)
{
    return lockon_far;
}

/* Nests: off == 0 adds a disable, else removes one. Lock state 1 while any is held. */
/* 0x0012a068 */
void lockon_set_enable(int off)
{
    LockMgr *w = lockon_work;
    struct LockonArea *la = &lockon_area;

    if (w) {
        if (off) {
            if (la->disable_cnt <= 0)
                goto clear;
            la->disable_cnt--;
        } else {
            la->disable_cnt++;
        }
        if (la->disable_cnt <= 0) {
clear:
            w->state_clear |= 1;
        } else {
            w->state_set |= 1;
        }
    }
}

/* As lockon_set_enable, for hiding (lock state 2). */
/* 0x0012a0c8 */
void lockon_set_enable_od(int off)
{
    LockMgr *w = lockon_work;
    struct LockonArea *la = &lockon_area;

    if (w) {
        if (off) {
            if (la->disable_od_cnt <= 0)
                goto clear;
            la->disable_od_cnt--;
        } else {
            la->disable_od_cnt++;
        }
        if (la->disable_od_cnt <= 0) {
clear:
            w->state_clear |= 2;
        } else {
            w->state_set |= 2;
        }
    }
}

/* 0x0012a128 */
void lockon_set_cursor_pos(float x, float y)
{
    lockon_area.cursor.x = x;
    lockon_area.cursor.y = y;
}

/* 0x0012a140 */
void lockon_get_cursor_pos(float *x, float *y)
{
    *x = lockon_area.cursor.x;
    *y = lockon_area.cursor.y;
}

/* 0 without a lock-on. */
/* 0x0012a160 */
int lockon_get_lock_time(void)
{
    LockMgr *w = lockon_work;

    if (w)
        return w->lock_time;
    return 0;
}

/* 0x0012a180 */
int lockon_get_time_limit(void)
{
    return lockon_time_limit;
}

/* Fraction of lockon_time_limit; 0 without a lock-on. */
/* 0x0012a190 */
float lockon_get_time_rate(void)
{
    LockMgr *w = lockon_work;

    if (w == 0)
        return 0.0f;
    return (float)w->lock_time / (float)lockon_time_limit;
}

/* Lock time reached lockon_time_limit. */
/* 0x0012a1d0 */
int lockon_is_current(void)
{
    LockMgr *w = lockon_work;

    if (w)
        return w->lock_time == lockon_time_limit;
    return 0;
}

/* 0x0012a200 */
int lockon_is_locking(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return w->state & 4;
}

/* 1..16. Returns the previous limit. */
/* 0x0012a230 */
int lockon_set_max_locks(int n)
{
    int old;

    if (n <= 0)
        n = 1;
    if (n > 16)
        n = 16;
    old = lockon_limit;
    lockon_free_min = 16 - n;
    lockon_limit = n;
    return old;
}

/* 0x0012a268 */
int lockon_get_max_locks(void)
{
    return lockon_limit;
}

/* 0 without a live lock-on. */
/* 0x0012a278 */
int lockon_get_count(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return w->count;
}

/* 0 without a live lock-on. */
/* 0x0012a2a8 */
int lockon_get_radar_count(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return w->radar_count;
}

/* 0 without a live lock-on. */
/* 0x0012a2d8 */
int lockon_get_active_count(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return w->act_count;
}

/* 16 less free locks; 0 without a live lock-on. */
/* 0x0012a308 */
int lockon_get_lock_count(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return 16 - w->free_locks;
}

/* 0 without a live lock-on. */
/* 0x0012a340 */
int lockon_get_radar_list(void)
{
    LockMgr *w = lockon_work;

    if (w == 0 || (TASK_FLAGS(w) & TASK_F_END))
        return 0;
    return w->radar_list;
}

/* Life at least 1, exactly 1 with flag 0x8000; lock state 8 unless given. No locks or shots. */
/* 0x0012a370 */
void lockon_obj_init(LockObj *target, void *owner, Vec3 *pos_vec, float *dir, char *label, int fx, int hp, int lock_max, int flags)
{
    if (hp <= 0)
        hp = 1;
    if (lock_max <= 0)
        lock_max = 0;
    if (flags & 0x8000)
        hp = 1;
    target->flags = flags;
    if ((flags & 0x38) == 0)
        target->flags = flags | 8;
    target->owner = owner;
    target->name = label;
    target->pos = pos_vec;
    do {
        target->dir = dir;
    } while (0);
    target->fx_slot = fx;
    target->lock_max = lock_max;
    target->shot_dmg = 0;
    target->life_max = hp;
    target->life = hp;
    target->locks = 0;
    target->shots = 0;
    target->lockable = hp;
}

/* Shown in the debug lists. */
/* 0x0012a3e8 */
void lockon_obj_set_name(int *obj, int name)
{
    obj[6] = name;
}

/* Flags 0x20000 registered, 0x100 lockable. Reports full (256) or duplicate; hangs on bad flags. */
/* 0x0012a3f0 */
void lockon_entry(LockObj *target)
{
    LockMgr *w = lockon_work;
    TaskHdr *task;
    LockNode *p;

    if (w) {
        if (256 <= w->count) {
            ERROR_PRINTF("LockEntry Over!\n");
        } else if (target->flags & LOCK_REGISTERED) {
            task = (TaskHdr *)((char *)target->owner - 0x40);
            ERROR_PRINTF("Lock Double Entry!\n");
            DEBUG_PRINTF("     lock : %08x\n     task : %s\n", target, task->name);
        } else if (target->flags & ~0x7ffff) {
            for (;;)
                ;
        } else {
            target->locks = 0;
            target->shots = 0;
            target->shot_dmg = 0;
            target->flags |= LOCK_REGISTERED | LOCK_LOCKABLE;
            p = w->slot;
            if (p->obj) {
                do
                    p++;
                while (p->obj);
            }
            p->timer = 0;
            p->obj = target;
            p->prev = 0;
            p->next = 0;
            p->link = 0;
            w->count++;
        }
    }
}

/* A locked object gives its locks back first. */
/* 0x0012a520 */
void lockon_remove(LockObj *target)
{
    LockMgr *w = lockon_work;

    if (w && !(TASK_FLAGS(w) & TASK_F_END)) {
        int i = 256;
        LockNode *p = w->slot;

        do {
            if (p->obj == target) {
                int fl = target->flags;

                if (!(fl & 0x20000))
                    return;
                target->flags = fl & 0xfffdfeff;
                if (fl & 0x400) {
                    w->free_locks += target->locks;
                    target->locks = 0;
                    lockon_obj_calc_remaining(target, 0);
                    lockon_list_remove(p);
                }
                w->count--;
                p->obj = 0;
                return;
            }
            p++;
        } while (--i);
    }
}

/* 0 if nothing locked or no lock-on. */
/* 0x0012a5e8 */
LockNode *lockon_list_first(void)
{
    LockMgr *w = lockon_work;

    if (w) {
        LockNode *first = w->head.next;
        if (first != &w->head)
            return first;
    }
    return 0;
}

/* 0 at the end. */
/* 0x0012a610 */
LockNode *lockon_list_next(LockNode *node)
{
    LockMgr *w = lockon_work;

    if (node == 0)
        return 0;
    return node->next == &w->head ? 0 : node->next;
}

/* See lockon_get_interval. */
/* 0x0012a638 */
void lockon_set_interval_table(int table)
{
    lockon_interval_table = table;
}

/* Drops the stage's interval table; lockon_interval applies again. */
/* 0x0012a648 */
void lockon_clear_interval_table(void)
{
    lockon_interval_table = 0;
}

/* From the 15 x 15 table, row cnt - 2; lockon_interval without a table. */
/* 0x0012a658 */
unsigned char lockon_get_interval(int cnt, int idx)
{
    unsigned char *p = lockon_interval_table;
    if (p) {
        int i = (cnt - 2) * 15;
        return p[idx + i];
    }
    return lockon_interval;
}

/* No row selected; task flag 8. */
/* 0x0012a690 */
void lockon_interval_editor_init(LockIntervalEditWork *w)
{
    TASK_FLAGS(w) |= TASK_F_RUN_PAUSED;
    w->row = -1;
    w->col = 0;
}

/* 0x0012a6b0 */
void lockon_interval_editor_release(void)
{
}

/* USB keys: Down/Up row, Left/Right column (0..14), A raises, Z lowers. */
/* 0x0012a6b8 */
void lockon_interval_editor_exec(LockIntervalEditWork *w)
{
    unsigned char *cell;

    if (kbd_state[6] == 81)
        w->row++;
    if (kbd_state[6] == 82)
        w->row--;
    w->row = MIN(MAX(w->row, 0), 14);
    if (kbd_state[6] == 80)
        w->col--;
    if (kbd_state[6] == 79)
        w->col++;
    w->col = MIN(MAX(w->col, 0), 14);
    if (w->row >= 0) {
        cell = lockon_interval_table + w->row * 15 + w->col;
        if (kbd_state[6] == 4)
            (*cell)++;
        if (kbd_state[6] == 29) {
            if (*cell)
                (*cell)--;
        }
    }
}

/* Lower triangle in use; cursor cell green. */
/* 0x0012a7d0 */
void lockon_interval_editor_disp(LockIntervalEditWork *w)
{
    unsigned char *row;
    unsigned char *p;
    int n = 0;
    int row_idx;
    int y = 13;
    int x;
    int col_idx;

    row_idx = 0;
    debug_set_origin(0, 13);
    row = lockon_interval_table;
    do {
        x = 0;
        p = row;
        n++;
        for (col_idx = 0; col_idx < n; col_idx++) {
            if (w->col == col_idx && w->row == row_idx)
                debug_set_color(0x80008000);
            else
                debug_set_color(0x80808080);
            debug_set_origin(x, y);
            debug_printf("%1d", *p++);
            x += 2;
        }
        row += 15;
        y++;
        row_idx++;
    } while (row_idx < 15);
    debug_set_color(0x80606060);
}

/* Opens only when the stage has an interval table. */
/* 0x0012a910 */
void lockon_interval_editor_toggle(void)
{
    if (interval_editor_task) {
        do {
            TASK_FLAGS(interval_editor_task) |= TASK_F_END;
        } while (0);
        interval_editor_task = 0;
    } else if (lockon_work && !(TASK_FLAGS(lockon_work) & TASK_F_END)) {
        if (lockon_interval_table)
            interval_editor_task = task_create(lockon_work, &interval_editor_desc, sizeof(LockIntervalEditWork));
    }
}

/* Messages 11 (hit: lockon_event_sound) and 12 (lock: lockon_event_lock_sound). */
/* 0x0012a990 */
void lockon_event_register(LockMgr *w)
{
    unsigned int i;

    for (i = 0; i < 2; i++)
        w->listeners[i] = listener_add(lockon_event_table[i * 2], lockon_event_table[i * 2 + 1]);
}

/* 0x0012a9f8 */
void lockon_event_unregister(LockMgr *w)
{
    unsigned int i;

    for (i = 0; i < 2; i++)
        listener_remove(lockon_event_table[i * 2], w->listeners[i]);
}

/*
 * Message 11. Not for silent (0x80) or kindless targets. Enemies (kinds 0..31): hit or destroy
 * sound at their screen position; bullets (32..45): hit sound, or destroy sound at position.
 */
/* 0x0012aa58 */
void lockon_event_sound(LockEv *ev)
{
    LockObj *target = ev->obj;
    int n;

    if (!(target->flags & LOCK_SILENT) && target->fx_slot >= 0) {
        n = enemy_status_ent_get_id(target->fx_slot);
        if (n < 46) {
            if (n >= 32) {
                if (target->life <= 0)
                    SoundRhythmSe_BulletPos(n, 1, &target->scr_x);
                else
                    SoundRhythmSe_Bullet(n, 2);
            } else {
                if (target->life <= 0)
                    SoundRhythmSe_EnemyPos(n, 1, &target->scr_x);
                else
                    SoundRhythmSe_EnemyPos(n, 0, &target->scr_x);
            }
        }
    }
}

/* Message 12: sound for the lock number. */
/* 0x0012ab10 */
void lockon_event_lock_sound(int *obj)
{
    SoundRhythmSe_Lockon(obj[5]);
}

/* 0x0012ab18 */
void lockon_info_list_header(int win, int x, int y)
{
    gw_win_text(win, x, y, "            :time  hpr hpe lns lnc lfn lfp cd kind status_____ x     y");
}

/* Lock time, life, lockable life, limit, locks, shots, damage, interval, state, flags, xy, name. */
/* 0x0012ab28 */
void lockon_info_list_row(int win, LockNode *node, int x, int y, int i)
{
    LockObj *target = node->obj;
    char *name = target->name;
    char *st;

    if (name == 0)
        name = "NULL";
    st = bits_to_string(target->flags >> 6, 11, "ECCHSRLPESS", "___________");
    gw_win_printf(win, x, y, "%3d %08x:%5d %3d %3d %3d %3d %3d %3d %2d %4s %s %5d %5d %s",
                 i, target, target->lock_time, target->life, target->lockable, target->lock_max, target->locks, target->shots, target->shot_dmg, node->timer,
                 lock_obj_state_names[(target->flags & LOCK_STATE) >> 3], st,
                 (int)target->scr_x, (int)target->scr_y, name);
}

/* '*' marks targets outside the lock-on rectangle. */
/* 0x0012ac58 */
void lockon_info_list_lockon(int win, int skip)
{
    int sz[2];
    LockMgr *w = lockon_work;
    LockNode *node = &w->head;
    int i = 0;
    int y;

    gw_get_cell_size(&sz[0], &sz[1]);
    lockon_info_list_header(win, 2, 2);
    y = sz[1] + 2;
    node = node->next;
    if (node != &w->head) {
        do {
            if (!lockon_point_on_screen(node->obj->scr_x, node->obj->scr_y))
                gw_win_text(win, 2, y, "*");
            if (skip > 0) {
                skip--;
            } else {
                lockon_info_list_row(win, node, 2, y, i);
                y += sz[1];
            }
            node = node->next;
            i++;
        } while (node != &w->head);
    }
}

/* 0x0012ad50 */
void lockon_info_list_possible(int win, int skip)
{
    int sz[2];
    LockNode *node = lockon_work->act;
    int y;
    int i = 0;

    gw_get_cell_size(&sz[0], &sz[1]);
    lockon_info_list_header(win, 2, 2);
    y = sz[1] + 2;
    if (node) {
        do {
            if (skip > 0) {
                skip--;
            } else {
                lockon_info_list_row(win, node, 2, y, i);
                y += sz[1];
            }
            node = node->link;
            i++;
        } while (node);
    }
}

/* 0x0012ae08 */
void lockon_info_list_entry(int win, int skip)
{
    int sz[2];
    LockNode *node = lockon_work->slot;
    int y;
    int k = 256;
    int i = 0;

    gw_get_cell_size(&sz[0], &sz[1]);
    lockon_info_list_header(win, 2, 2);
    y = sz[1] + 2;
    for (; k != 0; k--, node++) {
        if (node->obj) {
            if (skip > 0) {
                skip--;
            } else {
                lockon_info_list_row(win, node, 2, y, i);
                y += sz[1];
            }
            i++;
        }
    }
}

/* Lock state, request bits, object/radar/candidate/free-lock counts, limit; 0s without lock-on. */
/* 0x0012aef8 */
void lockon_info_text(int win)
{
    int sz[2];
    LockMgr *w = lockon_work;
    int lock, limit, mode, op, entry, radar, possi;
    int x, y;

    if (w == 0) {
        mode = 0;
        op = 0;
        entry = 0;
        radar = 0;
        possi = 0;
        lock = 0;
        limit = 0;
    } else {
        mode = w->state;
        op = w->request;
        entry = w->count;
        limit = lockon_limit;
        radar = w->radar_count;
        possi = w->act_count;
        lock = w->free_locks;
    }
    gw_get_cell_size(&sz[0], &sz[1]);
    gw_win_printf(win, 2, 2, "Mode  %s", bits_to_string(mode, 3, "CLC", "___"));
    y = sz[1] + 2;
    gw_win_printf(win, 2, y, "Op     %s", bits_to_string(op, 2, "BS", "__"));
    gw_win_printf(win, 2, y + sz[1], "Entry %3d", entry);
    x = sz[0] * 10 + 2;
    gw_win_printf(win, x, 2, "Radar %3d", radar);
    y = sz[1] + 2;
    gw_win_printf(win, x, y, "Possi %3d", possi);
    gw_win_printf(win, x, y + sz[1], "Lock  %3d", lock);
    gw_win_printf(win, (sz[0] * 20) | 2, 2, "Limit  %2d", limit);
}

/* 0x0012b0e8 */
void lockon_info_select_mode(int choice, int mode)
{
    lockon_info_list_mode = mode;
}

/* Also sizes the scroll bar. */
/* 0x0012b0f8 */
void lockon_info_list_update(int win)
{
    int sz[4];
    int n = 0;
    LockMgr *w = lockon_work;
    int bar, track, x, rows;

    if (w) {
        gw_window_get_size(win, &sz[0], &sz[1]);
        gw_get_cell_size(&sz[2], &sz[3]);
        rows = (int)gw_scrollbar_get_val(gw_window_get_vscrollbar(win));
        rows = (int)((float)rows / (float)sz[3]);
        switch (lockon_info_list_mode) {
        case 0:
            lockon_info_list_lockon(win, rows);
            n = 16 - w->free_locks;
            break;
        case 1:
            lockon_info_list_possible(win, rows);
            n = w->act_count;
            break;
        case 2:
            lockon_info_list_entry(win, rows);
            n = w->count;
            break;
        }
        bar = gw_window_get_vscrollbar(win);
        track = gw_scrollbar_get_track(bar);
        x = (int)((float)sz[1] / (float)(n * sz[3]) * (float)track);
        if (track < x)
            x = track;
        if (x <= 3)
            x = 4;
        gw_scrollbar_set_pos(bar, x);
        gw_scrollbar_set_max(bar, (float)((n - 1) * sz[3]));
    }
}

/* Scrolls by keys and mouse wheel while active. */
/* 0x0012b290 */
void lockon_info_window_update(int win)
{
    gw_window_size_to_window(lockon_info_list_window, lockon_info_window, 2, 2);
    if (gw_window_has_active(win)) {
        gw_scrollbar_keys(gw_window_get_vscrollbar(lockon_info_list_window));
        gw_scrollbar_wheel(gw_window_get_vscrollbar(lockon_info_list_window));
    }
}

/* List Type selector kept top right. */
/* 0x0012b310 */
void lockon_info_window_layout(int win)
{
    int size[4];

    lockon_info_text((void *)win);
    gw_window_get_size(win, &size[0], &size[1]);
    gw_window_get_size(lockon_info_list_choice, &size[2], &size[3]);
    size[0] -= 4;
    gw_window_set_pos(lockon_info_list_choice, size[0] - size[2], 3);
}

/* 0x0012b380 */
void lockon_info_window_closed(void)
{
    lockon_info_window = 0;
    lockon_info_open = 0;
}

/* List Type selector: LOCKON, POSSIBLE, ENTRY (default ENTRY). */
/* 0x0012b398 */
void lockon_info_window_toggle(void)
{
    float rect[8];
    int sz[2];
    int bar;

    if (lockon_info_window != 0) {
        gw_window_close(lockon_info_window);
        return;
    }
    gw_get_cell_size(&sz[0], &sz[1]);
    lockon_info_window = gw_window_open("Lockon", 95, (int)(screen_layout[6] + 4.0f),
                              (int)((screen_layout[9] - (float)(sz[1] << 3)) - 4.0f), sz[0] * 86, sz[1] << 3, 0);
    if (lockon_info_window == 0)
        return;
    gw_window_set_handlers(lockon_info_window, lockon_info_window_update, lockon_info_window_layout, lockon_info_window_closed);
    gw_window_set_color(lockon_info_window, 0xa0000000);
    gw_window_set_draw_color(lockon_info_window, 0xffffffff);
    lockon_info_list_window = gw_window_open("List", 0x4012c, 2, sz[1] * 3 + 4, 0, 0, lockon_info_window);
    gw_window_size_to_parent(lockon_info_list_window, 2, 2);
    gw_window_set_handlers(lockon_info_list_window, 0, lockon_info_list_update, 0);
    gw_window_set_color(lockon_info_list_window, 0xffffff);
    gw_window_set_draw_color(lockon_info_list_window, 0xffffffff);
    lockon_info_list_choice = gw_choicebox_create("List Type", sz[0] * 2 + 2, 2, sz[0] * 10, sz[1], lockon_info_window);
    gw_choicebox_add_item(lockon_info_list_choice, "LOCKON");
    gw_choicebox_add_item(lockon_info_list_choice, "POSSIBLE");
    gw_choicebox_add_item(lockon_info_list_choice, "ENTRY");
    gw_choicebox_set_sel(lockon_info_list_choice, 2);
    gw_choicebox_set_callback(lockon_info_list_choice, (void (*)(void))lockon_info_select_mode);
    lockon_info_select_mode(lockon_info_list_choice, 2);
    rect[2] = (float)(sz[1] * 10);
    ((int *)rect)[0] = 0;
    ((int *)rect)[1] = 0;
    rect[3] = (float)sz[1];
    rect[4] = (float)sz[1] * 5.0f;
    bar = gw_scrollbar_create(1, rect, 0, 0, 0, 0, lockon_info_list_window);
    gw_window_add_scrollbar(lockon_info_list_window, bar);
    lockon_info_open = 1;
}

static __inline__ void vadd(float *dst, float *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static __inline__ void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void vcopy_lockon_capture_disp(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* Start back on the shot origin. */
/* 0x0012b628 */
void lockon_capture_add_velocity(struct LockonCaptureWork *w)
{
    float vel[3];
    float *origin;
    float *rot;

    player_get_vec(vel);
    vadd(w->pos, vel);
    vadd(w->pos2, vel);
    player_get_shot_origin(w->slot, &origin, &rot);
    vcopy(w->dst, origin);
}

/* One step from the shot origin; 1 on reaching the target. */
/* 0x0012b6e8 */
int lockon_capture_move(LockonCaptureWorkView *w)
{
    float target[3];
    int done;

    lockon_obj_get_pos(w->node->obj, target);
    vcopy(w->pos2, target);
    if (w->move_t >= 1.0f) {
        vcopy(w->pos, target);
        done = 1;
    } else {
        done = 0;
        {
            float *from = w->dst;
            float fx, fy, fz, x, y, z;

            do {
                fz = from[2];
                do { } while (0);
                fx = from[0];
                fy = from[1];
            } while (0);
            do {
                x = target[0];
                y = target[1];
                z = target[2];
            } while (0);
            x -= fx;
            y -= fy;
            z -= fz;
            x *= w->move_t;
            y *= w->move_t;
            z *= w->move_t;
            {
                float *pos = w->pos;

                pos[0] = fx + x;
                pos[1] = fy + y;
                pos[2] = fz + z;
            }
        }
        w->move_t += w->dt;
    }
    return done;
}

/* 0x0012b7d8 */
void lockon_capture_snap(struct VcopyLockonCaptureWork *w)
{
    float target[3];

    lockon_obj_get_pos(w->node->obj, target);
    vcopy(w->pos, target);
    vcopy(w->pos2, target);
}

/* Reaches the target in distance / laser_spd s (max laser_time_max frames). Display list 6. */
/* 0x0012b840 */
void lockon_capture_init(LockonCaptureWorkShotNoView *w, LockonLaserArg *prm)
{
    float pos[3];
    LockObj *target;
    int n;

    w->shot_no = 0;
    w->dmg = 0;
    w->shot_cnt = 0;
    target = *(LockObj **)prm->target;
    w->node = prm->target;
    w->fired = 0;
    w->slot = prm->slot;
    w->hit_next = 0;
    lockon_capture_add_velocity(w);
    vcopy(w->pos, w->dst);
    lockon_obj_get_pos(target, pos);
    n = (int)(ulVec3Distance(pos, w->dst) * FPS / laser_spd);
    if (n >= laser_time_max)
        n = laser_time_max;
    if (n > 0)
        w->dt = 1.0f / (float)n;
    else
        w->dt = 1.0f;
    w->move_t = 0;
    w->alpha = 1.0f;
    w->ang = 0;
    w->rate = 0;
    w->life = (float)target->life;
    if (w->life <= w->move_t)
        w->life = 1.0f;
    w->mode = 1;
    task_set_dlist(w, 6);
}

/* 0x0012b998 */
void lockon_capture_release(void)
{
}

/* Ends once both fades reach zero. */
/* 0x0012b9a0 */
void lockon_capture_decay_exec(LockonCaptureWorkAlphaView *w)
{
    lockon_capture_add_velocity(w);
    w->alpha -= 0.02f;
    w->alpha = (w->alpha >= 0.0f) ? w->alpha : 0.0f;
    w->rate -= 0.05f;
    w->rate = (w->rate >= 0.0f) ? w->rate : 0.0f;
    if (fabsf(w->alpha) < FLT_EPSILON && fabsf(w->rate) < FLT_EPSILON) {
        if (w)
            TASK_FLAGS(w) |= TASK_F_END;
    }
}

/*
 * Ends if target/owner goes or it stops being lockable. States: 0 capturing (0x4000), 1 extend,
 * 2 hold until fired (0x8000), 3 drain life 1/frame, 4 captured (0x10000), fade out.
 */
/* 0x0012ba68 */
void lockon_capture_exec(LockonCaptureWorkView *w)
{
    LockObj *target = w->node->obj;

    if (target == 0) {
        do {
            TASK_FLAGS(w) |= TASK_F_END;
        } while (0);
        return;
    }
    if (target->owner == 0 || (TASK_FLAGS(target->owner) & 1) || !(target->flags & LOCK_LOCKABLE)) {
        target->flags &= ~(LOCK_CAPTURED | LOCK_HELD);
        do {
            TASK_FLAGS(w) |= TASK_F_END;
        } while (0);
        return;
    }
    lockon_capture_add_velocity(w);
    switch (w->state) {
    case 0:
        target->flags |= LOCK_CAPTURING;
        w->state = 1;
    case 1:
        if (lockon_capture_move(w))
            w->state = 2;
        break;
    case 2:
        lockon_capture_snap(w);
        if (w->fired) {
            target->flags = (target->flags & ~LOCK_CAPTURING) | LOCK_HELD;
            w->state = 3;
        }
        break;
    case 3:
        lockon_capture_snap(w);
        w->rate = (w->life - (float)target->life) / w->life;
        if (target->life == 0)
            w->state = 4;
        else
            target->life--;
        break;
    case 4:
        target->flags = (target->flags & ~LOCK_HELD) | LOCK_CAPTURED;
        lockon_obj_end_capture(target, w, w->dmg);
        TASK_OF(w)->exec = lockon_capture_decay_exec;
        break;
    }
}

/* Wide capped ribbon, brighter with progress; two counter-rotating rings on the target. */
/* 0x0012bc00 */
void lockon_capture_disp(LockonCaptureWorkAngleView *w)
{
    struct {
        unsigned int col[3];
        float pad;
        float uv[2];
        float pad2[2];
        float p[6];
    } poly;
    int n, st;
    float alpha;
    unsigned int col;

    draw_set_blend(4, 1);
    draw_set_fc_mode(0x2000);
    n = (int)(float)color_get_a(capture_col1);
    n = n + (int)((float)(255 - n) * w->rate);
    n = (int)((float)n * w->alpha);
    poly.col[1] = (capture_col1 & 0xffffff) | ((n + 1) >> 1) << 24;
    n = (int)((float)color_get_a(capture_col0) * w->alpha);
    poly.col[0] = (capture_col0 & 0xffffff) | ((n + 1) >> 1) << 24;
    alpha = color_get_a(capture_col2);
    poly.uv[0] = capture_w;
    poly.uv[1] = capture_wr;
    vcopy_lockon_capture_disp(&poly.p[0], w->pos);
    vcopy_lockon_capture_disp(&poly.p[3], w->dst);
    n = alpha * w->alpha;
    poly.col[2] = (capture_col2 & 0xffffff) | ((n + 1) >> 1) << 24;
    draw_polyline_wide_capped(poly.col, poly.p, 1);
    ulGxSetTextureList(system_ptm);
    draw_set_fc_mode(0x2000);
    st = fog_set_enable(0);
    w->angle += 182;
    col = (((int)(w->rate * 255.0f) + 1) >> 1) << 24 | 0x808080;
    billboard_draw_rot(17, col, w->angle, w->pos2, 20.0f, 20.0f);
    billboard_draw_rot(17, col, -w->angle, w->pos2, 18.0f, 18.0f);
    fog_set_enable(st);
}

/* 0x0012be68 */
void *lockon_capture_create(int prm)
{
    return task_create_inherit_arg(lockon_work, &lockon_capture_desc, 92, prm);
}

/* Laser types per evolution level: config choice in game type 5, else the standard table. */
/* 0x0012be98 */
void lockon_stage_setup(int col0, int col1, int col2)
{
    lockon_laser2_set_colors(col0, col1, col2);
    lockon_laser3_set_colors(col0, col1, col2);
    lockon_laser4_set_colors(col0, col1, col2);
    ((void (*)(int, int, int))lockon_laser5_set_colors)(col0, col1, col2); /* steer(cast) */
    lockon_laser6_set_colors(col0, col1, col2);
    ((void (*)(int, int, int))lockon_laser_m_set_colors)(col0, col1, col2); /* steer(cast) */
    if (game_type_get() == 5) {
        int idx = ConfigGetSelect(6);

        lockon_laser_create_cur = lockon_laser_create_tables[idx];
    } else {
        lockon_laser_create_cur = lockon_laser_create_table;
    }
}

/* Lock state 16: capture beam; else the evolution level's laser. Task or 0. */
/* 0x0012bf70 */
int lockon_laser_dispatch(LockonLaserArgNodeView *w)
{
    int idx = player_status_get_level();

    switch ((w->node->obj->flags & LOCK_STATE) >> 3) {
    case 1:
    case 3:
        return (*(int (* *(*))(LockonLaserArgNodeView *w))&lockon_laser_create_cur)[idx](w); /* steer(cast) */
    case 2:
        return lockon_capture_create(w);
    }
    return 0;
}

/* Shot pt[0] of pt[1]; mode 0 laser (with sound), 1 capture. Sends message 16. */
/* 0x0012c010 */
void lockon_laser_fire(LockonShotView *shot, unsigned short *pt)
{
    int *parent;
    struct {
        int *a;
        LockonShotView *b;
        int pad[6];
    } ev;

    shot->flags = 1;
    shot->shot_no = pt[0];
    parent = shot->target->parent;
    shot->he = pt[1];
    switch (shot->mode) {
    case 0:
        ((void (*)(int *, LockonShotView *, int))lockon_obj_fire_laser)(parent, shot, shot->dmg); /* steer(cast) */
        SoundRhythmSe_Laser(pt[0], pt[1]);
        break;
    case 1:
        ((void (*)(int *, LockonShotView *, int))lockon_obj_fire_capture)(parent, shot, shot->dmg); /* steer(cast) */
        break;
    }
    ev.a = parent;
    ev.b = shot;
    listener_dispatch(16, &ev);
}

/* From w->start on, within lockon_laser_dist_max; w->start's entry when none nearer. */
/* 0x0012c0c0 */
Ent *lockon_laser_find_nearest(LockonLaserManageWork *w)
{
    float player[4];
    float pos[4];
    Ent *best;
    Ent *p;
    float best_dist;
    int i;

    player_get_pos(player);
    best = &w->ent[w->start];
    best_dist = lockon_laser_dist_max;
    for (i = w->start, p = best; i < w->count; i++, p++) {
        if (p->valid) {
            float dist;

            lockon_obj_get_pos(*p->node, pos);
            dist = vec3_distance_squared(player, pos);
            if (dist < best_dist) {
                best_dist = dist;
                best = p;
            }
        }
    }
    return best;
}

/*
 * Damage per shot: lock-on power with b_l_magni, else 1. Ends at once with nothing to fire, else
 * sends message 15 with the shot count.
 */
/* 0x0012c190 */
void lockon_laser_manage_init(LockonLaserManageWorkView *w, LockArg *prm)
{
    struct {
        int pad[6];
        int count;
    } ev;
    int *ent = prm->ent;
    LockonEnt *out = w->ent;
    int n = 0;
    int i;

    for (i = 16; i != 0; i--) {
        int *work = (int *)ent[2];

        if (work && !(TASK_FLAGS(work) & TASK_F_END)) {
            int *node = (int *)ent[1];
            int *obj = (int *)node[0];

            if (obj) {
                int power;

                out->node = node;
                out->work = work;
                if (b_l_magni)
                    power = ((int (*)(int *))lockon_get_power)(obj); /* steer(cast) */
                else
                    power = 1;
                out->work[1] = power;
                n++;
                lockon_obj_add_shot(obj, power);
                out++;
            }
        }
        ent += 3;
    }
    if (n == 0) {
        do {
            if (w)
                TASK_FLAGS(w) |= TASK_F_END;
        } while (0);
    } else {
        ev.count = n;
        listener_dispatch(15, &ev);
        w->count = n;
        w->start = 0;
        w->wait = 0;
        w->shot_no = 0;
    }
}

/*
 * Fires the nearest shot when the wait runs out, then waits lockon_get_interval eighths of a beat;
 * zero-wait shots go the same frame. Ends when all are fired.
 */
/* 0x0012c2b0 */
void lockon_laser_manage_exec(LockonLaserManageWorkEntView *w)
{
    unsigned short pt[2];
    EntView *p;
    int i;
    int j;
    int k;
    int **node;

    i = w->start;
    p = &w->ent[i];
    for (j = i; j < w->count; j++, p++) {
        int *obj = *p->node;

        if (p->work == 0 || (TASK_FLAGS(p->work) & TASK_F_END) || obj == 0)
            p->work = 0;
    }
    i = w->start;
    for (;;) {
        if (w->ent[i].work == 0) {
            for (;;) {
                i++;
                w->start = i;
                if (i >= w->count)
                    goto done;
                if (w->ent[i].work != 0) {
                    i++;
                    break;
                }
            }
            do { } while (0);
        }
        if (w->wait > 0) {
            w->wait--;
        } else {
            p = lockon_laser_find_nearest(w);
            node = p->node;
            if (p->work == 0 || (TASK_FLAGS(p->work) & TASK_F_END)) {
                p->work = 0;
            } else {
                if ((*node)[4] & 0xfff80000)
                    for (;;)
                        ;
                pt[0] = w->shot_no;
                pt[1] = w->count;
                lockon_laser_fire(p->work, pt);
                k = (int)(beat_clock_get_interval() * 0.125f);
                k *= lockon_get_interval(w->count, w->shot_no++);
                p->work = 0;
                w->wait = k;
            }
        }
        {
            int start = w->start;

            i = start;
            if (start >= w->count)
                break;
        }
        if (w->wait > 0)
            return;
    }
    do { } while (0);
done:
    if (w)
        TASK_FLAGS(w) |= TASK_F_END;
}

/* 0x0012c4a8 */
void *lockon_laser_manage_create(void *parent, int prm)
{
    return task_create_arg(parent, &lockon_laser_manage_desc, 144, prm);
}

/* Flag 0x1000 while shots are in flight. */
/* 0x0012c4d0 */
void lockon_obj_add_shot(LockObj *target, int n)
{
    target->shots++;
    target->shot_dmg += n;
    target->flags |= LOCK_SHOT;
}

/* 0x0012c4f8 */
void lockon_obj_fire_laser(void)
{
    player_status_count_shot(1);
}

/* Marks hit (0x2000), takes dmg, adds dir (if any) to the impact, sends message 11. */
/* 0x0012c500 */
void lockon_obj_hit(LockObj *target, LockNode *node, float *dir, int dmg)
{
    struct {
        LockObj *o;
        LockNode *n;
        float *v;
        int dmg;
        int unk_10;     /* not set here */
    } ev;
    float *p;

    target->flags |= LOCK_HIT;
    target->life -= dmg;
    if (target->life < 0)
        target->life = 0;
    target->shots--;
    target->shot_dmg -= dmg;
    if (target->shots <= 0)
        target->flags &= ~LOCK_SHOT;
    /* steer(alias): both links are stored as ints */
    *(int *)&node->next = (int)target->hit_list;
    *(int *)&target->hit_list = (int)node;
    if (dir) {
        p = &target->imp.v.x;
        p[0] += dir[0];
        p[1] += dir[1];
        p[2] += dir[2];
    }
    player_status_count_hit(1, dmg);
    ev.o = target;
    ev.n = node;
    ev.v = dir;
    ev.dmg = dmg;
    listener_dispatch(11, &ev);
}

/* Without hitting. */
/* 0x0012c608 */
void lockon_obj_remove_shot(LockObj *target, int shot, int n)
{
    target->shots--;
    target->shot_dmg -= n;
    if (target->shots <= 0)
        target->flags &= ~LOCK_SHOT;
}

/* 0x0012c648 */
void lockon_obj_fire_capture(void)
{
    player_status_count_capture(1);
}

/* 0x0012c650 */
void lockon_obj_end_capture(LockObj *target, int shot, int n)
{
    target->shots--;
    target->shot_dmg -= n;
    player_status_count_capture_end(1, n);
}

/* 0x0012c678 */
void lockon_laser2_set_colors(int col0, int col1, int col2)
{
    laser2_cols[0] = col0;
    laser2_cols[1] = col1;
    laser2_cols[2] = col2;
}

/* With its 16-point trail. */
/* 0x0012c690 */
void lockon_laser2_add_velocity(LockonLaser2Work *w)
{
    float vel[3];
    float *p;
    float vx, vy, vz;
    int i;

    player_get_vec(vel);
    vadd(w->pos, vel);
    vadd(w->pos2, vel);
    p = &w->trail[0][0];
    vx = vel[0];
    vy = vel[1];
    vz = vel[2];
    for (i = 16; i != 0; i--) {
        float x, y, z;

        do {
            x = p[0];
            y = p[1];
            z = p[2];
        } while (0);
        p[0] = x + vx;
        p[1] = y + vy;
        p[2] = z + vz;
        p += 3;
    }
}

static inline void cw_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void cw_vsub(float *dst, float *src)
{
    dst[0] -= src[0];
    dst[1] -= src[1];
    dst[2] -= src[2];
}

static inline void cw_vscale(float *dst, float scale)
{
    dst[0] *= scale;
    dst[1] *= scale;
    dst[2] *= scale;
}

/* Shot origin less this frame's player motion; random spread up to 0.5 per axis; speed 1000/60. */
/* 0x0012c768 */
void lockon_laser2_trail_init(LockonLaser2Work *w)
{
    float vel[3];
    float start[3];
    float dir[3];
    float *pa;
    float *pb;
    float *zero;
    int i;

    player_get_vec(vel);
    player_get_shot_origin(w->slot, &pa, &pb);
    cw_vcopy(start, pa);
    cw_vcopy(dir, pb);
    cw_vsub(start, vel);
    cw_vcopy(w->dst, start);
    cw_vcopy(w->pos, start);
    cw_vcopy(w->pos2, start);
    {
        float *src;
        float ax, ay, az;
        float *t;

        do {
            src = start;
            az = src[2];
            do { } while (0);
            ax = src[0];
            ay = src[1];
        } while (0);
        for (i = 16, t = w->trail[0]; i != 0; i--, t += 3) {
            t[0] = ax;
            t[1] = ay;
            t[2] = az;
        }
    }
    zero = w->zero[0];
    for (i = 16; i != 0; i--) {
        zero[0] = 0;
        zero[1] = 0;
        zero[2] = 0;
        zero += 3;
    }
    vec3_normalize(dir, dir);
    dir[0] += RAND_FLOAT_A() + -0.5f;
    dir[1] += RAND_FLOAT_A() + -0.5f;
    dir[2] += RAND_FLOAT_A() + -0.5f;
    cw_vscale(dir, 16.666667f);
    cw_vcopy(w->dir, dir);
}

/* Points 1..14 move by rate toward their neighbours' midpoint; trail[0] -> laser, idx -> start. */
/* 0x0012c998 */
void lockon_laser2_trail_smooth(float rate, LockonLaser2Work *w)
{
    float buf[16][3];
    float *tr = w->trail[0];
    float *inner = w->trail[1];
    float *buf_inner;
    float *src;
    float *dst;
    float mx, my, mz;
    float px, py, pz, qx, qy, qz;
    int i;

    i = 16;
    dst = buf[0];
    src = tr;
    buf_inner = buf[1];
    for (; i != 0; i--) {
        lockon_vcopy(dst, src);
        src += 3;
        dst += 3;
    }
    src = tr;
    src += 6;
    for (i = 14, dst = buf_inner; i != 0; i--) {
        do {
            px = src[-6];
            py = src[-5];
            pz = src[-4];
        } while (0);
        do {
            qx = src[0];
            qy = src[1];
            qz = src[2];
        } while (0);
        mx = px + qx;
        my = py + qy;
        mz = pz + qz;
        mx *= 0.5f;
        my *= 0.5f;
        mz *= 0.5f;
        do {
            qx = dst[0];
            qy = dst[1];
            qz = dst[2];
        } while (0);
        mx -= qx;
        my -= qy;
        mz -= qz;
        mx *= rate;
        my *= rate;
        mz *= rate;
        qx += mx;
        qy += my;
        qz += mz;
        dst[0] = qx;
        dst[1] = qy;
        dst[2] = qz;
        src += 3;
        dst += 3;
    }
    {
        float *ss = buf_inner;

        src = inner;
        for (i = 14; i != 0; i--) {
            lockon_vcopy(src, ss);
            ss += 3;
            src += 3;
        }
    }
    lockon_vcopy(w->trail[0], w->pos);
    lockon_vcopy(w->trail[w->idx], w->dst);
}

/* Frames = distance * 60 / speed; speed += acceleration. */
/* 0x0012caf8 */
void lockon_laser2_calc_steps(LockonLaser2CalcStepsLockonLaser2Work *w)
{
    float target[3];

    lockon_obj_get_pos(*w->target, target);
    w->steps = (int)(ulVec3Distance(target, w->pos) * FPS / w->spd);
    w->spd = w->spd + w->accel;
}

/* Aim point 1 / cnt of the way to the target, laser an easing share to the aim. 1 on arrival. */
/* 0x0012cb68 */
int lockon_laser2_follow(LockonLaser2FollowLockonLaser2Work *w)
{
    float target[3];
    float old_x, old_y, old_z;
    int done;
    int n;
    float t;
    float s;

    lockon_obj_get_pos(*w->target, target);
    do {
        float *p_ = w->pos;
        old_x = p_[0];
        old_y = p_[1];
        old_z = p_[2];
    } while (0);
    n = --w->cnt;
    if (n <= 0) {
        lockon_vcopy(w->cur, target);
        lockon_vcopy(w->pos, target);
        done = 1;
    } else {
        float *cur;
        float cx, cy, cz, px, py, pz, tx, ty, tz;

        done = 0;
        t = 1.0f / (float)n;
        do { cur = w->cur; } while (0);
        do {
            cx = cur[0];
            cy = cur[1];
            cz = cur[2];
        } while (0);
        do {
            tx = target[0];
            ty = target[1];
            tz = target[2];
        } while (0);
        cx += (tx - cx) * t;
        cy += (ty - cy) * t;
        cz += (tz - cz) * t;
        cur[0] = cx;
        cur[1] = cy;
        cur[2] = cz;
        s = w->ease;
        px = w->pos[0];
        py = w->pos[1];
        pz = w->pos[2];
        s = s + (1.0f - s) / (float)n;
        w->ease = s;
        w->pos[0] = px + (cx - px) * s;
        w->pos[1] = py + (cy - py) * s;
        w->pos[2] = pz + (cz - pz) * s;
    }
    w->delta[0] = w->pos[0] - old_x;
    w->delta[1] = w->pos[1] - old_y;
    w->delta[2] = w->pos[2] - old_z;
    return done;
}

/* 0x0012cce8 */
void lockon_laser2_snap(LockonLaser2SnapLockonLaser2Work *w)
{
    float target[3];
    float old_x, old_y, old_z;
    float *p;

    lockon_obj_get_pos(*w->target, target);
    p = w->pos;
    do { old_x = p[0]; } while (0);
    do { old_y = p[1]; } while (0);
    do { old_z = p[2]; } while (0);
    lockon_vcopy(w->prev, target);
    lockon_vcopy(w->pos, target);
    w->delta[0] = w->pos[0] - old_x;
    w->delta[1] = w->pos[1] - old_y;
    w->delta[2] = w->pos[2] - old_z;
}

/* Slot src[1]; acceleration randomised by laser_accel_ratio %; at most laser_time_max frames. */
/* 0x0012cd80 */
void lockon_laser2_init(LockonLaser2InitLockonLaser2Work *w, int *src)
{
    int target;
    int slot;
    int r;

    w->dmg = 0;
    w->fired = 0;
    slot = src[1];
    target = src[0];
    w->shot_no = 0;
    w->shot_cnt = 0;
    w->hit_next = 0;
    w->trail_len = 0;
    w->trail_used = 0;
    w->hit = 0;
    w->target = target;
    w->slot = slot;
    lockon_laser2_trail_init(w);
    w->state = 0;
    w->ease = 0;
    w->glow = 0.7f;
    r = rand_lcg_next_a();
    w->spd = laser_spd;
    w->accel = laser_accel * ((float)r / 32769.0f * laser_accel_ratio * 0.01f + 1.0f);
    lockon_laser2_calc_steps(w);
    if (w->steps >= laser_time_max)
        w->steps = laser_time_max;
    w->mode = 0;
    task_set_dlist(w, 6);
}

/* 0x0012ce80 */
void lockon_laser2_release(void)
{
}

/* Trail shortens a point per frame, then ends. */
/* 0x0012ce88 */
void lockon_laser2_fade_exec(LockonLaser2FadeExecLockonLaser2Work *w)
{
    lockon_laser2_add_velocity(w);
    if (--w->trail_len <= 0) {
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
    w->trail_used = w->trail_len;
}

/*
 * Target lost or unlockable: shot taken back, ends. State 1 homes, 2 rides the target until fired
 * and the trail has run in, then hits and fades. Trail grows to 15; drawn part catches up 1/frame.
 */
/* 0x0012ced8 */
void lockon_laser2_exec(LockonLaser2ExecLockonLaser2Work *w)
{
    float dir[3];
    float *pv;
    int idx;
    LockObj *target = *w->target;

    if (target == 0) {
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    lockon_laser2_add_velocity(w);
    player_get_shot_origin(w->slot, &pv, &idx);
    lockon_vcopy(w->dst, pv);
    switch (w->state) {
    case 0:
        w->state = 1;
    case 1:
        if (lockon_laser2_follow(w))
            w->state = 2;
        lockon_laser2_trail_smooth(laser2_joint_rate, w);
        break;
    case 2:
        lockon_laser2_snap(w);
        lockon_laser2_trail_smooth(laser2_joint_rate, w);
        if (w->hit) {
            lockon_vsub(dir, w->pa, w->pb);
            lockon_obj_hit(target, w, dir, w->dmg);
            TASK_OF(w)->exec = lockon_laser2_fade_exec;
        }
        break;
    }
    if (w->trail_len < 15)
        w->trail_len++;
    if (w->fired != 0 && w->trail_used < w->trail_len) {
        w->trail_used++;
        w->glow = 0.9f;
        if (w->trail_used == w->trail_len)
            w->hit = 1;
    }
}

/* Fired part as a wide ribbon, the rest as thin guide lines. */
/* 0x0012d0d0 */
void lockon_laser2_disp(LockonLaser2DispLockonLaser2Work *w)
{
    int n;
    int line;
    V3 *from;
    V3 *to;

    draw_set_fc_mode(0x2000);
    if (w->used > 0)
        draw_polyline_wide_capped(&laser2_cols, &w->trail[w->total - w->used], w->used);
    n = w->total - w->used;
    if (n > 0) {
        from = w->trail;
        to = from + 1;
        line = line_set_color(0x800040ff);
        do {
            line = line_add_v(line, from, to);
            from++;
            to++;
            n--;
        } while (n != 0);
        line_flush(line);
    }
}

/* 0x0012d1a0 */
void *lockon_laser2_create(int prm)
{
    return task_create_inherit_arg(lockon_work, &lockon_laser2_desc, 496, prm);
}

/* Centre/edge colours from col0, col1; full alphas from col1, col2. */
/* 0x0012d1d0 */
void lockon_laser3_set_colors(int col0, int col1, int col2)
{
    laser3_cols[0] = col0;
    laser3_cols[1] = col1;
    laser3_col0_a = color_get_a(col1);
    laser3_col1_a = color_get_a(col2);
}

/* One of 24 axis orders; per axis a turn at 0.1..0.4 or 1.3..1.5 of the way. */
/* 0x0012d220 */
void lockon_laser3_random_shape(int *w)
{
    unsigned int i;
    float *p;

    i = 0;
    p = (float *)(w + 1);
    w[0] = rand_lcg_next_a() % 24;
    do {
        i++;
        if (rand_lcg_next_a() & 1)
            *p = RAND_FLOAT_A() * 0.3f + 0.1f;
        else
            *p = RAND_FLOAT_A() * 0.20000005f + 1.3f;
        p++;
    } while (i < 3);
}

/* Six axis-parallel legs in lockon_laser3_edge_axes order; seven corners, leg lengths, total. */
/* 0x0012d350 */
void lockon_laser3_split_edges(float *out, LockonLaser3Shape *shape, float *from, float *to)
{
    float ax, ay, az, bx, by, bz;
    float t[3][2];
    float *lp;
    int *tab;
    float *prev;
    float v;
    int k;
    int idx;

    V3_GET(from, ax, ay, az);
    V3_GET(to, bx, by, bz);
    lp = shape->len;
    {
        float dx = bx - ax;
        float dy = by - ay;
        float dz = bz - az;

        t[0][0] = dx * shape->sx;
        t[1][0] = dy * shape->sy;
        t[2][0] = dz * shape->sz;
        t[0][1] = dx - t[0][0];
        t[1][1] = dy - t[1][0];
        t[2][1] = dz - t[2][0];
    }
    shape->total = fabsf(t[0][0]) + fabsf(t[0][1]) + fabsf(t[1][0]) + fabsf(t[1][1]) + fabsf(t[2][0]) + fabsf(t[2][1]);
    out[0] = ax;
    out[1] = ay;
    out[2] = az;
    tab = lockon_laser3_edge_axes + shape->idx * 6;
    k = 6;
    do {
        prev = out;
        idx = *tab++;
        out += 3;
        out[0] = prev[0];
        out[1] = prev[1];
        out[2] = prev[2];
        if (k >= 4)
            v = t[idx][0];
        else
            v = t[idx][1];
        *lp++ = fabsf(v);
        out[idx] += v;
        k--;
    } while (k != 0);
}

/* Returns the leg number: 0 start .. 6 end. */
/* 0x0012d498 */
int lockon_laser3_edge_point(float t, float *p, float *shape, float *out)
{
    float *len;
    float edge_len;
    float rem;
    float ratio;
    int i;
    int k;

    if (t == 1.0f) {
        out[0] = p[18];
        out[1] = p[19];
        out[2] = p[20];
        return 6;
    }
    if (fabsf(t) < FLT_EPSILON) {
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
        return 0;
    }
    rem = shape[11] * t;
    len = shape + 4;
    i = 0;
    for (k = 6; k != 0; k--) {
        if (0.0f < rem) {
            i++;
            edge_len = *len;
            if (rem <= edge_len) {
                float px;
                out[0] = p[0];
                out[1] = p[1];
                out[2] = p[2];
                ratio = 0.0f;
                if (!(fabsf(edge_len) < FLT_EPSILON))
                    ratio = rem / edge_len;
                px = p[0];
                out[0] += (p[3] - px) * ratio;
                out[1] += (p[4] - p[1]) * ratio;
                out[2] += (p[5] - p[2]) * ratio;
                return i;
            }
            rem -= edge_len;
            len++;
        }
        p += 3;
    }
    return i;
}

/* Shot origin and target position. */
/* 0x0012d5e0 */
void lockon_laser3_update_ends(LockonLaser3Work *w)
{
    float *pv;
    int idx;
    float *pos;
    void *obj = *w->target;

    player_get_shot_origin(w->slot, &pv, &idx);
    pos = w->pos;
    pos[0] = pv[0];
    pos[1] = pv[1];
    pos[2] = pv[2];
    lockon_obj_get_pos(obj, w->pos2);
}

/* At most laser_time_max. */
/* 0x0012d650 */
int lockon_laser3_calc_steps(char *w)
{
    float speed = laser_spd;
    int steps = (int)(ulVec3Distance((struct Vec3 *)(w + 124), (struct Vec3 *)(w + 112)) * FPS / speed);

    if (steps >= laser_time_max)
        steps = laser_time_max;
    return steps;
}

/* 0x0012d6b0 */
void lockon_laser3_draw_path(V3 *p, V3 *end, int n, int col)
{
    V3 *to = p + 1;
    int line;
    int i = n - 1;

    line = line_set_color(col);
    if (i > 0) {
        do {
            line = line_add_v(line, p, to);
            p = p + 1;
            to++;
        } while (--i != 0);
    }
    line = line_add_v(line, p, end);
    line_flush(line);
}

/* Corners first .. end - 1, capped both ends. */
/* 0x0012d750 */
void lockon_laser3_draw_segments(V3 *pts, V3 *start, int first, V3 *end_pos, int end)
{
    float seg[6];
    float head[6];
    float tail[6];
    V3 *pt;
    int n;

    n = end - first;
    if (n <= 0) {
        seg[0] = start->x;
        seg[1] = start->y;
        seg[2] = start->z;
        seg[3] = end_pos->x;
        seg[4] = end_pos->y;
        seg[5] = end_pos->z;
        draw_polyline_capped(laser3_cols, seg, 1);
    } else {
        pt = pts + first;
        head[0] = start->x;
        head[1] = start->y;
        head[2] = start->z;
        head[3] = pt->x;
        head[4] = pt->y;
        head[5] = pt->z;
        draw_polyline_capped(laser3_cols, head, 1);
        n--;
        if (n > 0) {
            do {
                draw_polyline_nocap(laser3_cols, &pt->x, 1);
                pt++;
                n--;
            } while (n != 0);
        }
        tail[0] = pt->x;
        tail[1] = pt->y;
        tail[2] = pt->z;
        tail[3] = end_pos->x;
        tail[4] = end_pos->y;
        tail[5] = end_pos->z;
        draw_polyline_capped(laser3_cols, tail, 1);
    }
}

/* Slot src[1]; random shape; guide advance from flight time, beam 0.1. Display list 6. */
/* 0x0012d8b0 */
void lockon_laser3_init(LockonLaser3WorkView *w, int *src)
{
    int target;
    int slot;
    int n;

    w->dmg = 0;
    w->fired = 0;
    slot = src[1];
    target = src[0];
    w->shot_no = 0;
    w->shot_cnt = 0;
    w->hit_next = 0;
    w->target = target;
    w->slot = slot;
    lockon_laser3_random_shape(w->shape);
    lockon_laser3_update_ends(w);
    n = lockon_laser3_calc_steps(w);
    if (n > 0)
        w->head_step = 1.0f / (float)n;
    else
        w->head_step = 1.0f;
    w->mode = 0;
    w->mid_step = 0.1f;
    w->alpha = 1.0f;
    task_set_dlist(w, 6);
}

/* 0x0012d970 */
void lockon_laser3_release(void)
{
}

/* Path moves with the player, tail runs up, fades over 25 frames. */
/* 0x0012d978 */
void lockon_laser3_fade_exec(LockonLaser3FadeExecLockonLaser3Work *w)
{
    float vel[3];
    float *pt;
    int i;

    player_get_vec(vel);
    i = 7;
    pt = w->path;
    do {
        lockon_vadd(pt, vel);
        pt += 3;
        i--;
    } while (i != 0);
    lockon_vadd(w->mid_pos, vel);
    lockon_vadd(w->head_pos, vel);
    w->tail_edge = lockon_laser3_edge_point(w->tail_t * w->head_t, w->path, w->shape, w->tail_pos);
    w->tail_t += 0.02f;
    if (w->tail_t >= 1.0f)
        w->tail_t = 1.0f;
    w->alpha -= 0.04f;
    if (w->alpha <= 0.0f) {
        w->alpha = 0.0f;
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

/*
 * Target lost/unlockable: shot back, fade. State 0 guide to the target; 1 beam follows once fired;
 * 2 hits along the last leg. Path rebuilt every frame.
 */
/* 0x0012dae0 */
void lockon_laser3_exec(LockonLaser3ExecLockonLaser3Work *w)
{
    float dir[3];
    LockObj *target = *w->target;

    if (target == 0) {
        do { TASK_OF(w)->disp = 0; } while (0);
        TASK_OF(w)->flags |= TASK_F_END;
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->exec = lockon_laser3_fade_exec;
        lockon_laser3_fade_exec(w);
        return;
    }
    switch (w->state) {
    case 0:
        w->head_t += w->head_step;
        if (w->head_t >= 1.0f) {
            w->head_t = 1.0f;
            w->state = 1;
        }
        break;
    case 1:
        if (w->mid_t == 1.0f)
            w->state = 2;
        break;
    case 2:
        lockon_vsub(dir, w->last_to, w->last_from);
        vec3_normalize(dir, dir);
        lockon_obj_hit(target, w, dir, w->dmg);
        TASK_OF(w)->exec = lockon_laser3_fade_exec;
        break;
    }
    lockon_laser3_update_ends(w);
    if (w->fired != 0) {
        w->mid_t += w->mid_step;
        if (w->mid_t >= 1.0f)
            w->mid_t = 1.0f;
    }
    lockon_laser3_split_edges(w->path, w->shape, w->pos, w->pos2);
    w->head_edge = lockon_laser3_edge_point(w->head_t, w->path, w->shape, w->head_pos);
    w->tail_edge = lockon_laser3_edge_point(w->tail_t * w->head_t, w->path, w->shape, w->tail_pos);
    w->mid_edge = lockon_laser3_edge_point(w->mid_t * w->head_t, w->path, w->shape, w->mid_pos);
}

/* Guide at full alpha only; beam tail to mid; fogless glows flicker while fading. */
/* 0x0012dd10 */
void lockon_laser3_disp(LockonLaser3DispLockonLaser3Work *w)
{
    V3 *head;
    float x;
    int col;
    int i;
    void *old;

    x = 1.0f;
    if (w->alpha >= x) {
        draw_set_blend(4, 1);
        head = w->head_pos;
        ((void (*)(V3 *, V3 *, int, int, float))lockon_laser3_draw_path)(w->path, head, w->head_edge, 0x5a0040ff, 2.0f); /* steer(cast) */
    } else {
        head = w->head_pos;
    }
    if (w->mid_t > 0.0f) {
        V3 *mid = w->mid_pos;

        draw_set_blend(4, 1);
        laser3_cols[0] = (laser3_cols[0] & 0xffffff) | ((((int)((float)laser3_col0_a * w->alpha) + 1) >> 1) << 24);
        laser3_cols[1] = (laser3_cols[1] & 0xffffff) | ((((int)((float)laser3_col1_a * w->alpha) + 1) >> 1) << 24);
        lockon_laser3_draw_segments(w->path, w->tail_pos, w->tail_edge, mid, w->mid_edge);
    }
    {
        float y = w->alpha * 255.0f;

        if (w->alpha < x)
            y *= RAND_FLOAT_B();
        x = y;
    }
    col = (((int)x + 1) >> 1 << 24) | 0x808080;
    draw_set_blend(4, 1);
    old = fog_set_enable(0);
    ulGxSetTextureList(system_ptm);
    ulGxSelectTexture(ulGxGetTextureList(), 14);
    light_beam_draw_point(col, head, 0, 1.0f);
    ulGxSelectTexture(ulGxGetTextureList(), 13);
    if (w->mid_t > 0.0f) {
        for (i = 0; i < w->mid_edge; i++)
            light_beam_draw_point(col, &w->path[i], 0, 1.5f);
    }
    fog_set_enable(old);
}

/* 0x0012df88 */
void *lockon_laser3_create(int prm)
{
    return task_create_arg(lockon_work, &lockon_laser3_desc, 260, prm);
}

/* Middle colour starts at alpha 64, brightens as the beam flashes. */
/* 0x0012dfb8 */
void lockon_laser4_set_colors(int col0, int col1, int col2)
{
    laser4_col0 = col0;
    laser4_col1 = col1;
    laser4_col2 = col2;
    laser4_col0_a = color_get_a(col0);
    laser4_col1_a = 64;
    laser4_col2_a = color_get_a(col2);
}

/* Start on the shot origin. */
/* 0x0012e018 */
void lockon_laser4_add_velocity(LockonLaser4Work *w)
{
    float vel[3];
    float *pv;
    int idx;

    player_get_vec(vel);
    lockon_vadd(w->pos, vel);
    player_get_shot_origin(w->slot, &pv, &idx);
    lockon_vcopy(w->from, pv);
}

/* Over its flight time; 1 on arrival. */
/* 0x0012e0a8 */
int lockon_laser4_ease(LockonLaser4WorkView *w)
{
    float target[3];
    int done;
    float x, y, z;

    lockon_obj_get_pos(*w->target, target);
    done = 0;
    if (w->cnt <= 0) {
        lockon_vcopy(w->pos, target);
        done = 1;
    } else {
        float t = 1.0f - (float)w->cnt / (float)w->total;
        float *from;
        float fx, fy, fz;

        do {
            from = w->from;
            fz = from[2];
            do { } while (0);
            fx = from[0];
            fy = from[1];
        } while (0);
        do {
            x = target[0];
            y = target[1];
            z = target[2];
        } while (0);
        x -= fx;
        y -= fy;
        z -= fz;
        w->cnt--;
        x *= t;
        y *= t;
        z *= t;
        {
            float *pos = w->pos;

            pos[0] = fx + x;
            pos[1] = fy + y;
            pos[2] = fz + z;
        }
    }
    return done;
}

/* 0x0012e190 */
void lockon_laser4_snap(LockonLaser4SnapLockonLaser4Work *w)
{
    float target[3];

    lockon_obj_get_pos(*w->target, target);
    lockon_vcopy(w->pos, target);
}

/* Straight beam, slot src->slot; flight time from distance (max laser_time_max). Display list 6. */
/* 0x0012e1e0 */
void lockon_laser4_init(LockonLaser4InitLockonLaser4Work *w, LockonLaser4InitLockonLaserArg *src)
{
    float pos[3];
    void *obj;
    int n;
    void **target;
    int slot;

    w->dmg = 0;
    w->fired = 0;
    slot = src->slot;
    target = src->target;
    w->shot_no = 0;
    w->shot_cnt = 0;
    w->hit_next = 0;
    w->grow = 0;
    w->flash = 0;
    obj = *target;
    w->target = (int)target;
    w->slot = slot;
    w->width = 1.0f;
    w->alpha = 1.0f;
    lockon_laser4_add_velocity(w);
    lockon_vcopy(w->pos, w->from);
    lockon_obj_get_pos(obj, pos);
    n = (int)(ulVec3Distance(pos, w->from) * FPS / laser_spd);
    w->cnt = n;
    if (n >= laser_time_max)
        w->cnt = laser_time_max;
    w->total = w->cnt;
    task_set_dlist(w, 6);
}

/* 0x0012e2e0 */
void lockon_laser4_release(void)
{
}

/* Fades over 50 frames, narrowing by laser4_wredu per frame, then ends. */
/* 0x0012e2e8 */
void lockon_laser4_fade_exec(LockonLaser4FadeExecLockonLaser4Work *w)
{
    lockon_laser4_add_velocity(w);
    w->alpha -= 0.02f;
    w->alpha = MAX_LOCKON_LASER4_FADE_EXEC(w->alpha, 0.0f);
    w->width -= laser4_wredu;
    w->width = MAX_LOCKON_LASER4_FADE_EXEC(w->width, 0.0f);
    if (fabsf(w->alpha) < FLT_EPSILON) {
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
}

/*
 * Target gone: ends; unlockable: shot back, fade. State 1 reach, 2 hold until the fired beam grows
 * in (grow to 1 over ten frames), 3 flash and widen, 4 hit along its direction.
 */
/* 0x0012e398 */
void lockon_laser4_exec(LockonLaser4ExecLockonLaser4Work *w)
{
    float dir[3];
    float rate;
    float clamped;
    LockObj *target = *w->target;

    if (target == 0) {
        TASK_OF(w)->disp = 0;
        do { TASK_OF(w)->flags |= TASK_F_END; } while (0);
        lockon_laser4_add_velocity(w);
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->exec = lockon_laser4_fade_exec;
        lockon_laser4_fade_exec(w);
        return;
    }
    lockon_laser4_add_velocity(w);
    switch (w->state) {
    case 0:
        w->state = 1;
    case 1:
        if (lockon_laser4_ease(w))
            w->state = 2;
        break;
    case 2:
        lockon_laser4_snap(w);
        if (1.0f <= w->grow)
            w->state = 3;
        break;
    case 3:
        lockon_laser4_snap(w);
        w->flash += 0.1f;
        w->flash = MIN(w->flash, 1.0f);
        if (w->flash >= 1.0f) {
            w->width = 1.5f;
            w->state = 4;
        }
        break;
    case 4:
        lockon_vsub(dir, w->pos, w->from);
        vec3_normalize(dir, dir);
        lockon_obj_hit(target, w, dir, w->dmg);
        TASK_OF(w)->exec = lockon_laser4_fade_exec;
        break;
    }
    if (w->fired != 0) {
        w->grow += 0.1f;
        w->grow = MIN(w->grow, 1.0f);
    }
}

/* Wide capped ribbon, shot origin to head; middle colour brightens with the flash. */
/* 0x0012e5c8 */
void lockon_laser4_disp(LockonLaser4DispLockonLaser4Work *w)
{
    struct {
        int col[3];
        float pad;
        float uv[2];
        float pad2[2];
        float p[6];
    } poly;
    int n;
    float old;

    draw_set_blend(4, 1);
    n = laser4_col1_a;
    n = n + (int)((float)(255 - n) * w->flash);
    n = (int)((float)n * w->alpha);
    poly.col[1] = (laser4_col1 & 0xffffff) | ((n + 1) >> 1) << 24;
    n = (int)((float)laser4_col0_a * w->alpha);
    poly.col[0] = (laser4_col0 & 0xffffff) | ((n + 1) >> 1) << 24;
    poly.uv[0] = laser4_w * w->width;
    poly.uv[1] = laser4_wr;
    lockon_vcopy(&poly.p[0], w->pos);
    n = (int)((float)laser4_col2_a * w->alpha);
    poly.col[2] = (laser4_col2 & 0xffffff) | ((n + 1) >> 1) << 24;
    lockon_vcopy(&poly.p[3], w->from);
    old = polyline_set_width_min(0.0f);
    draw_polyline_wide_capped(poly.col, poly.p, 1);
    polyline_set_width_min(old);
}

/* 0x0012e758 */
void *lockon_laser4_create(int prm)
{
    return task_create_arg(lockon_work, &lockon_laser4_desc, 80, prm);
}

/* Full alphas of the colours. */
/* 0x0012e788 */
void lockon_laser5_decode_colors(void)
{
    laser5_col0_a = color_get_a(laser5_col0);
    laser5_col1_a = color_get_a(laser5_col1);
}

/* 0x0012e7c8 */
void lockon_laser5_set_colors(int col0, int col1)
{
    laser5_col0 = col0;
    laser5_col1 = col1;
    laser5_col0_a = color_get_a(col0);
    laser5_col1_a = color_get_a(col1);
}

/* With its 16-point trail. */
/* 0x0012e818 */
void lockon_laser5_add_velocity(LockonLaser5Work *w)
{
    float vel[3];
    LockonVec3 *p;
    float vx, vy, vz;
    int i;

    player_get_vec(vel);
    lockon_vadd(w->pos, vel);
    lockon_vadd(w->pos2, vel);
    i = 16;
    p = w->pts;
    vx = vel[0];
    vy = vel[1];
    vz = vel[2];
    do {
        float x, y, z;

        do {
            x = p->x;
            y = p->y;
            z = p->z;
        } while (0);
        p->x = x + vx;
        p->y = y + vy;
        p->z = z + vz;
        p++;
    } while (--i != 0);
}

/* As lockon_laser2_trail_init. */
/* 0x0012e8f0 */
void lockon_laser5_trail_init(LockonLaser5WorkView *w)
{
    float vel[3];
    float start[3];
    float dir[3];
    float *pa;
    float *pb;
    float *zero;
    int i;

    player_get_vec(vel);
    player_get_shot_origin(w->slot, &pa, &pb);
    lockon_vcopy(start, pa);
    lockon_vcopy(dir, pb);
    lockon_vsub(start, start, vel);
    lockon_vcopy(w->dst, start);
    lockon_vcopy(w->pos, start);
    lockon_vcopy(w->pos3, start);
    {
        float *src;
        float ax, ay, az;
        float *t;

        do {
            src = start;
            az = src[2];
            do { } while (0);
            ax = src[0];
            ay = src[1];
        } while (0);
        for (i = 16, t = w->trail[0]; i != 0; i--, t += 3) {
            t[0] = ax;
            t[1] = ay;
            t[2] = az;
        }
    }
    zero = w->zero[0];
    for (i = 16; i != 0; i--) {
        zero[0] = 0;
        zero[1] = 0;
        zero[2] = 0;
        zero += 3;
    }
    vec3_normalize(dir, dir);
    dir[0] += RAND_FLOAT_A() + -0.5f;
    dir[1] += RAND_FLOAT_A() + -0.5f;
    dir[2] += RAND_FLOAT_A() + -0.5f;
    lockon_vscale(dir, 16.666668f);
    lockon_vcopy(w->vel, dir);
}

/* As lockon_laser2_trail_smooth. */
/* 0x0012eb20 */
void lockon_laser5_trail_smooth(float rate, LockonLaser5TrailSmoothLockonLaser5Work *w)
{
    float buf[16][3];
    float *tr = w->trail[0];
    float *inner = w->trail[1];
    float *buf_inner;
    float *src;
    float *dst;
    float mx, my, mz;
    float px, py, pz, qx, qy, qz;
    int i;

    i = 16;
    dst = buf[0];
    src = tr;
    buf_inner = buf[1];
    for (; i != 0; i--) {
        lockon_vcopy(dst, src);
        src += 3;
        dst += 3;
    }
    src = tr;
    src += 6;
    for (i = 14, dst = buf_inner; i != 0; i--) {
        do {
            px = src[-6];
            py = src[-5];
            pz = src[-4];
        } while (0);
        do {
            qx = src[0];
            qy = src[1];
            qz = src[2];
        } while (0);
        mx = px + qx;
        my = py + qy;
        mz = pz + qz;
        mx *= 0.5f;
        my *= 0.5f;
        mz *= 0.5f;
        do {
            qx = dst[0];
            qy = dst[1];
            qz = dst[2];
        } while (0);
        mx -= qx;
        my -= qy;
        mz -= qz;
        mx *= rate;
        my *= rate;
        mz *= rate;
        qx += mx;
        qy += my;
        qz += mz;
        dst[0] = qx;
        dst[1] = qy;
        dst[2] = qz;
        src += 3;
        dst += 3;
    }
    {
        float *ss = buf_inner;

        src = inner;
        for (i = 14; i != 0; i--) {
            lockon_vcopy(src, ss);
            ss += 3;
            src += 3;
        }
    }
    lockon_vcopy(w->trail[0], w->pos);
    lockon_vcopy(w->trail[w->idx], w->dst);
}

/* As lockon_laser2_calc_steps. */
/* 0x0012ec80 */
void lockon_laser5_calc_steps(LockonLaser5CalcStepsLockonLaser5Work *w)
{
    float target[3];

    lockon_obj_get_pos(*w->target, target);
    w->steps = (int)(ulVec3Distance(target, w->pos) * FPS / w->spd);
    w->spd = w->spd + w->acc;
}

/* As lockon_laser2_follow. */
/* 0x0012ecf0 */
int lockon_laser5_follow(LockonLaser5FollowLockonLaser5Work *w)
{
    float target[3];
    float old_x, old_y, old_z;
    int done;
    int n;
    float t;
    float s;

    lockon_obj_get_pos(*w->target, target);
    do {
        float *p_ = w->pos;
        old_x = p_[0];
        old_y = p_[1];
        old_z = p_[2];
    } while (0);
    n = --w->cnt;
    if (n <= 0) {
        lockon_vcopy(w->cur, target);
        lockon_vcopy(w->pos, target);
        done = 1;
    } else {
        float *cur;
        float cx, cy, cz, px, py, pz, tx, ty, tz;

        done = 0;
        t = 1.0f / (float)n;
        do { cur = w->cur; } while (0);
        do {
            cx = cur[0];
            cy = cur[1];
            cz = cur[2];
        } while (0);
        do {
            tx = target[0];
            ty = target[1];
            tz = target[2];
        } while (0);
        cx += (tx - cx) * t;
        cy += (ty - cy) * t;
        cz += (tz - cz) * t;
        cur[0] = cx;
        cur[1] = cy;
        cur[2] = cz;
        s = w->ease;
        px = w->pos[0];
        py = w->pos[1];
        pz = w->pos[2];
        s = s + (1.0f - s) / (float)n;
        w->ease = s;
        w->pos[0] = px + (cx - px) * s;
        w->pos[1] = py + (cy - py) * s;
        w->pos[2] = pz + (cz - pz) * s;
    }
    w->delta[0] = w->pos[0] - old_x;
    w->delta[1] = w->pos[1] - old_y;
    w->delta[2] = w->pos[2] - old_z;
    return done;
}

/* As lockon_laser2_snap. */
/* 0x0012ee70 */
void lockon_laser5_snap(LockonLaser5SnapLockonLaser5Work *w)
{
    float target[3];
    float old_x, old_y, old_z;
    float *p;

    lockon_obj_get_pos(*w->target, target);
    p = w->pos;
    do { old_x = p[0]; } while (0);
    do { old_y = p[1]; } while (0);
    do { old_z = p[2]; } while (0);
    lockon_vcopy(w->prev, target);
    lockon_vcopy(w->pos, target);
    w->delta[0] = w->pos[0] - old_x;
    w->delta[1] = w->pos[1] - old_y;
    w->delta[2] = w->pos[2] - old_z;
}

/* Laser 2 with a glowing bead trail; same acceleration and flight time. Display list 6. */
/* 0x0012ef08 */
void lockon_laser5_init(LockonLaser5InitLockonLaser5Work *w, LockonLaser5InitLockonLaserArg *src)
{
    void *target;
    int slot;
    int r;

    w->dmg = 0;
    w->fired = 0;
    target = src->target;
    slot = src->slot;
    w->shot_no = 0;
    w->shot_cnt = 0;
    w->hit_next = 0;
    w->trail_len = 0;
    w->trail_used = 0;
    w->hit = 0;
    w->target = (int)target;
    w->slot = slot;
    lockon_laser5_trail_init(w);
    w->state = 0;
    w->wire_rate = 1.0f;
    w->ease = 0;
    w->rate = laser5_sr;
    w->glow = 0.7f;
    r = rand_lcg_next_a();
    w->spd = laser_spd;
    w->acc = laser_accel * ((float)r / 32769.0f * laser_accel_ratio * 0.01f + 1.0f);
    lockon_laser5_calc_steps(w);
    if (w->steps >= laser_time_max)
        w->steps = laser_time_max;
    w->mode = 0;
    task_set_dlist(w, 6);
}

/* 0x0012f020 */
void lockon_laser5_release(void)
{
}

/* Guide lines fade by laser5_vwr, beads by laser5_vr per frame; ends when the beads are gone. */
/* 0x0012f028 */
void lockon_laser5_fade_exec(LockonLaser5FadeExecLockonLaser5Work *w)
{
    lockon_laser5_add_velocity(w);
    w->wire_rate -= laser5_vwr;
    if (w->wire_rate <= 0.0f)
        w->wire_rate = 0.0f;
    w->rate -= laser5_vr;
    if (w->rate <= 0.0f) {
        w->rate = 0.0f;
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
}

/* As laser 2; after the hit the beads brighten by laser5_hr per frame. */
/* 0x0012f0a8 */
void lockon_laser5_exec(LockonLaser5ExecLockonLaser5Work *w)
{
    float dir[3];
    float *pv;
    int idx;
    float rate;
    LockObj *target = *w->target;

    if (target == 0) {
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->exec = lockon_laser5_fade_exec;
        lockon_laser5_fade_exec(w);
        return;
    }
    lockon_laser5_add_velocity(w);
    player_get_shot_origin(w->slot, &pv, &idx);
    lockon_vcopy(w->dst, pv);
    switch (w->state) {
    case 0:
        w->state = 1;
    case 1:
        if (lockon_laser5_follow(w))
            w->state = 2;
        break;
    case 2:
        lockon_laser5_snap(w);
        if (w->hit) {
            lockon_vsub(dir, w->pa, w->pb);
            lockon_obj_hit(target, w, dir, w->dmg);
            TASK_OF(w)->exec = lockon_laser5_fade_exec;
        }
        break;
    }
    lockon_laser5_trail_smooth(laser2_joint_rate, w);
    if (w->trail_len < 15)
        w->trail_len++;
    if (w->fired != 0 && w->trail_used < w->trail_len) {
        w->trail_used++;
        w->glow = 0.9f;
        if (w->trail_used == w->trail_len)
            w->hit = 1;
    }
    if (w->hit != 0) {
        float clamped;

        rate = w->rate + laser5_hr;
        w->rate = rate;
        if (rate < 1.0f)
            clamped = rate;
        else
            clamped = 1.0f;
        w->rate = clamped;
    }
}

/* Unfired trail as thin guide lines, fired part as beads. */
/* 0x0012f2e0 */
void lockon_laser5_disp(LockonLaser5DispLockonLaser5Work *w)
{
    int n;
    int line;
    V3 *from;
    V3 *to;
    V3 *p;
    int col;

    draw_set_fc_mode(0x2000);
    if (w->n1 > 0) {
        int k = w->n1;

        from = w->trail;
        to = from + 1;
        line = line_set_color((((int)(w->wire_rate * 255.0f) + 1) >> 1 << 24) | 0x40ff);
        do {
            line = line_add_v(line, from, to);
            from++;
            to++;
            k--;
        } while (k != 0);
        line_flush(line);
    }
    do { } while (0);
    n = w->n2;
    if (n > 0) {
        p = w->trail_end;
        ulGxSetTextureList(system_ptm);
        draw_set_blend(4, 1);
        col = (((int)(w->rate * 255.0f) + 1) >> 1 << 24) | 0x808080;
    loop:
        billboard_draw(10, col, p--, 2.0f, 2.0f);
        n--;
        if (n != 0)
            goto loop;
    }
}

/* 0x0012f410 */
void *lockon_laser5_create(int prm)
{
    return task_create_inherit_arg(lockon_work, &lockon_laser5_desc, 504, prm);
}

/* Stage's third colour. */
/* 0x0012f440 */
void lockon_laser6_set_colors(int col0, int col1, int col2)
{
    laser6_col_r = color_get_r(col2);
    laser6_col_g = color_get_g(col2);
    laser6_col_b = color_get_b(col2);
}

/* laser6_dist from the target, across the view: camera up turned randomly about the view axis. */
/* 0x0012f490 */
void lockon_laser6_random_start(LockonLaser6Work *w)
{
    float pos[3];
    float dir[3];
    float mtx[16];
    float *cam;
    int ang;

    lockon_obj_get_pos(*w->target, pos);
    cam = camera_get_current();
    ang = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f);
    lockon_vsub(dir, cam + 3, cam);
    vec3_normalize(dir, dir);
    matrix_from_axis_angle(mtx, dir, ang);
    ulMatrixApplyVector(mtx, cam + 6, dir);
    lockon_vscale(dir, laser6_dist);
    lockon_vadd3(w->pos, pos, dir);
}

/*
 * Screen quad laser6_lw wide (depth scaled), brightness width. Near-plane ends pulled onto it;
 * clipped to the drawing area; nothing when wholly outside.
 */
/* 0x0012f5f8 */
void lockon_laser6_draw_segment(float width, float *start, float *end)
{
    float va[4];
    float vb[4];
    float scale[4];
    Vtx vt[4];
    float x0, y0, x1, y1;
    float za, zb;
    float half_w;
    float one, zero;
    float sc0, sc1;
    float dx, dy;
    float ex1, ey1;
    int al;
    unsigned int col;

    ulMatrixApplyPoint(ulMatrixCur, start, va);
    ulMatrixApplyPoint(ulMatrixCur, end, vb);
    za = project_point(va, va);
    zb = project_point(vb, vb);
    x0 = va[0];
    y0 = va[1];
    x1 = vb[0];
    y1 = vb[1];
    if (za < 0.0f || 1.0f < za) {
        if (zb < 0.0f)
            return;
        if (1.0f < zb)
            return;
        {
            float iza = 1.0f / za;
            float izb = 1.0f / zb;
            float t;

            t = (1.0f - izb) / (iza - izb);
            va[2] = za = 1.0f;
            x0 = x1 * izb + (x0 * iza - x1 * izb) * t;
            y0 = y1 * izb + (y0 * iza - y1 * izb) * t;
        }
    } else if (zb < 0.0f || 1.0f < zb) {
        float izb = 1.0f / zb;
        float iza = 1.0f / za;
        float t;

        t = (1.0f - iza) / (izb - iza);
        vb[2] = zb = 1.0f;
        x1 = x0 * iza + (x1 * izb - x0 * iza) * t;
        y1 = y0 * iza + (y1 * izb - y0 * iza) * t;
    }
    one = 1.0f;
    zero = 0.0f;
    project_xyz_abs(one, zero, va[2], scale);
    sc0 = scale[0];
    project_xyz_abs(one, zero, vb[2], scale);
    sc1 = scale[0];
    if (x0 <= -704.0f) {
        if (x1 <= -704.0f)
            return;
        y0 = y1 + (y0 - y1) * (-704.0f - x1) / (x0 - x1);
        x0 = -704.0f;
    }
    if (1344.0f <= x0) {
        if (1344.0f <= x1)
            return;
        y0 = y1 + (y0 - y1) * (1344.0f - x1) / (x0 - x1);
        x0 = 1344.0f;
    }
    if (x1 <= -704.0f) {
        y1 = y0 + (y1 - y0) * (-704.0f - x0) / (x1 - x0);
        x1 = -704.0f;
    }
    if (1344.0f <= x1) {
        y1 = y0 + (y1 - y0) * (1344.0f - x0) / (x1 - x0);
        x1 = 1344.0f;
    }
    if (y0 <= -784.0f) {
        if (y1 <= -784.0f)
            return;
        x0 = x1 + (x0 - x1) * (-784.0f - y1) / (y0 - y1);
        y0 = -784.0f;
    }
    if (1264.0f <= y0) {
        if (1264.0f <= y1)
            return;
        x0 = x1 + (x0 - x1) * (1264.0f - y1) / (y0 - y1);
        y0 = 1264.0f;
    }
    if (y1 <= -784.0f) {
        x1 = x0 + (x1 - x0) * (-784.0f - y0) / (y1 - y0);
        y1 = -784.0f;
    }
    if (1264.0f <= y1) {
        x1 = x0 + (x1 - x0) * (1264.0f - y0) / (y1 - y0);
        y1 = 1264.0f;
    }
    dx = x1 - x0;
    dy = y1 - y0;
    half_w = rsqrtf(dy * dy + dx * dx) * 0.5f * laser6_lw;
    dy *= half_w;
    dx *= half_w;
    if (dy < zero) {
        dy = -dy;
        dx = -dx;
    }
    za = float_add_bits16(1, za);
    ey1 = dy * sc1;
    ex1 = dx * sc1;
    zb = float_add_bits16(1, zb);
    dy *= sc0;
    dx *= sc0;
    al = (((int)(width * 255.0f) + 1) >> 1) & 0xff;
    col = (al << 24) | (al << 16) | (al << 8) | al;
    vt[0].col = vt[1].col = vt[2].col = vt[3].col = col;
    vt[0].u = zero;
    vt[0].v = zero;
    vt[1].u = zero;
    vt[1].v = one;
    vt[2].u = one;
    vt[2].v = zero;
    vt[3].u = one;
    vt[3].v = one;
    vt[2].z = vt[0].z = za;
    vt[0].x = x0 - dy;
    vt[0].y = y0 + dx;
    vt[1].x = x1 - ey1;
    vt[1].y = y1 + ex1;
    vt[2].x = x0 + dy;
    vt[2].y = y0 - dx;
    vt[3].x = x1 + ey1;
    vt[3].y = y1 - ex1;
    vt[3].z = vt[1].z = zb;
    nj_tex_strip_draw(vt, 4, 1);
}

/* From a random point beside the target; not drawn yet. */
/* 0x0012fb80 */
void lockon_laser6_init(LockonLaser6WorkView *w, Src *src)
{
    w->shot_no = 0;
    w->dmg = 0;
    w->fired = 0;
    w->shot_cnt = 0;
    w->owner = src->owner;
    w->slot = src->arg;
    w->hit_next = 0;
    lockon_laser6_random_start(w);
    w->alpha = w->tail_t = w->head_t = 0;
}

/* 0x0012fbd8 */
void lockon_laser6_release(void)
{
}

static inline void v3_add(V3 *dst, float *src)
{
    dst->x += src[0];
    dst->y += src[1];
    dst->z += src[2];
}

/* Streak runs past the target by laser6_ls2 and fades by laser6_va per frame; ends when gone. */
/* 0x0012fbe0 */
void lockon_laser6_fade_exec(V3AddLockonLaser6Work *w)
{
    float vel[3];

    player_get_vec(vel);
    v3_add(&w->pa, vel);
    v3_add(&w->pb, vel);
    w->alpha -= laser6_va;
    w->head_t += laser6_ls2;
    w->tail_t += laser6_ls2;
    if (w->alpha <= 0.0f) {
        w->alpha = 0.0f;
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

/*
 * Target lost/unlockable: shot back, ends. State 0 waits to fire; head and tail run to the target
 * (laser6_ls0, laser6_ls1 per frame); laser6_wait frames after the tail arrives it hits and fades.
 */
/* 0x0012fcc8 */
void lockon_laser6_exec(LockonLaser6ExecLockonLaser6Work *w)
{
    float vel[3];
    float dir[3];
    LockObj *target = *w->target;
    float *goal;

    if (target == 0) {
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    goal = w->goal;
    lockon_obj_get_pos(target, goal);
    player_get_vec(vel);
    lockon_vadd(w->pos, vel);
    switch (w->state) {
    case 0:
        if (w->fired != 0) {
            w->state = 1;
            w->alpha = 1.0f;
        }
        break;
    case 1:
        w->head_t += laser6_ls0;
        w->tail_t += laser6_ls1;
        w->head_t = w->head_t < 1.0f ? w->head_t : 1.0f;
        w->tail_t = w->tail_t < 1.0f ? w->tail_t : 1.0f;
        if (w->tail_t == 1.0f)
            w->wait++;
        if (w->wait > laser6_wait) {
            lockon_vsub(dir, w->goal, w->pos);
            vec3_normalize(dir, dir);
            lockon_obj_hit(target, w, dir, w->dmg);
            TASK_OF(w)->exec = lockon_laser6_fade_exec;
            w->alpha = 1.0f;
            w->state++;
        }
        break;
    }
}

/* No fog. Before the hit a thin aiming line; once fired the streak; on-screen target sprite. */
/* 0x0012fee0 */
void lockon_laser6_disp(LockonLaser6DispLockonLaser6Work *w)
{
    float beam[3];
    float dir[3];
    float tip[3];
    float start[4];
    float end[4];
    float back[4];
    float scr[3];
    void *old;
    int line;
    float len_fwd, len_back, a, b;
    float wa_x, wa_y, wa_z, fwd_x, fwd_y, fwd_z, wb_x, wb_y, wb_z;
    float z;
    int col;

    old = fog_set_enable(0);
    lockon_vsub(beam, w->goal, w->pos);
    vec3_normalize(dir, beam);
    if (w->state < 2) {
        draw_set_blend(4, 5);
        line = line_set_translucent(1);
        line = ((int (*)(int, int, int, float *, float, float *))line_add_v_rgb)(line, laser6_scol0, laser6_scol1, w->pos, laser6_sw, w->goal); /* steer(cast) */
        lockon_vadd3(tip, beam, w->goal);
        line = ((int (*)(int, int, int, float *, float, float *))line_add_v_rgb)(line, laser6_scol0, laser6_scol1, tip, laser6_sw, w->goal); /* steer(cast) */
        line_end(line);
    }
    if (w->state > 0) {
        len_fwd = laser6_ll * 0.5f;
        len_back = laser6_ll * -0.5f;
        a = w->head_t;
        b = w->tail_t;
        wa_y = beam[1] * a;
        wa_x = beam[0] * a;
        fwd_x = dir[0] * len_fwd;
        back[0] = dir[0] * len_back;
        wa_z = beam[2] * a;
        fwd_y = dir[1] * len_fwd;
        back[1] = dir[1] * len_back;
        fwd_z = dir[2] * len_fwd;
        back[2] = dir[2] * len_back;
        start[0] = wa_x + fwd_x;
        wb_x = beam[0] * b;
        start[1] = wa_y + fwd_y;
        start[2] = wa_z + fwd_z;
        wb_y = beam[1] * b;
        end[0] = wb_x + back[0];
        end[1] = wb_y + back[1];
        wb_z = beam[2] * b;
        end[2] = wb_z + back[2];
        lockon_vadd(start, w->pos);
        lockon_vadd(end, w->pos);
        draw_set_blend(1, 1);
        ulGxSetTextureList(system_ptm);
        ulGxSelectTexture(ulGxGetTextureList(), 15);
        psGsSetZMask(1);
        lockon_laser6_draw_segment(w->alpha, start, end);
        psGsSetZMask(0);
        draw_set_blend(4, 1);
        col = (((int)(w->alpha * 255.0f) + 1) >> 1 << 24) | 0x808080;
        ulMatrixApplyPoint(ulMatrixCur, w->goal, scr);
        z = project_point(scr, scr);
        if (scr[0] >= -704.0f && scr[0] <= 1344.0f && scr[1] >= -784.0f && scr[1] <= 1264.0f && z > 0.0f && z <= 1.0f)
            tex_rect_draw(10, col, scr[0] - laser6_ps * 0.5f, scr[1] - laser6_ps * 0.5f, 1.0526316f, laser6_ps, laser6_ps);
    }
    fog_set_enable(old);
}

/* 0x00130280 */
void *lockon_laser6_create(int prm)
{
    return task_create_inherit_arg(lockon_work, &lockon_laser6_desc, 72, prm);
}

/* Fixed colours; stage colours ignored. */
/* 0x001302b0 */
void lockon_laser_m_set_colors(void)
{
}

/* Start on the shot origin. */
/* 0x001302b8 */
void lockon_laser_m_update_start(LockonLaserMWork *w)
{
    float *pv;
    int idx;
    float *pos;

    player_get_shot_origin(w->slot, &pv, &idx);
    pos = w->pos;
    pos[0] = pv[0];
    pos[1] = pv[1];
    pos[2] = pv[2];
}

/* Over its flight time; 1 on arrival. */
/* 0x00130308 */
int lockon_laser_m_ease(LockonLaserMWorkView *w)
{
    lockon_obj_get_pos(*w->target, w->goal);
    if (w->cnt <= 0) {
        w->move_t = 1.0f;
        return 1;
    }
    w->move_t = 1.0f - (float)w->cnt-- / (float)w->total;
    return 0;
}

/*
 * Orb with a tail of smaller orbs. Size from laserm_rr_start; flight time from distance at
 * laserm_spdrate * laser speed (at most laser_time_max). Display list 6.
 */
/* 0x00130390 */
void lockon_laser_m_init(LockonLaserMWorkModeView *w, LockonLaserArgView *src)
{
    float pos[3];
    float radius;
    int target;
    int slot;
    void *obj;
    int n;

    w->dmg = 0;
    w->fired = 0;
    radius = laserm_rr_start;
    target = src->target;
    slot = src->slot;
    w->shot_no = 0;
    w->shot_cnt = 0;
    w->hit_next = 0;
    w->move_t = 0;
    obj = *(void **)target;
    w->target = target;
    w->slot = slot;
    w->alpha = 1.0f;
    w->radius = radius;
    lockon_laser_m_update_start(w);
    lockon_obj_get_pos(obj, pos);
    n = (int)(ulVec3Distance(pos, w->pos) * FPS / (laser_spd * laserm_spdrate));
    w->cnt = n;
    if (n >= laser_time_max)
        w->cnt = laser_time_max;
    w->total = w->cnt;
    task_set_dlist(w, 6);
}

/* 0x00130478 */
void lockon_laser_m_release(void)
{
}

/* Fades by laserm_ra_end, size scales by laserm_rr_end_g per frame; ends when gone. */
/* 0x00130480 */
void lockon_laser_m_fade_exec(LockonLaserMWorkView *w)
{
    float vel[3];
    float t;
    float radius;

    player_get_vec(vel);
    lockon_vadd(w->goal, vel);
    lockon_laser_m_update_start(w);
    t = w->alpha - laserm_ra_end;
    radius = w->radius * laserm_rr_end_g;
    w->alpha = t;
    w->radius = radius;
    if (t <= 0.0f) {
        w->alpha = 0.0f;
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

/* Target gone: ends; unlockable: shot back, fade. State 0 wait, 1 grow and fly, 2 hit and fade. */
/* 0x00130530 */
void lockon_laser_m_exec(LockonLaserMWorkDmgView *w)
{
    float dir[3];
    LockObj *target = *w->target;

    if (target == 0) {
        TASK_OF(w)->disp = 0;
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    if (target->owner == 0 || (TASK_OF(target->owner)->flags & 1) || !(target->flags & 0x100)) {
        lockon_obj_remove_shot(target, w, w->dmg);
        TASK_OF(w)->exec = lockon_laser_m_fade_exec;
        lockon_laser_m_fade_exec(w);
        return;
    }
    lockon_laser_m_update_start(w);
    switch (w->state) {
    case 0:
        if (w->fired != 0)
            w->state = 1;
        break;
    case 1:
        w->radius += laserm_rr_grow;
        if (w->radius > 1.0f)
            w->radius = 1.0f;
        if (lockon_laser_m_ease(w))
            w->state = 2;
        break;
    case 2:
        lockon_laser_m_ease(w);
        lockon_vsub(dir, w->goal, w->pos);
        vec3_normalize(dir, dir);
        lockon_obj_hit(target, w, dir, w->dmg);
        TASK_OF(w)->exec = lockon_laser_m_fade_exec;
        break;
    }
}

/* Once fired, three trailing orbs, each laserm_folow back and 20% smaller, alternating colours. */
/* 0x001306d8 */
void lockon_laser_m_disp(LockonLaserMWorkDmgView *w)
{
    float p[3];
    float beam[3];
    int col;
    float frac;
    float sz;
    float t;
    int col_odd;
    int col_even;
    int i;

    col_even = (laserm_col0 & 0xffffff) | ((((int)(w->alpha * 255.0f) + 1) >> 1) << 24);
    col_odd = (laserm_col1 & 0xffffff) | ((((int)(w->alpha * 255.0f) + 1) >> 1) << 24);
    ulGxSetTextureList(system_ptm);
    draw_set_blend(4, 1);
    psGsSetZMask(1);
    lockon_vsub(beam, w->goal, w->pos);
    frac = w->move_t;
    sz = laserm_radius * w->radius;
    p[0] = w->pos[0] + beam[0] * frac;
    p[1] = w->pos[1] + beam[1] * frac;
    p[2] = w->pos[2] + beam[2] * frac;
    billboard_draw(16, col_even, p, sz, sz);
    if (w->fired != 0) {
        for (i = 1; i < 4; i++) {
            t = frac - laserm_folow;
            if (t <= 0.0f)
                t = 0.0f;
            frac = t;
            sz *= 0.8f;
            p[0] = w->pos[0] + beam[0] * frac;
            p[1] = w->pos[1] + beam[1] * frac;
            p[2] = w->pos[2] + beam[2] * frac;
            col = (i & 1) == 0 ? col_even : col_odd;
            billboard_draw(16, col, p, sz, sz);
        }
    }
    psGsSetZMask(0);
}

/* 0x001308e8 */
void *lockon_laser_m_create(int prm)
{
    return task_create_arg(lockon_work, &lockon_laser_m_desc, 76, prm);
}

/* 0x00130918 */
void lockon_lock_cursor_setup(void)
{
}

/* Diamond of half-size sx x sy scaled by t, turned (t - 1) * 4 degrees; white-to-black edges. */
/* 0x00130920 */
void lockon_lock_cursor_draw_box(float cx, float cy, float cz, float t, float sx, float sy)
{
    float sc[2];
    float ax, ay, bx, by;
    int line;

    ulSinCos((int)((t - 1.0f) * 728.0f), &sc[0], &sc[1]);
    sc[0] *= t;
    sc[1] *= t;
    line = line_set_translucent(0);
    ax = sc[0] * screen_cfg[7] * sx;
    by = sc[0] * screen_cfg[8] * sy;
    bx = sc[1] * screen_cfg[7] * sx;
    ay = sc[1] * screen_cfg[8] * sy;
    line = line_add_seg_rgb(line, 0x80ffffff, 0x80000000, cx - ax, cy + ay, cz, cx - bx, cy - by, cz);
    line = line_add_seg_rgb(line, 0x80000000, 0x80ffffff, cx - bx, cy - by, cz, cx + ax, cy - ay, cz);
    line = ((int (*)(int, int, unsigned int, float, float, float, float, float, float))line_add_seg_rgb)(line, 0x80ffffff, 0x80000000, cx + ax, cy - ay, cz, cx + bx, cy + by, cz); /* steer(cast) */
    line = line_add_seg_rgb(line, 0x80000000, 0x80ffffff, cx + bx, cy + by, cz, cx - ax, cy + ay, cz);
    line_end(line);
}

/*
 * Closing box for 15 frames after a lock, then the mark (grey on the lock list, red before); shot
 * power multiplier when >= 2 with b_l_magni_disp. Nothing behind the camera.
 */
/* 0x00130af8 */
void lockon_lock_cursor_draw(LockObj *w)
{
    char buf[16];
    float x;
    float y;
    float width;
    float height;
    float half_h;
    float half_w;
    int on;
    int n;
    unsigned int col;

    if ((w->flags & 0x3c00) == 0) {
        if (w->shots <= 0)
            return;
    }
    if (w->lock_anim < 16) {
        w->lock_anim += system_fps;
        if (w->lock_anim >= 15)
            w->lock_anim = 15;
    }
    if (!(w->key >= 1.0f))
        return;
    on = 1;
    x = w->scr_x;
    y = w->scr_y;
    width = 0.0f;
    if (w->lock_anim < 15) {
        lockon_lock_cursor_draw_box(x, y, 1.0526316f, (float)(15 - w->lock_anim) + 1.0f, 18.5f, 14.5f);
        on = on < w->locks ? on : 0;
    }
    if (on) {
        width = screen_cfg[7] * 37.0f;
        height = screen_cfg[8] * 29.0f;
        half_w = width * 0.5f;
        half_h = height * 0.5f;
        if (w->flags & LOCK_LOCKED) {
            tex_rect_draw_uv(0, 0x4b808080, lock_cursor_uv, floorf(x - half_w), floorf(y - half_h), 1.0526316f, width, height);
        } else {
            tex_rect_draw_uv(0, 0x4b000080, lock_cursor_uv, floorf(x - half_w), floorf(y - half_h), 1.0526316f, width, height);
        }
    }
    if (b_l_magni_disp != 0) {
        switch ((w->flags & LOCK_STATE) >> 3) {
        case 1:
        case 3:
            col = 0x80008000;
            n = ((int (*)(LockObj *))lockon_get_power)(w); /* steer(cast) */
            break;
        case 2:
            col = 0x80800000;
            n = 1;
            break;
        default:
            n = 0;
            col = 0x80000000;
            break;
        }
    } else {
        n = 0;
        col = 0x80000000;
    }
    if (n >= 2) {
        x += width * 0.5f;
        sprintf(buf, str_lock_cursor_count, n);
        draw_set_blend(4, 5);
        font_set(2);
        font_print(x, y, 1.0526316f, col, buf);
    }
}

/* Flash while the button is held; display list 6. */
/* 0x00130dd8 */
void lockon_lock_cursor_init(LockCursorWork *w)
{
    w->held = 1;
    w->alpha = 255;
    w->unk_08 = 0;
    w->scale = 1.0f;
    task_set_dlist(w, 6);
}

/* 0x00130e08 */
void lockon_lock_cursor_release(void)
{
}

/* 0x00130e10 */
void lockon_lock_cursor_update_pos(char *w)
{
    lockon_get_cursor_pos((float *)(w + 16), (float *)(w + 20));
}

/* After release grows 2% and fades 24 per frame, then ends. */
/* 0x00130e20 */
void lockon_lock_cursor_exec(LockCursorWork *w)
{
    switch (w->state) {
    case 0:
        if (w->held == 0)
            w->state = 1;
        lockon_lock_cursor_update_pos(w);
        break;
    case 1:
        w->scale = w->scale * 1.02f;
        w->alpha -= 24;
        if (w->alpha <= 0) {
            w->alpha = 0;
            do {
                TASK_OF(w)->flags |= TASK_F_END;
            } while (0);
        }
        break;
    }
    if (w->held != 0) {
        if (!((*(Pad * *)&pad_main)->player.on & PAD_A)) /* steer(cast) */
            w->held = 0;
    }
}

/* 128 px * scale, no depth test. */
/* 0x00130ed0 */
void lockon_lock_cursor_disp(LockCursorWorkView *w)
{
    int old_test;
    int old_ztest;
    float sx;
    float sy;

    ulGxSetTextureList(system_ptm);
    draw_set_blend(4, 1);
    old_test = draw_set_test_mode(0);
    old_ztest = draw_set_ztest_mode(0x60000);
    sx = screen_cfg[7] * 128.0f * w->scale;
    sy = screen_cfg[8] * 128.0f * w->scale;
    tex_rect_draw(12, (((w->alpha + 1) >> 1) << 24) | 0x808080, w->x - sx * 0.5f, w->y - sy * 0.5f, 1.0526316f, sx, sy);
    draw_set_ztest_mode(old_ztest);
    draw_set_test_mode(old_test);
}

/* 0x00130fc8 */
void *lockon_lock_cursor_create(void *parent)
{
    return task_create(parent, &lock_cursor_desc, 28);
}

/* OK marks unused: returns 0. */
/* 0x00130ff0 */
int lockon_ok_mark_create(void)
{
    return 0;
}

/* overdrive_tgt_c ahead along player + camera angles, up to overdrive_tgt_r random per axis. */
/* 0x00130ff8 */
void lockon_overdrive_calc_dir(float *out)
{
    int cam[3];
    int ang[3];
    float scaled[3];
    float x, y, z;
    float scale, div;

    player_get_rot(cam);
    player_camera_get_angles(ang);
    ang[0] += cam[0];
    ang[1] += cam[1];
    ang[2] += cam[2];
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    ROT_Z(ang[2], x, y);
    ROT_X(ang[0], y, z);
    ROT_Y(ang[1], x, z);
    scale = overdrive_tgt_c;
    out[0] = scaled[0] = x * scale;
    out[1] = scaled[1] = y * scale;
    out[2] = scaled[2] = z * scale;
    div = 32769.0f;
    out[0] += (float)rand_lcg_next_a() / div * (overdrive_tgt_r + overdrive_tgt_r) - overdrive_tgt_r;
    out[1] += (float)rand_lcg_next_a() / div * (overdrive_tgt_r + overdrive_tgt_r) - overdrive_tgt_r;
    out[2] += (float)rand_lcg_next_a() / div * (overdrive_tgt_r + overdrive_tgt_r) - overdrive_tgt_r;
}

/* Radius overdrive_a_r, centre to edge alpha; four fans of three 30-degree sectors from ang. */
/* 0x001311e8 */
void lockon_overdrive_draw_fan(float cx, float cy, float cz, float alpha, int ang)
{
    LockonVtx vtx[8];
    float sn;
    float cs;
    LockonVtx *p;
    int col_center;
    int col_edge;
    float radius;
    int k;
    int a;

    draw_set_blend(4, 1);
    p = vtx;
    for (k = 8; k != 0; k--, p++)
        p->z = cz;
    a = (int)((float)overdrive_a_a0 * alpha);
    col_center = (((a + 1) >> 1) << 24) | 0x808080;
    a = (int)((float)overdrive_a_a1 * alpha);
    col_edge = (((a + 1) >> 1) << 24) | 0x808080;
    radius = overdrive_a_r;
    p = &vtx[0];
    p->col = col_edge;
    p = &vtx[1];
    p->col = col_edge;
    p = &vtx[2];
    p->col = col_center;
    p = &vtx[3];
    p->col = col_edge;
    p = &vtx[4];
    p->col = col_edge;
    ulSinCos(ang, &sn, &cs);
    nj_color_strip_begin(1);
    for (k = 4; k != 0; k--) {
        p = &vtx[0];
        p->x = cx + radius * cs;
        p->y = cy - radius * sn;
        p = &vtx[1];
        ang += 5461;
        ulSinCos(ang, &sn, &cs);
        p->x = cx + radius * cs;
        p->y = cy - radius * sn;
        p = &vtx[2];
        p->x = cx;
        p->y = cy;
        p = &vtx[3];
        ang += 5461;
        ulSinCos(ang, &sn, &cs);
        p->x = cx + radius * cs;
        p->y = cy - radius * sn;
        ang += 5461;
        ulSinCos(ang, &sn, &cs);
        p = &vtx[4];
        p->x = cx + radius * cs;
        p->y = cy - radius * sn;
        nj_color_strip_vertices(vtx, 5);
    }
    nj_color_strip_end();
}

/* Spring (overdrive_ka, overdrive_kv); becomes the player's shot origin. */
/* 0x00131470 */
void lockon_overdrive_update_marker(Marker *w)
{
    float player_pos[3];

    spring3_step(overdrive_ka, overdrive_kv, w->goal, w->pos, w->vel, w->acc);
    player_get_pos(player_pos);
    lockon_vadd3(w->shot_pos, w->pos, player_pos);
    player_set_shot_origin(w->shot_pos, w->shot_dir);
}

/*
 * Player status flag 0x10000 (auto lock and fire), player command 1, stock used, lock-on display
 * hidden, message 17, sound. Display list 6.
 */
/* 0x00131508 */
void lockon_overdrive_init(OverdriveWork *w)
{
    int *p;
    void *od_time;

    player_status_set_flags(0x10000);
    player_command_start(1);
    player_status_use_overdrive();
    lockon_set_enable_od(0);
    od_time = player_status_get_od_time();
    p = w->pos;
    w->cnt = od_time;
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
    lockon_overdrive_calc_dir((char *)w + 52);
    w->alpha = 0;
    listener_dispatch(17, 0);
    SoundRhythmSe_StgCommonStart(0);
    task_set_dlist(w, 6);
}

/* Message 18; undoes lockon_overdrive_init. */
/* 0x00131590 */
void lockon_overdrive_release(void)
{
    SoundRhythmSe_StgCommonStop(0);
    listener_dispatch(18, 0);
    lockon_set_enable_od(1);
    player_command_end(1);
    player_status_clear_flags(0x10000);
}

/* Marker fades over 20 frames, then ends. */
/* 0x001315d0 */
void lockon_overdrive_fade_exec(LockonOverdriveWork *w)
{
    float t;
    float alpha;
    float fade;

    lockon_overdrive_update_marker(w);
    w->fan_ang += overdrive_a_r_s;
    t = w->alpha - 0.05f;
    w->alpha = t;
    alpha = 0.0f;
    if (0.0f <= t)
        alpha = t;
    t = w->fade - 0.05f;
    fade = 0.0f;
    w->alpha = alpha;
    w->fade = t;
    if (0.0f <= t)
        fade = t;
    w->fade = fade;
    if (fade <= 0.0f) {
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

static inline void od_vset(float *dst, float x, float y, float z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

/* New aiming point on every beat-grid hit; fades in over ten frames. */
/* 0x00131680 */
void lockon_overdrive_exec(OdVsetOverdriveWork *w)
{
    float x, y, z;

    if (--w->cnt <= 0)
        TASK_OF(w)->exec = lockon_overdrive_fade_exec;
    w->ang1 += 364;
    w->ang2 += 182;
    w->ang3 += 273;
    x = 0.0f;
    y = x;
    z = -1.0f;
    ROT_X_OD_VSET(w->ang1, y, z);
    ROT_Y_OD_VSET(w->ang2, x, z);
    ROT_Z_OD_VSET(w->ang3, x, y);
    od_vset(w->pos, x, y, z);
    w->rot += 182;
    if (beat_grid_hit(0))
        lockon_overdrive_calc_dir((char *)w + 52);
    lockon_overdrive_update_marker(w);
    w->fade += 0.1f;
    w->fade = (w->fade < 1.0f) ? w->fade : 1.0f;
    w->alpha += 0.1f;
    w->fan_ang += overdrive_a_r_s;
    w->alpha = (w->alpha < 1.0f) ? w->alpha : 1.0f;
}

/* Rotating additive sprite, lens flare, shaded disc. */
/* 0x00131838 */
void lockon_overdrive_disp(OverdriveWorkView *w)
{
    float scr[3];
    float x;
    float y;
    float z;
    int a;

    ulGxSetTextureList(system_ptm);
    draw_set_blend(1, 1);
    a = (((int)(w->alpha * 255.0f) + 1) >> 1) & 0xff;
    billboard_draw_rot(18, (a << 24) | (a << 16) | (a << 8) | a, w->rot, w->pos, overdrive_size_w, overdrive_size_h);
    lens_flare_draw_point(overdrive_flare_r, overdrive_flare_a * w->alpha, w->pos, 0, 0);
    ulMatrixApplyPoint(ulMatrixCur, w->pos, scr);
    project_point(scr, scr);
    x = scr[0];
    y = scr[1];
    z = float_add_bits16(1, 1.0526316f);
    lockon_overdrive_draw_fan(x, y, z, w->alpha, w->fan_ang);
}

/* Under the player, if any. */
/* 0x00131968 */
void *lockon_overdrive_create(void)
{
    void *parent = player_get_work();

    if (parent)
        return task_create(parent, &overdrive_desc, 108);
    return 0;
}

/* Becomes lockon_radar3d_work; system file models; message 19 starts the level text. List 6. */
/* 0x001319a8 */
void lockon_radar3d_init(LockonRadar3dWork *w)
{
    int *hp = &system_pnb;
    int root;
    int clip;
    void *wire;
    char *wire_obj = (char *)w + 92;

    lockon_radar3d_work = w;
    root = *pnb_get_entry(*hp, 1);
    clip = *pnb_get_entry(*hp, 2);
    dc_object_init(w, root, clip, 4096);
    root = *pnb_get_entry(*hp, 0);
    wire = model_get_part_cache(*hp, 0);
    dc_object_init(wire_obj, root, 0, 4096);
    dc_object_set_wire(wire_obj, wire);
    w->listener = listener_add(19, lockon_radar3d_lu_eff_start);
    task_set_dlist(w, 6);
}

/* 0x00131a80 */
void lockon_radar3d_release(int *w)
{
    listener_remove(19, w[46]);
    dc_object_release(w);
    dc_object_release((char *)w + 92);
    lockon_radar3d_work = 0;
}

/* 0x00131ac8 */
void lockon_radar3d_exec(char *w)
{
    dc_object_advance_time(w);
    dc_object_advance_time(w + 92);
}

/* Faint, in the console's player colour, while the player is drawn. */
/* 0x00131af0 */
void lockon_radar3d_disp(char *w)
{
    float player_pos[3];
    float player_rot[4];
    int mtx;

    if (player_is_drawn()) {
        player_get_pos(player_pos);
        player_get_rot(player_rot);
        ulGxSetTextureList(system_ptm);
        draw_set_blend(4, 1);
        draw_set_color((console_get_pcolp() & 0xffffff) | 0x19000000);
        do { ulMatrixPush(); } while (0);
        mtx = ulMatrixCur;
        vu0_matrix_translate(mtx, player_pos[0], player_pos[1], player_pos[2]);
        lockon_rotate_yxz(player_rot);
        dc_object_draw(w);
        dc_object_draw(w + 92);
        ulMatrixPop();
    }
}

/* 0x00131bb0 */
void *lockon_radar3d_create(void *parent)
{
    return task_create(parent, &lockon_radar3d_desc, 188);
}

/* Message 19: text 1 when p[0] < p[1] (level gained), else 0. */
/* 0x00131bd8 */
void lockon_radar3d_lu_eff_start(int *p)
{
    if (lockon_radar3d_work != 0)
        lockon_lu_eff_create(lockon_radar3d_work, p[0] < p[1]);
}

/* First letter 20% toward x, each later one halfway to the one before: a wave. Returns the sum. */
/* 0x00131c08 */
float lockon_lu_eff_smooth_ring(float x, LuEffWork *w)
{
    float (*row)[9] = w->letter;
    float (*p)[9];
    float sum;
    int n;

    row[0][8] = float_lerp(x, row[0][8], 0.2f);
    sum = row[0][8];
    row = &w->letter[w->count];
    p = row - 2;
    n = w->count - 1;
    do {                        /* a count below 2 never ends */
        p[1][8] = float_lerp(p[0][8], p[1][8], 0.5f);
        sum += p[1][8];
        p--;
    } while (--n);
    return sum;
}

/* Circle of radius 3.8 around the player. */
/* 0x00131ca8 */
void lockon_lu_eff_place_letters(LockonLuEffWork *w)
{
    float sc[2];
    LuEffLetter *letter = w->letter;
    float cc = lu_eff_neg_zero;
    float radius;
    float cos_z, sin_r, sin_c, cos_r;
    float z;
    int ga, gs;
    int n = *(int *)((char *)w + 868);

    do {
        radius = 3.8f;
        z = 0.0f;
        do {
            ulSinCos(letter->ang, &sc[0], &sc[1]);
        } while (0);
        cos_r = sc[1] * radius;
        sin_c = sc[0] * cc;
        do { } while (0);
        sin_r = sc[0] * radius;
        cos_z = sc[1] * z;
        radius = sin_c + cos_r;
        letter->x = cos_z + sin_r;
        do { } while (0);
        ga = letter->ang;
        do {
            n--;
        } while (0);
        gs = letter->step;
        do { } while (0);
        letter->z = radius;
        letter->ang = ga + gs;
        letter->y = z;
        letter++;
    } while (n != 0);
}

/* Spread from behind, menu colour, 1.5 degrees per frame, flat. Last character hidden. */
/* 0x00131d68 */
void lockon_lu_eff_init(LuEffWorkView *w, int idx)
{
    char *str = lu_eff_texts[idx];
    LockonLuEffLetter *letter = w->letter;
    int n;
    int cnt;
    int ang;
    int *state;
    int step;

    n = strlen(str) - 1;
    state = game_state;
    ang = ANG_180;
    w->count = n;
    cnt = n;
    do {
        step = (int)DEG_TO_ANG(360.0f / (float)(w->count + 1));
        letter->ch = *str;
        letter->sx = 0.035f;
        str++;
        ang += step;
        letter->angle = ang;
        letter->col = state[775];
        letter->step = -273;
        letter->sy = 0.0f;
        letter++;
    } while (--cnt != 0);
    lockon_lu_eff_place_letters(w);
    w->fade = 0;
}

/* 0x00131e60 */
void lockon_lu_eff_release(void)
{
}

/* Rise while player status flag 0x80000, then sink; ends when flat. */
/* 0x00131e68 */
void lockon_lu_eff_exec(LuEffWorkView *w)
{
    float t;
    float sum;

    if (w->fade == 0) {
        t = 1.0f;
        if (player_status_test_flags(0x80000) == 0)
            w->fade = 1;
    } else {
        t = 0.0f;
    }
    sum = lockon_lu_eff_smooth_ring(t, w);
    if (w->fade != 0) {
        if (sum <= 0.01f)
            TASK_OF(w)->flags |= TASK_F_END;
    }
    lockon_lu_eff_place_letters(w);
}

/* No fog, while the player is drawn. */
/* 0x00131f08 */
void lockon_lu_eff_disp(LuEffWorkDrawView *w)
{
    float player_pos[3];
    float player_rot[4];
    V3 pos;
    V3 scale;
    R3 rot;
    LuEffLetterView *letter;
    int old;
    int n;
    int col;

    if (player_is_drawn()) {
        font_set(3);
        draw_set_blend(4, 1);
        draw_set_fc_mode(0x2000);
        player_get_pos(player_pos);
        player_get_rot(player_rot);
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, player_pos[0], player_pos[1], player_pos[2]);
        } while (0);
        lockon_rotate_yxz(player_rot);
        letter = w->letter;
        old = fog_set_enable(0);
        n = w->count;
        col = game_menu_color;
        do {
            scale.x = letter->sx;
            scale.y = letter->sx * letter->sy;
            scale.z = letter->sx;
            pos.x = letter->x;
            pos.y = letter->y + scale.y * 42.0f * 0.5f;
            pos.z = letter->z;
            rot.x = 0;
            rot.y = letter->ang;
            rot.z = 0;
            n--;
            font_draw_char3d(*((unsigned char *)letter - 4), &pos, &rot, &scale, col);
            letter++;
        } while (n != 0);
        fog_set_enable(old);
        ulMatrixPop();
    }
}

/* 0x00132088 */
void *lockon_lu_eff_create(void *parent, int idx)
{
    return task_create_arg(parent, &lu_eff_desc, 872, idx);
}

/* y, then x, then z. */
/* 0x001320b0 */
void lockon_rotate_yxz(int *p)
{
    vu0_matrix_rotate_y(ulMatrixCur, p[1]);
    vu0_matrix_rotate_x(ulMatrixCur, p[0]);
    vu0_matrix_rotate_z(ulMatrixCur, p[2]);
}

static __inline__ void vlerp(float *dst, float *from, float *to, float t)
{
    float fx = from[0], fy = from[1], fz = from[2];
    float x = to[0] - fx, y = to[1] - fy, z = to[2] - fz;

    x *= t;
    y *= t;
    z *= t;
    dst[0] = fx + x;
    dst[1] = fy + y;
    dst[2] = fz + z;
}
