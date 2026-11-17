/*
 * Venus boss hex shell wall, the first part: hexagonal cells round the boss built row by row.
 * Lockable cells slide over the grid; those in front attack with sparks and lasers; when all are
 * shot the grid lines break apart. Also sound helpers and the beat cues the sequence waits on.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "camera.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_shell.h"
#include "venus.h"
#include "venus_mpod.h"
#include "venus_ptcl.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"

extern GameState game_state;
extern BossSe se_table;
extern char venus_shell_vardef[];
extern GwVarDef venus_shell_vardefs[];
extern struct GwVarNode *venus_shell_vars_group;
extern BossVolDef vol_table[];
extern int venus_cue_id;
extern int adx_files[2][2];
extern void (*venus_cue_func)(int arg);
extern int venus_cue_arg;
extern int pcm_files[][2];

/* grid line's two ends, 16 bytes apart (32 bytes) */
typedef struct VenusVecPair {
    Vec3 start;
    char pad_0c[4];
    Vec3 end;                     /* 0x10 */
    char pad_1c[4];
} VenusVecPair;

extern float cmd_hwall_atime;
extern float cmd_hwall_atime2;
extern unsigned int cm_wall_color[];

/* line_draw_vu1 vertex: position and packed colour, 16 bytes */
typedef struct VenusLineVtx {
    Vec3 pos;
    unsigned int color;         /* 0x0c */
} VenusLineVtx;

extern int venus_hwall_front_ofs;
extern float venus_hwall_hex_h;
extern float venus_hwall_hex_w;
extern float venus_hwall_hex_top_w;
extern float venus_hwall_hex_z;
extern float venus_hwall_hex_dz;
extern float venus_hwall_base_y;
extern float venus_hwall_r;
extern float venus_hwall_row_h;
extern float venus_hwall_appear_sec;
extern float venus_hwall_appear_row_sec;
extern float venus_hwall_appear_rnd;
extern float venus_hwall_b_drag;
extern float venus_hwall_b_grav;

/* falling piece of the broken grid, 84 bytes */
typedef struct VenusPiece {
    Vec3 pos;
    int ends[6];
    Vec3 vel;                   /* 0x24 */
    int ang_x, ang_y;                   /* 0x30 */
    char pad_38[4];
    int ang_spd_x, ang_spd_y;                 /* 0x3c */
    int ang_spd_z;              /* 0x44 */
    char pad_48[4];
    int life;                   /* 0x4c */
    int on;                     /* 0x50 */
} VenusPiece;

extern char str_venus_shell_c[];
extern struct TaskDesc venus_hwall_b_desc;
extern float venus_hwall_b_spin;
extern float venus_hwall_b_spd;
extern float venus_hwall_b_spd_rnd;
extern float venus_hwall_b_sec;
extern float venus_hwall_b_sec_rnd;
extern int venus_hwall_ofs_x;
extern int venus_hwall_ofs_y;
extern int cmd_hex_break_sw;
extern float venus_shell_fade_sec;
extern float venus_shell_b_drag;
extern float venus_shell_b_spin_damp;
extern float venus_shell_b_grav;
extern float venus_hwall_appear_sec2;
extern float venus_hwall_appear_sec3;
extern float cmd_hwall_mtime;
extern int venus_hwall_cell_hp;
extern float cmd_hwall_2_wayrate;
extern float cmd_hwall_atime_w;
extern float cmd_hwall_atime_e;
extern float venus_hwall_spring_k;
extern float venus_hwall_spring_damp;
extern float venus_hwall_open_h;
extern int venus_hwall_front_len;
extern int venus_hwall_few;
extern float venus_hwall_act_sec;
extern float cmd_atm_rate;
extern struct TaskDesc venus_hwall_desc;
extern int cmd_hex_test_sw;
extern int venus_hwall_nx;
extern int venus_hwall_ny;
extern float cmd_shell_break_t;
extern float cmd_hex_line_w2;
extern float venus_hwall_fade[4];

extern struct TaskDesc venus_shell_b_desc;
extern float venus_shell_b_spin;
extern float venus_shell_b_spd;
extern float venus_shell_b_spd_rnd;
extern float venus_shell_b_sec;
extern float venus_shell_b_sec_rnd;

typedef struct VenusCell {      /* hex wall cell, 236 bytes */
    char x, y, home_x, home_y;
    short id;
    char pad_06[2];
    int cell_kind;
    int unk_0c;                 /* 0x0c: set to 4 for cells and sub cells by venus_hwall_cell_add, never read */
    int two_way;
    char pad_14[12];
    int ang;
    char pad_24[4];
    Vec3 pos;
    char pad_34[84];
    char lock_arg[12];
    LockObj lock;
    char pad_d4[16];
    struct VenusCell *next;
    struct VenusCell *sub;
} VenusCell;

/* 88 bytes */
typedef struct VenusHwallWork {
    void *pack;
    void *textures;
    Vec3 *pos;
    int *parts;
    ModelPart *shell_a;
    ModelPart *shell_b;
    ModelPart *shell;
    int nx;
    int ny;
    int pl_x;
    int pl_y;
    VenusCell *cells;
    char *first;
    short *grid;
    int act_timer;
    char *boss;
    Vec3 *pts;
    char pad_44[4];
    int cells0;                         /* 0x48: cell groups at the start */
    int timer;
    VenusVecPair *pairs;
    int pair_cnt;
} VenusHwallWork;

/* venus_hwall_b_desc, 16 bytes: the wall's grid lines flying apart */
typedef struct VenusHwallBWork {
    VenusPiece *pieces;
    int n;                      /* 0x04 */
    VenusLineVtx *verts;        /* 0x08 */
    int nv;                     /* 0x0c */
} VenusHwallBWork;

/* venus_shell_b_desc, 76 bytes: a shell piece thrown off when shot */
typedef struct VenusShellBWork {
    void *models;
    void *textures;             /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    Vec3 vel;                   /* 0x14 */
    int ang[3];                 /* 0x20 */
    int spin[3];                /* 0x2c */
    int dang[3];                /* 0x38 */
    ModelPart *model;           /* 0x44 */
    int life;                   /* 0x48 */
} VenusShellBWork;

/* 0x00cae9c0 */
void venus_shell_vars_add(void)
{
    venus_shell_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_shell_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_shell_vars_group, venus_shell_vardefs);
}

/* 0x00caea10 */
void venus_shell_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_shell_vardef);
}

/* 0x00caea28 */
unsigned int venus_hwall_wall_col(int i, float alpha)
{
    unsigned int col = color_lerp(cm_wall_color[i], cm_wall_color[i + 3], beat_curve(1));

    return (col & 0xffffff) | (((int)(color_get_a(col) * alpha) + 1) >> 1 << 24);
}

/* 0x00caead0 */
void venus_hwall_cell_pos(int i, int j, int n, int ny, Vec3 *out, int *ang, float extra_r)
{
    int col_ang = i * 65535 / n;
    float row_ofs;
    float y, x, z;
    float sc[2];

    if (i & 1)
        row_ofs = venus_hwall_hex_h;
    else
        row_ofs = 0.0f;
    y = venus_hwall_base_y + j * venus_hwall_row_h + row_ofs;
    z = venus_hwall_r + extra_r;
    x = 0.0f;
    ROT_Y(col_ang, sc, x, z);
    out->x = x;
    out->y = y;
    out->z = z;
    if (ang)
        *ang = col_ang;
}

/* 0x00caebd0 */
void venus_hwall_pos_cell(Vec3 *pos, int n, int *i, int *j)
{
    int col_ang = RAD_TO_ANG(atan2f(pos->x, pos->z));
    int col_i;
    float row_ofs;

    if (col_ang < 0)
        col_ang += 65535;
    col_i = col_ang * n / 65535 % n;
    if (col_i & 1)
        row_ofs = venus_hwall_hex_h;
    else
        row_ofs = 0.0f;
    {
        float y = pos->y - venus_hwall_base_y - row_ofs;
        float row_h = venus_hwall_row_h;

        *i = col_i;
        *j = y / row_h;
    }
}

/* 0x00caecd0 */
int venus_hwall_cell_dir(int x, int y, int cx, int cy, int nx, int ny)
{
    int dy;
    float slope;

    if (y >= ny)
        y = ny - 1;
    else if (y < 0)
        y = 0;
    x = (x + nx) % nx - cx;
    if (nx >> 1 < x)
        x = -x;
    dy = y - cy;
    slope = (float)dy / (float)x;
    if (x >= 0 && dy >= 0) {
        if (slope <= 1.0f)
            return 0;
        return 1;
    } else if (x < 0 && dy >= 0) {
        if (slope <= -1.0f)
            return 1;
        return 2;
    } else if (x < 0 && dy < 0) {
        if (slope <= 1.0f)
            return 3;
        return 4;
    } else {
        if (slope <= -1.0f)
            return 4;
        return 5;
    }
}

/* 0x00caedd0 */
void venus_hwall_hex_pts(Vec3 *v)
{
    float hex_z = venus_hwall_hex_z;
    float hex_z2 = hex_z - venus_hwall_hex_dz;
    float hex_w = venus_hwall_hex_w;
    float hex_h = venus_hwall_hex_h;
    float hex_top_w = venus_hwall_hex_top_w;

    v[0].z = hex_z2;
    v[0].y = 0.0f;
    v[0].x = hex_w;
    v[1].z = hex_z;
    v[1].y = hex_h;
    v[1].x = hex_top_w;
    v[2].z = hex_z;
    v[2].y = hex_h;
    v[2].x = -hex_top_w;
    v[3].x = -hex_w;
    v[3].y = 0.0f;
    v[3].z = hex_z2;
    v[4].x = -hex_top_w;
    v[4].z = hex_z;
    v[4].y = -hex_h;
    v[5].y = -hex_h;
    v[5].x = hex_top_w;
    v[5].z = hex_z;
    v[6] = v[0];
}

/* 0x00caee70 */
void venus_hwall_hex_lines(BossV4 *verts)
{
    verts[0].y = 0.0f;
    verts[0].x = venus_hwall_hex_w;
    verts[0].z = venus_hwall_hex_z - venus_hwall_hex_dz;
    verts[11] = verts[0];
    verts[1].z = venus_hwall_hex_z;
    verts[1].y = venus_hwall_hex_h;
    verts[1].x = venus_hwall_hex_top_w;
    verts[2] = verts[1];
    verts[3].z = venus_hwall_hex_z;
    verts[3].y = venus_hwall_hex_h;
    verts[3].x = -venus_hwall_hex_top_w;
    verts[4] = verts[3];
    verts[5].z = venus_hwall_hex_z - venus_hwall_hex_dz;
    verts[5].y = 0.0f;
    verts[5].x = -venus_hwall_hex_w;
    verts[6] = verts[5];
    verts[7].z = venus_hwall_hex_z;
    verts[7].y = -venus_hwall_hex_h;
    verts[7].x = -venus_hwall_hex_top_w;
    verts[8] = verts[7];
    verts[9].z = venus_hwall_hex_z;
    verts[9].x = venus_hwall_hex_top_w;
    verts[9].y = -venus_hwall_hex_h;
    verts[10] = verts[9];
}

#ifdef VERSION_US
/*
 * Skips cells hidden behind the boss (eye line within cmd_test_clip_r of the centre); most parts
 * use the overlay's culled dc_object_draw.
 */
extern float cmd_test_clip_r;    /* 100.0f */

/* 0x00caeff8 */
void venus_hwall_cells_draw(void *work)
{
    VenusHwallWork *w = work;
    VenusLineVtx v[16];
    Vec3 eye, zero;
    unsigned int col_mark, col_base;
    char *list, *cell;
    VenusLineVtx *vtx;
    int i;

    col_mark = venus_hwall_wall_col(2, 1.0f);
    col_base = venus_hwall_wall_col(0, 1.0f);
    do {
        zero.x = zero.y = zero.z = 0.0f;
    } while (0);
    camera_get_eye(&eye);
    venus_hwall_hex_lines((BossV4 *)v);
    for (list = w->first; list; list = *(char **)(list + 228)) {
        if (*(int *)(list + 12) == 5) {
            for (cell = list; cell; cell = *(char **)(cell + 232)) {
                if (segment_circle_xz_hit(&eye, (Vec3 *)(cell + 40), &zero, cmd_test_clip_r))
                    continue;
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 28)) {
                        vu0_matrix_rotate_x(ulMatrixCur, *(int *)(cell + 28));
                        if (*(int *)(cell + 16))
                            dc_object_draw_us(w->shell_b);
                        else
                            dc_object_draw_us(w->shell_a);
                    } else {
                        dc_object_draw_us(w->shell);
                    }
                } else {
                    dc_object_draw_us(w->shell);
                }
                ulMatrixPop();
            }
        } else {
            for (cell = list; cell; cell = *(char **)(cell + 232)) {
                if (segment_circle_xz_hit(&eye, (Vec3 *)(cell + 40), &zero, cmd_test_clip_r))
                    continue;
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 28)) {
                        vu0_matrix_rotate_x(ulMatrixCur, *(int *)(cell + 28));
                        if (*(int *)(cell + 16)) {
                            dc_object_draw_us(w->shell_b);
                            for (vtx = v, i = 12; i; i--, vtx++)
                                vtx->color = col_mark;
                        } else {
                            dc_object_draw(w->shell_a);
                            for (vtx = v, i = 12; i; i--, vtx++)
                                vtx->color = col_base;
                        }
                    } else {
                        dc_object_draw_us(w->shell);
                        for (vtx = v, i = 12; i; i--, vtx++)
                            vtx->color = col_base;
                    }
                    line_begin_vu1(0);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                } else {
                    dc_object_draw_us(w->shell);
                }
                ulMatrixPop();
            }
        }
    }
}
#else
/* 0x00caeff8 */
void venus_hwall_cells_draw(void *work)
{
    VenusHwallWork *w = work;
    VenusLineVtx v[16];
    unsigned int col_mark, col_base;
    char *list, *cell;
    VenusLineVtx *vtx;
    int i;

    col_mark = venus_hwall_wall_col(2, 1.0f);
    col_base = venus_hwall_wall_col(0, 1.0f);
    venus_hwall_hex_lines((BossV4 *)v);
    for (list = w->first; list; list = *(char **)(list + 228)) {
        if (*(int *)(list + 12) == 5) {
            for (cell = list; cell; cell = *(char **)(cell + 232)) {
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 28)) {
                        vu0_matrix_rotate_x(ulMatrixCur, *(int *)(cell + 28));
                        if (*(int *)(cell + 16))
                            dc_object_draw(w->shell_b);
                        else
                            dc_object_draw(w->shell_a);
                    } else {
                        dc_object_draw(w->shell);
                    }
                } else {
                    dc_object_draw(w->shell);
                }
                ulMatrixPop();
            }
        } else {
            for (cell = list; cell; cell = *(char **)(cell + 232)) {
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 28)) {
                        vu0_matrix_rotate_x(ulMatrixCur, *(int *)(cell + 28));
                        if (*(int *)(cell + 16)) {
                            dc_object_draw(w->shell_b);
                            for (vtx = v, i = 12; i; i--, vtx++)
                                vtx->color = col_mark;
                        } else {
                            dc_object_draw(w->shell_a);
                            for (vtx = v, i = 12; i; i--, vtx++)
                                vtx->color = col_base;
                        }
                    } else {
                        dc_object_draw(w->shell);
                        for (vtx = v, i = 12; i; i--, vtx++)
                            vtx->color = col_base;
                    }
                    line_begin_vu1(0);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                } else {
                    dc_object_draw(w->shell);
                }
                ulMatrixPop();
            }
        }
    }
}
#endif

/* 0x00caf320 */
int venus_hwall_grid_build(int nx, int ny, Vec3 *out)
{
    Vec3 ctr, pt_a, pt_b;
    float sc[16];
    int i, j, step;
    int num = nx * 2 * (ny + 1) + nx * (ny + 1) * 2;

    if (!out)
        goto end;
    ctr.x = 0.0f;
    ctr.z = venus_hwall_r + venus_hwall_hex_z;
    step = 0xffff / nx;
    for (j = 0; j < ny; j++) {
        ctr.y = venus_hwall_base_y + j * venus_hwall_row_h;
        for (i = 0; i < nx; i++) {
            pt_a.x = ctr.x - venus_hwall_hex_top_w;
            pt_b.x = ctr.x + venus_hwall_hex_top_w;
            pt_b.z = pt_a.z = ctr.z;
            if (i & 1)
                pt_b.y = pt_a.y = ctr.y;
            else
                pt_b.y = pt_a.y = ctr.y - venus_hwall_hex_h;
            ROT_Y(step * i, sc, pt_a.x, pt_a.z);
            ROT_Y(step * i, (sc + 2), pt_b.x, pt_b.z);
            v3_copy(out, &pt_a);
            out++;
            out->x = pt_b.x;
            out->y = pt_b.y;
            out->z = pt_b.z;
            out++;
        }
        if (j == ny - 1) {
            for (i = 0; i < nx; i++) {
                pt_a.x = ctr.x - venus_hwall_hex_top_w;
                pt_b.x = ctr.x + venus_hwall_hex_top_w;
                pt_b.z = pt_a.z = ctr.z;
                if (i & 1)
                    pt_b.y = pt_a.y = ctr.y + venus_hwall_row_h;
                else
                    pt_b.y = pt_a.y = ctr.y + venus_hwall_hex_h;
                ROT_Y(step * i, (sc + 4), pt_a.x, pt_a.z);
                ROT_Y(step * i, (sc + 6), pt_b.x, pt_b.z);
                v3_copy(out, &pt_a);
                out++;
                out->x = pt_b.x;
                out->y = pt_b.y;
                out->z = pt_b.z;
                out++;
            }
        }
    }
    for (i = 0; i < nx; i++) {
        if (i & 1)
            continue;
        ctr.y = venus_hwall_base_y;
        for (j = 0; j <= ny; j++) {
            pt_a.x = ctr.x - venus_hwall_hex_top_w;
            pt_a.z = ctr.z;
            pt_a.y = ctr.y - venus_hwall_hex_h;
            pt_b.x = ctr.x - venus_hwall_hex_w;
            pt_b.z = ctr.z - venus_hwall_hex_dz;
            pt_b.y = ctr.y;
            ROT_Y(step * i, (sc + 8), pt_a.x, pt_a.z);
            ROT_Y(step * i, (sc + 10), pt_b.x, pt_b.z);
            out->x = pt_a.x;
            out->y = pt_a.y;
            out->z = pt_a.z;
            out++;
            out->x = pt_b.x;
            out->y = pt_b.y;
            out->z = pt_b.z;
            out++;
            ctr.y += venus_hwall_row_h;
        }
        ctr.y = venus_hwall_base_y;
        for (j = 0; j <= ny; j++) {
            pt_a.x = ctr.x + venus_hwall_hex_top_w;
            pt_a.z = ctr.z;
            pt_a.y = ctr.y - venus_hwall_hex_h;
            pt_b.x = ctr.x + venus_hwall_hex_w;
            pt_b.z = ctr.z - venus_hwall_hex_dz;
            pt_b.y = ctr.y;
            ROT_Y(step * i, (sc + 12), pt_a.x, pt_a.z);
            ROT_Y(step * i, (sc + 14), pt_b.x, pt_b.z);
            out->x = pt_a.x;
            out->y = pt_a.y;
            out->z = pt_a.z;
            out++;
            out->x = pt_b.x;
            out->y = pt_b.y;
            out->z = pt_b.z;
            out++;
            ctr.y += venus_hwall_row_h;
        }
    }
end:
    return num;
}

/* 0x00caf9f8 */
void venus_hwall_lines_draw(VenusLineVtx *v, int n)
{
    unsigned int col = venus_hwall_wall_col(1, 1.0f);
    int i;

    for (i = 0; i < n; i++)
        v[i].color = col;
    line_begin_vu1(0);
    line_draw_vu1(v, n);
    line_end_vu1();
}

/* 0x00cafa78 */
int venus_hwall_grid_line_num(int nx, int ny)
{
    int i, j, n = 0;

    for (i = 0; i < ny + 1; i++)
        n += (nx * 2 - 1) * 2;
    n += ((ny + 1) * 2 - 1) * 2;
    for (j = 0; j < nx - 1; j++)
        n += ny * 2;
    return n;
}

/* 0x00cafb00 */
VenusVecPair *venus_hwall_strip_pairs(VenusVecPair *pair, Vec3 *src, int n)
{
    Vec3 *start_pt = src++;

    for (n--; n; n--) {
        v3_copy(&pair->start, start_pt);
        start_pt = src;
        v3_copy(&pair->end, src);
        src++;
        pair++;
    }
    return pair;
}

/* 0x00cafb68 */
VenusVecPair *venus_hwall_seg_pairs(VenusVecPair *pair, Vec3 *src, int n)
{
    for (; n > 0; n -= 2) {
        Vec3 *start_pt = src++;
        Vec3 *end_pt = src++;
        v3_copy(&pair->start, start_pt);
        v3_copy(&pair->end, end_pt);
        pair++;
    }
    return pair;
}

/* 0x00cafbc8 */
void venus_hwall_grid_pairs(VenusVecPair *pair, Vec3 *src, int nx, int ny)
{
    int i, j;

    for (i = 0; i < ny + 1; i++) {
        pair = venus_hwall_strip_pairs(pair, src, nx * 2);
        src += nx * 2;
    }
    pair = venus_hwall_strip_pairs(pair, src, (ny + 1) * 2);
    src += (ny + 1) * 2;
    for (j = 0; j < nx - 1; j++) {
        pair = venus_hwall_seg_pairs(pair, src + 1, ny * 2);
        src += (ny + 1) * 2;
    }
}

/* 0x00cafcc0 */
int venus_hwall_cell_step(short *grid, int *px, int *py, int nc, int nr, int dir)
{
    int x = *px, y = *py;

    if (x & 1) {
        switch (dir) {
        case 0:
            if (++y >= nr)
                return 0;
            x = (x + 1) % nc;
            break;
        case 1:
            if (++y >= nr)
                return 0;
            break;
        case 2:
            if (++y >= nr)
                return 0;
            { int wrap_back = nc - 1; x = (x + wrap_back) % nc; }
            break;
        case 3:
            { int wrap_back = nc - 1; x = (x + wrap_back) % nc; }
            break;
        case 4:
            if (--y < 0)
                return 0;
            break;
        case 5:
            x = (x + 1) % nc;
            break;
        }
    } else {
        switch (dir) {
        case 0:
            x = (x + 1) % nc;
            break;
        case 1:
            if (++y >= nr)
                return 0;
            break;
        case 2:
            { int wrap_back = nc - 1; x = (x + wrap_back) % nc; }
            break;
        case 3:
            if (--y < 0)
                return 0;
            { int wrap_back = nc - 1; x = (x + wrap_back) % nc; }
            break;
        case 4:
            if (--y < 0)
                return 0;
            break;
        case 5:
            if (--y < 0)
                return 0;
            x = (x + 1) % nc;
            break;
        }
    }
    if (grid) {
        if (grid[y * nc + x])
            return 0;
        *px = x;
        *py = y;
        return 1;
    }
    *px = x;
    *py = y;
    return 1;
}

/* 0x00cafe28 */
void venus_hwall_cell_add(VenusHwallWork *w, int i, int j)
{
    int nb[2];
    VenusCell *cell, *sub;
    int id, k, idx;
    Vec3 *pos;
    LockObj *lk;
    VenusCell *tail;

    if (w->grid[j * w->nx + i])
        return;
    cell = w->cells + (j * w->nx + i);
    if ((tail = (VenusCell *)w->first) != 0) {
        while (tail->next)
            tail = tail->next;
        tail->next = cell;
    } else {
        w->first = (char *)cell;
    }
    id = j * w->nx + i + 1;
    cell->cell_kind = 3;
    cell->unk_0c = 4;
    cell->x = i;
    cell->home_x = i;
    cell->y = j;
    cell->home_y = j;
    cell->id = id;
    w->grid[j * w->nx + i] = id;
    cell->next = 0;
    cell->sub = 0;
    lk = &cell->lock;
    pos = &cell->pos;
    lock_init(3, lk, w, pos, (int)cell->lock_arg, venus_hwall_cell_hp, 1);
    lockon_entry(lk);
    cell->lock.flags &= ~LOCK_LOCKABLE;
    venus_hwall_cell_pos(cell->x, cell->y, w->nx, w->ny, pos, &cell->ang, 0.0f);
    if (RAND_FLOAT_A() < cmd_hwall_2_wayrate)
        cell->two_way = 1;
    else
        cell->two_way = 0;
    for (k = 0; k < 6; k++) {
        nb[0] = i;
        nb[1] = j;
        venus_hwall_cell_step(0, &nb[0], &nb[1], w->nx, w->ny, k);
        idx = nb[1] * w->nx + nb[0];
        if (w->grid[idx])
            continue;
        sub = w->cells + idx;
        w->grid[idx] = id;
        sub->cell_kind = 1;
        sub->unk_0c = 4;
        sub->id = id;
        sub->home_x = sub->x = nb[0];
        sub->home_y = sub->y = nb[1];
        venus_hwall_cell_pos(sub->x, sub->y, w->nx, w->ny, &sub->pos, &sub->ang, 0.0f);
        cell->sub = sub;
        sub->sub = 0;
        sub->next = 0;
        cell = sub;
    }
}

/* 0x00cb0120 */
void venus_hwall_cell_break(VenusHwallWork *w, char *cell)
{
    int wide = *(int *)(cell + 12) == 5;

    lock_remove_entry(3, (LockObj *)(cell + 148));
    for (; cell; cell = *(char **)(cell + 232)) {
        w->grid[cell[1] * w->nx + cell[0]] = 0;
        if (wide)
            w->grid[cell[3] * w->nx + cell[2]] = 0;
        if (*(int *)(cell + 8) & 2) {
            int model = *(int *)(cell + 16) ? (int)w->shell_b : (int)w->shell_a;

            venus_shell_b_create(w, (Vec3 *)(cell + 40), (Vec3 *)(cell + 28), model);
        } else
            venus_shell_b_create(w, (Vec3 *)(cell + 40), (Vec3 *)(cell + 28), (int)w->shell);
    }
}

/* 0x00cb0210 */
int venus_hwall_cell_move(VenusHwallWork *w, signed char *cell, int dir)
{
    signed char *head;
    short id;
    int x, y;
    short occ;
    signed char ox, oy;
    unsigned char unx, uny;

    if (*(int *)(cell + 12) != 4)
        return 0;
    head = cell;
    id = *(short *)(cell + 4);
    for (; cell; cell = *(signed char **)(cell + 232)) {
        x = cell[0];
        y = cell[1];
        if (!venus_hwall_cell_step(0, &x, &y, w->nx, w->ny, dir))
            return 0;
        occ = w->grid[y * w->nx + x];
        if (!occ || occ == id) {
            cell[2] = x;
            cell[3] = y;
            continue;
        }
        return 0;
    }
    for (cell = head; cell; cell = *(signed char **)(cell + 232)) {
        unx = ((unsigned char *)cell)[2];
        uny = ((unsigned char *)cell)[3];
        ox = cell[0];
        oy = cell[1];
        cell[0] = unx;
        cell[3] = oy;
        cell[2] = ox;
        cell[1] = uny;
        w->grid[(signed char)uny * w->nx + (signed char)unx] = *(short *)(cell + 4);
        *(int *)(cell + 20) = *(int *)(cell + 24) = cmd_hwall_mtime * FPS;
        *(int *)(cell + 12) = 5;
    }
    return 1;
}

/* 0x00cb03a0 */
int venus_hwall_cells_slide(int *work)
{
    VenusHwallWork *w = work;
    Vec3 from, to;
    int ang_from, ang_to;
    float pitch_sin, pitch_cos, yaw_sin, yaw_cos;
    char *grp, *cell;
    int moved, id;
    float rate;

    for (grp = w->first; grp; grp = *(char **)(grp + 228)) {
        if (*(int *)(grp + 12) == 5) {
            cell = grp;
            id = *(short *)(grp + 4);
            moved = 0;
            for (; cell; cell = *(char **)(cell + 232)) {
                if (--*(int *)(cell + 20) <= 0) {
                    *(int *)(cell + 12) = 4;
                    moved = 1;
                    w->grid[cell[3] * w->nx + cell[2]] = 0;
                    venus_hwall_cell_pos(cell[0], cell[1], w->nx, w->ny, (Vec3 *)(cell + 40), (int *)(cell + 32), 0.0f);
                } else {
                    rate = (float)*(int *)(cell + 20) / (float)*(int *)(cell + 24);
                    venus_hwall_cell_pos(cell[2], cell[3], w->nx, w->ny, &from, &ang_from, 0.0f);
                    venus_hwall_cell_pos(cell[0], cell[1], w->nx, w->ny, &to, &ang_to, 0.0f);
                    v3_lerp((Vec3 *)(cell + 40), &from, &to, rate);
                    if (cell[2] == 0 && cell[0] == w->nx - 1)
                        ang_from = 0xffff;
                    else if (cell[0] == 0 && cell[2] == w->nx - 1)
                        ang_to = 0xffff;
                    *(int *)(cell + 32) = (float)ang_from * rate + (float)ang_to * (1.0f - rate);
                }
            }
            if (moved) {
                for (cell = grp; cell; cell = *(char **)(cell + 232))
                    w->grid[cell[1] * w->nx + cell[0]] = id;
            }
        }
        {
            int ax = *(int *)(grp + 28);

            *(float *)(grp + 136) = *(float *)(grp + 140) = 0.0f;
            *(float *)(grp + 144) = 1.0f;
            ROTS_X(ax, pitch_sin, pitch_cos, *(float *)(grp + 140), *(float *)(grp + 144));
        }
        ROTS_Y(*(int *)(grp + 32), yaw_sin, yaw_cos, *(float *)(grp + 136), *(float *)(grp + 144));
    }
    return 1;
}

/* 0x00cb06e8 */
void venus_hwall_cell_pose(char *w)
{
    venus_hwall_hex_pts((Vec3 *)(w + 52));
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS((Vec3 *)(w + 40));
    vu0_matrix_rotate_y(ulMatrixCur, *(int *)(w + 32));
    matrix_apply_points((float *)(w + 52), (float *)(w + 52), 7);
    ulMatrixPop();
}

/* 0x00cb0768 */
void venus_hwall_lines_partial(Vec3 *strip, Vec3 *out, float t)
{
    int segs = t;
    float frac = t - segs;
    Vec3 seg[2];

    if (segs)
        lines_add_strip(strip, segs + 1);
    if (!(fabsf(frac) < FLT_EPSILON)) {
        v3_copy(&seg[0], &strip[segs]);
        v3_lerp(&seg[1], &strip[segs + 1], &strip[segs], frac);
        lines_add_pairs(seg, 2);
    }
    if (out) {
        if (!(fabsf(frac) < FLT_EPSILON))
            v3_copy(out, &seg[1]);
        else
            v3_copy(out, &strip[segs]);
    }
}

/* 0x00cb08d0 */
int venus_hwall_in_front(VenusHwallWork *w, signed char *cell, int len)
{
    int col, start, end;

    start = (w->pl_x + venus_hwall_front_ofs) % w->nx;
    col = start;
    end = col + len;
    if (end >= w->nx) {
        if (!(*cell < col && end - w->nx < *cell))
            return 1;
    } else {
        do {
            col = *cell - start;
        } while (0);
        if (col >= 0 && col <= len)
            return 1;
    }
    return 0;
}

/* 0x00cb0950 */
int venus_hwall_cell_start_attack(int owner, int *grp)
{
    if (grp[3] != 4)
        return 0;
    grp[3] = 6;
    if (grp[4])
        grp[5] = grp[6] = cmd_hwall_atime2 * FPS;
    else
        grp[5] = grp[6] = cmd_hwall_atime * FPS;
    grp[41] |= 0x100;
    return 1;
}

/* 0x00cb09c0 */
int venus_hwall_cells_attack(int *work)
{
    VenusHwallWork *w = work;
    char *grp;
    float rate;

    for (grp = w->first; grp; grp = *(char **)(grp + 228)) {
        switch (*(int *)(grp + 12)) {
        case 6:
            if (--*(int *)(grp + 20) <= 0) {
                *(int *)(grp + 20) = *(int *)(grp + 24) = cmd_hwall_atime_w * FPS;
                *(int *)(grp + 12) = 7;
                *(int *)(grp + 28) = 0x8000;
                *(int **)(grp + 216) = venus_ptcl_ex_create(w, (Vec3 *)(grp + 40), *(int *)(grp + 16) + 1);
            } else {
                rate = 1.0f - (float)*(int *)(grp + 20) / (float)*(int *)(grp + 24);
                *(int *)(grp + 28) = DEG_TO_ANG(rate * 180.0f);
            }
            break;
        case 7:
            *(int *)(grp + 28) = 0x8000;
            if (*(int *)(grp + 20) == 180) {
                *(int **)(grp + 212) = venus_l_ex_create(w, (Vec3 *)(grp + 40), 180, *(int *)(grp + 16) + 1);
                if (!*(int *)(grp + 16)) {
                    boss_se_play(4, 0, (int)(grp + 40));
                } else {
                    boss_se_play(7, 0, (int)(grp + 40));
                }
            }
            if (--*(int *)(grp + 20) <= 0) {
                *(int *)(grp + 20) = *(int *)(grp + 24) = cmd_hwall_atime_e * FPS;
                *(int *)(grp + 12) = 8;
            }
            if (*(int *)(grp + 212))
                spring_to((float *)(grp + 220), (float *)(grp + 224), venus_hwall_open_h, venus_hwall_spring_k, venus_hwall_spring_damp);
            else
                spring_step((float *)(grp + 220), (float *)(grp + 224), venus_hwall_spring_k, venus_hwall_spring_damp);
            venus_hwall_cell_pos(grp[0], grp[1], w->nx, w->ny, (Vec3 *)(grp + 40), 0, *(float *)(grp + 224));
            break;
        case 8:
            spring_step((float *)(grp + 220), (float *)(grp + 224), venus_hwall_spring_k, venus_hwall_spring_damp);
            venus_hwall_cell_pos(grp[0], grp[1], w->nx, w->ny, (Vec3 *)(grp + 40), 0, *(float *)(grp + 224));
            if (--*(int *)(grp + 20) <= 0) {
                *(int *)(grp + 28) = 0;
                *(int *)(grp + 12) = 4;
                ((LockObj *)(grp + 148))->flags &= ~LOCK_LOCKABLE;
            } else {
                rate = (float)*(int *)(grp + 20) / (float)*(int *)(grp + 24);
                *(int *)(grp + 28) = DEG_TO_ANG(rate * 180.0f);
            }
            break;
        }
    }
    return 1;
}

/* 0x00cb0cb8 */
int venus_hwall_pick_action(VenusHwallWork *w)
{
    signed char *grp;
    int cnt, in_front, few;
    float rate, cnt_f;

    if (--w->act_timer <= 0) {
        cnt = 0;
        if ((grp = (signed char *)w->first) != 0) {
            for (; grp; grp = *(signed char **)(grp + 228))
                cnt++;
            few = cnt < venus_hwall_few;
            /* random cell; cnt_f keeps the count for the attack rate */
            cnt = RAND_FLOAT_A() * (cnt_f = cnt);
            for (grp = (signed char *)w->first; cnt; cnt--)
                grp = *(signed char **)(grp + 228);
            rate = cmd_atm_rate * cnt_f / w->cells0;
            in_front = few ? venus_hwall_in_front(w, grp, 2) : venus_hwall_in_front(w, grp, venus_hwall_front_len);
            if (in_front && rate < RAND_FLOAT_A())
                venus_hwall_cell_start_attack((int)w, (int *)grp);
            else
                venus_hwall_cell_move(w, grp, venus_hwall_cell_dir(w->pl_x, w->pl_y, grp[0], grp[1], w->nx, w->ny));
        }
        w->act_timer = venus_hwall_act_sec * FPS;
    }
    return 1;
}

/* 0x00cb0eb8 */
int venus_hwall_cells_hit(int *work)
{
    VenusHwallWork *w = work;
    char *grp, *prev;
    LockObj *lock;

    prev = 0;
    grp = w->first;
    while (grp) {
        if (*(int *)(grp + 212) && (TASK_OF(*(int *)(grp + 212))->flags & 1))
            *(int *)(grp + 212) = 0;
        if (*(int *)(grp + 216) && (TASK_OF(*(int *)(grp + 216))->flags & 1))
            *(int *)(grp + 216) = 0;
        if (w->timer) {
            if (!(((LockObj *)(grp + 148))->flags & LOCK_HIT))
                goto keep;
            if (((LockObj *)(grp + 148))->life > 0)
                goto hit;
            if (!*(int *)(grp + 16)) {
                boss_se_play(6, 0, (int)(grp + 40));
            } else {
                boss_se_play(9, 0, (int)(grp + 40));
            }
            lock_score(3, (LockObj *)(grp + 148));
            venus_player_stop_cb(w->boss, 1);
        }
        lock = (LockObj *)(grp + 148);
        if (*(int *)(grp + 212))
            venus_l_ex_finish(*(void **)(grp + 212), 0);
        if (*(int *)(grp + 216))
            venus_ptcl_ex_hide(*(VenusTaskList **)(grp + 216), 0);
        lock_remove_entry(3, lock);
        if (prev) {
            *(char **)(prev + 228) = *(char **)(grp + 228);
            venus_hwall_cell_break((int *)w, grp);
            grp = *(char **)(grp + 228);
        } else if (*(char **)(grp + 228)) {
            venus_hwall_cell_break((int *)w, grp);
            grp = w->first = *(char **)(grp + 228);
        } else {
            w->first = 0;
            venus_hwall_cell_break((int *)w, grp);
            return 0;
        }
        continue;
    hit:
        if (!*(int *)(grp + 16))
            boss_se_play(5, 0, (int)(grp + 40));
        else
            boss_se_play(8, 0, (int)(grp + 40));
    keep:
        prev = grp;
        grp = *(char **)(grp + 228);
    }
    return 1;
}

/* 0x00cb1080 */
void venus_hwall_exec(void *work)
{
    VenusHwallWork *w = work;
    Vec3 pl;
    int n;

    player_get_pos(&pl);
    venus_hwall_pos_cell(&pl, w->nx, &w->pl_x, &w->pl_y);
    w->pl_x = (w->pl_x + venus_hwall_ofs_x + w->nx) % w->nx;
    w->pl_y = (w->pl_y + venus_hwall_ofs_y + w->ny) % w->ny;
    for (n = enemy_status_get_ticks(); n; n--) {
        venus_hwall_pick_action(w);
        venus_hwall_cells_slide(w);
        venus_hwall_cells_attack(w);
        if (w->timer)
            w->timer--;
    }
    venus_hwall_cells_hit(w);
    if (!w->first) {
        TASK_OF(w)->exec = 0;
        venus_seq_regist(w->boss, 1, 2);
    }
    if (cmd_hex_break_sw) {
        w->timer = 0;
        cmd_hex_break_sw = 0;
    }
}

/* 0x00cb11a0 */
void venus_hwall_disp(void *w)
{
    ulGxSetTextureList((void *)((int *)w)[1]);
    venus_hwall_cells_draw(w);
    venus_hwall_lines_draw((VenusLineVtx *)((int *)w)[20], ((int *)w)[21]);
}

static inline void venus_set_color(VenusLineVtx *vtx, unsigned int col)
{
    int i;
    for (i = 12; i; i--, vtx++)
        vtx->color = col;
}

/* steer(cast) */
/* 0x00cb11d8 */
void venus_hwall_disp_appear(void *work)
{
    VenusHwallWork *w = work;
    Vec3 pts[8];
    VenusLineVtx v[16];
#ifdef VERSION_US
    Vec3 eye, zero;
#endif
    unsigned int col_base, col_line, col_line_clear, col_mark, col;
    char *grp, *cell;
    VenusLineVtx *vtx;
    int i, old_blend;
    float rate;

    col_base = venus_hwall_wall_col(0, 1.0f);
    col_line_clear = venus_hwall_wall_col(1, 0.0f);
    col_line = venus_hwall_wall_col(1, 1.0f);
    col_mark = venus_hwall_wall_col(2, 1.0f);
#ifdef VERSION_US
    do {
        zero.x = zero.y = zero.z = 0.0f;
    } while (0);
    camera_get_eye(&eye);
#endif
    venus_hwall_hex_pts(pts);
    venus_hwall_hex_lines((BossV4 *)v);
    ulGxSetTextureList(w->textures);
    for (grp = w->first; grp; grp = *(char **)(grp + 228)) {
        for (cell = grp; cell; cell = *(char **)(cell + 232)) {
#ifdef VERSION_US
            if (segment_circle_xz_hit(&eye, (Vec3 *)(cell + 40), &zero, cmd_test_clip_r))
                continue;
#endif
            if (*(int *)(cell + 12) == 2) {
                lines_begin(cmd_hex_line_w2, col_line, 1);
                venus_hwall_lines_partial((Vec3 *)(cell + 52), 0, (1.0f - (float)*(int *)(cell + 20) / (float)*(int *)(cell + 24)) * 6.0f);
                lines_end();
            } else if (*(int *)(cell + 12) == 3) {
                do {
                    rate = (float)*(int *)(cell + 20) / (float)*(int *)(cell + 24);
                    lines_begin(cmd_hex_line_w2, venus_hwall_wall_col(1, rate), 1);
                    venus_hwall_lines_partial((Vec3 *)(cell + 52), 0, 6.0f);
                } while (0);
                lines_end();
                ulMatrixPush();
                venus_hwall_fade[0] = -rate;
                old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
                draw_push_override_color();
                draw_push_strip_flags();
                ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
                draw_set_override_color(venus_hwall_fade);
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                dc_object_draw_us(w->shell);
                draw_pop_override_color();
                draw_pop_strip_flags();
                draw_set_override_flags(old_blend);
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 16)) {
                        col = (col_mark & 0xffffff) | (((int)(color_get_a(col_mark) * (1.0f - rate)) + 1) >> 1 << 24);
                        for (vtx = v, i = 12; i; i--, vtx++)
                            vtx->color = col;
                    } else {
                        col = (col_base & 0xffffff) | (((int)(color_get_a(col_base) * (1.0f - rate)) + 1) >> 1 << 24);
                        for (vtx = v, i = 12; i; i--, vtx++)
                            vtx->color = col;
                    }
                    line_begin_vu1(1);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                } else {
                    col = (col_line_clear & 0xffffff) | (((int)(color_get_a(col_line_clear) * (1.0f - rate)) + 1) >> 1 << 24);
                    for (vtx = v, i = 12; i; i--, vtx++)
                        vtx->color = col;
                    line_begin_vu1(1);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                }
                ulMatrixPop();
            } else if (*(int *)(cell + 12) == 4) {
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(cell + 40));
                vu0_matrix_rotate_y(ulMatrixCur, *(int *)(cell + 32));
                dc_object_draw_us(w->shell);
                if (*(int *)(cell + 8) & 2) {
                    if (*(int *)(cell + 16)) {
                        venus_set_color(v, col_mark);
                    } else {
                        venus_set_color(v, col_base);
                    }
                    line_begin_vu1(0);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                } else {
                    venus_set_color(v, col_line);
                    line_begin_vu1(0);
                    line_draw_vu1(v, 12);
                    line_end_vu1();
                }
                ulMatrixPop();
            }
        }
    }
}

/* 0x00cb1730 */
int venus_hwall_appear_step(void *work)
{
    VenusHwallWork *w = work;
    char *grp, *cell;
    int done = 1;

    for (grp = w->first; grp; grp = *(char **)(grp + 228)) {
        for (cell = grp; cell; cell = *(char **)(cell + 232)) {
            switch (*(int *)(cell + 12)) {
            case 1:
                done = 0;
                if (--*(int *)(cell + 20) <= 0) {
                    *(int *)(cell + 20) = *(int *)(cell + 24) = venus_hwall_appear_sec2 * FPS;
                    *(int *)(cell + 12) = 2;
                    venus_hwall_cell_pose(cell);
                }
                break;
            case 2:
                done = 0;
                if (--*(int *)(cell + 20) <= 0) {
                    *(int *)(cell + 20) = *(int *)(cell + 24) = venus_hwall_appear_sec3 * FPS;
                    *(int *)(cell + 12) = 3;
                }
                break;
            case 3:
                done = 0;
                if (--*(int *)(cell + 20) <= 0) {
                    *(int *)(cell + 20) = *(int *)(cell + 24) = 0;
                    *(int *)(cell + 12) = 4;
                }
                break;
            }
        }
    }
    return done;
}

/* 0x00cb18b0 */
void venus_hwall_exec_appear(void *w)
{
    if (venus_hwall_appear_step(w)) {
        TASK_OF(w)->exec = venus_hwall_exec;
        TASK_OF(w)->disp = venus_hwall_disp;
    }
}

/* 0x00cb18f0 */
void venus_hwall_appear_start(VenusHwallWork *w)
{
    char *grp, *cell;

    for (grp = w->first; grp; grp = *(char **)(grp + 228)) {
        for (cell = grp; cell; cell = *(char **)(cell + 232)) {
            *(int *)(cell + 20) = *(int *)(cell + 24) = (venus_hwall_appear_sec + cell[1] * venus_hwall_appear_row_sec + RAND_FLOAT_A() * venus_hwall_appear_rnd) * FPS;
            *(int *)(cell + 12) = 1;
        }
    }
}

/* 0x00cb19f0 */
void venus_hwall_release(VenusHwallWork *w)
{
    char *grp;

    for (grp = w->first; grp; grp = *(char **)(grp + 228))
        lock_remove_entry(3, (LockObj *)(grp + 148));
    heap_free(w->cells, 0);
    heap_free(w->grid, 0);
    heap_free(w->pts, 0);
    heap_free(w->pairs, 0);
}

/* 0x00cb1a70 */
void *venus_hwall_create(void *parent, Vec3 *pos, int *parts)
{
    int *w = task_create_inherit(parent, &venus_hwall_desc, 88);
    int n, i, j;
    char *grp;
    Vec3 *pts;

    w[6] = parts[0];
    w[4] = parts[1];
    w[3] = (int)parts;
    w[5] = parts[2];
    w[2] = (int)pos;
    w[7] = venus_hwall_nx;
    w[8] = venus_hwall_ny;
    *(void **)&w[15] = parent;
    w[11] = (int)heap_block_tag(game_calloc(w[7] * 236 * w[8]), str_venus_shell_c, SRC_LINE(1662, 1709));
    w[13] = (int)heap_block_tag(game_calloc(2 * w[7] * w[8]), str_venus_shell_c, SRC_LINE(1663, 1710));
    w[19] = cmd_shell_break_t * FPS;
    n = venus_hwall_grid_build(w[7], w[8], 0);
    pts = heap_block_tag(game_calloc(n * 12), str_venus_shell_c, SRC_LINE(1668, 1715));
    w[16] = (int)pts;
    venus_hwall_grid_build(w[7], w[8], pts);
    w[21] = venus_hwall_grid_line_num(w[7], w[8]);
    w[20] = (int)heap_block_tag(game_calloc(w[21] << 4), str_venus_shell_c, SRC_LINE(1672, 1719));
    venus_hwall_grid_pairs((VenusVecPair *)w[20], (Vec3 *)w[16], w[7], w[8]);
    if (cmd_hex_test_sw)
        n = 2;
    else
        n = w[7] * w[8] / 4;
    while (n) {
        n--;
        i = RAND_FLOAT_A() * w[7];
        j = RAND_FLOAT_A() * w[8];
        venus_hwall_cell_add((VenusHwallWork *)w, i, j);
    }
    for (j = 0; j < w[8]; j++) {
        for (i = 0; i < w[7]; i++) {
            if (!((short *)w[13])[j * w[7] + i])
                venus_hwall_cell_add((VenusHwallWork *)w, i, j);
        }
    }
    w[18] = 0;
    if (w[12]) {
        int cnt = 0;

        for (grp = (char *)w[12]; grp; grp = *(char **)(grp + 228))
            cnt++;
        w[18] = cnt;
    }
    venus_hwall_appear_start((VenusHwallWork *)w);
    TASK_OF(w)->exec = venus_hwall_exec_appear;
    TASK_OF(w)->disp = venus_hwall_disp_appear;
    return w;
}

/* 0x00cb1da8 */
void venus_hwall_b_piece_init(int pts, VenusPiece *piece, int idx, float spin, float spd, float spd_rnd, float sec, float sec_rnd)
{
    Vec3 half;
    float len;

    v3_lerp(&piece->pos, (Vec3 *)pts + idx, (Vec3 *)pts + idx + 1, 0.5f);
    v3_sub(&half, (Vec3 *)pts + idx, &piece->pos);
    len = vec3_length(&half);
    *(float *)&piece->ends[2] = len;
    *(float *)&piece->ends[5] = -len;
    do {
        piece->ang_y = RAD_TO_ANG(atan2f(-half.x, -half.z));
        piece->ang_x = RAD_TO_ANG(atan2f(half.y, sqrtf(half.x * half.x + half.z * half.z)));
    } while (0);
    piece->ang_spd_x = DEG_TO_ANG(RAND_FLOAT_A() * (spin + spin) - spin);
    piece->ang_spd_y = DEG_TO_ANG(RAND_FLOAT_A() * (spin + spin) - spin);
    do {
        piece->ang_spd_z = DEG_TO_ANG(RAND_FLOAT_A() * (spin + spin) - spin);
    } while (0);
    piece->vel.x = piece->pos.x;
    piece->vel.y = 0.0f;
    piece->vel.z = piece->pos.z;
    vec_set_length(&piece->vel, spd + RAND_FLOAT_A() * spd_rnd);
    piece->life = (sec + RAND_FLOAT_A() * sec_rnd) * FPS;
    piece->on = 1;
}

/* 0x00cb2080 */
int venus_hwall_b_pieces_build(int pts, VenusPiece *piece, int nx, int ny, float spin, float spd, float spd_rnd, float sec, float sec_rnd)
{
    int n = 0;
    int i, j, k;

    for (i = 0; i < ny + 1; i++) {
        for (j = 0; j < nx * 2 - 1; j++) {
            if (piece) {
                venus_hwall_b_piece_init(pts, piece, j, spin, spd, spd_rnd, sec, sec_rnd);
                piece++;
            }
            n++;
        }
        pts += nx * 24;
    }
    for (j = 0; j < (ny + 1) * 2 - 1; j++) {
        if (piece) {
            venus_hwall_b_piece_init(pts, piece, j, spin, spd, spd_rnd, sec, sec_rnd);
            piece++;
        }
        n++;
    }
    pts += (ny + 1) * 24;
    for (k = 0; k < nx - 1; k++) {
        for (j = 0; j < ny; j++) {
            if (piece) {
                venus_hwall_b_piece_init(pts, piece, j * 2, spin, spd, spd_rnd, sec, sec_rnd);
                piece++;
            }
            n++;
        }
        pts += (ny + 1) * 24;
    }
    return n;
}

/* 0x00cb22a8 */
void venus_hwall_b_exec(VenusHwallBWork *w)
{
    VenusPiece *piece = w->pieces;
    int n = w->n;
    int any = 0;
    float drag, grav;

    if (n) {
        drag = venus_hwall_b_drag;
        grav = venus_hwall_b_grav;
        for (; n; n--, piece++) {
            if (!piece->on)
                continue;
            any = 1;
            v3_scale(&piece->vel, drag);
            piece->vel.y -= grav;
            piece->ang_spd_x *= drag;
            piece->ang_spd_y *= drag;
            v3_sum(&piece->pos, &piece->vel, &piece->pos);
            piece->ang_x += piece->ang_spd_x;
            piece->ang_y += piece->ang_spd_y;
            if (--piece->life <= 0)
                piece->on = 0;
        }
    }
    if (!any)
        FINISH(w);
}

/* 0x00cb23b0 */
void venus_hwall_b_disp(VenusHwallBWork *w)
{
    int fade_frm = venus_shell_fade_sec * FPS;
    VenusPiece *piece;
    VenusLineVtx *vtx;
    unsigned int col;
    int n;

    w->nv = 0;
    n = w->n;
    piece = w->pieces;
    ulMatrixPush();
    matrix_unit_set(0);
    for (; n; n--, piece++) {
        if (!piece->on)
            continue;
        ulMatrixPush();
        MTX_TRANS(&piece->pos);
        venus_shell_rotate_yxz(&piece->ang_x);
        if (piece->life < fade_frm) {
            col = (cm_wall_color[6] & 0xffffff) | (((int)(color_get_a(cm_wall_color[6]) * (float)piece->life / (float)fade_frm) + 1) >> 1 << 24);
            vtx = w->verts;
            vtx[w->nv].color = vtx[w->nv + 1].color = col;
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&piece->ends[0], (float *)(vtx + w->nv));
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&piece->ends[3], (float *)(w->verts + (w->nv + 1)));
            w->nv += 2;
        } else {
            vtx = w->verts;
            vtx[w->nv].color = vtx[w->nv + 1].color = cm_wall_color[6];
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&piece->ends[0], (float *)(vtx + w->nv));
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&piece->ends[3], (float *)(w->verts + (w->nv + 1)));
            w->nv += 2;
        }
        ulMatrixPop();
    }
    ulMatrixPop();
    line_begin_vu1(1);
    line_draw_vu1((struct WireLineVtx *)w->verts, w->nv);
    line_end_vu1();
}

/* 0x00cb2588 */
void venus_hwall_b_release(VenusHwallBWork *w)
{
    heap_free(w->pieces, 0);
    heap_free(w->verts, 0);
}

/* 0x00cb25c0 */
int *venus_hwall_b_create(int parent, int pts, int nx, int ny)
{
    VenusHwallBWork *w = task_create_inherit((void *)parent, &venus_hwall_b_desc, 16);

    w->n = venus_hwall_b_pieces_build(pts, 0, nx, ny, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    w->pieces = heap_block_tag(game_calloc(w->n * 84), str_venus_shell_c, SRC_LINE(1905, 1952));
    w->verts = heap_block_tag(game_calloc((w->n + 1) * 32), str_venus_shell_c, SRC_LINE(1907, 1954));
    venus_hwall_b_pieces_build(pts, w->pieces, nx, ny, venus_hwall_b_spin, venus_hwall_b_spd, venus_hwall_b_spd_rnd, venus_hwall_b_sec, venus_hwall_b_sec_rnd);
    return (int *)w;
}

/* 0x00cb26d8 */
void venus_hwall_shatter(int parent, char *wall)
{
    int buf[3];

    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    boss_se_play(10, 0, (int)buf);
    /* never taken */
    if (((int)wall & 1) >> 1)
        buf[0] = 1;
    venus_hwall_b_create(parent, (int)((VenusHwallWork *)wall)->pts, ((VenusHwallWork *)wall)->nx, ((VenusHwallWork *)wall)->ny);
}

/* 0x00cb2738 */
void venus_shell_b_exec(VenusShellBWork *w)
{
    float drag;

    v3_scale(&w->vel, venus_shell_b_drag);
    drag = venus_shell_b_spin_damp;
    w->vel.y -= venus_shell_b_grav;
    w->dang[0] = w->dang[0] * drag + w->spin[0] * (1.0f - drag);
    w->dang[1] = w->dang[1] * drag + w->spin[1] * (1.0f - drag);
    w->dang[2] = w->dang[2] * drag + w->spin[2] * (1.0f - drag);
    v3_sum(&w->pos, &w->vel, &w->pos);
    i3_sum(w->ang, w->ang, w->dang);
    if (--w->life <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00cb2898 */
void venus_shell_b_disp(VenusShellBWork *w)
{
    float fade[4];
    int old_blend, fade_frm;

    memset(fade, 0, 16);
    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    ulMatrixPush();
    fade_frm = venus_shell_fade_sec * FPS;
    if (w->life < fade_frm)
        fade[0] = (float)w->life / (float)fade_frm - 1.0f;
    draw_set_override_color(fade);
    MTX_TRANS(&w->pos);
    venus_shell_rotate_yxz(w->ang);
    dc_object_draw(w->model);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(old_blend);
}

/* 0x00cb29b8 */
int *venus_shell_b_create(int *w, Vec3 *pos, Vec3 *ang, int model)
{
    VenusShellBWork *piece = task_create_inherit(w, &venus_shell_b_desc, 76);
    float rnd_div = 32769.0f;

    v3_copy(&piece->pos, pos);
    i3_copy(piece->ang, (int *)ang);
    piece->model = (ModelPart *)model;
    v3_copy(&piece->vel, pos);
    piece->vel.y = 0.0f;
    vec3_normalize(&piece->vel, &piece->vel);
    vec_set_length(&piece->vel, venus_shell_b_spd + rand_lcg_next_a() / rnd_div * venus_shell_b_spd_rnd);
    piece->life = (venus_shell_b_sec + rand_lcg_next_a() / rnd_div * venus_shell_b_sec_rnd) * FPS;
    do {
        piece->spin[0] = DEG_TO_ANG(rand_lcg_next_a() / rnd_div * (venus_shell_b_spin + venus_shell_b_spin) - venus_shell_b_spin);
        piece->spin[1] = DEG_TO_ANG(rand_lcg_next_a() / rnd_div * (venus_shell_b_spin + venus_shell_b_spin) - venus_shell_b_spin);
        piece->spin[2] = DEG_TO_ANG(rand_lcg_next_a() / rnd_div * (venus_shell_b_spin + venus_shell_b_spin) - venus_shell_b_spin);
    } while (0);
    return (int *)piece;
}

/* 0x00cb2bc8 */
void venus_shell_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00cb2c18 */
void boss_se_play(int id, int lock, int pos)
{
    se_table_play(0, &se_table, id, lock, pos);
}

/* 0x00cb2c38 */
void boss_vol_play(int id, int arg)
{
    vol_table_play(0, vol_table, id, arg);
}

/* 0x00cb2c50 */
void boss_vol_stop(int id)
{
    vol_table_stop(0, vol_table, id);
}

/* 0x00cb2c68 */
void boss_bgm_exit_cb(BgmMan *bgm)
{
    se_table_stop(&se_table);
}

/* 0x00cb2c78 */
BgmMan *boss_bgm_start(void *parent)
{
    se_table_reset(&se_table);
    return cm_bgm_sound_man_create(parent, boss_bgm_exit_cb);
}

/* 0x00cb2cb8 */
void boss_adx_request(void)
{
    cm_bgm_sound_man_add_file(0, adx_files[0], (int)adx_files[1]);
}

/* 0x00cb2cd0 */
void boss_bgm_switch(int cb, int cb_arg, int idx)
{
    cm_bgm_sound_sub_start(0, cb, cb_arg, 0, 0, idx, idx);
}

/* 0x00cb2cf8 */
void venus_cue_cb(void)
{
    boss_vol_play(venus_cue_id, 0);
    peri_act_play_boss_break();
}

/* 0x00cb2d20 */
void venus_cue(int idx, int id, int delay)
{
    venus_cue_id = id;
    cm_bgm_sound_sub_start(0, (int)venus_cue_cb, 0, 0, delay, idx, idx);
}

/* 0x00cb2d50 */
void venus_cue_fn_cb(void)
{
    venus_cue_func(venus_cue_arg);
    boss_vol_play(venus_cue_id, 0);
    peri_act_play_boss_break();
}

/* 0x00cb2d90 */
void venus_cue_set(int idx, int id, void (*fn)(int arg), int arg)
{
    venus_cue_id = id;
    venus_cue_func = fn;
    venus_cue_arg = arg;
    cm_bgm_sound_sub_start(0, (int)venus_cue_fn_cb, 0, 0, 0, idx, idx);
}

/* 0x00cb2dd0 */
void boss_pcm_load(int i)
{
    if (game_state.fps < 55)
        boss_stream_load((int)pcm_files[1], 1);
    else
        boss_stream_load((int)pcm_files[i], 1);
}

/* 0x00cb2e10 */
void boss_pcm_start(void)
{
    boss_stream_start();
}
