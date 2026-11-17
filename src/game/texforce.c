/*
 * Texture force: a feedback effect in a 256x256 texture laid over the screen. Each frame gets a
 * moved, scaled, rotated, tinted copy of the last (screen plane, stage presets) plus sprite parts
 * ("TfParts_EneBomb00", ...) that leave trails. "KTF0" holds parts and presets; "TfCtrl" runs it.
 */

#include "system.h"
#include "types_game.h"
#include "mathdef.h"
#include "debug.h"
#include "gw_varedit.h"
#include "nj_strip.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_texture.h"
#include "ul_vu0.h"
#include "texforce.h"
#include "camera.h"
#include "enemy_status.h"
#include "fog.h"
#include "heap.h"
#include "light.h"
#include "map_manager.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "render.h"
#include "screen.h"
#include "stage.h"
#include "task.h"
#include "color.h"

typedef struct SurfaceDesc {
    int w;
    int h;
    int unk_08; /* 0 in texforce_surface_create's descriptor; never read */
    int unk_0c; /* 0 in texforce_surface_create's descriptor; never read */
    void *data;
} SurfaceDesc;

typedef struct TexObj {
    char pad_00[8];
    unsigned long *tex0;        /* 0x08: GS TEX0 register value */
} TexObj;

typedef struct KtfHead {
    int count;                  /* 0x00: entries of the part table */
    int stride;                 /* 0x04 */
    int ofs_08;                 /* 0x08 */
    int ofs_tbl;                /* 0x0c: part table */
    int ofs_parts;              /* 0x10: part definitions */
} KtfHead;

#define GS_SET_PRIM(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix) \
    ((unsigned long)(prim) | ((unsigned long)(iip) << 3) | ((unsigned long)(tme) << 4) | \
    ((unsigned long)(fge) << 5) | ((unsigned long)(abe) << 6) | ((unsigned long)(aa1) << 7) | \
    ((unsigned long)(fst) << 8) | ((unsigned long)(ctxt) << 9) | ((unsigned long)(fix) << 10))

#define GIF_SET_TAG(nloop, eop, pre, prim, flg, nreg) \
    ((unsigned long)(nloop) | ((unsigned long)(eop) << 15) | ((unsigned long)(pre) << 46) | \
    ((unsigned long)(prim) << 47) | ((unsigned long)(flg) << 58) | ((unsigned long)(nreg) << 60))

typedef union Qword {
    float stq[4];
    int i[4];
} Qword;

typedef struct GxStateOrg {
    char pad_00[1152];
    float x;                    /* 0x480 */
    float y;                    /* 0x484 */
} GxStateOrg;

typedef struct TfPlaneVtx {
    float x;
    float y;
    float z;
    unsigned int color;
} TfPlaneVtx;

extern int texforce_surface;
extern int texforce_tex_desc[];
extern int texforce_varedit;
extern float screen_layout[];
extern int gw_varedit_root[];
extern int texforce_vardef[];
extern int texforce_vardefs[];
extern int texforce_test_vardef[];
extern int texforce_test_vardefs[];
extern unsigned int system_flags;
extern void *system1_work;
extern TaskDesc texforce_desc;
extern TaskDesc texforce_end_desc;
extern int texforce_test_pos[];
extern int texforce_test_num[];
extern TfCtrl *tfctrl_work;
extern TaskDesc tfctrl2_desc;
extern TaskDesc tfctrl_desc;
extern float tfscreen_capture_scl_y;

extern float tfscreen_capture_ofs_y;

extern KtfHead *texforce_ktf_head;
extern void *texforce_ktf_screens;
extern int *texforce_ktf_parts;
extern TaskDesc tfpart_desc;
extern int tfscreen_type;
extern int tfscreen_col_a;
extern int tfscreen_col_r;
extern int tfscreen_col_g;
extern int tfscreen_col_b;
extern float tfscreen_cx;
extern float tfscreen_cy;
extern float tfscreen_pos_x;
extern float tfscreen_pos_y;
extern float tfscreen_scl_x;
extern float tfscreen_scl_y;
extern float tfscreen_rot_z;
extern int tfscreen_vardef[];
extern int tfscreen_vardefs[];
extern int tfscreen_vars_group;
extern int sys_state;
extern TaskDesc tfscreen_desc;
extern char str_tfplane_tag[];
extern unsigned long tfplane_gif_tag[2];
extern int tfplane_dma_tag;
extern GxStateOrg ulGxState;
extern void ulMatrixPush(void);

static void *texforce_end_create(TexForce *w);
static int tfctrl_get_texforce(int *ctrl);
static TfPart *tfpart_spawn(TexForce *tf, Vec3 *pos, KtfPart *part);
static void tfscreen_set_table(TfScreen *tbl);
static void tfscreen_clear(void);
static void tfscreen_disp(TfScreenTask *w);
static TfScreenTask *tfscreen_create(TexForce *tf, int *out);
static int tfplane_alloc(void);
static void tfplane_free(void *plane);
static void tfplane_draw_capture(float size);
static void tfplane_draw(float *uv, unsigned int color);
static void tfplane_build_uv(float *uv);

/* 256x128. */
/* steer(cast) */
/* 0x00170bd8 */
void texforce_surface_create(void)
{
    SurfaceDesc desc;

    desc.w = 256;
    desc.h = 128;
    desc.unk_08 = 0;
    desc.unk_0c = 0;
    desc.data = &desc;
    texforce_surface = ((int (*)(SurfaceDesc *))ulTextureCreateSurface)(&desc);
}

/* 0x00170c18 */
int texforce_surface_get_addr(void)
{
    return ulTextureGetAddr(texforce_surface);
}

/* 0x00170c40 */
int *texforce_get_texture(void)
{
    return texforce_tex_desc;
}

/* Nothing for null w. */
/* 0x00170c50 */
static void texforce_end(TexForce *w)
{
    TASK_END(w);
}

/* The "TexForce" work itself. */
/* 0x00170c70 */
static void *texforce_get_task(void *w)
{
    return w;
}

/* 0 unless running. */
/* 0x00170c78 */
static int texforce_get_size(TexForce *w)
{
    if (w && !(TASK_HDR(w)->flags & 1))
        return w->size;
    return 0;
}

/* If running. */
/* 0x00170ca0 */
static void texforce_bind(TexForce *w)
{
    if (w && !(TASK_HDR(w)->flags & 1))
        ulGxSelectTexture(texforce_tex_desc, 0);
}

/* 0x00170cd0 */
void texforce_enable(void)
{
    texforce_b_eff = 1;
}

/* 0x00170ce0 */
void texforce_disable(void)
{
    texforce_b_eff = 0;
}

/* Drawing this frame. */
/* 0x00170cf0 */
static int texforce_is_active(void)
{
    TexForce *w = texforce_work;

    if (w && !(TASK_HDR(w)->flags & 1))
        return w->active;
    return 0;
}

/* 0x00170d20 */
static int texforce_get_varedit(void)
{
    return texforce_varedit;
}

/* 0x00170d30 */
static void texforce_init_hook(void *w)
{
}

/* 0x00170d38 */
static void texforce_release_hook(void *w)
{
}

/* Exec-only frame: keeps the screen size, switches to 256x256. */
/* 0x00170d40 */
static void texforce_frame(TexForce *w)
{
    w->x = screen_layout[0];
    w->y = screen_layout[1];
    screen_set_size(256.0f, 256.0f);
    screen_apply_scale_frame();
    camera_update();
}

/*
 * Drawing frame: frame into the texture's buffer, fog off (kept), screen size kept, 256x256 with
 * camera and lights updated.
 */
/* 0x00170d88 */
void texforce_begin(TexForce *w)
{
    render_submit_frame(0, w->fbp);
    w->handle = fog_set_enable(0);
    w->x = screen_layout[0];
    w->y = screen_layout[1];
    screen_set_size(256.0f, 256.0f);
    screen_apply_scale_frame();
    camera_update();
    light_update();
    screen_frame_hook();
}

/* Exec-only frame: restores the screen size. */
/* 0x00170e00 */
static void texforce_end_frame(TexForce *w)
{
    screen_set_size(w->x, w->y);
    screen_apply_scale_frame();
    camera_update();
}

/* Drawing frame: flips the texture frame, restores screen size (camera, lights) and fog. */
/* 0x00170e28 */
void texforce_finish(TexForce *w)
{
    render_flip();
    screen_set_size(w->x, w->y);
    screen_apply_scale_frame();
    camera_update();
    light_update();
    screen_redraw();
    if (w->handle)
        fog_set_enable(w->handle);
}

/* Texture size *prm; "TexForceEnd" after it; variables and test; runs while paused. */
/* 0x00170e90 */
static void texforce_init(TexForce *w, int *prm)
{
    texforce_work = w;
    w->size = *prm;
    w->end = texforce_end_create(w);
    w->fbp = 0xf800;
    texforce_init_hook(w);
    w->active = 0;
    texforce_varedit = gw_varedit_add(gw_varedit_root, 0, texforce_vardef);
    gw_varedit_add_table(gw_varedit_root, texforce_varedit, texforce_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, texforce_varedit, texforce_test_vardef), texforce_test_vardefs);
    TASK_HDR(w)->flags |= 8;
}

/* 0x00170f58 */
static void texforce_release(TexForce *w)
{
    gw_varedit_remove(gw_varedit_root, 0, texforce_varedit);
    texforce_varedit = 0;
    texforce_release_hook(w);
    TASK_END(w->end);
    texforce_work = 0;
}

/* Before its children: with both enable flags, starts the pass so they draw into the texture. */
/* 0x00170fc8 */
static void texforce_exec(TexForce *w)
{
    w->active = texforce_b_eff & texforce_b_eff2;
    if (w->active) {
        if (system_flags & SYS_DISPLAY_PASS)
            texforce_begin(w);
        else
            texforce_frame(w);
    }
}

/* Under the system task. */
/* 0x00171018 */
static void *texforce_create(int *prm)
{
    return task_create_arg(system1_work, &texforce_desc, 32, prm);
}

/* Runs while paused. */
/* 0x00171048 */
static void texforce_end_init(TexForce **w, TexForce *tf)
{
    *w = tf;
    TASK_HDR(w)->flags |= 8;
}

/* After the children: ends the pass while active. */
/* 0x00171060 */
static void texforce_end_exec(TexForce **w)
{
    TexForce *tf = *w;

    if (tf->active) {
        if (system_flags & SYS_DISPLAY_PASS)
            texforce_finish(tf);
        else
            texforce_end_frame(tf);
    }
}

/* 0x001710a0 */
static void *texforce_end_create(TexForce *w)
{
    return task_create_sibling_arg(w, &texforce_end_desc, 4, w);
}

/* Part texforce_test_num at texforce_test_pos (camera-relative); needs controller, stage task. */
/* 0x001710c8 */
static void texforce_test_start(void)
{
    int ctrl;
    Vec4 pos;

    ctrl = stage_get_tfctrl();
    if (ctrl && stage_get_child()) {
        ctrl = tfctrl_get_texforce((int *)ctrl);
        ulMatrixApplyPoint(camera_get_inv_mtx(), texforce_test_pos, &pos);
        tfpart_spawn(ctrl, &pos, texforce_test_num);
    }
}

/* 0x00171140 */
static void tfctrl2_set(void **w, void *tf)
{
    *w = tf;
}

/* 0x00171148 */
static void tfctrl2_release(void *w)
{
}

/* 0x00171150 */
static void tfctrl2_exec(void *w)
{
}

/* 0x00171158 */
static void tfctrl2_disp(void *w)
{
}

/* Stage controller: a 256-pixel "TexForce" with "TfCtrl2" and the screen plane under it. */
/* 0x00171160 */
static void tfctrl_init(TfCtrl *w)
{
    int arg;
    int out;

    arg = 256;
    w->tf = texforce_create(&arg);
    w->ctrl2 = task_create(texforce_get_task(w->tf), &tfctrl2_desc, 4);
    tfctrl2_set(w->ctrl2, w->tf);
    tfscreen_create(w->tf, &out);
    tfctrl_work = w;
}

/* 0x001711d8 */
static void tfctrl_release(TfCtrl *w)
{
    if (w->ctrl2)
        TASK_HDR(w->ctrl2)->flags |= 1;
    texforce_end(w->tf);
    tfctrl_work = 0;
}

/* 0x00171218 */
static void tfctrl_exec(void *w)
{
}

/* 0x00171220 */
static void tfctrl_disp(void *w)
{
}

/* Unless one runs; returns the running one. */
/* 0x00171228 */
void *tfctrl_create(void *parent)
{
    void *w = tfctrl_work;

    if (!w)
        w = task_create(parent, &tfctrl_desc, 12);
    return w;
}

/* 0 without one. */
/* 0x00171258 */
static int tfctrl_get_texforce(int *ctrl)
{
    int tf = 0;

    if (ctrl)
        tf = *ctrl;
    return tf;
}

/* Over the whole screen (list 3, no fog, texture alpha ignored) while active with a controller. */
/* 0x00171270 */
void texforce_draw_screen(void)
{
    FontVtx vtx[2];
    int dl;
    int saved;
    TexObj **tex;

    if (texforce_is_active() && tfctrl_work && !(TASK_HDR(tfctrl_work)->flags & 1)) {
        vtx[0].y = vtx[0].x = 0.0f;
        vtx[0].z = 1e-05f;
        vtx[0].v = vtx[0].u = 0.0f;
        vtx[1].x = 640.0f;
        vtx[1].y = 480.0f;
        vtx[0].color = COL_WHITE;
        vtx[1].u = 1.0f;
        vtx[1].z = 1e-05f;
        vtx[1].v = 1.0f;
        vtx[1].color = COL_WHITE;
        dl = RenderSetCurrentDisplayList(3);
        saved = fog_set_enable(0);
        tex = texforce_get_texture();
        ulGxSelectTexture(tex, 0);
        psGsWriteReg(6, *(*tex)->tex0 & ~0x400000000);
        ulGxBeginSprite(1);
        ulGxAddSprites(vtx, 2);
        ulGxEndSprite();
        fog_set_enable(saved);
        RenderSetCurrentDisplayList(dl);
    }
}

/* Nothing for null w. */
/* 0x001713a8 */
static void texforce_task_end(void *w)
{
    TASK_END(w);
}

/* In front of the camera: screen position (y moved, scaled to the capture area); returns 1. */
/* 0x001713c8 */
static int texforce_project(Vec3 *pos, float *out_x, float *out_y)
{
    Vec4 view;

    ulMatrixApplyPoint(ulMatrixCur, pos, &view);
    if (view.z >= -1.0f)
        return 0;
    project_point(&view, &view.x);
    *out_x = view.x;
    *out_y = (view.y + tfscreen_capture_ofs_y) * tfscreen_capture_scl_y;
    return 1;
}

/*
 * "KTF0" (tag check result ignored): screen and part tables; part indices -> pointers (negative:
 * none); screen table to the screen plane.
 */
/* 0x00171460 */
void texforce_load_ktf(char *data)
{
    KtfHead *head;
    int *tbl;
    int *ent;
    KtfPart *parts;
    int stride;
    int n;
    int idx;

    fourcc_equal(data, "KTF0");
    head = (KtfHead *)(data + 8);
    texforce_ktf_screens = (void *)(head->ofs_08 + (int)head);
    tbl = (int *)(head->ofs_tbl + (int)head);
    parts = (KtfPart *)(head->ofs_parts + (int)head);
    n = head->count;
    stride = head->stride;
    texforce_ktf_head = head;
    texforce_ktf_parts = tbl;
    ent = tbl;
    if (n > 0) {
        do {
            if (*ent < 0) {
                *ent = 0;
            } else {
                idx = *ent * 3;
                *ent = (int)&parts[stride * idx];
            }
            ent++;
        } while (--n);
    }
    tfscreen_set_table((TfScreen *)texforce_ktf_screens);
}

/* 0x00171510 */
void texforce_unload_ktf(void)
{
    tfscreen_clear();
    texforce_ktf_head = 0;
    texforce_ktf_screens = 0;
    texforce_ktf_parts = 0;
}

static inline void vsub(Vec3 *dst, Vec3 *to, Vec3 *from)
{
    dst->x = to->x - from->x;
    dst->y = to->y - from->y;
    dst->z = to->z - from->z;
}

/*
 * Lifetime, start values and steps of angle, colour, size; pos projected and kept camera-relative.
 * Ends with no lifetime or pos behind the camera.
 */
/* 0x00171548 */
static void tfpart_setup(TfPart *w, TexForce *tf, Vec3 *pos, KtfPart *part)
{
    float inv;
    Vec3 cam;

    w->tf = tf;
    do { } while (0);
    w->life = part->frames;
    if (w->life <= 0)
        goto kill;
    inv = 1.0f / w->life;
    w->tex = part->tex;
    w->angle = DEG_TO_ANG(part->angle0);
    w->dangle = DEG_TO_ANG((part->angle1 - part->angle0) * inv);
    w->col[0] = part->col0[0];
    w->col[1] = part->col0[1];
    w->col[2] = part->col0[2];
    w->col[3] = part->col0[3];
    w->dcol[0] = (part->col1[0] - part->col0[0]) * inv;
    w->dcol[1] = (part->col1[1] - part->col0[1]) * inv;
    w->dcol[2] = (part->col1[2] - part->col0[2]) * inv;
    w->dcol[3] = (part->col1[3] - part->col0[3]) * inv;
    w->w = part->w_start;
    w->h = part->h_start;
    w->dw = (part->w_end - part->w_start) * inv;
    w->dh = (part->h_end - part->h_start) * inv;
    if (!texforce_project(pos, &w->sx, &w->sy)) {
        TASK_HDR(w)->disp = 0;
kill:
        TASK_HDR(w)->flags |= 1;
        return;
    }
    camera_get_eye(&cam);
    vsub(&w->ofs, pos, &cam);
    task_set_dlist(w, 0);
}

/* 0x00171700 */
static void tfpart_release(void *w)
{
}

static inline void vadd(Vec3 *dst, Vec3 *ofs)
{
    dst->x += ofs->x;
    dst->y += ofs->y;
    dst->z += ofs->z;
}

/*
 * Counts down, reprojects (moves with the camera), steps angle, colour, size; ends at time up or
 * behind the camera.
 */
/* 0x00171708 */
static void tfpart_exec(TfPart *w)
{
    Vec3 pos;

    if (--w->life <= 0)
        goto kill;
    camera_get_eye(&pos);
    vadd(&pos, &w->ofs);
    if (texforce_project(&pos, &w->sx, &w->sy))
        goto ok;
    do {
    kill:
        TASK_HDR(w)->flags |= 1;
    } while (0);
    TASK_HDR(w)->disp = 0;
    return;
ok:
    w->angle += w->dangle;
    w->col[0] += w->dcol[0];
    w->col[1] += w->dcol[1];
    w->col[2] += w->dcol[2];
    w->col[3] += w->dcol[3];
    w->w += w->dw;
    w->h += w->dh;
}

/* Centred at (x, y), rotated by angle. */
/* 0x00171810 */
static void tfpart_draw_quad(float x, float y, int angle, int color, float width, float height)
{
    FontVtx vtx[4];
    float s;
    float c;
    float ax;
    float ay;
    float end_x;
    float end_y;

    ulSinCos(angle, &s, &c);
    vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = color;
    ax = s * width * 0.5f;
    ay = c * width * 0.5f;
    vtx[3].z = 1.0526316f;
    height *= 0.5f;
    {
        float hs = s * height, hc = c * height;
        c = hc;
        s = hs;
    }
    end_x = x + c;
    end_y = y - s;
    x -= c;
    y += s;
    vtx[2].z = 1.0526316f;
    vtx[1].z = 1.0526316f;
    vtx[0].z = 1.0526316f;
    vtx[0].u = 1.0f;
    vtx[0].v = 0.0f;
    vtx[1].u = 0.0f;
    vtx[1].v = 0.0f;
    vtx[2].u = 1.0f;
    vtx[2].v = 1.0f;
    vtx[3].v = 1.0f;
    vtx[3].u = 0.0f;
    nj_tex_strip_begin(1);
    vtx[2].x = end_x + ax;
    vtx[2].y = end_y + ay;
    vtx[1].x = x - ax;
    vtx[1].y = y - ay;
    vtx[0].x = end_x - ax;
    vtx[0].y = end_y - ay;
    vtx[3].x = x + ax;
    vtx[3].y = y + ay;
    nj_tex_strip_vertices(vtx, 4);
    nj_tex_strip_end();
}

/* While active: into the texture, texture from texforce.ptm, current colour. */
/* 0x00171970 */
static void tfpart_disp(TfPart *w)
{
    int red;
    int green;
    int blue;
    int alpha;
    int color;

    if (texforce_is_active()) {
        draw_set_blend(4, 1);
        alpha = (int)(w->col[3] * 255.0f);
        red = (int)(w->col[0] * 255.0f);
        green = (int)(w->col[1] * 255.0f);
        blue = (int)(w->col[2] * 255.0f);
        color = ((red + 1) >> 1) << 24 | ((alpha + 1) >> 1 & 0xff) << 16 | ((blue + 1) >> 1 & 0xff) << 8
                | ((green + 1) >> 1 & 0xff);
        ulGxSetTextureList(system_texforce_ptm);
        ulGxSelectTexture(ulGxGetTextureList(), w->tex);
        tfpart_draw_quad(w->sx, w->sy, w->angle, color, w->w, w->h);
    }
}

/* 0x00171a80 */
static TfPart *tfpart_create(TexForce *tf, Vec3 *pos, KtfPart *part)
{
    TfPart *w;

    w = task_create(texforce_get_task(tf), &tfpart_desc, 88);
    tfpart_setup(w, tf, pos, part);
    return w;
}

/* 0x00171af8 */
static TfPart *tfpart_spawn(TexForce *tf, Vec3 *pos, KtfPart *part)
{
    return tfpart_create(tf, pos, part);
}

/*
 * Up to three parts for the current scene from the part table (enemy ids 16..45); needs a stage
 * controller.
 */
/* 0x00171b18 */
void TextureForce(int id, Vec3 *pos)
{
    int obj;
    int sel;
    TexForce *tf;
    int k;
    KtfPart *part;

    if (id < 0)
        return;
    obj = stage_get_tfctrl();
    sel = stage_get_child();
    if (!obj || !sel)
        return;
    tf = tfctrl_get_texforce((int *)obj);
    id = enemy_status_ent_get_id(id);
    k = map_manager_get_scene_id(sel);
    obj = id - 16;
    if (obj < 0) {
        ERROR_PRINTF("[E]TextureForce() RegistID is too Small! : %d\n", id);
        return;
    }
    if (obj < 30 && texforce_ktf_parts[obj]) {
        part = (*(KtfPart * * *)&texforce_ktf_parts)[obj] + k * 3; /* steer(cast) */
        if (part->count > 0)
            tfpart_spawn(tf, pos, part);
        part++;
        if (part->count > 0)
            tfpart_spawn(tf, pos, part);
        part++;
        if (part->count > 0)
            tfpart_spawn(tf, pos, part);
    }
}

/* Preset 0. */
/* 0x00171c58 */
static void tfscreen_reset(void)
{
    tfscreen_select(0);
}

/* Up to one with a negative type; selects preset 0. */
/* 0x00171c60 */
static void tfscreen_set_table(TfScreen *tbl)
{
    int n = 0;

    tfscreen_table = tbl;
    while (tbl->type_col[0] >= 0) {
        tbl++;
        n++;
    }
    tfscreen_table_num = n;
    tfscreen_select(0);
}

/* 0x00171cb0 */
static void tfscreen_clear(void)
{
    tfscreen_table = 0;
    tfscreen_table_num = 0;
    tfscreen_pattern = 0;
}

/* i out of range: 0. Type, colour, pivot, move, scale, rotation become current. */
/* 0x00171cd0 */
void tfscreen_select(int i)
{
    TfScreen *tbl;

    if (tfscreen_table) {
        if (i >= tfscreen_table_num)
            i = 0;
        tbl = &tfscreen_table[i];
        tfscreen_pattern = i;
        tfscreen_type = tbl->type_col[0];
        tfscreen_col_a = tbl->type_col[1];
        tfscreen_col_r = tbl->type_col[2];
        tfscreen_col_g = tbl->type_col[3];
        tfscreen_col_b = tbl->type_col[4];
        tfscreen_cx = tbl->xform[0];
        tfscreen_cy = tbl->xform[1];
        tfscreen_pos_x = tbl->xform[2];
        tfscreen_pos_y = tbl->xform[3];
        tfscreen_scl_x = tbl->xform[4];
        tfscreen_scl_y = tbl->xform[5];
        tfscreen_rot_z = tbl->xform[6];
    }
}

/*
 * Variables, UV grid, preset 0; capture area: y scale 0.9375 from the top in mode 2, else 1 from
 * 16 pixels down.
 */
/* 0x00171dd0 */
static void tfscreen_init(TfScreenTask *w, TexForce *tf, int *out)
{
    int obj;

    w->tf = tf;
    obj = texforce_get_varedit();
    if (obj) {
        tfscreen_vars_group = gw_varedit_add(gw_varedit_root, obj, tfscreen_vardef);
        gw_varedit_add_table(gw_varedit_root, tfscreen_vars_group, tfscreen_vardefs);
    }
    w->plane = tfplane_alloc();
    tfscreen_select(0);
    task_set_dlist(w, 0);
    if (sys_state == 2) {
        tfscreen_capture_scl_y = 0.9375f;
        tfscreen_capture_ofs_y = 0.0f;
    } else {
        tfscreen_capture_scl_y = 1.0f;
        tfscreen_capture_ofs_y = 16.0f;
    }
}

/* 0x00171ea8 */
static void tfscreen_release(TfScreenTask *w)
{
    int obj;

    tfplane_free(w->plane);
    obj = texforce_get_varedit();
    if (obj)
        gw_varedit_remove(gw_varedit_root, obj, tfscreen_vars_group);
}

/* 0x00171ef0 */
static void tfscreen_exec(void *w)
{
}

/* Once active: clears the texture (no previous image yet) -> tfscreen_disp. */
/* 0x00171ef8 */
static void tfscreen_disp_idle(TfScreenTask *w)
{
    float size;

    if (texforce_is_active()) {
        size = ((unsigned int (*)(TexForce *))texforce_get_size)(w->tf); /* steer(cast) */
        tfplane_draw_capture(size);
        TASK_HDR(w)->disp = (void (*)(void *))tfscreen_disp;
    }
}

/*
 * Clears the texture, draws the previous image back through the transformed grid in the preset
 * colour; back to tfscreen_disp_idle when the pass stops.
 */
/* 0x00171f68 */
static void tfscreen_disp(TfScreenTask *w)
{
    float size;
    int color;
    int old;
    int saved;

    if (!texforce_is_active()) {
        TASK_HDR(w)->disp = (void (*)(void *))tfscreen_disp_idle;
        return;
    }
    size = ((unsigned int (*)(TexForce *))texforce_get_size)(w->tf); /* steer(cast) */
    color = ((tfscreen_col_a + 1) >> 1) << 24 | ((tfscreen_col_b + 1) >> 1 & 0xff) << 16
            | ((tfscreen_col_g + 1) >> 1 & 0xff) << 8 | ((tfscreen_col_r + 1) >> 1 & 0xff);
    ulGxSetTexFilter(1);
    old = draw_set_test_mode(0x18000);
    texforce_bind(w->tf);
    tfplane_draw_capture(size);
    tfplane_build_uv(w->plane);
    saved = fog_set_enable(0);
    tfplane_draw(w->plane, color);
    fog_set_enable(saved);
    draw_set_test_mode(old);
}

/* 0x001720b8 */
static TfScreenTask *tfscreen_create(TexForce *tf, int *out)
{
    TfScreenTask *w;

    w = task_create(texforce_get_task(tf), &tfscreen_desc, 8);
    tfscreen_init(w, tf, out);
    return w;
}

/* 8x8 cells of 8 floats. */
/* 0x00172120 */
static int tfplane_alloc(void)
{
    return heap_block_tag(game_malloc(0x804), str_tfplane_tag, -1);
}

/* 0x00172150 */
static void tfplane_free(void *plane)
{
    heap_free(plane, 0);
}

/* Textured Gouraud strips, blended if abe. */
/* 0x00172158 */
static void tfplane_set_tag(int abe)
{
    int *tag = &tfplane_dma_tag;
    int blend = abe != 0;

    tfplane_gif_tag[0] = GIF_SET_TAG(0, 0, 1, GS_SET_PRIM(4, 1, 1, 0, blend, 0, 0, 0, 0), 0, 3);
    tfplane_gif_tag[1] = 0x412;
    *tag = abe;
}

/*
 * One 32x32 cell straight to the GS as a four-vertex strip: UVs in texels of the 256 texture, y
 * halved, relative to the drawing origin.
 */
/* 0x00172198 */
static void tfplane_draw_cell(float x, float y, unsigned int color, float *uv, float z)
{
    GxStateOrg *org = &ulGxState;
    int alpha = color >> 24;
    int blue = color >> 16 & 0xff;
    int green = color >> 8 & 0xff;
    int red = color & 0xff;
    void *pkt;
    void *dl;
    Qword *tag;
    Qword *qw;
    float top;
    float bottom;
    float right;

    pkt = psGsGetDisplayList();
    dl = psDisplayListOpenRefPkt(pkt);
    psVifPktOpenGifPkt(dl);
    psDisplayListFlushGsRegs(pkt, dl);
    tfplane_gif_tag[0] = (tfplane_gif_tag[0] & ~0x7fff) | 4;
    tag = psVifPktAddGifTag(dl, tfplane_gif_tag);
    qw = tag + 1;
    top = y * 0.5f;
    bottom = (y + 32.0f) * 0.5f;
    right = x + 32.0f;
    qw->stq[0] = uv[0] * 0.00390625f;
    qw->stq[1] = uv[1] * 0.00390625f;
    qw->stq[2] = 1.0f;
    qw++;
    qw->i[3] = alpha;
    qw->i[2] = blue;
    qw->i[1] = green;
    qw->i[0] = red;
    qw++;
    qw->i[0] = (x + org->x) * 16.0f;
    qw->i[1] = (top + org->y) * 16.0f;
    qw->i[2] = 16;
    qw->i[3] = 0;
    qw++;
    qw->stq[0] = uv[2] * 0.00390625f;
    qw->stq[1] = uv[3] * 0.00390625f;
    qw->stq[2] = 1.0f;
    qw++;
    qw->i[3] = alpha;
    qw->i[2] = blue;
    qw->i[1] = green;
    qw->i[0] = red;
    qw++;
    qw->i[0] = (x + org->x) * 16.0f;
    qw->i[1] = (bottom + org->y) * 16.0f;
    qw->i[2] = 16;
    qw->i[3] = 0;
    qw++;
    qw->stq[0] = uv[4] * 0.00390625f;
    qw->stq[1] = uv[5] * 0.00390625f;
    qw->stq[2] = 1.0f;
    qw++;
    qw->i[3] = alpha;
    qw->i[2] = blue;
    qw->i[1] = green;
    qw->i[0] = red;
    qw++;
    qw->i[0] = (right + org->x) * 16.0f;
    qw->i[1] = (top + org->y) * 16.0f;
    qw->i[2] = 16;
    qw->i[3] = 0;
    qw++;
    qw->stq[0] = uv[6] * 0.00390625f;
    qw->stq[1] = uv[7] * 0.00390625f;
    qw->stq[2] = 1.0f;
    qw++;
    qw->i[3] = alpha;
    qw->i[2] = blue;
    qw->i[1] = green;
    qw->i[0] = red;
    qw++;
    qw->i[0] = (right + org->x) * 16.0f;
    qw->i[1] = (bottom + org->y) * 16.0f;
    qw->i[2] = 16;
    qw->i[3] = 0;
    qw++;
    psVifPktAdvanceGif(dl, qw, 12);
    psVifPktCloseGifPkt(dl);
    psDisplayListCloseRefPkt(pkt);
}

/* Black, alpha 0x80. */
/* 0x00172478 */
static void tfplane_draw_capture(float size)
{
    TfPlaneVtx vtx[4];
    float x = 0.0f;
    float y = tfscreen_capture_ofs_y;

    vtx[0].y = y;
    vtx[1].x = x;
    vtx[3].y = vtx[1].y = y + size;
    vtx[2].y = y;
    vtx[3].x = vtx[2].x = x + size;
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = 0.01f;
    vtx[2].color = vtx[3].color = COL_BLACK;
    vtx[0].color = vtx[1].color = COL_BLACK;
    vtx[0].x = x;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* 8x8 cells of 32 pixels, blended. */
/* 0x00172510 */
static void tfplane_draw(float *uv, unsigned int color)
{
    int i;
    int j;
    float x;
    float y;
    float next_y;

    ulGxSetAlphaBlend(1, 1);
    tfplane_set_tag(1);
    y = tfscreen_capture_ofs_y;
    for (j = 0; j < 8; j++) {
        next_y = y + 32.0f;
        x = 0.0f;
        for (i = 0; i < 8; i++) {
            tfplane_draw_cell(x, y, color, uv, 0.01f);
            uv += 8;
            x += 32.0f;
        }
        y = next_y;
    }
}

/* About pivot (cx, cy), current matrix. */
/* 0x001725e8 */
static void tfplane_transform(float cx, float cy, float x, float y, float *ox, float *oy)
{
    Vec4 pt;

    pt.x = x - cx;
    pt.y = y - cy;
    pt.z = 0.0f;
    ulMatrixApplyPoint(ulMatrixCur, &pt, &pt);
    *ox = pt.x + cx;
    *oy = pt.y + cy;
}

/*
 * Type 0: whole grid scaled, rotated (degrees), moved about (tfscreen_cx, tfscreen_cy) from the
 * texture centre: zoom or spin trail.
 */
/* 0x00172670 */
static void tfplane_build_global(float *uv)
{
    int i;
    int j;
    float cx;
    float x;
    float next_x;
    float cy;
    float y;
    float next_y;
    float cell;

    ulMatrixPush();
    cx = tfscreen_cx + 128.0f;
    cy = tfscreen_cy + 128.0f;
    matrix_unit_set(0);
    vu0_matrix_scale(ulMatrixCur, tfscreen_scl_x, tfscreen_scl_y, 1.0f);
    vu0_matrix_rotate_z(ulMatrixCur, DEG_TO_ANG(tfscreen_rot_z));
    vu0_matrix_translate(ulMatrixCur, tfscreen_pos_x, tfscreen_pos_y, 0.0f);
    cell = 32.0f;
    y = 0.0f;
    for (j = 0; j < 8; j++) {
        x = 0.0f;
        next_y = y + cell;
        for (i = 0; i < 8; i++) {
            tfplane_transform(cx, cy, x, y, &uv[0], &uv[1]);
            next_x = x + cell;
            tfplane_transform(cx, cy, x, y + cell, &uv[2], &uv[3]);
            tfplane_transform(cx, cy, next_x, y, &uv[4], &uv[5]);
            tfplane_transform(cx, cy, next_x, y + cell, &uv[6], &uv[7]);
            x = next_x;
            uv += 8;
        }
        y = next_y;
    }
    ulMatrixPop();
}

/* Type 1: each 32-pixel cell about its own centre. */
/* 0x00172838 */
static void tfplane_build_local(float *uv)
{
    Vec3 center;
    int i;
    int j;
    float x;
    float y;
    float next_x;
    float next_y;
    float cx;
    float cy;
    float cell;

    center.z = 0.0f;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_scale(ulMatrixCur, tfscreen_scl_x, tfscreen_scl_y, 1.0f);
    vu0_matrix_rotate_z(ulMatrixCur, DEG_TO_ANG(tfscreen_rot_z));
    vu0_matrix_translate(ulMatrixCur, tfscreen_pos_x, tfscreen_pos_y, 0.0f);
    cell = 32.0f;
    y = 0.0f;
    for (j = 0; j < 8; j++) {
        next_y = y + cell;
        cy = y + 16.0f;
        x = 0.0f;
            for (i = 0; i < 8; i++) {
                cx = x + 16.0f;
                next_x = x + cell;
                tfplane_transform(cx, cy, x, y, &uv[0], &uv[1]);
                tfplane_transform(cx, cy, x, y + cell, &uv[2], &uv[3]);
                tfplane_transform(cx, cy, next_x, y, &uv[4], &uv[5]);
                tfplane_transform(cx, cy, next_x, y + cell, &uv[6], &uv[7]);
                x = next_x;
                uv += 8;
            }
            y = next_y;
    }
    ulMatrixPop();
}

/* 0x00172a00 */
static void tfplane_build_uv(float *uv)
{
    switch (tfscreen_type) {
    case 0:
        tfplane_build_global(uv);
        break;
    case 1:
        tfplane_build_local(uv);
        break;
    }
}
