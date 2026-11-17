/*
 * System start-up, tasks, vsync, debug keys, font, sprites; task header and texforce types
 * shared with task.c and texforce.c.
 */

#ifndef REZ_SYSTEM_H
#define REZ_SYSTEM_H

/* system_flags bits; quoted names are the game's own (system_flag_names) */
#define SYS_PAUSED          0x1         /* "PAUSE" */
#define SYS_PAUSE_KBD       0x2         /* "PAUSE KBD": debug pause from the keyboard */
#define SYS_SOFT_RESET      0x4         /* "S-RESET" */
#define SYS_DOOR_OPEN       0x8         /* "DOOR OPEN": disc tray */
#define SYS_SLOW_MOTION     0x10        /* "S-MOTION" */
#define SYS_STABILIZER      0x20        /* "STABILIZER": frame-rate stabiliser */
#define SYS_HEAP_CHECK      0x40        /* "MEMOVERRUN" */
#define SYS_HEAP_FILL       0x80        /* "FREE FILL": freed blocks filled with "UGAK" */
#define SYS_NO_PAUSE        0x100       /* "*PAUSE" */
#define SYS_NO_SOFT_RESET   0x200       /* "*S-RESET" */
#define SYS_NO_SCREEN_SAVER 0x400       /* "*SCR_SAVER" */
#define SYS_NO_VIBRATION    0x800       /* "*VIBRATION" */
#define SYS_PLAY_NOW        0x1000      /* "PLAY NOW": a stage played, not a replay */
#define SYS_WARNING_PENDING 0x2000      /* the boot warning still to show */
#define SYS_DEBUG_INPUT     0x4000
#define SYS_DEBUG           0x8000
#define SYS_DISPLAY_PASS    0x10000     /* tasks drawing, not updating */
#define SYS_TITLE_LOOP      0x20000     /* "TITLE LOOP" */

/*
 * System font: glyph grid. Index = char - 32 (0..208); column = index % cols, row = index / cols;
 * entries hold start and end texture coords.
 */
typedef struct FontVtx {
    float x;
    float y;
    float z;
    float u;
    float v;
    int color;
} FontVtx;                      /* 0x18 bytes */

extern int font_color;
extern float (*font_cols)[2];  /* (u0, u1) */
extern float (*font_rows)[2];  /* (v0, v1) */
extern float font_w;
extern float font_h;
extern int font_id;          /* 0..3 */
extern int font_per_row;
extern int font_tex_ids[];

#include "types_common.h"

typedef struct Vec4 {
    float x;
    float y;
    float z;
    float w;
} Vec4;

/* System sprites (24): texture rectangle, texture id and size. */
typedef struct SpriteUV {
    char pad_00[16];
} SpriteUV;

typedef struct SpriteDef {
    int tex;                    /* in the system texture list */
    float w;
    float h;
} SpriteDef;

extern void sprite_get(unsigned int id, int *tex, SpriteUV **uv, float *w, float *h);

extern void *ulMatrixCur;

/* Translates the current matrix by v (x, y, z first). */
#define MATRIX_TRANSLATE(v) \
    do { \
        float *v_ = &(v)->x; \
        vu0_matrix_translate(ulMatrixCur, v_[0], v_[1], v_[2]); \
    } while (0)

/* Task header helpers; task.h has the public view. */
#include "task.h"

#define TASK_MAGIC 0x4b534154
#define TASK_HDR(work) ((TaskHdr *)(work) - 1)
#define TASK_WORK(t) ((void *)((t) + 1))

/* Per-frame profile record of a running task; reset by task_prof_reset. */
typedef struct TaskProf {
    int depth;                  /* 0 ends the list (with task -1) */
    void *task;
    float exec_time;
    float disp_time;
} TaskProf;

extern int perf_record_cnt;          /* tasks profiled */
extern TaskProf *task_prof_cur;
extern int task_run_depth;
extern int task_default_flags;

/* Ends the task of work area w (flag 1); null w is ignored. */
#define TASK_END(w) \
    do { \
        if (w) \
            TASK_HDR(w)->flags |= 1; \
    } while (0)

#include "types_game.h"

extern TexForce *texforce_work;
extern int texforce_b_eff;          /* TexForce enabled */
extern int texforce_b_eff2;

/* Texture-force part (68 bytes, 3 per KTF0 part table entry). */
typedef struct KtfPart {
    int count;                  /* 0x00: > 0 when used */
    int tex;                    /* 0x04 */
    int frames;                 /* 0x08 */
    float angle0;               /* 0x0c: degrees */
    float angle1;               /* 0x10 */
    float col0[4];              /* 0x14 */
    float col1[4];              /* 0x24 */
    float w_start;                   /* 0x34 */
    float h_start;                   /* 0x38 */
    float w_end;                   /* 0x3c */
    float h_end;                   /* 0x40 */
} KtfPart;

/* "TfParts_EneBomb00" task work (0x58 bytes). */
typedef struct TfPart {
    int life;                   /* 0x00: frames left */
    int angle;                  /* 0x04: 0x10000 = 360 degrees */
    int dangle;                 /* 0x08 */
    Vec3 ofs;                   /* 0x0c: position relative to the camera */
    float sx;                   /* 0x18: projected position */
    float sy;                   /* 0x1c */
    float col[4];               /* 0x20 */
    float dcol[4];              /* 0x30 */
    float w;                    /* 0x40 */
    float h;                    /* 0x44 */
    float dw;                   /* 0x48 */
    float dh;                   /* 0x4c */
    int tex;                    /* 0x50 */
    TexForce *tf;               /* 0x54 */
} TfPart;

/* Texture-force screen preset (48 bytes; a table ends with a negative first word). */
typedef struct TfScreen {
    int type_col[5];                   /* 0x00: type, then colour (a, r, g, b) */
    float xform[7];                 /* 0x14: pivot x, y, move x, y, scale x, y, rotation */
} TfScreen;

/* "TfParts_Screen" task work. */
typedef struct TfScreenTask {
    TexForce *tf;               /* 0x00 */
    void *plane;                /* 0x04: "TfPlane" texture coordinate grid */
} TfScreenTask;

extern TfScreen *tfscreen_table;
extern int tfscreen_pattern;          /* current set */
extern int tfscreen_table_num;

typedef struct TvRect {
    float x;
    float y;
    float w;
    float h;
} TvRect;

struct Sys1Work;
struct Sys1Work;
struct SystemTimeAcc;
struct Time3;
struct VarEditSave;
struct Vec3;
struct SpriteUV;

extern int auto_load_drive;
extern char str_empty[];
extern void *system_texforce_ptm;
void system_info_print(void);
void system_init(void);
void vsync_handler_install(void);
void system_start(void);
void peri_act_pak_shutdown(void);
void system_print_video(void);
void system_state_clear(void);
void system_print_bootrom(void);
void debug_log_save(void);
void debug_keys(void);
void system_debug_update(void);
void system_idle_timer(struct Sys1Work *w);
void system1_task_init(int *w);
void system1_task_exec(struct Sys1Work *w);
void system1_task_disp(void);
void system2_task_init(int w);
void system2_task_exec(void);
void system2_task_disp(void);
void system_tasks_create(void);
void system_kpro_menu_toggle(void);
void system_gs_finish_hook(void);
int vsync_handler(void);
void debug_on(int drive);
void debug_off(int drive);
void system_time_save(struct SystemTimeAcc *out);
void system_time_load(struct Time3 *in);
void debug_print_time(void *win, int x, int y, const char *label, int frames);
void system_debug_disp(void *win);
void system_debug_close(void);
void system_debug_open(void);
void varedit_save_init(struct VarEditSave *w);
void varedit_save_release(struct VarEditSave *w);
void varedit_save_exec(struct VarEditSave *w);
void varedit_save_start(void);
void varedit_load_init(int *w);
void varedit_load_release(void);
void varedit_load_exec(int *w);
void varedit_load_start(void);
void system_pause_disable(void);
void system_pause_enable(void);
void system_set_frame_rate(int rate);
int system_get_frame_rate(void);
void system_load_textures(void);
void system_pnb_entries_init(void);
int system_pnb_entry_get(int i);
void *font_begin(int color, int id);
void font_end(void *buf);
void font_get_glyph(unsigned char ch, int *tex, float *u_left, float *v_top, float *u_right, float *v_bottom);
void *font_put_char(void *buf, unsigned char ch, float (*col)[2], float (*row)[2], int cols,
                    float x, float y, float width, float height, float z);
void font_print(float x, float y, float z, int color, unsigned char *str);
void *font_begin_current(int color);
void font_end_current(void *buf);
void *font_put_char_scaled(void *buf, float x, float y, float z, unsigned char ch, float scale);
void font_build_cols(float (*table)[2], float x, float step, float size);
void font_build_rows(float (*table)[2], float x, float step, float size);
void font_init(void);
void font_put_char3d(unsigned char ch, struct Vec3 *corner, int color);
void font_draw_char3d(unsigned char ch, struct Vec3 *pos, struct IVec3 *rot, struct Vec3 *scale, int color);
void font_print3d(unsigned char *str, struct Vec3 *pos, struct Vec3 *step, struct IVec3 *rot, struct Vec3 *scale, int color);
void font_print3d_rev(unsigned char *str, struct Vec3 *pos, struct Vec3 *step, struct IVec3 *rot, struct Vec3 *scale, int color);
void sprite_draw(float x, float y, float z, int col, int id, int halign, int valign);
void sprite_draw3d(struct Vec3 *pos, float scale, int col, int id, int halign, int valign);
void matrix_rotate_yxz(struct IVec3 *rot);

#endif
