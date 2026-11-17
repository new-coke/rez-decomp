/*
 * The player: "Player" task carrying the avatar along the camera passes; pad view turning; avatar
 * of the current level (solid and wire models blended, motions on the beat); level changes,
 * damage, death (wire break); effect/shot positions on the model; "Player" debug window.
 */

#include "types_game.h"
#include "task.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "player.h"
#include "debugmenu.h"
#include "system.h"

#define PUT_POS(v) do { float *pv = (v); vu0_matrix_translate(ulMatrixCur, pv[0], pv[1], pv[2]); } while (0)

typedef struct PlayerWireBreakDesc {
    void *buf;
    int timer;
    int grav_x;
    int grav_y;
    int grav_z;
    float drag;
    float spd;
    float rot_range;
    unsigned int col;
    int col_to;
    int blend_src;
    int blend_dst;
    float spd_min;
    char pad_34[12];
} PlayerWireBreakDesc;

typedef struct PlayerWorkHeadView {
    char pad_00[8];
    float x, y;
} PlayerWorkHeadView;

typedef struct PadDataFlagsView {
    char pad_00[28];
    unsigned flags;
} PadDataFlagsView;

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

typedef struct PlayerWorkRatioView {
    char pad_00[8];
    float x, y;
    char pad_10[1092];
    int neck_ang;                /* +1108 */
    float ratio;            /* +1112 */
} PlayerWorkRatioView;

#define WRAP16(v) (((v) & 0x8000) ? ((v) | ~0xffff) : ((v) & 0xffff))

typedef struct PlayerEvalArgs {
    float *out;
    float *rot;
    float *scale;
} PlayerEvalArgs;

typedef struct PlayerPassNodeVaView {
    int state;              /* +0  */
    float dur;              /* +4  */
    float dur2;             /* +8  */
    int cam_intro, cam_loop, roll_intro, roll_loop, view_intro, view_loop;
    float va[3];            /* +36 */
    float vb[3];            /* +48 */
} PlayerPassNodeVaView;

/* 60 bytes */
typedef struct PlayerWorkPassAView {
    char pad_00[1120];
    int pending;            /* +1120 */
    PlayerPassNodeVaView pass_a;          /* +1124 */
    PlayerPassNodeVaView pass_b;          /* +1184 */
    char pad_4dc[4];
    float pass_time;                /* +1248 */
} PlayerWorkPassAView;

/* 60 bytes */
typedef struct PlayerWorkPassClockView {
    char pad_00[1116];
    int inpass;             /* +1116 */
    int pending;            /* +1120 */
    PlayerPassNodeVaView pass_a;         /* +1124 */
    PlayerPassNodeVaView pass_b;         /* +1184 */
    char pad_4dc[4];
    float pass_time;                /* +1248 */
    float pass_clock;               /* +1252 */
} PlayerWorkPassClockView;

typedef struct PlayerCamEnt {
    float pos[3];           /* +0  */
    char pad_0c[12];
    int ang[3];             /* +24 */
    char pad_24[24];
    float vec[3];           /* +60 */
} PlayerCamEnt;
/* steer(alias): the code that matched reads PlayerObj through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerObjView {
    char pad_00[4];
    float off[3];
    char pad_10[12];
    int aoff[3];
    char pad_28[156];
    PlayerCamEnt next_cam[2];
    char pad_154[24];
    int pitch;
    int yaw;
    char pad_174[28];
    int angout[3];
    char pad_19c[28];
    float time;
    int motion_idx;
    struct PlayerMotion *motion;
} PlayerObjView;

/* 72 bytes */

/* steer(alias): the code that matched reads PlayerEvalArgs through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerEvalArgsView {
    void *out;
    void *rot;
    void *scale;
} PlayerEvalArgsView;

typedef struct PlayerWorkPassAStateView {
    char pad_00[1116];
    int inpass;
    char pad_460[4];
    int pass_a_state;
    char pad_468[120];
    float pass_time;                /* +1248 */
} PlayerWorkPassAStateView;

#define V3(o, n) ((PlayerV3 *)((char *)(o) + (n)))

typedef struct PlayerSmooth {
    int state;
    PlayerV3 cur;              /* +4 */
    PlayerV3 dst;              /* +16 */
    IVec3 aoff;                /* +28 */
    IVec3 aim_ang;                /* +40 */
} PlayerSmooth;

typedef struct PlayerWorkLevelView {
    char pad_00[24];
    int no_damage;
    unsigned int level;
    float blend;
    char pad_24[1208];
    int visible;
    char pad_4e0[12];
    int next_mode;
    int phase;              /* +1264 */
} PlayerWorkLevelView;

typedef struct PlayerWorkCountView {
    char pad_00[24];
    int no_damage;
    int level;
    char pad_20[1212];
    int visible;
    char pad_4e0[12];
    int next_mode;
    int phase;              /* +1264 */
    int count;              /* +1268 */
} PlayerWorkCountView;

typedef struct PlayerFxBlk {
    char pad_00[4];
    int used;               /* +4 */
    char pad_08[4];
    void *ents;
    int neff;
    int nshot;
    char pad_18[4];
} PlayerFxBlk;

typedef struct PlayerMsg {
    int old_level;
    int new_level;
} PlayerMsg;

typedef struct PlayerWorkBlkDrawWireView {
    char pad_00[24];
    int no_damage;
    unsigned int level;
    float blend;
    char pad_24[8];
    int blk_draw_wire;
    char pad_30[1044];
    float turn;
    char pad_448[164];
    int next_mode;
    int phase;              /* +1264 */
} PlayerWorkBlkDrawWireView;

typedef struct PlayerPassNodeView {
    int state;
    float dur;
    float dur2;
} PlayerPassNodeView;

typedef struct PlayerWorkPassView {
    char pad_00[1124];
    PlayerPassNodeView pass_a;         /* +1124 */
    char pad_470[112];
    float pass_time;                /* +1248 */
} PlayerWorkPassView;

typedef struct PlayerPassVecs {
    float cur[3];           /* +0  */
    float vec[3];           /* +12 */
    char pad_18[12];
    float loop[3];          /* +36 */
    float noloop[3];        /* +48 */
    char pad_3c[12];
} PlayerPassVecs;

/* 72 bytes */
typedef struct PlayerObjCamView {
    char pad_00[52];
    PlayerPassVecs cam[4];       /* +52 */
} PlayerObjCamView;

typedef struct PlayerWorkPosView {
    int res0;
    int res1;
    PlayerV3 pos;              /* +8 */
    char pad_14[8];
    unsigned int level;
    int blend;
    char pad_24[4];
    int blk_used;
    char pad_2c[524];
    int blk2_used;
    char pad_23c[544];
    int inpass;
    int pending;
    char pad_464[120];
    int visible;
    int pass_time;
    int pass_clock;
    int mode;
    int next_mode;
    int phase;              /* +1264 */
    char pad_4f4[12];
    void *sub;              /* +1280 */
    char pad_504[8];
    int player2_task;
} PlayerWorkPosView;

typedef struct PlayerObjOffView {
    int aim_mode;
    IVec3 off;                /* +4 */
    char pad_10[348];
    PlayerV3 eye;                /* +364 */
    char pad_178[36];
    float radius;
} PlayerObjOffView;

typedef struct PlayerWorkNoDamageOnView {
    char pad_00[20];
    int no_damage_on;
    char pad_18[1256];
    void *sub;              /* +1280 */
} PlayerWorkNoDamageOnView;

typedef struct PlayerBlob {
    char b[144];
} PlayerBlob;

#define WORD_AT(w, o) (*(int *)((char *)(w) + (o)))

typedef struct PlayerWorkDispView {
    char pad_00[4];
    void *obj;              /* +4 */
    char pad_08[1236];
    int visible;
} PlayerWorkDispView;

typedef struct Player2Work {
    char *parent;
} Player2Work;

typedef struct PlayerWorkVelView {
    char pad_00[1092];
    float turn;              /* +1092 */
    float vel;              /* +1096 */
    float acc;              /* +1100 */
} PlayerWorkVelView;

typedef struct PlayerBlend {
    char pad_00[32];
    float blend;                /* +32 */
} PlayerBlend;

typedef struct PlayerPartDesc {
    float speed;            /* +0 */
    int turn_motion;
    int id;                 /* +8 */
    int model_motion[4];              /* +12 */
    int wire_motion[5];              /* +28 [0] doubles as the resource number */
} PlayerPartDesc;

/* steer(alias): the code that matched reads PlayerModelSlot through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerModelSlotFxView {
    int used;
    int id;
    int turn_motion;
    char pad_0c[4];
    char model[92];
    char wire[92];
    char fx[28];
    float time;
    float speed;
} PlayerModelSlotFxView;

/* steer(alias): the code that matched reads PlayerModel through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerModelView {
    int kind;
    int used;
    int draw_wire;
    char pad_0c[12];
    char slot_a[252];
    char slot_b[252];
} PlayerModelView;

typedef struct PlayerPartSub {
    char pad_00[4];
    int root;                 /* +4 */
    char pad_08[52];
    float time;              /* +60 */
    float length;
    float speed;
    char pad_48[20];
} PlayerPartSub;
/* steer(alias): the code that matched reads PlayerModelSlot through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerModelSlotView {
    int used;
    int id;
    struct PlayerMotion *turn_motion;
    struct PlayerMotion *motion;
    PlayerPartSub model;
    PlayerPartSub wire;
    char blk[28];
    float time;
    float speed;
    float pos;
    float rate;
    unsigned int col2;
    unsigned int col;
} PlayerModelSlotView;

/* 92 bytes */

/* 252 bytes */
/* steer(alias): the code that matched reads PlayerModel through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerModelRecView {
    char pad_00[4];
    int used;
    char pad_08[16];
    PlayerModelSlotView rec[2];
} PlayerModelRecView;

typedef struct PlayerTimeRec {
    float vals[32];
} PlayerTimeRec;

typedef struct PlayerWorkPtclView {
    char pad_00[1284];
    char *sub;
} PlayerWorkPtclView;

typedef struct PlayerFxEnt {
    PlayerV3 pos;
    PlayerV3 off;
    char pad_18[16];
    unsigned int flags;     /* +40 */
    int fx;                 /* +44 */
} PlayerFxEnt;

typedef struct PlayerFxBlkView {
    char pad_00[12];
    PlayerFxEnt *ents;         /* +12 */
    int neff;                  /* +16 */
} PlayerFxBlkView;

typedef struct PlayerFxDef {
    int id;
    unsigned int mask;
} PlayerFxDef;

typedef struct PlayerWorkWorldView {
    char pad_00[1284];
    void *world;            /* +1284 */
} PlayerWorkWorldView;

typedef struct PlayerObjSelectView {
    char pad_00[444];
    int motion_idx;                  /* +444 */
    int motion;                  /* +448 */
} PlayerObjSelectView;

typedef struct PlayerObjMotionView {
    char pad_00[440];
    float time;
    int motion_idx;
    void *motion;              /* +448 */
} PlayerObjMotionView;

/* steer(alias): the code that matched reads PlayerEvalArgs through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerEvalArgsScratchView {
    int *out;
    int a1;
    int *scratch;
} PlayerEvalArgsScratchView;

/* steer(alias): the code that matched reads PlayerModel through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerModelEntsView {
    char pad_00[12];
    void *ents;
    int neff;
    int nshot;
    char recs[504];
} PlayerModelEntsView;

/* steer(alias): the code that matched reads PlayerFxEnt through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerFxEntView {
    float pos[3];
    float off[3];
    int pitch;
    int yaw;
    int roll;
    char pad_24[4];
    int id;
    char pad_2c[4];
} PlayerFxEntView;

typedef struct PlayerIVec3View { int a, b, c; } PlayerIVec3View;
typedef struct PlayerQuat { int a, b, c, d; } PlayerQuat;

typedef struct PlayerXformTab {
    char pad_00[4];
    PlayerIVec3View *pos;
    PlayerQuat *rot;
    PlayerIVec3View *scale;
} PlayerXformTab;

typedef struct PlayerModelData { char pad_00[200]; PlayerXformTab tab; } PlayerModelData;
typedef struct PlayerWorkPartView { char pad_00[1108]; int neck_ang; } PlayerWorkPartView;

typedef struct PlayerModelSlotColView {
    int used;
    char pad_04[12];
    char draw16[92];
    char draw108[136];
    int col2;
    int col;
} PlayerModelSlotColView;

typedef struct PlayerModelDrawView { char pad_00[8]; int draw_wire; void *ents; char pad_10[8]; PlayerModelSlotColView slot[2]; } PlayerModelDrawView;

typedef struct PlayerCurve {
    int count;
    void *pos;
    void *rot;
    void *scale;
    void *base_pos;
    float *base_rot;
    void *base_scale;
} PlayerCurve;

typedef struct PlayerCurveSrc { char pad_00[4]; int root; } PlayerCurveSrc;
typedef struct PlayerQuatView { float w, x, y, z; } PlayerQuatView;

/* Always 0 (bit 0 shifted out). */
#define AZ(p) ((((int)(p)) & 1) >> 1)

/* Second state word values (only 2 used). */
typedef enum { PSTATE_0, PSTATE_1, PSTATE_2 } PlyState2;

/* steer(alias): the code that matched reads PlayerFxEnt through these member types; one view
 * shared by those sites (src/game/player.c). */
typedef struct PlayerFxEntFlagsView {
    char pad_00[12];
    char off[12];
    char pad_18[16];
    unsigned int flags;
    char pad_2c[4];
} PlayerFxEntFlagsView;

typedef struct PlayerWorkEntsView {
    char pad_00[48];
    PlayerFxEntFlagsView *ents;
    int neff;
    int nshot;
    char pad_3c[1212];
    unsigned int cmd_flags;
} PlayerWorkEntsView;

typedef struct PlayerCellDim { char pad_00[4]; unsigned int frames; } PlayerCellDim;

typedef struct PlayerWorkInfoView {
    char pad_00[24];
    int no_damage;
    char pad_1c[1228];
    int mode;
    char pad_4ec[4];
    int phase;
} PlayerWorkInfoView;

extern char str_player[];
extern char gw_varedit_root[];
extern char player_vardef[];
extern char player_vardefs[];
extern char player_color_vardef[];
extern char player_color_vardefs[];
extern char player_pnb_file[];
extern char player_tex_file[];
extern int player_wire_ids[];
extern int player_tex;
extern int player_vars_group;
extern void soft_reset(char *name);
extern int gw_varedit_add(char *a, int b, char *c);
extern void gw_varedit_add_table(char *a, int b, char *c);
extern void player_camera_vars_register(void);
extern int model_pnb_load(char *name);
extern int model_load_timed(char *name, int size);
extern void model_pnb_get_wire(int res, int n);
extern void *player_pnb;
extern void model_pnb_unload(void *p);
extern void model_unload_timed(void *p);
extern char str_player_init[];
extern char str_player_exit_null[];
extern char str_player_exit[];
extern void vu0_matrix_translate(int a, float x, float y, float z);
extern void vu0_matrix_rotate_y(int a, int b);
extern char player_obj[];
extern char *player_draw_slot;
extern int camera_get_inv_mtx(void);
extern void vu0_matrix_mul(int a, int b, int c);
extern void wire_break_set_globals(void *a, void *b);
extern void wire_break_task_create(void *list, PlayerWireBreakDesc *d);
extern void ulMatrixPop(void);
extern void player_camera_copy(void *a, void *b);
extern Pad *pad_main;
extern int player_margin_ax;
extern int player_margin_ay;
extern PadSystem pad_system;
extern int player_status_test_flags(int a);
extern int player_status_has_overdrive(void);
extern void lockon_overdrive_create(void);
extern int player_neck_o;
extern const int player_neck_m;
extern float player_neck_r;
extern int player_eye_spd_y;
extern int player_eye_spd_x;
extern int ang_approach(int target, int cur, float t);
extern void player_status_set_flags(int a);
extern char str_player_pass_loop[];
extern PlayerV3 player_cam_pos;
extern PlayerV3 player_cam_dir;
extern int player_cam_roll;
extern PlayerV3 player_view_pos;
extern PlayerV3 player_view_dir;
extern int player_view_roll;
extern int player_p_ang_x;
extern int player_p_ang_y;
extern int player_p_ang_z;
extern float game_pass_time_get(void);
extern void anim_eval_hierarchy(int ctx, void *obj, PlayerEvalArgsView *args, float t);
extern float game_pass_count_spd_get(void);
extern void vec3_approach(PlayerV3 *a, PlayerV3 *b, float rate);
extern unsigned int stage_exit_flags;
extern int player_damage_no_c;
extern void listener_dispatch(int a, int b);
extern void pad_input_disable(int a);
extern void pad_input_enable(int a);
extern void lockon_set_enable(int a);
extern void player_status_clear_items(void);
extern int player_status_clear_flags(int a);
extern char str_player_demo_motion_null[];
extern int player_damage_no_d;
extern int beat_grid_hit(int a);
extern float beat_clock_get_phase(void);
extern int player_status_get_level(void);
extern void SoundRhythmSe_StgCommonStart(int a);
extern void SoundRhythmSe_StgCommonStop(int a);
extern int player_disp_count;
extern void *player_camera_start(void *w);
extern void player_status_set_next_level(unsigned int v);
extern void player_task_end_child(void *p);
extern unsigned int system_flags;
extern void player_status_no_damage_start(void);
extern void player_status_no_damage_tick(void);
extern int player_draw;
extern int player_2nd_draw;
extern int ulGxSetTextureList(void *o);
extern struct TaskDesc player_desc;
extern int scr_root;
extern struct TaskDesc player2_desc;
extern float player_rot_ka;
extern float player_rot_kv;
extern void player_camera_get_angles(void *out);
extern void spring_step_pos(float *pos, float *vel, float *acc, float target, float k, float damp);
extern int player_draw_obj;
extern void model_node_run_default(void *p);
extern int player_disp_shotpos;
extern void psGsSetZMask(int a);
extern int *pnb_get_entry(void *a, int n);
extern void dc_object_init(void *o, int m, int a, int b);
extern void dc_object_set_clip(void *o, int a);
extern void dc_object_set_clip_cursor(void *o, int a, int b, int c);
extern int model_get_part_cache(void *a, int n);
extern void dc_object_set_wire(void *m, int res);
extern int dc_object_child_count(void *m);
extern void dc_object_set_callback(void *m, void (*cb)(void), void *w);
extern unsigned int player_cols[];
extern char *player_part_descs[];
extern int color_get_a(unsigned int c);
extern void dc_object_release(void *p);
extern float beat_clock_get_bar(void);
extern struct TaskDesc player_ptcl_desc;
extern PlayerFxDef player_fx_defs[];
extern char player_obj_pos[];
extern int ptcl_data_get_entry(int a, int b);
extern int ptcl_task_create(void *w, int h, int a, int b);
extern int ptcl_task_create_world(void *w, int h, int a, int b, int c);
extern void ptcl_task_set_src(int fx, void *a, void *b);
extern int player_motion_ids[];
extern int beat_clock_get_haku(void);
extern char str_player_effpos_normal[];
extern char str_player_effpos_wire[];
extern char str_player_shotpos_normal[];
extern char str_player_shotpos_wire[];
extern char str_player_effpos_total[];
extern char str_player_shotpos_total[];
extern char str_player_effect_position[];
extern int dc_object_count_flag(void *m, int mask);
extern void *game_calloc(int size);
extern void heap_free(void *p, int flag);
extern char player_axis_z[];
extern void ulMatrixGetTranslation(int cam, float *out);
extern int debug_draw_cross(int color, float x, float y, float z, float r);
extern void njTranslateEx(void *);
extern void quat_to_matrix(void *);
extern void njScaleEx(void *);
extern void vu0_matrix_rotate_z(int, int);
extern void draw_set_strip_flags(unsigned int, int);
extern void draw_set_override_color(float *);
extern int color_blend(int, int, float);
extern int draw_set_override_flags(int);
extern void dc_object_draw(void *);
extern void draw_set_blend(int, int);
extern void draw_set_color(int);
extern void *game_malloc(int);
extern void *heap_block_tag(void *, const char *, int);
extern char model_wire_buf[];
extern void quat_from_euler(void *, void *);
extern void quat_multiply(void *, void *, void *);
extern PlayerWork *player_work;
extern int player_demo_motion_ids[];
extern char player_zero_vec[];
extern void game_pass_time_reset(void);
extern void game_pass_time_set(float);
extern Vec3 player_obj_frame_pos;
extern IVec3 player_obj_frame_ang;
extern Vec3 player_obj_vec;
extern Vec3 player_obj_vec_old;
extern Vec3 player_obj_accel;
extern Vec3 player_obj_frame_vel;
extern Vec3 player_obj_frame_vel_old;
extern Vec3 player_obj_frame_accel;
extern void player_camera_set_aim(void *, Vec3 *);
extern int player_model_active;
extern char player_model[];
extern const char *player_mode_names[];
extern FlagName player_flag_names[];
extern int player_status_get_flags(void);
extern float vec3_length(float *);
extern void gw_get_cell_size(int *, int *);
extern void gw_win_printf(int, int, int, const char *, ...);
extern int player_info_window;
extern char screen_layout[];
extern int gw_window_close(int);
extern int gw_window_open(const char *, int, int, int, int, int, int);
extern void gw_window_set_handlers(int, int, void (*)(void), int);
extern void vu0_matrix_rotate_x(int, int);
extern void player_apply_matrix();
extern void player_draw_parts();

extern void player_update_camera();
extern void ulMatrixApplyPoint(float *m, float *in, float *out);
extern float vec3_distance_squared(float *a, float *b);

extern int debug_disp_level;
extern int player_disp_axis;
extern int ulMatrixPush(void);
extern void vu0_matrix_translate(int cam, float x, float y, float z);
extern void vu0_matrix_rotate_y(int cam, int v);
extern void debug_draw_box(int color, float size);

/* 0x0014f240 */
int player_return_zero(void)
{
    return 0;
}

/* Once: "Player" soft-reset module, variables, debug menu item, models. */
/* 0x0014f248 */
void player_setup(void)
{
    soft_reset(str_player);
    player_tex = 0;
    player_pnb = 0;
    player_vars_group = gw_varedit_add(gw_varedit_root, 0, player_vardef);
    gw_varedit_add_table(gw_varedit_root, player_vars_group, player_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, player_vars_group, player_color_vardef), player_color_vardefs);
    player_camera_vars_register();
    player_info_menu_add();
    player_model_reset();
    player_load_models();
}

/* Pack and textures once; wire versions of its 11 models. */
/* 0x0014f300 */
int player_load_models(void)
{
    int i;
    int *p;

    if (player_pnb == 0)
        player_pnb = model_pnb_load(player_pnb_file);
    if (player_tex == 0)
        player_tex = model_load_timed(player_tex_file, 0x8000);
    p = player_wire_ids;
    for (i = 11; i != 0; i--)
        model_pnb_get_wire(player_pnb, *p++);
    return 0;
}

/* Returns 0. */
/* 0x0014f3a0 */
int player_unload(void)
{
    model_pnb_unload(player_pnb);
    model_unload_timed(player_tex);
    player_pnb = 0;
    player_tex = 0;
    return 0;
}

/* 0x0014f3e8 */
int player_init(void *parent)
{
    DEBUG_PRINT(str_player_init);
    return player_task_create(parent);
}

/* Warns on null. */
/* 0x0014f420 */
void player_exit(char *w)
{
    if (w == 0) {
        DEBUG_PRINT(str_player_exit_null);
        return;
    }
    DEBUG_PRINT(str_player_exit);
    do {
        TASK_OF(w)->flags |= TASK_F_END;
    } while (0);
}

/* 0x0014f478 */
int player_get_work(void)
{
    return player_work;
}

/* Position and angles, view angles, then the motion's offset and angles; half a turn about Y. */
/* 0x0014f488 */
void player_apply_matrix(int w, char *obj)
{
    int vec[4];
    int blk[4];

    float *pos = (float *)(obj + 52);

    float x, y, z;

    player_motion_eval(vec, blk);
    do { } while (0);
    z = pos[2];
    y = pos[1];
    x = pos[0];
    vu0_matrix_translate(ulMatrixCur, x, y, z);
    do { } while (0);
    player_rotate_yxz(obj + 76);
    player_rotate_yxz(obj + 388);
    vu0_matrix_translate(ulMatrixCur, ((float *)vec)[0], ((float *)vec)[1], ((float *)vec)[2]);
    player_rotate_yxz(blk);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
}

/*
 * One WireBreak task per model in use: 120 frames, fading from red, built in world space from the
 * current pose.
 */
/* 0x0014f520 */
void player_wire_break_start(char *w)
{
    PlayerWireBreakDesc desc;
    char *st = player_obj;
    char *rec = w + 60;
    int n = 2;
    char *list;

    desc.grav_x = 0;
    desc.grav_y = 0;
    desc.grav_z = 0;
    desc.spd = 0.5f;
    desc.drag = 0.95f;
    desc.rot_range = 0.3f;
    desc.timer = 120;
    desc.col = 0x80000080;
    desc.col_to = 0x800000;
    desc.blend_src = 4;
    desc.blend_dst = 1;
    desc.spd_min = 0.1f;
    ulMatrixPush();
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, camera_get_inv_mtx());
    player_apply_matrix(w, st);
    list = 0;
    if (TASK_OF(w)->parent)
        list = (char *)TASK_OF(w)->parent + 64;
    wire_break_set_globals(player_part_node_cb, w);
    for (; n != 0; n--) {
        if (*(int *)rec != 0) {
            player_draw_slot = rec;
            desc.buf = rec + 108;
            wire_break_task_create(list, &desc);
        }
        rec += 252;
    }
    wire_break_set_globals(0, 0);
    player_draw_slot = 0;
    ulMatrixPop();
}

/* Takes over the current player's position and camera; that one becomes w->prev. */
/* 0x0014f680 */
void player_push(PlayerWorkView *w)
{
    PlayerWorkView *head = player_work;

    if (head != 0) {
        float *src = &head->x;
        float *pos = &w->x;

        w->x = head->x;
        pos[1] = src[1];
        pos[2] = src[2];
        player_camera_copy(head->sub, w->sub);
        head = player_work;
    }
    player_work = w;
    w->prev = head;
}

/* Previous player current again, taking over w's position and camera. */
/* 0x0014f6f0 */
void player_pop(PlayerWorkView *w)
{
    PlayerWorkView *prev = w->prev;

    player_work = prev;
    if (prev != 0) {
        PlayerWorkView *cur;
        float *src;
        float *pos;

        player_camera_copy(w->sub, prev->sub);
        cur = player_work;
        src = &w->x;
        pos = &cur->x;
        cur->x = w->x;
        pos[1] = src[1];
        pos[2] = src[2];
    }
}

/* Diagonals 0.707 per axis. */
/* 0x0014f758 */
void player_input_digital(PlayerWorkHeadView *w)
{
    float one = 1.0f;
    float diag = 0.707f;
    unsigned btn = pad_main->player.on;

    if (btn & 0x10) {
        if (btn & 0x80) {
            w->x = w->x + diag;
            w->y = w->y - diag;
        } else if (btn & 0x40) {
            w->x = w->x - diag;
            w->y = w->y - diag;
        } else {
            w->y = w->y - one;
        }
    } else if (btn & 0x20) {
        if (btn & 0x80) {
            w->x = w->x + diag;
            w->y = w->y + diag;
        } else if (btn & 0x40) {
            w->x = w->x - diag;
            w->y = w->y + diag;
        } else {
            w->y = w->y + one;
        }
    } else if (btn & 0x80) {
        w->x = w->x + one;
    } else if (btn & 0x40) {
        w->x = w->x - one;
    }
}

/* Beyond the dead zone (player_margin_ax/ay); full deflection = 1. */
/* 0x0014f850 */
void player_input_analog(PlayerWorkHeadView *w)
{
    int margin_x = player_margin_ax;
    float inv = 1.0f / (float)(128 - margin_x);
    int stick = pad_system.pad[0]->player.stick[0];
    int ofs;
    int margin_y;

    if (!(stick < margin_x)) {
        ofs = stick - margin_x;
        w->x = w->x + (float)ofs * inv;
    } else if (!(-margin_x < stick)) {
        ofs = stick + margin_x;
        w->x = w->x + (float)ofs * inv;
    }
    margin_y = player_margin_ay;
    inv = 1.0f / (float)(128 - margin_y);
    stick = pad_system.pad[0]->player.stick[1];
    if (!(stick < margin_y)) {
        ofs = stick - margin_y;
        w->y = w->y + (float)ofs * inv;
    } else if (!(-margin_y < stick)) {
        ofs = stick + margin_y;
        w->y = w->y + (float)ofs * inv;
    }
}

/* -1..1 per axis. Overdrive on button 0x400 if one is stocked and status flag 0x10000 clear. */
/* 0x0014f948 */
void player_read_input(PlayerWorkHeadView *w)
{
    w->x = 0.0f;
    w->y = 0.0f;
    player_input_digital(w);
    player_input_analog(w);
    w->x = MIN(MAX(w->x, -1.0f), 1.0f);
    w->y = MIN(MAX(w->y, -1.0f), 1.0f);
    if ((*(PadDataFlagsView * *)&pad_main)->flags & 0x400) { /* steer(cast) */
        if (player_status_test_flags(0x10000) == 0) {
            if (player_status_has_overdrive() != 0)
                lockon_overdrive_create();
        }
    }
}

/*
 * Pitch clamped to player_view_ang_lo..hi; yaw to +-player_view_ang_mid with player_eye_ang_limit.
 * Pitch position (w->ratio) bends the neck (w->neck_ang eases at player_neck_r).
 */
/* 0x0014fa50 */
void player_update_view_angles(PlayerWorkRatioView *w, PlayerObjView *view)
{
    int lo;
    int hi;
    int pitch;
    int range_hi;
    int yaw;
    int pitch_raw;
    float ratio;
    int yaw_raw;

    view->pitch = (int)((float)view->pitch - w->y * (float)player_eye_spd_x);
    lo = player_view_ang_lo;
    hi = player_view_ang_hi;
    pitch = MAX(view->pitch, lo);
    range_hi = hi;
    hi = pitch < hi ? pitch : hi;
    view->pitch = hi;
    ratio = (float)(hi - lo) / (float)(range_hi - lo);
    w->ratio = ratio;
    w->neck_ang = ang_approach((int)-((float)player_neck_o + (float)player_neck_m * ratio), w->neck_ang, player_neck_r);
    yaw = (int)((float)view->yaw - w->x * (float)player_eye_spd_y);
    view->yaw = yaw;
    if (player_eye_ang_limit != 0) {
        int mid = player_view_ang_mid;

        view->yaw = MIN(MAX(yaw, -mid), mid);
    }
    pitch_raw = view->pitch;
    view->pitch = WRAP16(pitch_raw);
    yaw_raw = view->yaw;
    view->yaw = WRAP16(yaw_raw);
}

/* out = root position of anim_a at time_a minus anim_b at time_b. */
/* 0x0014fbd8 */
void player_anim_eval_diff(void *anim_a, void *anim_b, float time_a, float *out, float time_b)
{
    float pos_a[4];
    float pos_b[4];
    float ang[4];
    float aux[4];
    PlayerEvalArgs args;
    int ctx;

    args.rot = ang;
    args.scale = aux;
    ctx = system_pnb_entry_get(0);
    args.out = pos_a;
    anim_eval_hierarchy(ctx, anim_a, &args, time_a);
    args.out = pos_b;
    anim_eval_hierarchy(ctx, anim_b, &args, time_b);
    out[0] = pos_a[0] - pos_b[0];
    out[1] = pos_a[1] - pos_b[1];
    out[2] = pos_a[2] - pos_b[2];
}

static inline void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void vcopy_player_update_deltas(PlayerV3 *dst, PlayerV3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void vcopy_player_smooth_update(PlayerV3 *dst, PlayerV3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/*
 * Intro then loop animations (camera, roll, view; missing ones follow the camera); a queued pass_b
 * takes over at a loop wrap. Flags 8 next node, 16 loop end, 32 intro end; va/vb: continuity.
 */
/* 0x0014fcc0 */
float player_pass_follow(PlayerWorkPassAView *w, int *cam_anim, int *roll_anim, int *view_anim, float *va, float *vb)
{
    PlayerPassNodeVaView *node = &w->pass_a;
    float time;
    float time_start;
    int st;

top:
    {
        time = w->pass_time;
        time_start = time;
        if (time < node->dur - 0.0001f) {
            st = node->state;
            *cam_anim = node->cam_intro;
            *roll_anim = node->roll_intro;
            *view_anim = node->view_intro;
            if (st == 1) {
                node->state = 0;
                player_anim_eval_diff((void *)node->cam_intro, (void *)node->cam_loop, 0.0f, va, time);
                player_anim_eval_diff((void *)node->view_intro, (void *)node->view_loop, 0.0f, vb, time);
                DEBUG_PRINT(str_player_pass_loop);
            }
            goto done;
        }
        time = time - node->dur;
        if (node->state == 0) {
            node->state = 1;
            if (node->cam_intro == node->cam_loop) {
                if (w->pending != 0) {
                    w->pending = 0;
                    w->pass_time = time_start - node->dur;
                    w->pass_a = w->pass_b;
                    player_status_set_flags(8);
                    goto top;
                }
                player_status_set_flags(16);
                vcopy(va, node->va);
                vcopy(vb, node->vb);
            } else {
                player_status_set_flags(32);
            }
        }
        if (node->dur2 - 0.0001f <= time) {
            time = time - node->dur2;
            w->pass_time = w->pass_time - node->dur2;
            if (w->pending != 0) {
                w->pending = 0;
                w->pass_time = w->pass_time - node->dur;
                w->pass_a = w->pass_b;
                player_status_set_flags(8);
                goto top;
            }
            player_status_set_flags(16);
            vcopy(va, node->va);
            vcopy(vb, node->vb);
        }
        *cam_anim = node->cam_loop;
        *roll_anim = node->roll_loop;
        *view_anim = node->view_loop;
    }
done:
    if (*roll_anim == 0)
        *roll_anim = *cam_anim;
    if (*view_anim == 0)
        *view_anim = *cam_anim;
    return time;
}

static inline void vadd(float *dst, float *src)
{
    dst[0] = dst[0] + src[0];
    dst[1] = dst[1] + src[1];
    dst[2] = dst[2] + src[2];
}

static inline void icopy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void iadd(int *dst, int *src)
{
    dst[0] = dst[0] + src[0];
    dst[1] = dst[1] + src[1];
    dst[2] = dst[2] + src[2];
}

/*
 * Pass time follows the game's pass clock (reset restarts it, taking a queued pass). Outside a
 * pass: fixed player_cam_ and player_view settings. Adds obj->off/aoff, then player_p_ang_x/y/z.
 */
/* 0x0014ffa8 */
void player_update_camera(PlayerWorkPassClockView *w, PlayerObjView *obj)
{
    PlayerCamEnt *ent;
    float pass_time;
    float t;
    float out[4];
    int ang[4];
    int aux[4];
    PlayerEvalArgsView args;
    void *anim[3];
    int ctx;

    ent = &obj->next_cam[0];
    pass_time = game_pass_time_get();
    if (fabsf(pass_time) < FLT_EPSILON) {
        w->pass_time = 0;
        w->pass_clock = 0;
        if (w->pending != 0) {
            w->pending = 0;
            w->pass_a = w->pass_b;
            player_status_set_flags(8);
        }
    } else {
        w->pass_time = w->pass_time + (pass_time - w->pass_clock);
        w->pass_clock = pass_time;
    }
    if (w->inpass == 0) {
        vcopy(ent[0].pos, &player_cam_pos.x);
        do {
            ent[0].ang[1] = (int)RAD_TO_ANG(atan2f(-player_cam_dir.x, -player_cam_dir.z));
            ent[0].ang[0] = (int)RAD_TO_ANG(atan2f(player_cam_dir.y,
                                             sqrtf(player_cam_dir.x * player_cam_dir.x
                                                   + player_cam_dir.z * player_cam_dir.z)));
        } while (0);
        ent[0].ang[2] = player_cam_roll;
        vadd(ent[0].pos, obj->off);
        icopy(obj->angout, ent[0].ang);
        vcopy(ent[1].pos, &player_view_pos.x);
        do {
            ent[1].ang[1] = (int)RAD_TO_ANG(atan2f(-player_view_dir.x, -player_view_dir.z));
            ent[1].ang[0] = (int)RAD_TO_ANG(atan2f(player_view_dir.y,
                                             sqrtf(player_view_dir.x * player_view_dir.x
                                                   + player_view_dir.z * player_view_dir.z)));
        } while (0);
        ent[1].ang[2] = player_view_roll;
        vadd(ent[1].pos, obj->off);
        iadd(ent[1].ang, obj->aoff);
    } else {
        args.out = out;
        args.rot = ang;
        args.scale = aux;
        t = player_pass_follow(w, &anim[0], &anim[1], &anim[2], ent[0].vec, ent[1].vec);
        ctx = system_pnb_entry_get(0);
        anim_eval_hierarchy(ctx, anim[0], &args, t);
        icopy(ent[0].ang, ang);
        vcopy(ent[0].pos, out);
        iadd(ent[0].ang, obj->aoff);
        vadd(ent[0].pos, obj->off);
        icopy(obj->angout, ent[0].ang);
        anim_eval_hierarchy(ctx, anim[1], &args, t);
        obj->angout[2] = ang[2];
        anim_eval_hierarchy(ctx, anim[2], &args, t);
        vcopy(ent[1].pos, out);
        icopy(ent[1].ang, ang);
        iadd(ent[1].ang, obj->aoff);
        vadd(ent[1].pos, obj->off);
    }
    ent[0].ang[0] = ent[0].ang[0] + player_p_ang_x;
    ent[0].ang[1] = ent[0].ang[1] + player_p_ang_y;
    ent[0].ang[2] = ent[0].ang[2] + player_p_ang_z;
}

/*
 * dt frames ahead, outputs optional. Outside a pass: player_cam_pos/dir. Intro (time < lo): objA;
 * then objB loops with period, back by dir per loop. Offsets as in player_update_camera.
 */
/* 0x00150538 */
void player_pass_eval(float dt, PlayerWorkPassAStateView *w, float *vecA, int *ivec, void *objA, float lo,
                      void *objB, float period, float *dir)
{
    float time;
    float loops;
    float off[4];
    float pos_tmp[4];
    float ang_tmp[4];
    float scr[4];
    PlayerEvalArgsView args;

    if (w->inpass == 0) {
        if (vecA != 0) {
            vcopy(vecA, &player_cam_pos.x);
        }
        if (ivec != 0) {
            do {
                ivec[1] = (int)RAD_TO_ANG(atan2f(-player_cam_dir.x, -player_cam_dir.z));
                ivec[0] = (int)RAD_TO_ANG(atan2f(player_cam_dir.y,
                                             sqrtf(player_cam_dir.x * player_cam_dir.x + player_cam_dir.z * player_cam_dir.z)));
            } while (0);
            ivec[2] = player_cam_roll;
        }
    } else {
        time = w->pass_time;
        time += dt;
        time = time - game_pass_count_spd_get();
        if (time < lo) {
            objB = objA;
            off[0] = 0;
            off[1] = 0;
            off[2] = 0;
        } else {
            time = time - lo;
            loops = floorf(time / period);
            if (player_status_test_flags(16))
                loops = loops + 1.0f;
            if (w->pass_a_state == 0 && objA == objB)
                loops = loops + 1.0f;
            off[0] = dir[0] * -loops;
            off[1] = dir[1] * -loops;
            off[2] = dir[2] * -loops;
            while (period <= time)
                time = time - period;
        }
        if (vecA)
            args.out = vecA;
        else
            args.out = pos_tmp;
        if (ivec)
            args.rot = ivec;
        else
            args.rot = ang_tmp;
        args.scale = scr;
        anim_eval_hierarchy(system_pnb_entry_get(0), objB, &args, time);
        if (vecA != 0)
            vadd(vecA, off);
    }
    if (vecA != 0)
        {
            PlayerObjView *obj = &player_obj;

            vadd(vecA, obj->off);
        }
    if (ivec != 0) {
        int ang_x = player_p_ang_x;
        int ang_y = player_p_ang_y;
        int ang_z = player_p_ang_z;

        ivec[0] = ivec[0] + ang_x;
        ivec[1] = ivec[1] + ang_y;
        ivec[2] = ivec[2] + ang_z;
    }
}

static inline void vsub(PlayerV3 *dst, PlayerV3 *to, PlayerV3 *from)
{
    dst->x = to->x - from->x;
    dst->y = to->y - from->y;
    dst->z = to->z - from->z;
}

static inline void vzero(PlayerV3 *dst)
{
    dst->x = 0;
    dst->y = 0;
    dst->z = 0;
}

/* Velocity (next - current) and acceleration (velocity - last) for player and view point. */
/* 0x00150880 */
void player_update_deltas(int w, char *obj)
{
    PlayerV3 *acc;

    {
        PlayerV3 *pos = V3(obj, 52);
        PlayerV3 *cam_pos = V3(obj, 196);

        vsub(V3(obj, 208), cam_pos, pos);
    }
    vsub(V3(obj, 280), V3(obj, 268), V3(obj, 124));
    acc = V3(obj, 256);
    vsub(V3(obj, 244), V3(obj, 208), acc);
    vcopy_player_update_deltas(V3(obj, 232), acc);
    vzero(acc);
    acc = V3(obj, 328);
    vsub(V3(obj, 316), V3(obj, 280), acc);
    vcopy_player_update_deltas(V3(obj, 304), acc);
    vzero(acc);
}

static inline void vcopyi(IVec3 *dst, IVec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

/* State 1: eases cur toward dst 5% a frame until within 1 unit; 2: jumps. Angles taken directly. */
/* 0x001509c0 */
void player_smooth_update(int w, PlayerSmooth *sm)
{
    PlayerV3 *to;
    PlayerV3 *cur;

    switch (sm->state) {
    case 0:
        break;
    case 1:
        to = &sm->dst;
        cur = &sm->cur;
        vec3_approach(to, cur, 0.05f);
        if (vec3_distance_squared(to, cur) <= 1.0f) {
            sm->state = 0;
            vcopy_player_smooth_update(cur, to);
        }
        break;
    case 2:
        sm->state = 0;
        vcopy_player_smooth_update(&sm->cur, &sm->dst);
        break;
    }
    vcopyi(&sm->aoff, &sm->aim_ang);
}

/*
 * Status flag 1 -> DEAD; 0x100 (level change) -> LEVEL UP/DOWN with 10 frames of no damage, or
 * clears the signal if the level is unchanged.
 */
/* 0x00150ac0 */
void player_mode_normal(PlayerWorkLevelView *w)
{
    int ph = w->phase;
    void *blk;
    unsigned int level;

    switch (ph) {
    case 0:
        blk = (char *)w + 36;
        player_blk_setup(blk, 0, w->level);
        player_blk_release((char *)w + 564);
        w->visible = 1;
        w->blend = 0;
        w->phase = w->phase + 1;
        break;
    case 1:
        if (player_status_get_flags() & 1)
            w->next_mode = ph;
        if (player_status_test_flags(256) != 0) {
            level = player_status_get_level();
            if (level == w->level) {
                player_status_clear_flags(256);
            } else {
                if (w->level < level)
                    w->next_mode = 3;
                else
                    w->next_mode = 4;
            }
            blk = (char *)w + 36;
            w->no_damage = 10;
        }
        break;
    }
    player_part_update((char *)w + 36, *(float *)((char *)w + 1092));
}

/*
 * No damage. Wire break, event 22, input and lock-on off; after 30 frames (no pausing) asks the
 * stage (stage_exit_flags bit 2); then level 1, no items, player_damage_no_c safe frames, event 20.
 */
/* 0x00150bb0 */
void player_mode_dead(PlayerWorkCountView *w)
{
    int one = 1;
    int state;

    w->no_damage = one;
    switch (w->phase) {
    case 0:
        player_wire_break_start(w);
        listener_dispatch(22, 0);
        w->visible = 0;
        pad_input_disable(0);
        lockon_set_enable(0);
        w->count = 0;
        w->phase = w->phase + 1;
        break;
    case 1:
        system_pause_disable();
        w->count = w->count + 1;
        if (30.0f <= (float)w->count) {
            stage_exit_flags |= 4;
            w->phase = w->phase + 1;
        }
        break;
    case 2:
        if (stage_exit_flags & 4)
            return;
        w->level = one;
        player_status_set_next_level(1);
        player_blk_release((char *)w + 36);
        player_blk_release((char *)w + 564);
        player_status_clear_items();
        w->phase = w->phase + 1;
        break;
    case 3:
        pad_input_enable(0);
        lockon_set_enable(1);
        player_status_clear_flags(256);
        w->visible = one;
        state = player_damage_no_c;
        w->no_damage = state;
        listener_dispatch(20, 0);
        w->next_mode = 0;
        break;
    }
}

/* No damage. Straightens turn and neck, plays the demo motion on the first model to its end. */
/* 0x00150d20 */
void player_demo_motion(PlayerWorkView *w)
{
    int ph;
    PlayerModelSlotView *rec = (PlayerModelSlotView *)((char *)w + 60);

    w->no_damage = 1;
    ph = w->phase;
    if (ph == 0) {
        w->turn = 0;
        w->neck_ang = 0;
        if (rec->motion == 0) {
            DEBUG_PRINTF(str_player_demo_motion_null);
            w->phase = w->phase + 1;
        } else if ((float)rec->motion->frames - 1.0f <= rec->pos) {
            rec->motion = 0;
            w->phase = ph + 1;
        }
    } else {
        w->next_mode = 0;
    }
    player_part_update((char *)w + 36, w->turn);
}

/*
 * No damage, one step per beat: change animation, event 19 (old, new level), stage sound 6; new
 * avatar; blend, old removed (event 20); animation back; event 21, player_damage_no_d safe frames.
 */
/* 0x00150de8 */
void player_clear_effect(PlayerWorkView *w)
{
    float sc;
    PlayerFxBlk *old_blk;
    PlayerFxBlk *new_blk;
    PlayerMsg msg;
    PlayerMotion *motion;
    char *rec;
    int pnb;
    float t;
    int level;

    sc = 0.0f;
    w->no_damage = 1;
    switch (w->phase) {
    case 0:
        w->phase = w->phase + 1;
    case 1:
        if (beat_grid_hit(0)) {
            player_blk_setup((char *)w + 36, 2, w->level);
            player_draw_parts(w);
            msg.old_level = w->level;
            msg.new_level = player_status_get_level();
            listener_dispatch(19, &msg);
            SoundRhythmSe_StgCommonStart(6);
            w->phase = w->phase + 1;
        }
        break;
    case 2:
        sc = beat_clock_get_phase();
        if (beat_grid_hit(0)) {
            level = player_status_get_level();
            w->level = level;
            player_blk_setup((char *)w + 564, 1, level);
            sc = 1.0f;
            player_blk_reset_time((char *)w + 564, 0);
            w->blend = 0;
            w->phase = w->phase + 1;
        }
        break;
    case 3:
        w->blend = beat_clock_get_phase();
        sc = 1.0f;
        if (beat_grid_hit(0)) {
            w->blend = sc;
            player_blk_release((char *)w + 36);
            listener_dispatch(20, 0);
            w->phase = w->phase + 1;
        }
        break;
    case 4:
        sc = 1.0f - beat_clock_get_phase();
        if (beat_grid_hit(0)) {
            sc = 0.0f;
            w->phase = w->phase + 1;
        }
        break;
    case 5:
        if (beat_grid_hit(0)) {
            SoundRhythmSe_StgCommonStop(6);
            listener_dispatch(21, 0);
            player_status_clear_flags(256);
            level = player_damage_no_d;
            w->next_mode = 0;
            w->no_damage = level;
        }
        break;
    }
    old_blk = (PlayerFxBlk *)((char *)w + 36);
    new_blk = (PlayerFxBlk *)((char *)w + 564);
    player_part_update(old_blk, w->turn);
    player_part_update(new_blk, w->turn);
    motion = *pnb_get_entry(player_pnb, 0);
    if (motion != 0) {
        if (old_blk->used != 0) {
            rec = (char *)w + 60;
            pnb = *(int *)(rec + 20);
            t = ((float)motion->frames - 1.0f) * sc;
            player_curve_update(rec + 200, pnb, motion, t);
        }
        if (new_blk->used != 0) {
            rec = (char *)w + 588;
            pnb = *(int *)(rec + 20);
            t = ((float)motion->frames - 1.0f) * sc;
            player_curve_update(rec + 200, pnb, motion, t);
        }
    }
}

/*
 * No damage. Old avatar breaks, event 19, new fades in over 30 frames; then events 20, 21 and
 * player_damage_no_d safe frames.
 */
/* 0x001510d0 */
void player_mode_level_down(PlayerWorkBlkDrawWireView *w)
{
    int ph;
    float blend;
    unsigned int msg[2];

    w->no_damage = 1;
    ph = w->phase;
    switch (ph) {
    case 0:
        player_wire_break_start(w);
        msg[0] = w->level;
        msg[1] = player_status_get_level();
        listener_dispatch(19, msg);
        player_blk_setup((char *)w + 36, 3, w->level);
        w->blk_draw_wire = 0;
        w->level = player_status_get_level();
        player_blk_setup((char *)w + 564, 4, w->level);
        w->phase = w->phase + 1;
        ph = w->phase;
    case 1:
        w->blend = 0;
        w->phase = ph + 1;
        break;
    case 2:
        blend = w->blend + 0.033333335f;
        w->blend = blend;
        if (1.0f <= blend) {
            w->blend = 1.0f;
            player_blk_release((char *)w + 36);
            listener_dispatch(20, 0);
            listener_dispatch(21, 0);
            player_status_clear_flags(256);
            w->no_damage = player_damage_no_d;
            w->next_mode = 0;
        }
        break;
    }
    player_part_update((char *)w + 36, w->turn);
    player_part_update((char *)w + 564, w->turn);
}

/*
 * With player_disp_count: node intro/loop lengths, time, loop offsets and positions of player
 * and view point.
 */
/* 0x00151250 */
void player_pass_debug_print(PlayerWorkPassView *w)
{
    PlayerPassNodeView *node;
    PlayerPassVecs *ent;
    float time;
    PlayerObjCamView *obj = &player_obj;

    if (player_disp_count == 0)
        return;
    debug_set_origin(0, -15);
    node = &w->pass_a;
    debug_printf("PassFrame % 8.3f: % 8.3f", node->dur, node->dur2);
    debug_newline();
    time = w->pass_time;
    if (node->dur <= time)
        debug_printf("PassCount          : % 8.3f", time - node->dur);
    else
        debug_printf("PassCount % 8.3f", time);
    debug_newline();
    ent = &obj->cam[0];
    debug_printf("Vec      : % 8.3f % 8.3f % 8.3f", ent->vec[0], ent->vec[1], ent->vec[2]);
    debug_newline();
    debug_printf(" NoLoop  : % 8.3f % 8.3f % 8.3f", ent->noloop[0], ent->noloop[1], ent->noloop[2]);
    debug_newline();
    debug_printf(" Loop    : % 8.3f % 8.3f % 8.3f", ent->loop[0], ent->loop[1], ent->loop[2]);
    debug_newline();
    debug_printf("Cur  Pos : % 8.3f % 8.3f % 8.3f", obj->cam[0].cur[0], ent->cur[1], ent->cur[2]);
    ent = &obj->cam[2];
    debug_newline();
    debug_printf("Next Pos : % 8.3f % 8.3f % 8.3f", obj->cam[2].cur[0], ent->cur[1], ent->cur[2]);
    ent = &obj->cam[0];
    debug_newline();
    debug_newline();
    debug_printf("Base Vec : % 8.3f % 8.3f % 8.3f", ent[1].vec[0], ent[1].vec[1], ent[1].vec[2]);
    debug_newline();
    debug_printf(" NoLoop  : % 8.3f % 8.3f % 8.3f", ent[1].noloop[0], ent[1].noloop[1], ent[1].noloop[2]);
    debug_newline();
    debug_printf(" Loop    : % 8.3f % 8.3f % 8.3f", ent[1].loop[0], ent[1].loop[1], ent[1].loop[2]);
    debug_newline();
    ent = &obj->cam[0];
    debug_printf("Cur  Pos : % 8.3f % 8.3f % 8.3f", ent[1].cur[0], ent[1].cur[1], ent[1].cur[2]);
    ent = &obj->cam[2];
    debug_newline();
    debug_printf("Next Pos : % 8.3f % 8.3f % 8.3f", ent[1].cur[0], ent[1].cur[1], ent[1].cur[2]);
    debug_newline();
}

/* Starts outside a pass in mode NORMAL at the status level; hit radius 1.5; display list 6. */
/* 0x00151610 */
void player_task_init(PlayerWorkPosView *w)
{
    PlayerObjOffView *obj = &(*(PlayerObjOffView *)&player_obj); /* steer(cast) */

    do {
        w->res0 = player_pnb;
        w->res1 = player_tex;
    } while (0);
    w->sub = player_camera_start(w);
    player_ptcl_start(w);
    (*(PlayerObjOffView *)&player_obj).aim_mode = 0; /* steer(cast) */
    {
        int *pa = &obj->off.x;
        float *pb = &obj->eye.x;

        pa[0] = 0;
        pb[0] = 0;
        pa[1] = 0;
        pb[1] = 0;
        pa[2] = 0;
        pb[2] = 0;
    }
    {
        float *pos = &w->pos.x;

        pos[0] = 0;
        w->next_mode = 0;
        w->phase = 0;
        w->mode = 0;
        pos[1] = 0;
        pos[2] = 0;
    }
    w->level = player_status_get_level();
    player_status_set_next_level(w->level);
    player_push(w);
    w->blend = 0;
    obj->radius = 1.5f;
    *(float *)&w->blk_used = 0.0f;
    w->blk2_used = 0;
    w->inpass = 0;
    w->pending = 0;
    w->pass_time = 0;
    w->pass_clock = 0;
    w->visible = 0;
    w->player2_task = player2_task_create(w);
    player_select_motion(1);
    task_set_dlist(w, 6);
}

/* Also ends a running no-damage period and restores the previous player. */
/* 0x00151718 */
void player_task_release(PlayerWorkNoDamageOnView *w)
{
    if (w->no_damage_on != 0) {
        w->no_damage_on = 0;
        ((void (*)(PlayerWorkNoDamageOnView *))player_status_no_damage_tick)(w); /* steer(cast) */
    }
    player_ptcl_end(w);
    player_task_end_child(w->sub);
    player_blk_release((char *)w + 36);
    player_blk_release((char *)w + 564);
    player_pop(w);
}

/*
 * Last frame's prediction becomes current. No-damage counter held at 1 while command flag 1;
 * mode changes apply at frame end from phase 0; no posing with SYS_DISPLAY_PASS.
 */
/* 0x00151770 */
void player_task_exec(void *w)
{
    char *obj = player_obj;

    *(PlayerBlob *)(obj + 52) = *(PlayerBlob *)(obj + 196);
    player_smooth_update(w, obj);
    player_update_camera(w, obj);
    player_read_input(w);
    player_update_view_angles(w, obj);
    player_update_deltas(w, obj);
    player_sync_motion_to_beat();
    player_follow_view_angle(w);
    switch (WORD_AT(w, 1256)) {
    case 0:
        player_mode_normal(w);
        break;
    case 1:
        player_mode_dead(w);
        break;
    case 2:
        player_demo_motion(w);
        break;
    case 3:
        player_clear_effect(w);
        break;
    case 4:
        player_mode_level_down(w);
        break;
    }
    if (WORD_AT(w, 1272) & 1) {
        if (WORD_AT(w, 24) <= 0)
            WORD_AT(w, 24) = 1;
    }
    if (WORD_AT(w, 24) > 0) {
        WORD_AT(w, 24) = WORD_AT(w, 24) - 1;
        if (WORD_AT(w, 20) == 0) {
            WORD_AT(w, 20) = 1;
            player_status_no_damage_start();
        }
    } else if (WORD_AT(w, 20) != 0) {
        WORD_AT(w, 20) = 0;
        player_status_no_damage_tick();
    }
    if (WORD_AT(w, 1256) != WORD_AT(w, 1260)) {
        WORD_AT(w, 1256) = WORD_AT(w, 1260);
        WORD_AT(w, 1264) = 0;
    }
    if (!(system_flags & SYS_DISPLAY_PASS))
        player_draw_parts(w);
}

/*
 * Draws when visible and player_draw is on (unless handed to "Player2" by player_2nd_draw); else
 * only poses.
 */
/* 0x001519d8 */
void player_task_disp(PlayerWorkDispView *w)
{
    if (player_draw == 0 || player_2nd_draw != 0) {
        player_draw_parts(w);
    } else {
        ulGxSetTextureList(w->obj);
        if (w->visible)
            player_draw_blend(w);
    }
}

/* 0x00151a48 */
int player_task_create(void *parent)
{
    return task_create(parent, &player_desc, 0x510);
}

/* "Player2" draws the player in a later display list (13) and shows its debug views. */
/* 0x00151a70 */
void player2_task_init(int *w, int parent)
{
    *w = parent;
    task_set_dlist(w, 13);
}

/* 0x00151a80 */
void player2_task_release(void)
{
}

/* 0x00151a88 */
void player2_task_exec(Player2Work *w)
{
    if (w->parent == 0 || (TASK_OF(w->parent)->flags & TASK_F_END))
        TASK_OF(w)->flags |= TASK_F_END;
}

/*
 * Pass debug print; player_disp_axis: red box at the player, green at the view point; avatar when
 * player_draw and player_2nd_draw are both on.
 */
/* 0x00151ab8 */
void player2_task_disp(PlayerWorkView **w)
{
    PlayerWorkView *pl = *w;
    char *obj = player_obj;
    float *pos;

    if (debug_disp_level == 0)
        player_pass_debug_print(pl);
    if (player_disp_axis != 0) {
        ulMatrixPush();
        pos = (float *)(obj + 52);
        PUT_POS(pos);
        player_rotate_yxz(obj + 76);
        player_rotate_yxz(obj + 388);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        debug_draw_box(0x80000080, 10.0f);
        ulMatrixPop();
        ulMatrixPush();
        PUT_POS(pos + 18);
        player_rotate_yxz(obj + 148);
        debug_draw_box(0x80008000, 10.0f);
        ulMatrixPop();
    }
    if (player_draw != 0 && player_2nd_draw != 0) {
        ulGxSetTextureList(pl->obj);
        if (pl->draw_blend != 0)
            player_draw_blend(pl);
    }
}

/* 0x00151c18 */
int player2_task_create(void *parent)
{
    return task_create_arg(scr_root, &player2_desc, 4, parent);
}

/*
 * w->turn (fraction of a turn) springs (player_rot_ka, player_rot_kv) toward camera turn plus view
 * yaw, the short way round.
 */
/* 0x00151c48 */
void player_follow_view_angle(PlayerWorkVelView *w)
{
    PlayerObjView *obj = &player_obj;
    int buf[8];
    float t;
    float cur;
    float curx;
    float wrapped;

    ((float *)buf)[4] = w->turn;
    player_camera_get_angles(buf);
    t = (float)((buf[1] + obj->yaw) & 0xffff) * 1.5258789e-05f;
    t = 1.0f - t;
    cur = ((float *)buf)[4];
    if (t - cur >= 0.5f)
        t -= 1.0f;
    else if (t - cur <= -0.5f)
        t += 1.0f;
    spring_step_pos((float *)buf + 4, &w->vel, &w->acc, t, player_rot_ka, player_rot_kv);
    curx = ((float *)buf)[4];
    if (curx < 0.0f) {
        wrapped = curx + 1.0f;
        ((float *)buf)[4] = wrapped;
        curx = wrapped;
    } else if (curx >= 1.0f) {
        wrapped = curx - 1.0f;
        ((float *)buf)[4] = wrapped;
        curx = wrapped;
    }
    w->turn = curx;
}

/* Records effect positions on the way (player_part_node_cb). */
/* 0x00151d68 */
void player_draw_parts(char *w)
{
    char *st = player_obj;
    char *blk = w + 36;
    char *rec;
    int i;

    ulMatrixPush();
    player_apply_matrix(w, st);
    rec = w + 60;
    player_draw_obj = *(int *)(blk + 12);
    i = 2;
    do {
        player_draw_slot = rec;
        if (*(int *)rec != 0) {
            model_node_run_default(rec + 16);
            model_node_run_default(rec + 108);
        }
        i--;
        rec += 252;
    } while (i != 0);
    player_draw_slot = 0;
    player_draw_obj = 0;
    ulMatrixPop();
}

/*
 * No depth write. Blend 0: old alone, 1: new alone, else both faded by 1 - blend and blend.
 * player_disp_shotpos marks effect positions.
 */
/* 0x00151e08 */
void player_draw_blend(PlayerBlend *w)
{
    char *st = player_obj;
    float t;

    psGsSetZMask(0);
    ulMatrixPush();
    player_apply_matrix(w, st);
    t = w->blend;
    if (fabsf(t) < FLT_EPSILON) {
        player_model_draw_slots((char *)w + 36, 1.0f);
    } else if (t == 1.0f) {
        player_model_draw_slots((char *)w + 564, 1.0f);
    } else {
        player_model_draw_slots((char *)w + 36, 1.0f - t);
        player_model_draw_slots((char *)w + 564, w->blend);
    }
    ulMatrixPop();
    psGsSetZMask(1);
    if (player_disp_shotpos != 0)
        player_draw_debug_points((char *)w + 36);
}

/* Model no1, up to three clips (negative = none). */
/* 0x00151ef8 */
void player_part_set_motion(void *obj, int no1, int no2, int no3, int no4)
{
    int mot1;
    int mot2;
    int mot3;
    int mot4;

    if (no1 < 0)
        mot1 = 0;
    else
        mot1 = *pnb_get_entry(player_pnb, no1);
    dc_object_init(obj, mot1, 0, 4096);
    if (no2 >= 0) {
        mot2 = *pnb_get_entry(player_pnb, no2);
        if (no3 >= 0) {
            mot3 = *pnb_get_entry(player_pnb, no3);
            mot4 = *pnb_get_entry(player_pnb, no4);
        } else {
            mot3 = 0;
            mot4 = 0;
        }
        dc_object_set_clip_cursor(obj, mot2, mot3, mot4);
    } else {
        dc_object_set_clip(obj, 0);
    }
}

/* Null desc clears it. Negative speed starts the clip from its end. */
/* 0x00151ff0 */
void player_part_setup(PlayerModelSlotFxView *rec, PlayerPartDesc *desc)
{
    char *model;
    char *wire;
    int cnt;

    if (desc == 0) {
        rec->used = 0;
        return;
    }
    rec->id = desc->id;
    rec->speed = desc->speed;
    if (rec->speed >= 0.0f)
        rec->time = 0.0f;
    else
        rec->time = 99999.0f;
    model = rec->model;
    wire = rec->wire;
    player_part_set_motion(model, desc->model_motion[0], desc->model_motion[1], desc->model_motion[2], desc->model_motion[3]);
    player_part_set_motion(wire, desc->wire_motion[0], desc->wire_motion[1], desc->wire_motion[2], desc->wire_motion[3]);
    if (desc->wire_motion[0] >= 0)
        dc_object_set_wire(wire, model_get_part_cache(player_pnb, desc->wire_motion[0]));
    if (desc->turn_motion >= 0)
        rec->turn_motion = *pnb_get_entry(player_pnb, desc->turn_motion);
    else
        rec->turn_motion = 0;
    cnt = dc_object_child_count(model);
    player_curve_build(rec->fx, cnt, model);
    dc_object_set_callback(model, player_part_node_cb, player_work);
    dc_object_set_callback(wire, player_part_node_cb, player_work);
    rec->used = 1;
}

/*
 * idx: 0 normal, 1/2 new/old of a level up, 3/4 old/new of a level down. Colours from player_cols;
 * second colour fixed at half alpha while status flags 0x180000.
 */
/* 0x00152140 */
void player_blk_setup(PlayerModelView *blk, int idx, int kind)
{
    unsigned int col;
    unsigned int col2;
    char *desc;
    PlayerModelSlotView *rec;

    if (blk->used != 0)
        player_blk_release(blk);
    col = player_cols[kind];
    col2 = col;
    if (player_status_test_flags(0x180000) != 0)
        col2 = (((color_get_a(col) + 1) >> 1) << 24) | 0x5a6400;
    if (player_part_descs[kind] != 0)
        desc = player_part_descs[kind] + idx * 44;
    else
        desc = 0;
    rec = (PlayerModelSlotView *)blk->slot_a;
    ((void (*)(void *, char *, int))player_part_setup)(rec, desc, idx); /* steer(cast) */
    rec->col = col;
    rec->motion = 0;
    rec->pos = 0;
    rec->col2 = col2;
    rec->rate = 1.0f;
    if (player_part_descs[kind + 8] != 0)
        desc = player_part_descs[kind + 8] + idx * 44;
    else
        desc = 0;
    rec = (PlayerModelSlotView *)blk->slot_b;
    ((void (*)(void *, char *, int))player_part_setup)(rec, desc, idx); /* steer(cast) */
    rec->col2 = col2;
    rec->motion = 0;
    rec->pos = 0;
    rec->rate = 1.0f;
    rec->col = col;
    player_count_effect_positions(blk);
    blk->kind = kind;
    blk->used = 1;
    blk->draw_wire = 1;
}

/* 0x001522b8 */
void player_blk_release(PlayerModelView *blk)
{
    char *rec;
    int i;

    if (blk->used != 0) {
        rec = (char *)blk + 24;
        i = 2;
        do {
            player_curve_free(rec + 200);
            i--;
            dc_object_release(rec + 16);
            dc_object_release(rec + 108);
            *(int *)rec = 0;
            rec += 252;
        } while (i != 0);
        blk->used = 0;
    }
    player_blk_free_data(blk);
}

/*
 * Beat-synced (rec->id): clip at speed loops per bar, backward if negative; else once at speed a
 * frame. Pose from the playing motion (rec->rate a frame, holds last frame), else turn motion.
 */
/* 0x00152328 */
void player_part_update(PlayerModelRecView *blk, float phase)
{
    PlayerModelSlotView *rec;
    int i;
    float x;
    float y;
    float lim;

    if (blk->used != 0) {
        rec = &blk->rec[0];
        i = 2;
        do {
            if (!(rec->used != 0 && !(fabsf(rec->model.speed) < FLT_EPSILON))) {
                rec++;
            } else {
                if (rec->id != 0) {
                    float t;
                    float u;

                    t = beat_clock_get_bar();
                    if (rec->speed < 0.0f) {
                        t = 1.0f - t;
                        x = t * -rec->speed;
                    } else
                        x = t * rec->speed;
                    while (1.0f < x)
                        x -= 1.0f;
                    {
                        PlayerPartSub *sub = &rec->model;

                        u = sub->length * x;
                        sub->time = u;
                    }
                    {
                        PlayerPartSub *sub = &rec->wire;

                        u = sub->length * x;
                        sub->time = u;
                    }
                } else {
                    rec->time = rec->time + rec->speed;
                    y = MIN(MAX(rec->time, 0.0f), rec->model.length);
                    rec->wire.time = rec->model.time = rec->time = y;
                }
                player_curve_reset(rec->blk);
                if (rec->motion != 0) {
                    lim = (float)rec->motion->frames - 1.0f;
                    rec->pos = rec->pos + rec->rate;
                    rec->pos = MIN(rec->pos, lim);
                    player_curve_update(rec->blk, rec->model.root, rec->motion, rec->pos);
                } else if (rec->turn_motion != 0) {
                    player_curve_update(rec->blk, rec->model.root, rec->turn_motion, ((float)rec->turn_motion->frames - 1.0f) * phase);
                }
                rec++;
            }
            i--;
        } while (i != 0);
    }
}

/* Start of the clips if flag, else end. */
/* 0x001525d8 */
void player_blk_reset_time(char *blk, int flag)
{
    int i = 2;
    char *slot = blk + 24;
    float time;
    PlayerTimeRec *rec;

    do {
        rec = (PlayerTimeRec *)(slot + 16);
        time = flag ? 0.0f : rec->vals[16] - 1.0f;
        rec->vals[15] = time;
        rec = (PlayerTimeRec *)(slot + 108);
        time = flag ? 0.0f : rec->vals[16] - 1.0f;
        rec->vals[15] = time;
        slot += 252;
    } while (--i != 0);
}

/* 0x00152638 */
void player_ptcl_start(PlayerWorkPtclView *w)
{
    w->sub = task_create(w, &player_ptcl_desc, 0);
}

/* 0x00152670 */
void player_ptcl_end(PlayerWorkPtclView *w)
{
    if (w->sub != 0)
        TASK_OF(w->sub)->flags |= TASK_F_END;
    w->sub = 0;
}

/*
 * Kind 1: one effect at the player's position; else player_fx_defs' effect at each effect position
 * with one of the kind's flags, following it.
 */
/* 0x00152690 */
void player_fx_spawn(PlayerWorkWorldView *w, PlayerFxBlkView *blk, int kind)
{
    PlayerFxEnt *ent;
    int n;
    int id;
    unsigned int mask;
    PlayerFxDef *def;

    if (kind == 1) {
        ptcl_task_set_src(ptcl_task_create(w->world, ptcl_data_get_entry(0, 0), 0, 0), player_obj_pos, 0);
        return;
    }
    def = &player_fx_defs[kind];
    id = def->id;
    mask = def->mask;
    ent = blk->ents;
    n = blk->neff;
    if (n > 0) {
        do {
            if (ent->flags & mask) {
                ent->fx = ptcl_task_create_world(w->world, ptcl_data_get_entry(0, id), 1, 0, 0);
                ptcl_task_set_src(ent->fx, ent, (char *)ent + 24);
            }
            n--;
            ent++;
        } while (n != 0);
    }
}

/* player_motion_ids gives its player pack entry. */
/* 0x001527a0 */
void player_select_motion(int n)
{
    (*(PlayerObjSelectView *)&player_obj).motion_idx = n; /* steer(cast) */
    (*(PlayerObjSelectView *)&player_obj).motion = *pnb_get_entry(player_pnb, player_motion_ids[n]); /* steer(cast) */
}

/* The whole motion spans 16 beats. */
/* 0x001527f8 */
void player_sync_motion_to_beat(void)
{
    PlayerObjView *ps = &player_obj;
    PlayerMotion *motion;
    int haku;
    float ph, pos;

    debug_set_origin(0, 10);
    motion = ps->motion;
    if (motion != 0) {
        haku = beat_clock_get_haku();
        haku %= 16;
        ph = beat_clock_get_phase();
        pos = ph * 0.0625f;
        pos += (float)haku * 0.0625f;
        ps->time = ((float)motion->frames - 1.0f) * pos;
    }
}

/* Zero offset when no motion. */
/* 0x001528d0 */
void player_motion_eval(int *out, int ang_out)
{
    int scratch[4];
    PlayerEvalArgsScratchView args;
    void *obj = (*(PlayerObjMotionView *)&player_obj).motion; /* steer(cast) */

    if (obj == 0) {
        out[2] = out[1] = out[0] = 0;
    } else {
        args.out = out;
        args.a1 = ang_out;
        args.scratch = scratch;
        anim_eval_hierarchy(system_pnb_entry_get(0), obj, &args, (*(PlayerObjMotionView *)&player_obj).time); /* steer(cast) */
    }
}

/*
 * Effect positions: nodes flagged 0x50000000; shot positions 0x40000000. Logs the counts,
 * allocates the per-frame effect position table.
 */
/* 0x00152940 */
void player_count_effect_positions(PlayerModelEntsView *blk)
{
    int eff = 0;
    int shot = 0;
    char *rec = blk->recs;
    int i = 2;
    int n;

    do {
        if (*(int *)rec != 0) {
            char *model_obj = rec + 16;
            char *wire_obj = rec + 108;

            n = dc_object_count_flag(model_obj, 0x50000000);
            eff += n;
            DEBUG_PRINTF(str_player_effpos_normal, n);
            n = dc_object_count_flag(wire_obj, 0x50000000);
            DEBUG_PRINTF(str_player_effpos_wire, n);
            eff += n;
            n = dc_object_count_flag(model_obj, 0x40000000);
            shot += n;
            DEBUG_PRINTF(str_player_shotpos_normal, n);
            n = dc_object_count_flag(wire_obj, 0x40000000);
            DEBUG_PRINTF(str_player_shotpos_wire, n);
            shot += n;
        }
        i--;
        rec += 252;
    } while (i != 0);
    DEBUG_PRINTF(str_player_effpos_total, eff);
    DEBUG_PRINTF(str_player_shotpos_total, shot);
    blk->nshot = shot;
    blk->neff = eff;
    if (eff > 0)
        blk->ents = heap_block_tag(game_calloc(eff * 48), str_player_effect_position, -1);
    else
        blk->ents = 0;
}

/* 0x00152aa8 */
void player_blk_free_data(PlayerFxBlk *w)
{
    if (w->ents != 0) {
        heap_free(w->ents, 0);
        w->ents = 0;
    }
    w->neff = 0;
    w->nshot = 0;
}

/* During posing: world position, flags n, Z axis direction (offset, and pitch/yaw). */
/* 0x00152ae8 */
void player_record_effect_pos(int n)
{
    float pos[3];
    int cam;
    PlayerFxEntView *ent;
    float *dst;

    if (player_draw_obj != 0) {
        cam = camera_get_inv_mtx();
        (*(PlayerFxEntView * *)&player_draw_obj)->id = n; /* steer(cast) */
        ulMatrixGetTranslation(ulMatrixCur, pos);
        ulMatrixApplyPoint(cam, pos, (*(PlayerFxEntView * *)&player_draw_obj)); /* steer(cast) */
        ulMatrixApplyPoint(ulMatrixCur, player_axis_z, pos);
        ulMatrixApplyPoint(cam, pos, pos);
        ent = (*(PlayerFxEntView * *)&player_draw_obj); /* steer(cast) */
        dst = ent->off;
        dst[0] = pos[0] - ent->pos[0];
        dst[1] = pos[1] - ent->pos[1];
        dst[2] = pos[2] - ent->pos[2];
        do {
            (*(PlayerFxEntView * *)&player_draw_obj)->yaw = (int)RAD_TO_ANG(atan2f(-ent->off[0], -ent->off[2])); /* steer(cast) */
            (*(PlayerFxEntView * *)&player_draw_obj)->pitch = (int)RAD_TO_ANG(atan2f((*(PlayerFxEntView * *)&player_draw_obj)->off[1], sqrtf((*(PlayerFxEntView * *)&player_draw_obj)->off[0] * (*(PlayerFxEntView * *)&player_draw_obj)->off[0] + (*(PlayerFxEntView * *)&player_draw_obj)->off[2] * (*(PlayerFxEntView * *)&player_draw_obj)->off[2]))); /* steer(cast) */
        } while (0);
        (*(PlayerFxEntView * *)&player_draw_obj)->roll = 0; /* steer(cast) */
        (*(PlayerFxEntView * *)&player_draw_obj)++; /* steer(cast) */
    }
}

/* Grey cross; direction end in red. */
/* 0x00152c80 */
void player_draw_debug_points(PlayerFxBlk *blk)
{
    PlayerFxEnt *ent;
    float *ofs;
    PlayerV3 pos;
    int n = blk->neff;

    if (n > 0) {
        ent = blk->ents;
        ofs = (float *)ent + 3;
        do {
            ulMatrixApplyPoint(ulMatrixCur, ent, &pos);
            project_point(&pos, &pos);
            debug_draw_cross(0x80808080, pos.x, pos.y, 0.0f, 5.0f);
            pos.x = ofs[-3] + ofs[0];
            pos.y = ofs[-2] + ofs[1];
            pos.z = ofs[-1] + ofs[2];
            ent++;
            ofs += 12;
            ulMatrixApplyPoint(ulMatrixCur, &pos, &pos);
            project_point(&pos, &pos);
            debug_draw_cross(0x80000080, pos.x, pos.y, 0.0f, 3.0f);
            n--;
        } while (n != 0);
    }
}

/*
 * Animated offset/rotation/scale from the pose buffers; neck nodes (flag 0x20000000) bend by the
 * neck angle. Returns 1.
 */
/* 0x00152d90 */
int player_part_node_cb(unsigned int *node, int i, PlayerWorkPartView *who)
{
    PlayerModelData *slot = (PlayerModelData *)player_draw_slot;
    unsigned int fl = *node;

    if (slot) {
        PlayerXformTab *tab = &slot->tab;
        PlayerIVec3View *trans = &tab->pos[i];
        PlayerQuat *quat = &tab->rot[i];
        PlayerIVec3View *scl = &tab->scale[i];

        njTranslateEx(trans);
        quat_to_matrix(quat);
        njScaleEx(scl);
    }
    if (fl & 0x50000000)
        player_record_effect_pos(fl);
    if (who && (fl & 0x20000000))
        vu0_matrix_rotate_z(ulMatrixCur, who->neck_ang);
    return 1;
}

/*
 * Solid, then wire additively in its colour, easing toward the target (fixed half alpha while
 * status flags 0x180000).
 */
/* 0x00152e50 */
void player_model_draw_slots(PlayerModelDrawView *blk, float rate)
{
    int mode = 0;
    int i;
    PlayerModelSlotColView *slot;
    float col[4];

    if (!(1.0f <= rate)) {
        draw_set_strip_flags(0xffffffff, 2048);
        mode = 2064;
        col[1] = 1.0f;
        col[2] = 1.0f;
        col[3] = 1.0f;
        col[0] = rate;
        draw_set_override_color(col);
    }
    player_draw_obj = blk->ents;
    slot = blk->slot;
    for (i = 2; i; i--, slot++) {
        player_draw_slot = slot;
        if (slot->used) {
            slot->col2 = color_blend(slot->col2,
                                  player_status_test_flags(0x180000) ? (((color_get_a(slot->col) + 1) >> 1) << 24) | 0x5a6400 : slot->col,
                                  0.05f);
            draw_set_override_flags(mode);
            dc_object_draw(slot->draw16);
            draw_set_override_flags(0);
            if (blk->draw_wire) {
                draw_set_blend(4, 1);
                draw_set_color(slot->col2);
                dc_object_draw(slot->draw108);
            }
        }
    }
    player_draw_slot = 0;
    player_draw_obj = 0;
}

/* Current and rest offset/rotation/scale; rest rotations stored inverted. */
/* 0x00152fb8 */
void player_curve_build(PlayerCurve *curve, int n, PlayerCurveSrc *src)
{
    int size_v3 = n * 12;
    int size_quat = n << 4;
    void *scl_tab;
    char *tbl;
    float *quat;
    void *args[3];
    int i;

    curve->count = n;
    curve->pos = heap_block_tag(game_malloc(size_v3), "player.c", 3916);
    curve->rot = heap_block_tag(game_malloc(size_quat), "player.c", 3917);
    curve->scale = heap_block_tag(game_malloc(size_v3), "player.c", 3918);
    curve->base_pos = heap_block_tag(game_malloc(size_v3), "player.c", 3919);
    curve->base_rot = heap_block_tag(game_malloc(size_quat), "player.c", 3920);
    scl_tab = heap_block_tag(game_malloc(size_v3), "player.c", 3921);
    curve->base_scale = scl_tab;
    if (n > 0) {
        args[0] = curve->base_pos;
        args[1] = model_wire_buf;
        args[2] = scl_tab;
        anim_eval_hierarchy(src->root, 0, args, 0.0f);
        tbl = model_wire_buf;
        quat = curve->base_rot;
        i = curve->count;
        do {
            quat_from_euler(tbl, quat);
            tbl += 12;
            quat[1] = -quat[1];
            quat[2] = -quat[2];
            quat[3] = -quat[3];
            i--;
            quat += 4;
        } while (i);
    }
}

/* 0x00153150 */
void player_curve_free(PlayerCurve *curve)
{
    heap_free(curve->pos, 0);
    heap_free(curve->rot, 0);
    heap_free(curve->scale, 0);
    heap_free(curve->base_pos, 0);
    heap_free(curve->base_rot, 0);
    heap_free(curve->base_scale, 0);
    curve->count = 0;
    curve->pos = 0;
    curve->rot = 0;
    curve->scale = 0;
    curve->base_pos = 0;
    curve->base_rot = 0;
    curve->base_scale = 0;
}

/* No offset, no rotation, rest scale. */
/* 0x001531d8 */
void player_curve_reset(PlayerCurve *curve)
{
    int n;
    float *clr;
    float *src, *dst;
    PlayerQuatView *quat;

    n = curve->count;
    if (n > 0) {
        src = (float *)curve->base_scale;
        dst = (float *)curve->scale;
        clr = (float *)curve->pos;
        quat = curve->rot;
        do {
            *clr = 0.0f;
            clr++;
            *clr = 0.0f;
            clr++;
            *clr = 0.0f;
            clr++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            quat->w = 1.0f;
            quat->x = 0.0f;
            quat->y = 0.0f;
            quat->z = 0.0f;
            quat++;
        } while (--n);
    }
}

/* Per node: offset from rest, scale, rotation relative to rest. */
/* 0x00153278 */
void player_curve_update(PlayerCurve *curve, int pnb, int anim, float time)
{
    void *args[3];
    Vec3 *pos_buf;
    Vec3 *rot_buf;
    Vec3 *scl_buf;
    PlayerQuat *inv_rest;
    PlayerQuat *rot_tab;
    PlayerQuat quat_tmp;
    int n;
    float *pos, *rest_pos, *scl, *pa, *pc;
    PlayerQuat *rot;
    PlayerQuat *quat;
    Vec3 *bw;
    int k;
    int j;
    int cnt;

    n = curve->count;
    if (n <= 0)
        return;
    pos_buf = model_wire_buf;
    rot_buf = pos_buf + n;
    scl_buf = rot_buf + n;
    args[0] = pos_buf;
    args[1] = rot_buf;
    pc = (float *)((char *)scl_buf + AZ(scl_buf)); /* steer(zero) */
    args[2] = scl_buf;
    anim_eval_hierarchy(pnb, anim, args, time);
    pos = (float *)curve->pos;
    rest_pos = (float *)curve->base_pos;
    scl = (float *)curve->scale;
    cnt = curve->count;
    rot_tab = curve->rot;
    inv_rest = curve->base_rot;
    pa = (float *)pos_buf;
    k = cnt;
    do {
        *pos = *pos + (*pa - *rest_pos); rest_pos++; pa++; pos++;
        *pos = *pos + (*pa - *rest_pos); rest_pos++; pa++; pos++;
        *pos = *pos + (*pa - *rest_pos); rest_pos++; pa++; pos++;
        *scl = *scl * *pc; scl++; pc++;
        *scl = *scl * *pc; scl++; pc++;
        *scl = *scl * *pc; scl++; pc++;
    } while (--k);
    rot = rot_tab;
    quat = inv_rest;
    bw = rot_buf;
    j = cnt;
    do {
        quat_from_euler(bw, &quat_tmp);
        bw++;
        quat_multiply(quat, &quat_tmp, &quat_tmp);
        quat++;
        quat_multiply(&quat_tmp, rot, rot);
        rot++;
    } while (--j);
}

/* Hit radius player_obj.radius. */
/* 0x00153440 */
int player_sphere_test(Vec3 *pos, float radius)
{
    PlayerObj *obj = &player_obj;
    int hit = 0;
    float dist_sq = vec3_distance_squared(pos, &obj->pos);

    radius = radius + obj->radius;
    radius = radius * radius;
    if (dist_sq <= radius)
        hit = 1;
    return hit;
}

/* In mode NORMAL also the level's demo motion (player_demo_motion_ids), if any. */
/* 0x001534a8 */
void player_motion_fx_start(void)
{
    PlayerWork *player = player_work;
    int id;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        player_fx_spawn(player, (char *)player + 36, 0);
        if (player->mode == 0) {
            id = player_demo_motion_ids[player->level];
            if (id >= 0)
                player_demo_motion_start(*pnb_get_entry(player_pnb, id));
        }
        return;
    }
}

/* 0: motion effects; 1: no damage, shoots from the player_set_shot_origin origin. */
/* 0x00153530 */
int player_command_start(int cmd)
{
    PlayerWork *player = player_work;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        switch (cmd) {
        case 0:
            return ((int (*)(void))player_motion_fx_start)(); /* steer(cast) */
        case 1:
            player->cmd_flags |= 1;
            break;
        }
    }
}

/* 0x00153590 */
void player_command_end(int cmd)
{
    PlayerWork *player = player_work;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        switch (cmd) {
        case 0:
            break;
        case 1:
            player->cmd_flags &= ~1;
            break;
        }
    }
}

/* 0x001535e0 */
void player_set_draw(int on)
{
    player_draw = on;
}

/* Drawing on and avatar visible. */
/* 0x001535f0 */
int player_is_drawn(void)
{
    PlayerWork *player = player_work;
    int drawn;

    if (player) {
        drawn = 0;
        if (player_draw)
            drawn = player->visible != 0;
        return drawn;
    }
    return 0;
}

/* 0x00153628 */
void player_set_2nd_draw(int on)
{
    player_2nd_draw = on;
}

/* 0x00153638 */
void player_set_aim_pos(Vec3 *pos)
{
    (*(PlayerObj *)&player_obj).aim_mode = 1; /* steer(cast) */
    c13_v3copy(&(*(PlayerObj *)&player_obj).aim_vec, pos); /* steer(cast) */
}

/* 0x00153668 */
void player_set_aim_dir(Vec3 *dir)
{
    (*(PlayerObj *)&player_obj).aim_mode = 2; /* steer(cast) */
    c13_v3copy(&(*(PlayerObj *)&player_obj).aim_vec, dir); /* steer(cast) */
}

/* 0x00153698 */
void player_set_aim_ang(IVec3 *ang)
{
    PlayerObj *obj = &player_obj;

    c13_i3copy(&obj->aim_ang, ang);
}

/* Only from mode NORMAL. */
/* 0x001536c0 */
void player_demo_motion_start(int motion)
{
    PlayerWork *player = player_work;

    if (player && !(TASK_OF(player)->flags & TASK_F_END) && player->mode == 0) {
        PlayerWorkSub *sub = &player->sub;

        *(PlyState2 *)&player->next_mode = PSTATE_2;
        sub->motion = motion;
        sub->pos = 0;
    }
}

/* 0x00153708 */
void player_demo_motion_end(void)
{
    PlayerWork *player = player_work;

    if (player && !(((int *)player)[-9] & 1) && player->mode == 2) {
        PlayerWorkSub *sub = &player->sub;

        player->next_mode = 0;
        sub->motion = 0;
        sub->pos = 0;
    }
}

/*
 * Command 1: the set origin; else shot position idx on the avatar (wraps); else the player
 * position, no direction.
 */
/* 0x00153748 */
void player_get_shot_origin(int idx, void **pos_out, void **rot_out)
{
    PlayerWorkEntsView *player = player_work;
    PlayerObj *obj = &player_obj;
    PlayerFxEntFlagsView *ent;
    int cnt;
    void *rot;

    if (player->cmd_flags & 1) {
        void *shot_pos;
        void *shot_rot;

        shot_pos = &obj->shot_pos;
        shot_rot = &obj->shot_rot;
        do { *pos_out = shot_pos; } while (0);
        *rot_out = shot_rot;
        return;
    }
    if (player->nshot > 0) {
        if (idx >= player->nshot)
            idx = idx % player->nshot;
    } else {
        idx = 0;
    }
    ent = player->ents;
    cnt = player->neff;
    if (ent) {
        do {
            if (ent->flags & 0x40000000) {
                if (idx <= 0) {
                    rot = ent->off;
                    goto done;
                }
                idx--;
            }
            ent++;
        } while (--cnt);
    }
    ent = (PlayerFxEntFlagsView *)&obj->pos;
    rot = player_zero_vec;
done:
    *pos_out = ent;
    *rot_out = rot;
}

/* 0x00153800 */
void player_set_shot_origin(Vec3 *pos, Vec3 *rot)
{
    PlayerObj *obj = &player_obj;

    c13_v3copy(&obj->shot_pos, pos);
    c13_v3copy(&obj->shot_rot, rot);
}

/* 0x00153848 */
void player_cell_move_clear(void)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player) {
        node = &player->pass_a;
        if (player->inpass) {
            node->state = 0;
            *(int *)&node->dur2 = 0;
            player->inpass = 0;
            node->cam_loop = 0;
            node->roll_intro = 0;
            node->roll_loop = 0;
            node->view_intro = 0;
            node->view_loop = 0;
            node->dur = 0;
            node->cam_intro = 0;
        }
    }
}

/*
 * Next cell: camera intro/loop animations and lengths, roll and view, continuity offsets. Outside
 * a pass starts at once, pass clock 0.
 */
/* 0x00153898 */
void player_cell_move_start(PlayerCellDim *cam_from, PlayerCellDim *cam_to, int roll_from, int roll_to, int view_from, int view_to)
{
    PlayerWork *player = player_work;
    PlayerObj *obj = &player_obj;
    PlayerPassCursor *pass;
    unsigned int n;

    if (player == 0)
        return;
    pass = &player->pass_b;
    player->pending = 1;
    pass->roll_intro = roll_from;
    n = cam_from->frames;
    pass->roll_loop = roll_to;
    pass->view_intro = view_from;
    pass->view_loop = view_to;
    *(int *)&pass->cam_intro = (int)cam_from;
    *(int *)&pass->cam_loop = (int)cam_to;
    pass->dur = (float)n - 1.0f;
    pass->dur2 = (float)cam_to->frames - 1.0f;
    player_anim_eval_diff(pass->cam_loop, pass->cam_loop, 0.0f, pass->va, pass->dur2);
    player_anim_eval_diff(pass->view_loop, pass->view_loop, 0.0f, pass->vb, pass->dur2);
    if (player->inpass == 0) {
        game_pass_time_reset();
        game_pass_time_set(0.0f);
        pass->state = 0;
        player->inpass = 1;
    } else if (player->pass_time < pass->dur) {
        pass->state = 0;
    } else {
        pass->state = 1;
    }
    player_update_camera(player, obj);
}

/* 0x00153a20 */
void player_nop(void)
{
}

/* player_eye_ang_limit; returns the previous setting. */
/* 0x00153a28 */
int player_swap_fullrange(int on)
{
    int old = player_eye_ang_limit;
    player_eye_ang_limit = on;
    return old;
}

/* Fixed placement outside passes (see player_update_camera). */
/* 0x00153a38 */
void player_set_view_pos(Vec3 *pos)
{
    c13_v3copy(&player_view_pos, pos);
}

/* 0x00153a60 */
void player_set_view_dir(Vec3 *dir)
{
    c13_v3copy(&player_view_dir, dir);
}

/* 0x00153a88 */
void player_set_view_roll(int roll)
{
    player_view_roll = roll;
}

/* 0x00153a98 */
void player_set_cam_pos(Vec3 *pos)
{
    c13_v3copy(&player_cam_pos, pos);
}

/* 0x00153ac0 */
void player_set_cam_dir(Vec3 *dir)
{
    c13_v3copy(&player_cam_dir, dir);
}

/* 0x00153ae8 */
void player_set_cam_roll(int roll)
{
    player_cam_roll = roll;
}

/* Outputs optional. */
/* 0x00153af8 */
void player_get_state_vectors(Vec3 *pos_out, Vec3 *vec_out, Vec3 *v76_out, Vec3 *eye_out)
{
    if (pos_out)
        *pos_out = (*(PlayerObj *)&player_obj).pos; /* steer(cast) */
    if (vec_out)
        *vec_out = (*(PlayerObj *)&player_obj).vec; /* steer(cast) */
    if (v76_out)
        *v76_out = (*(PlayerObj *)&player_obj).rot; /* steer(cast) */
    if (eye_out)
        *eye_out = (*(PlayerObj *)&player_obj).eye; /* steer(cast) */
}

/* 0x00153b90 */
void player_get_pos(Vec3 *out)
{
    *out = (*(PlayerObj *)&player_obj).pos; /* steer(cast) */
}

/* Getters below copy a per-frame vector to out (if given). */
/* 0x00153bb8 */
void player_get_frame_pos(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_frame_pos);
}

/* 0x00153be8 */
void player_get_frame_ang(IVec3 *out)
{
    if (out)
        c13_i3copy(out, &player_obj_frame_ang);
}

/* 0x00153c18 */
void player_get_vec(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_vec);
}

/* 0x00153c48 */
void player_get_vec_old(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_vec_old);
}

/* 0x00153c78 */
void player_get_accel(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_accel);
}

/* 0x00153ca8 */
void player_get_frame_vel(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_frame_vel);
}

/* 0x00153cd8 */
void player_get_frame_vel_old(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_frame_vel_old);
}

/* 0x00153d08 */
void player_get_frame_accel(Vec3 *out)
{
    if (out)
        c13_v3copy(out, &player_obj_frame_accel);
}

/* time frames ahead on the current node (player_pass_eval, camera animations). */
/* 0x00153d38 */
void player_pass_pos_a(void *pos, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        if (pos) {
            node = &player->pass_a;
            player_pass_eval(time, player, pos, 0, node->cam_intro, node->dur, node->cam_loop, node->dur2, &player->pass_a.va);
        }
    }
}

/* As player_pass_pos_a, angles. */
/* 0x00153d88 */
void player_pass_rot_a(void *rot, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        if (rot) {
            node = &player->pass_a;
            player_pass_eval(time, player, 0, rot, node->cam_intro, node->dur, node->cam_loop, node->dur2, &player->pass_a.va);
        }
    }
}

/* As player_pass_pos_a, position and angles. */
/* 0x00153dd8 */
void player_pass_eval_a(void *pos, void *rot, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        node = &player->pass_a;
        player_pass_eval(time, player, pos, rot, node->cam_intro, node->dur, node->cam_loop, node->dur2, &player->pass_a.va);
    }
}

/* As player_pass_pos_a, view point (view animations). */
/* 0x00153e20 */
void player_pass_pos_b(void *pos, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        if (pos) {
            node = &player->pass_a;
            player_pass_eval(time, player, pos, 0, node->view_intro, node->dur, node->view_loop, node->dur2, &player->pass_a.vb);
        }
    }
}

/* As player_pass_pos_b, angles. */
/* 0x00153e70 */
void player_pass_rot_b(void *rot, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        if (rot) {
            node = &player->pass_a;
            player_pass_eval(time, player, 0, rot, node->view_intro, node->dur, node->view_loop, node->dur2, &player->pass_a.vb);
        }
    }
}

/* As player_pass_pos_b, position and angles. */
/* 0x00153ec0 */
void player_pass_eval_b(void *pos, void *rot, float time)
{
    PlayerWork *player = player_work;
    PlayerPassNode *node;

    if (player && !(TASK_OF(player)->flags & TASK_F_END)) {
        node = &player->pass_a;
        player_pass_eval(time, player, pos, rot, node->view_intro, node->dur, node->view_loop, node->dur2, &player->pass_a.vb);
    }
}

/* 0x00153f08 */
void player_get_rot(Vec3 *out)
{
    *out = (*(PlayerObj *)&player_obj).rot; /* steer(cast) */
}

/* 0x00153f30 */
void player_get_eye(Vec3 *out)
{
    *out = (*(PlayerObj *)&player_obj).eye; /* steer(cast) */
}

/* 0x00153f58 */
void player_set_eye(Vec3 *in)
{
    (*(PlayerObj *)&player_obj).eye = *in; /* steer(cast) */
}

/* Also points the player camera there at once. */
/* 0x00153f80 */
void player_set_eye_sync(Vec3 *eye)
{
    (*(PlayerObj *)&player_obj).eye = *eye; /* steer(cast) */
    player_camera_set_aim((void *)player_work->camera, eye);
}

/* Outputs optional. */
/* 0x00153fb8 */
void player_get_next_pos(Vec3 *pos, Vec3 *rot)
{
    if (pos)
        *pos = (*(PlayerObj *)&player_obj).next_pos; /* steer(cast) */
    if (rot)
        *rot = (*(PlayerObj *)&player_obj).next_rot; /* steer(cast) */
}

/* player_model: a second, stand-alone avatar with its own motion (player_model_set). */
/* 0x00154008 */
void player_model_reset(void)
{
    player_model_active = 0;
}

/* For the current level. */
/* 0x00154018 */
void player_model_init(void)
{
    int level = player_status_get_level();

    player_blk_setup(player_model, 0, level);
}

/* 0x00154048 */
void player_model_release(void)
{
    player_blk_release(player_model);
}

/* Both models, first frame, normal speed. */
/* 0x00154058 */
void player_model_set(int motion)
{
    PlayerModel *model = &player_model;
    PlayerModelSlot *slot = &model->slot_a;

    slot->turn_motion = 0;
    slot->motion = motion;
    slot->pos = 0;
    slot->rate = 1.0f;
    slot = &model->slot_b;
    slot->turn_motion = 0;
    slot->motion = motion;
    slot->pos = 0;
    slot->rate = 1.0f;
}

/* 0x00154098 */
void player_model_set_rate(float v)
{
    (*(PlayerModel *)&player_model).slot_a.rate = v; /* steer(cast) */
    (*(PlayerModel *)&player_model).slot_b.rate = v; /* steer(cast) */
}

/* 0x001540b0 */
void player_model_update(void)
{
    player_part_update(player_model, 0.0f);
}

/* 0x001540c0 */
void player_model_draw(void)
{
    player_status_get_level();
    ulGxSetTextureList(player_tex);
    player_model_draw_slots(player_model, 1.0f);
}

/* Level, position, velocity, speed (ms, kmh), view angles, mode, NoDamage, set status flags. */
/* 0x001540f8 */
void player_info_window_draw(int win)
{
    PlayerWorkInfoView *player = player_work;
    float pos[3];
    float vec[3];
    int ang[3];
    int cell[2];
    float spd;
    float ms;
    float kmh;
    int y;
    int flags;
    FlagName *ent;
    unsigned int i;

    if (player == 0)
        return;
    if (TASK_OF(player)->flags & TASK_F_END)
        return;
    player_get_pos(pos);
    player_get_vec(vec);
    player_get_eye(ang);
    spd = vec3_length(vec);
    ms = spd * FPS;
    kmh = spd * 216.0f;
    gw_get_cell_size(&cell[0], &cell[1]);
    gw_win_printf(win, 4, 0, "Level %2d", player_status_get_level());
    y = cell[1];
    gw_win_printf(win, 4, y, "Pos    X% 10.3f\n       Y% 10.3f\n       Z% 10.3f\n", pos[0], pos[1], pos[2]);
    y += cell[1] * 3;
    gw_win_printf(win, 4, y, "Vec    X% 10.3f\n       Y% 10.3f\n       Z% 10.3f\n", vec[0], vec[1], vec[2]);
    y += cell[1] * 3;
    gw_win_printf(win, 4, y, "Spd %5.1fms/%5.1fkh", ms, kmh);
    y += cell[1];
    gw_win_printf(win, 4, y, "eye    X% 10.3f\n       Y% 10.3f\n       Z% 10.3f\n",
                 (float)ang[0] * DEG_PER_ANG, (float)ang[1] * DEG_PER_ANG, (float)ang[2] * DEG_PER_ANG);
    y += cell[1] * 3;
    gw_win_printf(win, 4, y, "MODE [%s]%d", player_mode_names[player->mode], player->phase);
    y += cell[1];
    gw_win_printf(win, 4, y, "NoDamage %4d", player->no_damage);
    y += cell[1];
    flags = player_status_get_flags();
    for (i = 0; i < 14; i++) {
        ent = &player_flag_names[i];
        if (flags & ent->bit)
            gw_win_printf(win, 4, y, "%s", ent->name);
        y += cell[1];
    }
}

/* 20 x 26 characters, top right. */
/* 0x00154418 */
void player_info_window_toggle(void)
{
    int sz[2];
    int win;

    if (player_info_window) {
        gw_window_close(player_info_window);
        player_info_window = 0;
        menu_item_set_value(info_menu_name, "PlayerInfo", 0);
    } else {
        gw_get_cell_size(&sz[0], &sz[1]);
        win = gw_window_open("PlayerInfo", 95,
                           (int)(((ScreenView *)screen_layout)->right - (float)(sz[0] * 20) - 4.0f),
                           (int)(((ScreenView *)screen_layout)->top + 32.0f),
                           sz[0] * 20, sz[1] * 26, 0);
        player_info_window = win;
        if (win) {
            gw_window_set_handlers(win, 0, player_info_window_draw, 0);
            menu_item_set_value(info_menu_name, "PlayerInfo", 1);
        }
    }
}

/* 0x00154530 */
void player_info_menu_add(void)
{
    menu_item_add(info_menu_name, "PlayerInfo", player_info_window_toggle, 0, 0);
}

/* 0x00154558 */
void player_rotate_yxz(int *p)
{
    vu0_matrix_rotate_y(ulMatrixCur, p[1]);
    vu0_matrix_rotate_x(ulMatrixCur, p[0]);
    vu0_matrix_rotate_z(ulMatrixCur, p[2]);
}
