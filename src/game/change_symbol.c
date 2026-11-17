/*
 * Change symbol: the lock-on target that takes the stage to its next layer. Pauses the game flow
 * while shot; each hit grows one of eight guide lines; destroyed -> layer analyzed, "CsEff"
 * letters, beat-synced sound, score, wire fragments.
 */

#include "task.h"
#include "types_common.h"
#include "color.h"
#include "types_game.h"
#include "types_gw.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"

typedef struct { int type; float pos[3]; int ang[3]; unsigned int col; char pad_20[16]; } CsEnt;
typedef struct { int hdr[2]; CsEnt ent[17]; float org[3]; } CsEff;

#define ROTZ(a, n) do { float rot_u = x, rot_v = y; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] - rot_v * sc[n][0]; y = rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define ROTX(a, n) do { float rot_u = y, rot_v = z; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    y = rot_u * sc[n][1] - rot_v * sc[n][0]; z = rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define ROTY(a, n) do { float rot_u = x, rot_v = z; ulSinCos(a, &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] + rot_v * sc[n][0]; z = -rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

typedef struct { int ang[3]; unsigned int col; int vel[3]; float scl; } CsMov;
typedef struct { int type; float pos[3]; CsMov mov; } ChangeSymbolCsEnt;
typedef struct { int state; int next; ChangeSymbolCsEnt ent[17]; float org[3]; float scale; } ChangeSymbolCsEff;
typedef struct { unsigned char type; char pad_01[3]; float pos[3]; int ang[3]; unsigned int col; int vel[3]; float scl; } VscaleCsEnt;
typedef struct { int state; int next; VscaleCsEnt ent[17]; float org[3]; float scale; } VscaleCsEff;

typedef struct { char pad_00[36]; Vec3 pos; } ChangeSymbolWork;

typedef struct {
    char pad_00[168];
    float weight[11];
    float line_w;
    float line_w_add;
} ChangeSymbolWorkWeightView;

typedef struct {
    char pad_00[8];
    float item[3];
    float item_ofs[3];
    char pad_20[16];
    int ang[3];
    char pad_3c[76];
    int ang_spd[3];
    char pad_94[4];
    float vel[3];
} CsVaddfChangeSymbolWork;

typedef struct { char pad_00[36]; float x, y, z; } ChangeSymbolWorkHitView;

typedef struct {
    int pnb;
    int ptm;
    char item[28];
    int pos[3];
    int ang[3];
    char pad_3c[12];
    char lock[16];
    int lock_flags;
    char pad_5c[56];
    float alpha;
    float vel[3];
    int flow_stop;
    char pad_a8[44];
    float line_w;
    float line_w_add;
    int flow_count;
    int show_core;
    int status_taken;
    int se_stopped;
} VcopyIChangeSymbolWork;

typedef struct {
    char pad_00[12];
    int pos[3];
    char pad_18[16];
    int skip;
} ChangeSymbolArg;

/* steer(alias): the code that matched reads ChangeSymbolWork through these member types; one view
 * shared by those sites (src/game/change_symbol.c). */
typedef struct ChangeSymbolWorkView {
    char pad_00[36];
    float pos[3];
    char pad_30[30];
    short life;
    char pad_50[60];
    int ang_spd_y;
    int ang_spd_z;
    float alpha;
    char pad_98[48];
    int ev;
    int state;
    int flow_ret;
    char pad_d4[4];
    float line_w_add;
    int flow_count;
    int show_core;
    int status_taken;
    int se_stopped;
    char *csf;
} ChangeSymbolWorkView;

typedef struct {
    char pad_00[72];
    char lock[6];
    short life;
    char pad_50[8];
    unsigned int lock_flags;
    char pad_5c[32];
    float lock_imp[3];
    int ang_spd_x;
    int ang_spd_y;
    int ang_spd_z;
    char pad_94[4];
    float vel[3];
    int flow_stop;
    char pad_a8[36];
    int state;
    char pad_d0[12];
    int flow_count;
    char pad_e0[8];
    int se_stopped;
    char pad_ec[4];
    int vanish_wait;
} CxVcopyChangeSymbolWork;

extern int item_models;
extern char change_symbol_model[];
extern char gw_varedit_root[];
extern GwVarDef change_symbol_vardef[];
extern GwVarDef change_symbol_vardefs[];
extern int *pnb_get_entry(int, int);
extern void *model_pnb_get_wire(int, int);
extern void dc_object_init(void *, int, int, int);
extern void dc_object_set_wire(void *, void *);
extern int item_textures;
extern void *ulMatrixCur;
extern int fog_set_enable(int);
extern int RenderSetCurrentDisplayList(int);
extern void draw_set_blend(int, int);
extern void light_beam_draw_mtx(unsigned int, int, float);
extern void lens_flare_draw_mtx(int, int, float, float);
extern void draw_set_color(unsigned int);
extern void dc_object_draw(void *);
extern signed char cs_eff_letters[];
extern void cs_eff_calc_positions(CsEff *);
extern float float_lerp(float, float, float);
extern int ang_approach(int, int, float);
extern void font_set(int);
extern void draw_set_fc_mode(int);
extern void font_draw_char3d(int, float *, int *, float *, unsigned int);
extern struct TaskDesc cs_eff_desc;
extern void ulMatrixPush(void);
extern int camera_get_inv_mtx(void);
extern void wire_break_task_create(void *, void *);
extern float project_xyz_abs(float, float, float, float *);
extern V3 change_symbol_axis_dirs[];
extern int transform_and_clip_line(float *, float *);
extern void change_symbol_draw_line_quad(float, float, float *, float *, float *);
extern float base_spd;
extern float hit_ref_back_br;
extern float hit_ref_reduce;
extern char change_symbol_zero[];
extern void item_approach_exec(void *, float);
extern void item_resolve_pos(void *, void *, void *);
extern void vec3_approach(void *, void *, float);
extern int out_ang;
extern float out_len;
extern void player_get_pos(float *);
extern void player_camera_get_fwd(float *);
extern float vec3_distance_squared(float *, float *);
extern float stay_dist;
extern float stay_rate;
extern void camera_get_view_vec(Vec3 *);
extern int stop_flow;
extern char str_change_symbol_lock_name[];
extern void item_set_origin(void *, void *);
extern void lockon_obj_init(void *, void *, void *, int, void *, int, int, int, int);
extern void lockon_entry(void *);
extern void GameFlowCountStart(void);
extern ScreenCfg screen_cfg;
extern void SoundRhythmSe_ChangeSymbolStop(void);
extern void GameFlowCountStop(void);
extern void player_status_clear_flags(unsigned int);
extern void lockon_remove(void *);
extern void wipedoor_set_center(float, float);
extern int beat_grid_hit(int);
extern int player_status_test_flags(unsigned int);
extern void player_status_count_analyzed(void);
extern int player_status_get_stage_analyzed(void);
extern void listener_dispatch(int, int *);
extern int player_status_set_flags(unsigned int);
extern float beat_clock_get_bar(void);
extern int beat_clock_get_haku(void);
extern int game_flow_get_current(void);
extern int change_symbol_se_create(int, int);
extern void player_status_add_score(int);
extern float hit_ref;
extern float hit_ref_back;
extern float hit_ref_dist;
extern float hit_ref_limit;
extern char str_change_symbol_vanish[];
extern int lockon_hits_damage(void *);
extern int lockon_hits_count(void *);
extern void lockon_obj_get_screen_pos(void *, void *);
extern void player_get_accel(float *);
extern void vec3_clamp_length(float, float *, float *);
extern void SoundRhythmSe_ChangeSymbol(int);
extern void draw_set_test_mode(int);
extern void draw_set_ztest_mode(int);
extern struct TaskDesc change_symbol_desc;
extern int game_flow_get_task_parent(void);
extern void change_symbol_draw_axis_lines(ChangeSymbolWorkWeightView *w, int idx);
extern void change_symbol_exec_end(ChangeSymbolWorkView *w);
extern void change_symbol_follow_view(char *w);
extern int change_symbol_hit_test(ChangeSymbolWorkHitView *w);
extern void change_symbol_move(CsVaddfChangeSymbolWork *w);
extern void change_symbol_rotate_yxz(int *p);
extern void change_symbol_update_weights(char *w, int mode);
extern void change_symbol_wire_break_create(ChangeSymbolWork *w);
extern int cs_eff_create(void *parent);
extern void cs_eff_set_origin(char *w, Vec3 *pos);
extern void cs_eff_set_state(int *p, int state);

/* Model: item model 12 with its wireframe. Also the debug tuning variables. */
/* 0x00107628 */
void change_symbol_params_register(void)
{
    int model;
    void *wire;
    struct GwVarNode *id;

    model = *pnb_get_entry(item_models, 12);
    wire = model_pnb_get_wire(item_models, 12);
    dc_object_init(change_symbol_model, model, 0, 4352);
    dc_object_set_wire(change_symbol_model, wire);
    id = gw_varedit_add(gw_varedit_root, 0, change_symbol_vardef);
    gw_varedit_add_table(gw_varedit_root, id, change_symbol_vardefs);
}

/* No fog. Beam and lens flare on display list 6; model at 4x size. */
/* 0x001076d0 */
void change_symbol_model_draw(void)
{
    int fog;
    int dl;

    ulGxSetTextureList(item_textures);
    fog = fog_set_enable(0);
    dl = RenderSetCurrentDisplayList(6);
    draw_set_blend(1, 1);
    ulGxSetTextureList(item_textures);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    light_beam_draw_mtx(COL_WHITE, 0, 10.0f);
    lens_flare_draw_mtx(0, 0, 20.0f, 0.5f);
    RenderSetCurrentDisplayList(dl);
    vu0_matrix_scale(ulMatrixCur, 4.0f, 4.0f, 4.0f);
    draw_set_color(RGBA(0xc8, 0, 0, 0x80));
    dc_object_draw(change_symbol_model);
    fog_set_enable(fog);
}

/* 17 letters on a unit sphere: +y turned by z, x, y. */
/* 0x001077c0 */
void cs_eff_calc_positions(CsEff *w)
{
    float sc[3][2];
    CsEnt *ent = w->ent;
    int i;
    float x, y, z;

    for (i = 17; i != 0; i--) {
        float *pos = ent->pos;

        x = 0.0f;
        y = 1.0f;
        z = 0.0f;
        ROTZ(ent->ang[2], 0);
        ROTX(ent->ang[0], 1);
        ROTY(ent->ang[1], 2);
        pos[0] = x;
        pos[1] = y;
        pos[2] = z;
        ent++;
    }
}

/* Letters from cs_eff_letters, 20 degrees apart. */
/* 0x001078f0 */
void cs_eff_init(CsEff *w)
{
    int i = 17;
    int half = ANG_180;
    int ang = 0;
    unsigned int col = RGBA(0, 0, 0x80, 0x80);
    signed char *letter = cs_eff_letters;
    int *ent = w->ent[0].ang;
    float *org = w->org;

    do {
        ent[-4] = *letter++;
        ent[3] = col;
        ent[0] = 0;
        ent[1] = half;
        ent[2] = ang;
        ang -= 3640;
        ent += 12;
    } while (--i);
    w->org[0] = 0;
    org[1] = 0;
    org[2] = 0;
    cs_eff_calc_positions(w);
}

/* 0x00107950 */
void cs_eff_release(void)
{
}

/*
 * State 0: same slow spin, wide sphere; 1: faster spins dropping letter by letter, smaller sphere.
 * Takes the state from cs_eff_set_state at the end.
 */
/* 0x00107958 */
void cs_eff_exec(ChangeSymbolCsEff *w)
{
    ChangeSymbolCsEnt *ent;
    CsMov *mov;
    int i;
    int base_x;
    int base_y;
    int z;
    int da;
    int db;
    int dc;
    float scl_goal;
    float goal;
    int x;
    int y;

    switch (w->state) {
    default:
    case 0:
        scl_goal = 0.6f;
        goal = 90.0f;
        base_x = 0;
        base_y = 182;
        da = 0;
        db = 0;
        dc = 0;
        break;
    case 1:
        scl_goal = 0.8f;
        goal = 30.0f;
        base_x = 1547;
        base_y = 1406;
        da = -91;
        db = -72;
        dc = -72;
        break;
    }
    do {
        w->scale = float_lerp(goal, w->scale, 0.02f);
        x = base_x;
        y = base_y;
        z = 182;
        for (i = 17, ent = w->ent; i != 0; i--, ent++) {
            mov = &ent->mov;
            mov->vel[0] = ang_approach(x, mov->vel[0], 0.05f);
            x += da;
            mov->vel[1] = ang_approach(y, mov->vel[1], 0.05f);
            y += db;
            mov->vel[2] = ang_approach(z, mov->vel[2], 0.05f);
            z += dc;
            mov->ang[0] += mov->vel[0];
            mov->ang[1] += mov->vel[1];
            mov->ang[2] += mov->vel[2];
            mov->scl = float_lerp(scl_goal, mov->scl, 0.02f);
        }
    } while (0);
    cs_eff_calc_positions(w);
    w->state = w->next;
}

static inline void vscale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void vadd(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

/* 0x00107b18 */
void cs_eff_disp(VscaleCsEff *w)
{
    float pos[4];
    float sv[4];
    int i;
    int saved;
    float scale;
    VscaleCsEnt *ent;

    font_set(3);
    draw_set_blend(4, 1);
    draw_set_fc_mode(8192);
    saved = fog_set_enable(0);
    scale = w->scale;
    ent = w->ent;
    for (i = 17; i != 0; ent++, i--) {
        vscale(pos, ent->pos, scale * ent->scl);
        sv[2] = sv[1] = sv[0] = ent->scl;
        vadd(pos, pos, w->org);
        font_draw_char3d(ent->type, pos, ent->ang, sv, 0x80800000);
    }
    fog_set_enable(saved);
}

/* 0x00107c40 */
int cs_eff_create(void *parent)
{
    return task_create(parent, &cs_eff_desc, 0x348);
}

/* Applied on the next frame. */
/* 0x00107c68 */
void cs_eff_set_state(int *p, int state)
{
    p[1] = state;
}

/* 0x00107c70 */
void cs_eff_set_origin(char *w, Vec3 *pos)
{
    Vec3 *org = (Vec3 *)(w + 824);
    org->x = pos->x;
    org->y = pos->y;
    org->z = pos->z;
}

/* Wire-break under the symbol's parent task. */
/* 0x00107c90 */
void change_symbol_wire_break_create(ChangeSymbolWork *w)
{
    WireBreakArg desc;
    char *parent;
    desc.model = change_symbol_model;
    desc.spd = 0.16666667f;
    desc.drag = 0.999f;
    desc.rot_range = 0.3f;
    desc.v[1] = -0.0055555557f;
    desc.w = 120;
    desc.flags = 0x800000c8;
    desc.h = 200;
    desc.v[0] = 0;
    desc.v[2] = 0;
    ulMatrixPush();
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, camera_get_inv_mtx());
    do {
        Vec3 *p_ = &w->pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    change_symbol_rotate_yxz((char *)w + 48);
    vu0_matrix_scale(ulMatrixCur, 4.0f, 4.0f, 4.0f);
    parent = (char *)TASK_OF(w)->parent;
    wire_break_task_create(parent ? parent + 64 : 0, &desc);
    ulMatrixPop();
}

/*
 * Two strips either side of the line; half widths fa, fb perspective-scaled. Screen-space UVs
 * (u = x / 640, v = y / 480). Colour words of vtx kept.
 */
/* 0x00107da0 */
void change_symbol_draw_line_quad(float fa, float fb, float *vtx, float *start, float *end)
{
    float proj[4];
    float px, py, qx, qy, qz;
    float dx, dy, inv_len;
    float one, scr_w, scr_h;
    float z, start_scale;
    float *vp;

    do {
        px = start[0];
        py = start[1];
    } while (0);
    do {
        qx = end[0];
        qy = end[1];
        qz = end[2];
    } while (0);
    one = 1.0f;
    scr_w = 640.0f;
    dx = qx - px;
    z = -project_xyz_abs(fa, one, start[2], proj);
    dy = qy - py;
    start_scale = proj[0];
    project_xyz_abs(fb, one, qz, proj);
    scr_h = 480.0f;
    fb = proj[0];
    inv_len = rsqrtf(dx * dx + dy * dy);
    dy *= inv_len;
    dx *= inv_len;
    vp = vtx;
    vp[0] = qx;
    vp[1] = qy;
    vp[2] = z;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    vp += 6;
    vp[0] = px;
    vp[1] = py;
    vp[2] = z;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    vp += 6;
    vp[0] = qx + dy * fb;
    vp[1] = qy - dx * fb;
    vp[2] = z;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    vp += 6;
    vp[0] = px + dy * start_scale;
    vp[1] = py - dx * start_scale;
    vp[2] = z;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(vtx, 4);
    vp = vtx + 12;
    vp[0] = qx - dy * fb;
    vp[1] = qy + dx * fb;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    vp += 6;
    vp[0] = px - dy * start_scale;
    vp[1] = py + dx * start_scale;
    vp[3] = vp[0] / scr_w;
    vp[4] = vp[1] / scr_h;
    ulGxAddTexStrip(vtx, 4);
    ulGxEndTexStrip();
}

static inline void gl_vscale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void gl_vcopy(float *dst, float *src)
{
    float x = src[0], y = src[1], z = src[2];

    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void gl_vscale2(float *dst, float *src)
{
    dst[0] = src[0] + src[0];
    dst[1] = src[1] + src[1];
    dst[2] = src[2] + src[2];
}

/*
 * Along change_symbol_axis_dirs[idx]; length and brightness grow with weight (stretched to stay
 * visible), widening by line_w_add; plus a short fixed beam near the core.
 */
/* 0x00108020 */
void change_symbol_draw_axis_lines(ChangeSymbolWorkWeightView *w, int idx)
{
    int vtx[24];
    float start[4];
    float end[4];
    float base[4];
    float wt;
    float size;
    float dist;
    float scale;
    float t;
    float width0;
    float width1;

    wt = w->weight[idx];
    scale = 4.0f;
    gl_vscale(start, &change_symbol_axis_dirs[idx].x, scale);
    vtx[5] = 0x323232;
    vtx[11] = ((((int)(wt * 255.0f) + 1) >> 1) << 24) | 0xffffff;
    vtx[17] = 0;
    vtx[23] = 0;
    dist = wt * 100.0f;
    scale = dist + 4.0f;
    gl_vscale(end, &change_symbol_axis_dirs[idx].x, scale);
    size = view_project_size(dist, end);
    if (0.0f < size) {
        t = wt * 400.0f;
        if (size <= t) {
            dist = t / size * 100.0f;
            scale = dist + 4.0f;
            gl_vscale(end, &change_symbol_axis_dirs[idx].x, scale);
        }
    }
    gl_vcopy(base, start);
    if (transform_and_clip_line(start, end) >= 0) {
        width0 = w->line_w;
        width1 = width0;
        if (0.0f < scale)
            width1 += w->line_w_add * dist / 100.0f;
        change_symbol_draw_line_quad(width0, width1, (float *)vtx, start, end);
        gl_vscale2(end, &change_symbol_axis_dirs[idx].x);
        if (transform_and_clip_line(base, end) >= 0)
            change_symbol_draw_line_quad(w->line_w, 5.0f, (float *)vtx, base, end);
    }
}

/* Mode 0 (shot): one line per hit so far (8 - life) grows 0.04/frame to 1. Mode 1: all shrink. */
/* 0x001082d0 */
void change_symbol_update_weights(char *w, int mode)
{
    float *p;
    int n;

    switch (mode) {
    case 0:
        n = 8 - *(short *)(w + 78);
        p = (float *)(w + 168);
        if (n > 0) {
            do {
                *p = *p + 0.04f;
                if (*p >= 1.0f)
                    *p = 1.0f;
                p++;
            } while (--n);
        }
        break;
    case 1:
        n = 8;
        p = (float *)(w + 168);
        do {
            *p = *p * 0.93f;
            p++;
        } while (--n);
        break;
    }
}

static inline void cs_vaddf(float *dst, float *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void cs_vaddi(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void cs_vscale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

/* Part of the knock-back goes to the path offset too; x spin slows; knock-back decays. */
/* 0x00108378 */
void change_symbol_move(CsVaddfChangeSymbolWork *w)
{
    float step[3];
    float rate;

    item_approach_exec(w->item, base_spd);
    item_resolve_pos(w->item, (char *)w + 36, (char *)w + 60);
    rate = hit_ref_back_br;
    cs_vaddf(w->item, w->vel);
    cs_vaddi(w->ang, w->ang_spd);
    w->ang_spd[0] = (int)((float)w->ang_spd[0] * 0.95f);
    cs_vscale(step, w->vel, rate);
    cs_vaddf(w->item_ofs, step);
    vec3_approach(change_symbol_zero, w->vel, hit_ref_reduce);
}

/* In view cone (out_ang) or within out_len of the player. View-direction dot product unused. */
/* 0x001084c0 */
int change_symbol_hit_test(ChangeSymbolWorkHitView *w)
{
    float p[4];
    float fwd[4];
    float dir[4];
    int hit;

    player_get_pos(p);
    player_camera_get_fwd(fwd);
    hit = ray_hits(p, fwd, out_ang, &w->x, 0.0f);
    dir[0] = w->x - p[0];
    dir[1] = w->y - p[1];
    dir[2] = w->z - p[2];
    vec3_normalize(dir, dir);
    vec3_normalize(fwd, fwd);
    ulVec3Dot(dir, fwd);
    if (hit == 0) {
        if (vec3_distance_squared(&w->x, p) <= out_len * out_len)
            hit = 1;
    }
    return hit;
}

/* Eases the item offset to stay_dist along the view direction at stay_rate. */
/* 0x001085c8 */
void change_symbol_follow_view(char *w)
{
    Vec3 dir;

    camera_get_view_vec(&dir);
    vec3_normalize(&dir, &dir);
    dir.x *= stay_dist;
    dir.y *= stay_dist;
    dir.z *= stay_dist;
    vec3_approach(&dir, w + 20, stay_rate);
}

static inline void vcopy_i(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void vzero_f(float *dst)
{
    dst[0] = 0.0f;
    dst[1] = 0.0f;
    dst[2] = 0.0f;
}

/* Pauses the game flow. spawn->skip: starts destroyed (not lockable). */
/* 0x00108640 */
void change_symbol_init(VcopyIChangeSymbolWork *w, ChangeSymbolArg *spawn)
{
    do {
        w->pnb = item_models;
        w->ptm = item_textures;
    } while (0);
    item_set_origin(w->item, spawn);
    vcopy_i(w->ang, spawn->pos);
    vzero_f(w->vel);
    lockon_obj_init(w->lock, w, w->pos, 0, str_change_symbol_lock_name, -1, 8, 8, 152);
    lockon_entry(w->lock);
    w->flow_stop = stop_flow;
    GameFlowCountStart();
    w->flow_count = 0;
    w->show_core = 1;
    w->se_stopped = 1;
    w->status_taken = 0;
    w->alpha = 1.0f;
    w->line_w = 7.0f;
    w->line_w_add = 5.0f;
    if (spawn->skip != 0) {
        w->lock_flags &= ~0x100;
        TASK_OF(w)->exec = (void (*)(void *))change_symbol_exec_end;
        w->se_stopped = 0;
    }
}

/* Resumes the flow, drops the "analyzing" status flag if held, recentres the wipe door. */
/* 0x00108778 */
void change_symbol_release(char *w)
{
    if (*(int *)(w + 232) == 0)
        SoundRhythmSe_ChangeSymbolStop();
    if (*(int *)(w + 220) != 0)
        GameFlowCountStop();
    if (*(int *)(w + 164) > 0)
        GameFlowCountStop();
    if (*(int *)(w + 228) != 0)
        player_status_clear_flags(0x20000);
    lockon_remove(w + 72);
    wipedoor_set_center(screen_cfg.cx, screen_cfg.cy);
}

/*
 * States on the beat: 0 layer analyzed (event 7), CsEff; 1-2 bar line, letters, layer sound (event
 * 8); 3 spin to next bar line, break, 300 points per analyzed layer (event 9); 4 fade, end.
 */
/* 0x00108800 */
void change_symbol_exec_end(ChangeSymbolWorkView *w)
{
    float scr_pos[4];
    int analyzed_msg;
    int se_msg;
    int score_msg;

    change_symbol_move(w);
    change_symbol_follow_view(w);
    ulMatrixApplyPoint(ulMatrixCur, w->pos, scr_pos);
    project_point(scr_pos, scr_pos);
    wipedoor_set_center(scr_pos[0], scr_pos[1]);
    change_symbol_update_weights(w, 1);
    switch (w->state) {
    case 0:
        if (player_status_test_flags(0x20000) == 0) {
            player_status_count_analyzed();
            w->ev = player_status_get_stage_analyzed();
            analyzed_msg = w->ev;
            listener_dispatch(7, &analyzed_msg);
            player_status_set_flags(0x20000);
            w->status_taken = 1;
        }
        w->csf = cs_eff_create(w);
        if (beat_clock_get_bar() <= 0.875f)
            w->state += 2;
        else
            w->state++;
        break;
    case 1:
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0)
            w->state++;
        break;
    case 2:
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            cs_eff_set_state(w->csf, 1);
            if (w->status_taken != 0) {
                w->flow_ret = change_symbol_se_create(game_flow_get_current(), w->ev - 1);
                se_msg = w->ev;
                listener_dispatch(8, &se_msg);
            }
            w->state++;
        }
        break;
    case 3:
        w->ang_spd_y += 18;
        w->ang_spd_z += 18;
        w->line_w_add = w->line_w_add * 0.97f;
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            SoundRhythmSe_ChangeSymbolStop();
            w->se_stopped = 1;
            change_symbol_wire_break_create(w);
            if (w->status_taken != 0) {
                score_msg = w->ev;
                listener_dispatch(9, &score_msg);
                player_status_add_score(w->ev * 300);
                player_status_clear_flags(0x20000);
                w->status_taken = 0;
            }
            w->show_core = 0;
            w->state++;
        }
        break;
    case 4:
        w->alpha = w->alpha * 0.98f;
        if (w->alpha <= 0.03f) {
            do {
                if (w != 0)
                    TASK_OF(w)->flags |= TASK_F_END;
            } while (0);
        }
        break;
    }
    cs_eff_set_origin(w->csf, w->pos);
}

static inline void cx_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void cx_vscale(float *vec, float scale)
{
    vec[0] *= scale;
    vec[1] *= scale;
    vec[2] *= scale;
}

static inline void cx_vadd(float *dst, float *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

/*
 * Vanishes out of range. Hits speed the spin (capped) and push it away, extra push along player
 * acceleration when close; sound follows life. Flow stays paused for a set time.
 */
/* 0x00108ab8 */
void change_symbol_exec(CxVcopyChangeSymbolWork *obj)
{
    float imp[4];
    float w[4];
    float accel[4];
    float push;
    float dmg;
    int life;
    int stop;
    int small;

    change_symbol_move(obj);
    if (change_symbol_hit_test(obj) == 0) {
        obj->vanish_wait = 60;
        if (obj->flow_stop > 0) {
            obj->flow_stop = 0;
            GameFlowCountStop();
        }
        DEBUG_PRINTF(str_change_symbol_vanish);
        do {
            if (obj != 0)
                TASK_OF(obj)->flags |= TASK_F_END;
        } while (0);
        return;
    }
    if (obj->lock_flags & 0x2000) {
        dmg = (float)lockon_hits_damage(obj->lock);
        obj->ang_spd_y += (int)DEG_TO_ANG(dmg * 0.3f);
        obj->ang_spd_z += (int)DEG_TO_ANG(dmg * 0.1f);
        obj->ang_spd_x += (int)DEG_TO_ANG(dmg * 4.0f);
        if (obj->ang_spd_x >= 0x71c)
            obj->ang_spd_x = 0x71c;
        if (obj->ang_spd_y >= 0x38e)
            obj->ang_spd_y = 0x38e;
        if (obj->ang_spd_z >= 0x38e)
            obj->ang_spd_z = 0x38e;
        cx_vcopy(imp, obj->lock_imp);
        small = 0;
        if (fabsf(imp[0]) < 0.01f && fabsf(imp[1]) < 0.01f && fabsf(imp[2]) < 0.01f)
            small = 1;
        if (!small) {
            push = (float)lockon_hits_count(obj->lock);
            vec3_normalize(imp, imp);
            push *= hit_ref;
            cx_vscale(imp, push);
            cx_vadd(obj->vel, imp);
        }
        lockon_obj_get_screen_pos(obj->lock, w);
        if (0.0f <= w[2] && w[2] <= hit_ref_dist) {
            player_get_accel(accel);
            cx_vscale(accel, hit_ref_back);
            cx_vadd(obj->vel, accel);
        }
        vec3_clamp_length(hit_ref_limit, obj->vel, obj->vel);
        life = obj->life;
        SoundRhythmSe_ChangeSymbol(life);
        if (life == 0) {
            obj->se_stopped = 0;
            GameFlowCountStart();
            obj->flow_count = 1;
            *(void (**)(void *))((char *)obj - 48) = (void (*)(void *))change_symbol_exec_end;
            obj->state = 0;
        }
    }
    stop = obj->flow_stop;
    if (stop > 0) {
        obj->flow_stop = --stop;
        if (stop <= 0)
            GameFlowCountStop();
    }
    change_symbol_update_weights(obj, 0);
}

/* Lens flare and model only while the core is shown. */
/* 0x00108e00 */
void change_symbol_disp(ChangeSymbolWorkView *w)
{
    int n;
    int i;
    int saved;
    int dl;
    unsigned int alpha;
    unsigned int col;
    float *pos;

    ulGxSetTextureList(item_textures);
    ((int (*)(void))ulMatrixPush)(); /* steer(cast) */
    pos = w->pos;
    do {
        vu0_matrix_translate(ulMatrixCur, w->pos[0], pos[1], pos[2]);
    } while (0);
    change_symbol_rotate_yxz((char *)w + 48);
    saved = fog_set_enable(0);
    n = 8 - w->life;
    if (n > 0) {
        draw_set_blend(4, 1);
        draw_set_fc_mode(0x2000);
        draw_set_test_mode(0);
        draw_set_ztest_mode(0x60000);
        ulGxSelectTexture(ulGxGetTextureList(), 1);
        do {
            i = n - 1;
            change_symbol_draw_axis_lines(w, i);
            n = i;
        } while (n != 0);
    }
    dl = RenderSetCurrentDisplayList(6);
    draw_set_blend(1, 1);
    alpha = (((int)(w->alpha * 255.0f) + 1) >> 1) & 0xff;
    col = (alpha << 24) | (alpha << 16) | (alpha << 8) | alpha;
    ulGxSetTextureList(item_textures);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    light_beam_draw_mtx(col, 0, 10.0f);
    if (w->show_core != 0) {
        lens_flare_draw_mtx(0, 0, 20.0f, 0.5f);
        vu0_matrix_scale(ulMatrixCur, 4.0f, 4.0f, 4.0f);
    }
    RenderSetCurrentDisplayList(dl);
    if (w->show_core != 0) {
        draw_set_color(RGBA(0xc8, 0, 0, 0x80));
        dc_object_draw(change_symbol_model);
    }
    fog_set_enable(saved);
    ulMatrixPop();
}

/* Under the game flow's task parent; 0 without one. */
/* 0x00108fe8 */
void *change_symbol_create(int spawn)
{
    int parent = game_flow_get_task_parent();
    if (parent)
        return task_create_arg(parent, &change_symbol_desc, 244, spawn);
    return 0;
}

/* 0x00109038 */
void change_symbol_rotate_yxz(int *p)
{
    vu0_matrix_rotate_y(ulMatrixCur, p[1]);
    vu0_matrix_rotate_x(ulMatrixCur, p[0]);
    vu0_matrix_rotate_z(ulMatrixCur, p[2]);
}
