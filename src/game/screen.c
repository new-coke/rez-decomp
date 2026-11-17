/*
 * Screen: size and safe-area geometry, viewport, and full-screen effects rendered through a
 * capture texture (mosaic: scene drawn smaller and stretched back; coloured fade overlay). Also
 * the "ScreenEffect" debug menu and its debug-pad test tasks.
 */

#include "types_game.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_menu.h"
#include "gw_varedit.h"
#include "nj_draw.h"
#include "nj_view.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_texture.h"
#include "screen.h"
#include "camera.h"
#include "debugmenu.h"
#include "float_bits.h"
#include "fog.h"
#include "heap.h"
#include "light.h"
#include "node_draw_state.h"
#include "render.h"
#include "screen_lens.h"
#include "task.h"
#include "color.h"

extern int screen_wave_on;
extern int scree_size;
extern ScreenVtx screen_fade_quad[4];
extern ScreenVtx screen_quad[4];
extern int screen_surface;
extern const unsigned int sys_state;
extern int screen_effect_mode;
extern unsigned blur_col;
extern int blur_rate;
extern int blur_radial;
extern float mosaic_rate;
extern int b_mosaic_bilinear;
extern float screen_scl;
extern float screen_mosaic_scl;
extern float screen_mosaic_scl_prev;
extern int screen_varedit;
extern char gw_varedit_root[];
extern GwVarDef screen_vardef[];
extern GwVarDef screen_vardefs[];
extern GwVarDef screen_effect_vardefs[];
extern int soft_reset(char *);
extern float screen_layout[2];
extern int screen_active;
extern int b_render;
extern int b_renderback;
extern float screen_cfg[];
extern float screen_clip_scl[];
extern int screen_viewport_scaled;
extern void (*screen_effect_funcs[])(void);
extern int screen_render_prev;
extern int screen_fade_armed;
extern int screen_effect_cnt;
extern float *screen_mosaic_work;
extern PadAnalog *pad_debug;
extern void *scr_root;
extern struct TaskDesc screen_mosaic_desc;
extern int screen_wave_test_task;
extern struct TaskDesc screen_wave_test_desc;
extern int *screen_blur_task;
extern struct TaskDesc screen_blur_desc;
extern char screen_tex_list[];

static void screen_sync_filter_flag(int enable);
static void screen_fade_draw(int enable);
static void screen_sync_fade_flag(void);
static void screen_apply_scale(int enable);
static void screen_effect_pass(int enable);
static void screen_filter_toggle(int item);
static void screen_mosaic_toggle(int item);
static void screen_wave_test_toggle(int item);
static void screen_blur_toggle(int item);

/* 0x0015ffa0 */
static void screen_nop(void)
{
}

/* 0x0015ffa8 */
static void screen_nop_2(void)
{
}

/* 0x0015ffb0 */
static void screen_nop_3(void)
{
}

/* (Nearly) 0 stops the wave; anything else starts it if needed. */
/* 0x0015ffb8 */
static void screen_wave_set_level(float x)
{
    if (fabsf(x) < FLT_EPSILON) {
        if (screen_wave_on) {
            screen_wave_stop();
            screen_wave_on = 0;
        }
        screen_wave_set_strength(0.0f);
    } else {
        if (!screen_wave_on) {
            screen_wave_start();
            screen_wave_on = 1;
        }
        screen_wave_set_strength(x);
    }
}

/* 0x00160050 */
static void screen_filter_select(void)
{
}

/* Mode 1: centre, scale from 640x480, no side or top margin, 2-pixel bottom margin. */
/* 0x00160058 */
static void screen_geom_set_fullscreen(void)
{
    ScreenGeom *geom = &screen_layout;

    geom->cx = geom->w * 0.5f;
    geom->cy = geom->h * 0.5f;
    geom->sx = geom->w / 640.0f;
    geom->sy = geom->h / 480.0f;
    geom->left = 0.0f;
    geom->right = geom->w - geom->left;
    geom->top = geom->left;
    geom->bottom = geom->h - 2.0f;
}

/* Default mode: margins 16 left/right, 40 top/bottom. */
/* 0x001600d8 */
static void screen_geom_set_default(void)
{
    ScreenGeom *geom = &screen_layout;

    geom->cx = geom->w * 0.5f;
    geom->cy = geom->h * 0.5f;
    geom->sx = geom->w / 640.0f;
    geom->sy = geom->h / 480.0f;
    geom->left = 16.0f;
    geom->right = geom->w - 16.0f;
    geom->top = 40.0f;
    geom->bottom = geom->h - 40.0f;
}

/* Mode 2: same margins as the default. */
/* 0x00160160 */
static void screen_geom_set_safe_area(void)
{
    ScreenGeom *geom = &screen_layout;

    geom->cx = geom->w * 0.5f;
    geom->cy = geom->h * 0.5f;
    geom->sx = geom->w / 640.0f;
    geom->sy = geom->h / 480.0f;
    geom->left = 16.0f;
    geom->right = geom->w - 16.0f;
    geom->top = 40.0f;
    geom->bottom = geom->h - 40.0f;
}

/* 0x001601e8 */
static void screen_geom_update(void)
{
    switch (scree_size) {
    case 1:
        screen_geom_set_fullscreen();
        break;
    case 2:
        screen_geom_set_safe_area();
        break;
    case 0:
    default:
        screen_geom_set_default();
        break;
    }
}

static inline void c15_setz(ScreenVtx *vtx, unsigned col, float z)
{
    vtx->z = z;
    vtx->c = col;
}

/*
 * 1024x256 capture surface in its own VRAM heap; effect and fade quad depth and colour; mode from
 * sys_state; 640x480.
 */
/* steer(cast) */
/* 0x00160230 */
void screen_init(void)
{
    int req[5];
    int old;
    float z;
    ScreenVtx *vtx;
    unsigned col;

    old = ulTextureSetVramHeap(ulTextureInitVramHeap((int)heap_block_tag(game_malloc(384), "screen.c", 402), 384, 0xc0000, 0x40000, 10));
    req[0] = 1024;
    req[1] = 256;
    req[2] = 0;
    req[3] = 0;
    req[4] = (int)req;
    screen_surface = ((int (*)(int *))ulTextureCreateSurface)(req);
    ulTextureSetVramHeap(old);
    vtx = screen_quad;
    z = float_round_mantissa(0.002f);
    col = COL_WHITE;
    c15_setz(&vtx[3], col, z);
    c15_setz(&vtx[2], col, z);
    c15_setz(&vtx[1], col, z);
    c15_setz(&vtx[0], col, z);
    vtx = screen_fade_quad;
    z = float_round_mantissa(1.0f);
    col = RGBA(0x80, 0x80, 0x80, 0x40);
    {
        ScreenVtx *vtx1 = &vtx[1];
        c15_setz(&vtx[3], col, z);
        c15_setz(&vtx[2], col, z);
        c15_setz(vtx1, col, z);
    }
    c15_setz(&vtx[0], col, z);
    scree_size = sys_state;
    screen_set_size(640.0f, 480.0f);
    switch (*(volatile unsigned int *)&sys_state) { /* steer(volatile) */
    case 0:
        ((void (*)(int))screen_filter_select)(1); /* steer(cast) */
        break;
    case 1:
        ((void (*)(int))screen_filter_select)(0); /* steer(cast) */
        break;
    case 2:
        ((void (*)(int))screen_filter_select)(0); /* steer(cast) */
        break;
    }
}

/*
 * Effects reset (no fade, mosaic and scales 1.0); "Screen" soft reset; "ScreenEffect" menu
 * (Render, Mosaic, Lens, Wave, Blur) and variables.
 */
/* 0x001603d0 */
void screen_menu_init(void)
{
    screen_effect_flags = 0;
    screen_effect_mode = 0;
    soft_reset("Screen");
    blur_rate = 0;
    blur_col = 0x808080;
    blur_radial = 0;
    mosaic_rate = 1.0f;
    b_mosaic_bilinear = 0;
    screen_scl = screen_mosaic_scl = screen_mosaic_scl_prev = 1.0f;
    menu_item_add(0, "ScreenEffect", 0, 0, 1);
    menu_item_add("ScreenEffect", "Render", screen_filter_toggle, 0, 0);
    menu_item_add("ScreenEffect", "Mosaic", screen_mosaic_toggle, 0, 0);
    menu_item_add("ScreenEffect", "Lens", screen_lens_toggle, 0, 0);
    menu_item_add("ScreenEffect", "Wave", screen_wave_test_toggle, 0, 0);
    menu_item_add("ScreenEffect", "Blur", screen_blur_toggle, 0, 0);
    screen_varedit = gw_varedit_add(gw_varedit_root, 0, screen_vardef);
    gw_varedit_add_table(gw_varedit_root, screen_varedit, screen_vardefs);
    gw_varedit_add_table(gw_varedit_root, screen_varedit, screen_effect_vardefs);
}

/* 0x00160568 */
static void screen_reset_size(void)
{
    screen_set_size(screen_layout[0], screen_layout[1]);
}

/* 0x00160580 */
static int screen_get_varedit(void)
{
    return screen_varedit;
}

/* From screen size, mosaic scale (share of the capture texture holding the frame), fade margin. */
/* 0x00160590 */
static void screen_quads_update(void)
{
    float width = screen_layout[0];
    float height = screen_layout[1];
    float margin = (float)blur_radial;
    float scale = screen_mosaic_scl;
    float left, top, ww, hh;
    float right, bottom, u_left, v_top, u_right, v_bottom;
    float ofs;
    ScreenVtx *vtx;

    if (scree_size == 2) {
        left = 0.0f;
        top = 0.0f;
        ofs = 0.5f;
        ww = width + ofs;
        hh = 512.0f + ofs;
    } else {
        left = 0.0f;
        top = 16.0f;
        ofs = 0.5f;
        ww = width + ofs;
        hh = height + ofs;
    }
    right = left + width;
    bottom = top + height;
    u_left = ofs / 1024.0f;
    v_top = ofs / 512.0f;
    u_right = scale * ww / 1024.0f;
    v_bottom = scale * hh / 512.0f;
    vtx = screen_quad;
    vtx[0].x = vtx[1].x = left;
    vtx[0].y = vtx[2].y = top;
    vtx[2].x = vtx[3].x = right;
    vtx[1].y = vtx[3].y = bottom;
    vtx[0].u = vtx[1].u = u_left;
    vtx[0].v = vtx[2].v = v_top;
    vtx[2].u = vtx[3].u = u_right;
    vtx[1].v = vtx[3].v = v_bottom;
    vtx = screen_fade_quad;
    vtx[0].x = vtx[1].x = left - margin;
    vtx[0].y = vtx[2].y = top - margin;
    vtx[2].x = vtx[3].x = right + margin;
    vtx[1].y = vtx[3].y = bottom + margin;
    vtx[0].u = vtx[1].u = u_left;
    vtx[0].v = vtx[2].v = v_top;
    vtx[2].u = vtx[3].u = u_right;
    vtx[1].v = vtx[3].v = v_bottom;
}

/* Warns on zero size; re-derives quads and geometry. */
/* 0x001606e8 */
void screen_set_size(float width, float height)
{
    if (fabsf(width) < FLT_EPSILON)
        ERROR_PRINTF("Width  is ZERO! old = % 8.3f\n", screen_layout[0]);
    if (fabsf(height) < FLT_EPSILON)
        ERROR_PRINTF("Height is ZERO! old = % 8.3f\n", screen_layout[1]);
    screen_layout[0] = width;
    screen_layout[1] = height;
    screen_quads_update();
    screen_geom_update();
}

/* 2 while a screen effect is active, else 1. */
/* 0x001607b0 */
int screen_get_pass_cnt(void)
{
    return screen_active ? 2 : 1;
}

/* Rendering through the capture texture; returns the previous setting. */
/* 0x001607c8 */
int screen_swap_render(int on)
{
    int old = b_render;

    b_render = on;
    return old;
}

/* Returns the previous setting. */
/* 0x001607d8 */
int screen_swap_render_back(int on)
{
    int old = b_renderback;

    b_renderback = on;
    return old;
}

/* Clamped 0..255; 0 switches the overlay off. */
/* 0x001607e8 */
void screen_set_fade_alpha(int val)
{
    int alpha = val;

    if (alpha < 0)
        alpha = 0;
    blur_rate = alpha < 256 ? alpha : 255;
}

/* RGB. */
/* 0x00160810 */
void screen_set_fade_color(int col)
{
    blur_col = col & 0xffffff;
}

/* Reach past the screen edges, clamped 0..2000. */
/* 0x00160828 */
void screen_set_margin(int val)
{
    int margin = val;

    if (margin < 0)
        margin = 0;
    blur_radial = margin < 2001 ? margin : 2000;
    screen_quads_update();
}

/* 1.0 off; smaller draws the scene smaller (coarser). */
/* 0x00160850 */
static void screen_set_mosaic_rate(float v)
{
    mosaic_rate = v;
}

/* 0x00160860 */
static float screen_get_mosaic_rate(void)
{
    return mosaic_rate;
}

/* 0x00160870 */
float screen_get_mosaic_scale(void)
{
    return screen_mosaic_scl;
}

/* 0x00160880 */
void screen_frame_hook(void)
{
}

/* 0x00160888 */
void screen_redraw(void)
{
}

/* Centre, Ninja aspect relative to 640x480, clip area, whether scaled at all. */
/* 0x00160890 */
static void screen_set_viewport(float width, float height)
{
    float hh;
    float ww;

    ww = width * 0.5f;
    hh = height * 0.5f;
    screen_cfg[3] = hh;
    screen_cfg[2] = ww;
    screen_cfg[11] = 480.0f;
    screen_cfg[10] = 640.0f;
    njSetAspect(width * 0.0015625f, height * (1.0f / 480.0f));
    screen_clip_scl[0] = 0.0f;
    screen_clip_scl[1] = 0.0f;
    screen_clip_scl[2] = (width - 1.0f) * 0.03125f;
    screen_clip_scl[3] = (height - 1.0f) * 0.03125f;
    if (width == 640.0f && height == 480.0f)
        screen_viewport_scaled = 0;
    else
        screen_viewport_scaled = 1;
}

/* Viewport = screen size x scale. */
/* 0x00160990 */
static void screen_set_scale(float scale)
{
    float prev = screen_scl;

    if (prev != scale) {
        screen_scl = scale;
        prev = scale;
    }
    screen_set_viewport(screen_layout[0] * prev, screen_layout[1] * prev);
}

/* Non-zero while any screen effect is on. */
/* 0x001609d0 */
int screen_get_effect_mode(void)
{
    return screen_effect_mode;
}

/* Mosaic scale while the mosaic is on. */
/* 0x001609e0 */
void screen_apply_scale_frame(void)
{
    screen_apply_scale(1);
}

/*
 * Fade overlay; with a screen effect also flips and starts a full-scale second pass (camera and
 * lights updated again).
 */
/* 0x001609e8 */
void screen_enable(void)
{
    screen_fade_draw(1);
    if (screen_effect_mode) {
        screen_active = 1;
        render_flip();
        render_submit_frame(2, -1);
        screen_set_scale(1.0f);
        screen_redraw();
        camera_update();
        light_update();
    } else
        screen_active = 0;
}

/* ((rate * 80 + 20) / 100)^2; keeps the previous; rebuilds the quads. */
/* 0x00160a60 */
static void screen_scale_apply(void)
{
    float z = mosaic_rate;

    screen_mosaic_scl_prev = screen_mosaic_scl;
    z = z * 80.0f + 20.0f;
    z = z * z * 0.0001f;
    screen_mosaic_scl = z;
    screen_quads_update();
}

/*
 * Runs the handler for this frame's effect flags, updates capture/fade/mosaic flags (mosaic draws
 * the captured frame back), latches the flags.
 */
/* 0x00160ab0 */
void screen_update(void)
{
    if (screen_effect_funcs[screen_effect_mode])
        screen_effect_funcs[screen_effect_mode]();
    screen_sync_filter_flag(1);
    ((void (*)(int))screen_sync_fade_flag)(1); /* steer(cast) */
    screen_effect_pass(1);
    screen_effect_mode = screen_effect_flags;
}

/*
 * A change of b_render (menu or screen_swap_render) sets or clears flag 1; b_render then follows
 * the flag.
 */
/* 0x00160b20 */
static void screen_sync_filter_flag(int enable)
{
    if (screen_render_prev != b_render) {
        screen_render_prev = b_render;
        if (b_render)
            screen_effect_flags |= 1;
        else
            screen_effect_flags &= ~1;
    }
    if (screen_effect_flags & 1)
        b_render = 1;
    else
        b_render = 0;
}

/* While flag 0x10, from its second frame: captured frame tinted by fade colour/alpha, list 9. */
/* 0x00160b88 */
static void screen_fade_draw(int enable)
{
    unsigned col;
    int *obj;
    int old;

    if (screen_effect_mode & 0x10) {
        if (screen_fade_armed == 0) {
            screen_fade_armed = 1;
            return;
        }
        col = blur_col | (((blur_rate + 1) >> 1) << 24);
        screen_fade_quad[3].c = col;
        screen_fade_quad[2].c = col;
        screen_fade_quad[1].c = col;
        screen_fade_quad[0].c = col;
        RenderSetCurrentDisplayList(9);
        obj = screen_get_tex_list();
        ulGxSetTextureList(obj);
        ulGxSelectTexture(ulGxGetTextureList(), 0);
        psGsWriteReg(6, *(unsigned long *)(*(int *)(*obj + 8)) & ~((unsigned long)1 << 34));
        old = draw_set_test_mode(0x18000);
        draw_set_blend(4, 5);
        psGsSetDepthTest(1, 1);
        psGsSetZMask(1);
        nj_tex_strip_draw(screen_fade_quad, 4, 1);
        psGsSetDepthTest(1, 2);
        psGsSetZMask(0);
        draw_set_test_mode(old);
        return;
    }
    screen_fade_armed = 0;
}

/* While the alpha is non-zero. */
/* 0x00160ce0 */
static void screen_sync_fade_flag(void)
{
    if (blur_rate)
        screen_effect_flags |= 0x10;
    else
        screen_effect_flags &= ~0x10;
}

/* Mosaic scale while effect flag 2, else 1.0. */
/* 0x00160d18 */
static void screen_apply_scale(int enable)
{
    float scale;

    if (screen_effect_flags & 2)
        scale = screen_mosaic_scl;
    else
        scale = 1.0f;
    screen_set_scale(scale);
    screen_redraw();
}

/*
 * Captured (smaller) frame stretched over the screen, bilinear with b_mosaic_bilinear. On as soon
 * as the rate leaves 1.0, off two frames after it returns.
 */
/* 0x00160d58 */
static void screen_effect_pass(int enable)
{
    int prev_filt;
    int prev_blend;
    int prev_fog;

    if (screen_effect_mode & 2) {
        if (b_mosaic_bilinear)
            prev_filt = draw_set_fc_mode(8192);
        else
            prev_filt = draw_set_fc_mode(0);
        prev_blend = draw_set_test_mode(0x18000);
        prev_fog = fog_set_enable(0);
        ulGxSetTextureList(screen_get_tex_list());
        ulGxSelectTexture(ulGxGetTextureList(), 0);
        nj_tex_strip_draw(screen_quad, 4, 0);
        fog_set_enable(prev_fog);
        draw_set_test_mode(prev_blend);
        draw_set_fc_mode(prev_filt);
    }
    if (mosaic_rate == 1.0f) {
        screen_effect_cnt = screen_effect_cnt + 1;
        if (screen_effect_cnt == 2) {
            screen_effect_flags &= ~2;
            screen_scale_apply();
        }
    } else {
        screen_effect_flags |= 2;
        screen_effect_cnt = 0;
        screen_scale_apply();
    }
}

/* 0x00160eb8 */
static void screen_filter_toggle(int item)
{
    b_render ^= 1;
    gw_item_set_value(item, b_render);
}

/* Rate 1.0. */
/* 0x00160ed0 */
static void screen_mosaic_init(float *w)
{
    screen_mosaic_work = w;
    w[0] = 1.0f;
    w[1] = 1.0f;
}

/* Rate from the debug pad's first analog axis, a frame late, at most 1.0. */
/* 0x00160ef0 */
static void screen_mosaic_disp(float *w)
{
    w[0] = (float)(255 - pad_debug->analog0) / 255.0f;
    if (w[1] < 1.0f)
        screen_set_mosaic_rate(w[1]);
    else
        screen_set_mosaic_rate(1.0f);
    w[1] = w[0];
}

/* 0x00160f78 */
static void screen_mosaic_release(void)
{
    screen_mosaic_work = 0;
    screen_set_mosaic_rate(1.0f);
}

/* 0x00160f90 */
static void screen_mosaic_toggle(int item)
{
    if (screen_mosaic_work == 0) {
        task_create(scr_root, &screen_mosaic_desc, 8);
        gw_item_set_value(item, 1);
    } else {
        do {
            if (screen_mosaic_work)
                TASK_OF(screen_mosaic_work)->flags |= TASK_F_END;
        } while (0);
        gw_item_set_value(item, 0);
    }
}

/* 0x00161008 */
static void screen_wave_test_init(void)
{
    screen_wave_start();
}

/* 0x00161010 */
static void screen_wave_test_release(void)
{
    screen_wave_stop();
    screen_wave_test_task = 0;
}

/* Strength from the debug pad's second analog axis. */
/* 0x00161038 */
static void screen_wave_test_exec(void)
{
    screen_wave_set_strength((float)pad_debug->analog1 / 255.0f);
}

/* 0x00161068 */
static void screen_wave_test_toggle(int item)
{
    if (screen_wave_test_task == 0) {
        screen_wave_test_task = task_create(scr_root, &screen_wave_test_desc, 0);
        gw_item_set_value(item, 1);
    } else {
        do {
            if (screen_wave_test_task)
                TASK_OF(screen_wave_test_task)->flags |= TASK_F_END;
        } while (0);
        gw_item_set_value(item, 0);
    }
}

/* Fade alpha from the debug pad's first analog axis. */
/* 0x001610e8 */
static void screen_blur_disp(int *w)
{
    int analog = pad_debug->analog0;

    *w = analog;
    screen_set_fade_alpha(analog);
}

/* 0x00161100 */
static void screen_blur_toggle(int item)
{
    if (screen_blur_task == 0) {
        screen_blur_task = task_create(scr_root, &screen_blur_desc, 4);
        gw_item_set_value(item, 1);
    } else {
        do {
            if (screen_blur_task)
                TASK_OF(screen_blur_task)->flags |= TASK_F_END;
        } while (0);
        screen_blur_task = 0;
        gw_item_set_value(item, 0);
    }
}

/* 0x00161188 */
char *screen_get_tex_list(void)
{
    return screen_tex_list;
}

/* 0 when there is none. */
/* 0x00161198 */
int screen_get_surface_addr(void)
{
    if (screen_surface == 0)
        return 0;
    return ulTextureGetAddr(screen_surface);
}
