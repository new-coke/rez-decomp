/*
 * Ninja view glue: frame begin/end, clear colour, clip, projection, fov, aspect, defaults (640 x
 * 480 at (320, 240)), debug console, chunk-renderer model overrides.
 */

#include "ninja.h"
#include "mathdef.h"
#include <stdarg.h>
#include "libc.h"
#include "nj_view.h"
#include "nj_cnk_direct.h"
#include "nj_math.h"
#include "ps_gs.h"
#include "ul_console.h"
#include "ul_gx.h"
#include "nj_cnk_vu1.h"

static void nj_view_defaults(void);
static int nj_console_is_open(void);

extern float nj_scale_ratio[2];
extern void sceGsSyncV(int a);
extern int nj_console_opened;
extern char nj_console_buf[];
extern char nj_console[];
extern int nj_cnk_override_mode;
extern int nj_override_flags;
extern int nj_strip_and_mask;
extern int nj_strip_or_bits;
extern float nj_cnk_override_rgba[4];

/* 0x001a2060 */
static void nj_set_scale_ratio(int src_width, int src_height, int dst_width, int dst_height)
{
    nj_scale_ratio[0] = (float)dst_width / (float)src_width;
    nj_scale_ratio[1] = (float)dst_height / (float)src_height;
}

/* Resets renderer counters; clears the debug console if open. */
/* 0x001a20a8 */
static void nj_frame_begin(void)
{
    ulGxResetCounters();
    if (nj_console_is_open())
        ulConsoleClear();
}

/* Draws the debug console if open, waits for the GS paths; returns 0. */
/* 0x001a20d8 */
static int nj_frame_end(void)
{
    if (nj_console_is_open())
        ulConsoleDraw();
    ulGxSyncPath();
    return 0;
}

/* count 64-byte vertices; resets the screen scale. */
/* 0x001a2110 */
static void nj_vertex_buffer_init(int buf, int count)
{
    ulGxSetVertexBuffer_thunk(buf, count * 64);
    nj_set_screen_scale(1.0f, 1.0f);
}

/* 0x001a2138 */
void nj_system_reset(void)
{
}

/* 0xRRGGBB. */
/* 0x001a2140 */
void nj_set_clear_color(unsigned int col)
{
    psGsSetClearColor((col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
}

/* 0x001a2160 */
static int nj_view_init(void)
{
    nj_view_defaults();
    return 0;
}

/* 0x001a2180 */
void nj_view_exit(void)
{
}

/* 0x001a2188 */
static void nj_view_nop(void)
{
}

/* 0x001a2190 */
static void nj_view_nop_2(void)
{
}

/* 0x001a2198 */
static void njWaitVSync(void)
{
    sceGsSyncV(0);
}

/* 0x001a21a0 */
static void nj_view_nop_3(void)
{
}

/* 0x001a21a8 */
static void nj_view_nop_4(void)
{
}

/* 0x001a21b0 */
void nj_profile_get_timing(void)
{
}

/* Signs dropped. */
/* 0x001a21b8 */
static void njClipZ(float near_z, float far_z)
{
    cam_far.near = fabsf(near_z);
    cam_far.zfar = fabsf(far_z);
    ulGxSetDepthRange(cam_far.near, cam_far.zfar);
}

/* Screen distance in pixels; rebuilds the projection matrix. */
/* 0x001a21d8 */
static void nj_set_proj_distance(float dist)
{
    NjMatrix mtx;

    screen_cfg.dist = dist;
    screen_cfg.sx = screen_cfg.xscale * dist;
    screen_cfg.sy = -(screen_cfg.yscale * dist);
    screen_cfg.inv_sx = 1.0f / screen_cfg.sx;
    screen_cfg.inv_sy = 1.0f / screen_cfg.sy;
    ulGxProj.dist = dist;
    ulGxMakeProjMatrix(&mtx, dist, ulGxProj.xscale, ulGxProj.yscale, ulGxProj.cx, ulGxProj.cy);
    ulGxSetMatrix(2, &mtx);
}

/* Horizontal; 65536 = full turn. */
/* 0x001a2270 */
void njSetPerspective(int ang)
{
    float dist;

    dist = screen_cfg.width / (njTan(ang / 2) * 2.0f);
    screen_cfg.dist = dist;
    screen_cfg.sx = screen_cfg.xscale * dist;
    screen_cfg.sy = -(screen_cfg.yscale * dist);
    screen_cfg.inv_sx = 1.0f / screen_cfg.sx;
    screen_cfg.inv_sy = 1.0f / screen_cfg.sy;
    ulGxSetHFov((float)ang * (3.141592f / ANG_HALF));
}

/* Pixel aspect. */
/* 0x001a2318 */
void njSetAspect(float sx, float sy)
{
    screen_cfg.xscale = sx;
    screen_cfg.yscale = sy;
    screen_cfg.aspect = sy / sx;
    screen_cfg.sx = sx * screen_cfg.dist;
    screen_cfg.sy = -(sy * screen_cfg.dist);
    screen_cfg.inv_sx = 1.0f / screen_cfg.sx;
    screen_cfg.inv_sy = 1.0f / screen_cfg.sy;
    ulGxSetAspect(sx, sy);
}

/* Distance, screen centre, frustum centred relative to (320, 240) around GS 2048. */
/* 0x001a2388 */
static void nj_view_apply(NjViewDesc *view)
{
    NjScreen screen;
    float sx, sy;
    float dist;

    dist = view->dist;
    screen_cfg.dist = dist;
    screen_cfg.cx = view->cx;
    screen_cfg.cy = view->cy;
    screen_cfg.sy = -(screen_cfg.yscale * dist);
    screen_cfg.sx = screen_cfg.xscale * dist;
    screen_cfg.inv_sx = 1.0f / screen_cfg.sx;
    screen_cfg.inv_sy = 1.0f / screen_cfg.sy;
    ulGxProj.dist = view->dist;
    ulGxProj.cx = view->cx;
    ulGxProj.cy = view->cy;
    ulGxGetPixelScale(&sx, &sy);
    ulGxGetFrustum(&screen);
    screen.dist = view->dist;
    screen.ofs_x = (view->cx - 320.0f) * sx + 2048.0f;
    screen.ofs_y = (view->cy - 240.0f) * sy + 2048.0f;
    ulGxSetFrustum(&screen);
}

/* 640 x 480 at (320, 240), distance 500, unit scales, clip 1..60000. */
/* 0x001a2498 */
static void nj_view_defaults(void)
{
    float dist;

    dist = 500.0f;
    screen_cfg.yscale = 1.0f;
    screen_cfg.cx = 320.0f;
    screen_cfg.cy = 240.0f;
    screen_cfg.width = 640.0f;
    screen_cfg.xscale = 1.0f;
    screen_cfg.height = 480.0f;
    screen_cfg.sx = dist;
    screen_cfg.dist = dist;
    screen_cfg.sy = -dist;
    screen_cfg.aspect = 1.0f;
    screen_cfg.inv_sx = 0.002f;
    screen_cfg.inv_sy = -0.002f;
    njClipZ(1.0f, 60000.0f);
}

/* 0x001a2518 */
static int nj_console_is_open(void)
{
    return nj_console_opened != 0;
}

/* size x size pixel cells. */
/* 0x001a2528 */
static void nj_console_set_cell_size(int size)
{
    size &= 0xffff;
    ulConsoleSetCellSize(size, size);
}

/* loc = x << 16 | y, in characters. */
/* 0x001a2538 */
static void njPrintC(unsigned int loc, const char *str)
{
    ulConsoleSetCursor(loc >> 16, loc & 0xffff);
    ulConsolePuts(str);
}

/* printf-style, as njPrintC. */
/* 0x001a2568 */
static void njPrint(unsigned int loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsprintf(nj_console_buf, fmt, ap);
    ulConsoleSetCursor(loc >> 16, loc & 0xffff);
    ulConsolePuts(nj_console_buf);
}

/* 0x001a25d0 */
void nj_console_close(void)
{
    ulConsoleExit();
    nj_console_opened = 0;
}

/* 80 x 60 characters. */
/* 0x001a25f0 */
static void nj_console_open(void)
{
    ulConsoleInit(nj_console, 80, 60);
    nj_console_opened = 1;
}

/* 0x001a2620 */
void nj_cnk_set_override_mode(int mode)
{
    nj_cnk_override_mode = mode;
}

/* Both chunk renderers. */
/* 0x001a2628 */
void nj_cnk_set_override_flags(int flags)
{
    nj_override_flags = flags;
    nj_cnk_override_flags = flags;
}

/* Both chunk renderers: flags = (flags & and_mask) | or_bits. */
/* 0x001a2638 */
void nj_cnk_set_override_strip_flags(int and_mask, int or_bits)
{
    nj_strip_and_mask = and_mask;
    nj_strip_or_bits = or_bits;
    nj_cnk_strip_and_mask = and_mask;
    nj_cnk_strip_or_bits = or_bits;
}

/* col[0..3] = a, r, g, b; nj_cnk_override_rgba keeps r, g, b, a. */
/* 0x001a2650 */
void nj_cnk_set_override_color(float *col)
{
    nj_cnk_override_color[0] = col[0];
    nj_cnk_override_color[1] = col[1];
    nj_cnk_override_color[2] = col[2];
    nj_cnk_override_color[3] = col[3];
    nj_cnk_override_rgba[3] = col[0];
    nj_cnk_override_rgba[0] = col[1];
    nj_cnk_override_rgba[1] = col[2];
    nj_cnk_override_rgba[2] = col[3];
}
