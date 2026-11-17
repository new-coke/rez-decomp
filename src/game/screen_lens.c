/*
 * Full-screen distortions from the capture texture: the screen saver (rippling water with random
 * drops and optional fish, refracting the frame), the sine screen wave, the unfinished lens test.
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "kernel_syscalls.h"
#include "nj_math.h"
#include "nj_system.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_texture.h"
#include "screen_lens.h"
#include "color.h"
#include "fade.h"
#include "fog.h"
#include "heap.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "pad.h"
#include "primitive.h"
#include "render.h"
#include "screen.h"
#include "task.h"

#define C15_MAX(a, b) ((a) >= (b) ? (a) : (b))
#define C15_MIN(a, b) ((a) < (b) ? (a) : (b))

/* ScrSaver work (40 bytes). */
typedef struct ScrSaverTask {
    int mode;       /* 0: propagate the ripple, else interpolate between the maps */
    int count;      /* frames since the last ripple */
    int timer;      /* countdown 1000..0 */
    float *prev;    /* 34x34 height maps (row stride 34) */
    float *cur;
    float *next;
    int col;
    int *child;
    int flag;
    int fish;
} ScrSaverTask;

extern float *screen_lens_work;
extern float screen_cfg[];
extern float screen_layout[2];
extern float gx_vertex_buffer[];
extern PadAnalog *pad_debug;
extern void *scr_root;
extern struct TaskDesc screen_lens_desc;
extern int *scr_saver_work;
extern int scr_saver_surface;
extern ScreenVtx scr_saver_border_quad[4];
extern char scr_saver_disp_desc[];
extern struct TaskDesc scr_saver_desc;
extern FishRec *fish_work;
extern char fish_desc[];
extern ScreenWaveWork *screen_wave_work;
extern int sys_state;
extern float screen_wave_strength;
extern int screen_wave_req_cnt;
extern struct TaskDesc screen_wave_desc;
extern ScrWvUvArg scr_wv_uv_arg;
extern struct TaskDesc scr_wv_uv_desc;

extern void *scr_saver_disp_work;
extern int scr_saver_flush_req;
extern int scr_saver_pixels[];
extern float scr_saver_height[];
extern char scr_saver_map0[];
extern char scr_saver_map1[];
extern char scr_saver_map2[];
extern char scr_saver_vtx[];
extern char scr_saver_dx[];
extern char scr_saver_dy[];
extern char str_scr_saver[];

static void *fish_create(void *parent);
static void fish_spawn(int x, int y);
static void *scr_wv_uv_create(float scl, float z, int cols, int rows, int vtx, int dx, int dy, int height);

/*
 * Builds the 21x16 warp grid (outside the lens radius pushed far out) but never draws it; only
 * prints "screen_lens.c : njCnkEasyDrawModel()".
 */
/* 0x001611c8 */
static void screen_lens_warp(void)
{
    float *w = screen_lens_work;
    float *vtx = gx_vertex_buffer;
    float rr, cx, cy, wd, ht, fx, fy, dx, dy, ox, oy, scale;
    int i;
    int j;

    if (w) {
        scale = screen_get_mosaic_scale();
        primitive_grid_set_uv(scale * 641.0f, scale * 961.0f, *(struct PrimitiveGrid **)screen_lens_work, 20, 15);
        rr = w[1];
        rr *= rr;
        fx = 0.0f;
        for (i = 0; i < 21; i++) {
            fy = 0.0f;
            dx = fx - screen_cfg[2];
            for (j = 15; j >= 0; j--) {
                dy = fy - screen_cfg[3];
                if (dx * dx + dy * dy > rr) {
                    oy = dy * 100.0f;
                    ox = dx * 100.0f;
                } else {
                    oy = 0.0f;
                    ox = 0.0f;
                }
                vtx[0] = fx + ox;
                vtx[2] = 0.01f;
                vtx[1] = fy + oy;
                vtx[3] = 1.0f;
                vtx += 4;
                fy += screen_layout[1] / 15.0f;
            }
            fx += screen_layout[0] / 20.0f;
        }
        printf("screen_lens.c : njCnkEasyDrawModel()\n");
    }
}

/* 20x15 grid; effect flag 8. */
/* 0x00161358 */
static void screen_lens_init(ScreenLensWork *w)
{
    screen_lens_work = w;
    w->grid = primitive_grid_create(20, 15);
    screen_effect_flags |= 8;
}

/* 0x001613a0 */
static void screen_lens_release(void **w)
{
    screen_effect_flags &= ~8;
    screen_lens_work = 0;
    primitive_grid_free(*w);
}

/* Lens radius from the debug pad's second analog axis. */
/* 0x001613c8 */
static void screen_lens_disp(float *w)
{
    w[1] = 320.0f - (float)pad_debug->analog1;
}

/* 0x001613f0 */
void screen_lens_toggle(void)
{
    if (screen_lens_work == 0)
        task_create(scr_root, &screen_lens_desc, 8);
    else
        TASK_OF(screen_lens_work)->flags |= TASK_F_END;
}

/* 32x32 water image, when a new one is ready. */
/* 0x00161428 */
static void scr_saver_flush_buffer(void)
{
    if (scr_saver_flush_req) {
        ulTextureLoadImage(scr_saver_surface, scr_saver_pixels);
        scr_saver_flush_req = 0;
    }
}

/* Runs while paused; display list 13. */
/* 0x00161468 */
static void scr_saver_disp_init(void *w)
{
    scr_saver_disp_work = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    task_set_dlist(w, 13);
}

/* Once the first image is ready: water texture over the screen, one cell past each edge. */
/* 0x00161488 */
static void scr_saver_disp_draw(void)
{
    float width;
    float height;
    float margin;
    float aspect;
    int prev_filt;

    if (scr_saver_work) {
        width = screen_layout[0];
        margin = width * 0.03125f;
        scr_saver_border_quad[2].y = scr_saver_border_quad[1].x = scr_saver_border_quad[0].y = scr_saver_border_quad[0].x = -margin;
        height = screen_layout[1];
        scr_saver_border_quad[1].y = height + margin;
        scr_saver_border_quad[3].y = height + margin;
        aspect = height / width;
        scr_saver_border_quad[3].x = scr_saver_border_quad[2].x = width + margin;
        scr_saver_border_quad[3].v = scr_saver_border_quad[1].v = aspect;
        if (scr_saver_work[8] == 0) {
            draw_set_blend(5, 4);
            prev_filt = draw_set_fc_mode(8192);
            ulGxSetTexture(scr_saver_surface);
            ulGxBeginTexStrip(1);
            ulGxAddTexStrip(scr_saver_border_quad, 4);
            ulGxEndTexStrip();
            do { draw_set_fc_mode(prev_filt); } while (0);
        }
    }
}

/* Under the fade task. */
/* 0x00161570 */
static void *scr_saver_disp_create(void)
{
    return task_create(fade_get_work(), scr_saver_disp_desc, 4);
}

/* 32x32 water texture and its "ScrSaverDisp" task. */
/* 0x001615a0 */
void scr_saver_setup(void)
{
    int req[5];
    void **pd = &scr_saver_disp_work;

    heap_bzero(req, 20);
    req[0] = 32;
    req[1] = 32;
    req[2] = 0;
    req[3] = 0;
    req[4] = (int)scr_saver_pixels;
    scr_saver_surface = ulTextureCreateSurface(req);
    ulTextureSetId(scr_saver_surface, 12288);
    ulTextureSetName(scr_saver_surface, str_scr_saver);
    ulTextureSetImage(scr_saver_surface, scr_saver_pixels);
    *pd = scr_saver_disp_create();
}

/* 0x00161640 */
static void scr_saver_end(void)
{
    ulTextureFree(scr_saver_surface);
    do {
        if (scr_saver_disp_work)
            TASK_OF(scr_saver_disp_work)->flags |= TASK_F_END;
    } while (0);
}

/*
 * Clears three height maps, random water colour; fish under fish_parent while keyboard key 44 is
 * held; "ScrWvUV" bends the screen with the 32x32 grid.
 */
/* 0x00161680 */
static void scr_saver_init(int *w, int fish_parent)
{
    int cr;
    int cg;
    int cb;
    int *task;
    unsigned fl;

    scr_saver_work = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    w[3] = (int)scr_saver_map0;
    w[4] = (int)scr_saver_map1;
    w[5] = (int)scr_saver_map2;
    heap_bzero(scr_saver_map0, 4624);
    heap_bzero((void *)w[4], 4624);
    heap_bzero((void *)w[5], 4624);
    w[0] = 0;
    w[1] = 0;
    w[2] = 1000;
    cr = rand_lcg_next_b();
    cg = rand_lcg_next_b();
    cb = rand_lcg_next_b();
    w[6] = ((((int)((float)cr / 32769.0f * 255.0f) + 1) >> 1 & 0xff) << 16)
         | (((int)((float)cg / 32769.0f * 255.0f) + 1) << 7 & 0xff00)
         | (((int)((float)cb / 32769.0f * 255.0f) + 1) >> 1 & 0xff);
    nj_fill8(scr_saver_pixels, 0, 4096);
    task_set_dlist(w, 13);
    w[9] = 0;
    if (pad_keyboard_key_down(44))
        w[9] = (int)fish_create((void *)fish_parent);
    heap_bzero(scr_saver_height, 4356);
    task = scr_wv_uv_create(0.1f, 1.0f, 32, 32, (int)scr_saver_vtx, (int)scr_saver_dx, (int)scr_saver_dy,
                            (int)scr_saver_height);
    fl = TASK_OF(task)->flags;
    w[7] = (int)task;
    TASK_OF(task)->flags = fl | 8;
    w[8] = 1;
}

/* 0x00161890 */
static void scr_saver_release(int *w)
{
    int *task = (int *)w[7];
    int *child;

    do {
        if (task)
            TASK_OF(task)->flags |= TASK_F_END;
    } while (0);
    child = (int *)w[9];
    if (child)
        TASK_OF(child)->flags |= TASK_F_END;
    scr_saver_work = 0;
}

/* Radius 4 cells, cosine profile up to 255, heights capped at 30720; cx, cy in 4..28. */
/* 0x001618d0 */
static void scr_saver_add_ripple(float *buf, int cx, int cy)
{
    int i;
    int j;
    float dist_sq;
    float dist;
    float height;
    float v;
    float lim;
    int row;

    if ((unsigned)(cx - 4) < 25 && (unsigned)(cy - 4) < 25) {
        for (j = -4; j < 5; j++) {
            row = (cy + j) * 34;
            for (i = -4; i < 5; i++) {
                dist_sq = (float)(i * i + j * j);
                if (dist_sq <= 16.0f) {
                    dist = sqrtf(dist_sq) * 0.25f;
                    v = njCos((int)(dist * ANG_HALF));
                    lim = 30720.0f;
                    height = (v + 1.0f) * 0.5f * 255.0f;
                    {
                        float *cell = &buf[cx + i + row];
                        *cell += height;
                        *cell = MIN(*cell, lim);
                    }
                }
            }
        }
    }
}

/*
 * 34x34 maps; every other frame: neighbours / 2 - previous, damped 0.8 -> 0.9 over the countdown,
 * maps rotated; between: shading halfway. Slope + colour -> 32x32 image; bump, fish every 60.
 */
/* steer(cast) */
/* 0x00161a98 */
static void scr_saver_exec(ScrSaverTask *w)
{
    int base;
    int i;
    int j;
    int slope;
    int cx;
    int cy;
    float damp;
    float height;
    float mix;
    float *src;
    float *dd;

    w->timer = w->timer - 1 > -1 ? w->timer - 1 : 0;
    base = w->timer * 127 / 1000 + 128;
    damp = (float)((double)(1000 - w->timer) * (0.9 - 0.8) / 1000.0 + 0.8);
    scr_saver_flush_req = 0;
    if (w->mode == 0) {
        int r, g, b;
        int sh;
        int *pix = scr_saver_pixels;
        float *prev = w->prev;
        float *cur = w->cur;
        float *next = w->next;

        for (j = 1; j < 33; j++) {
            for (i = 1; i < 33; i++) {
                height = (cur[j * 34 + i - 1] + cur[j * 34 + i + 1] + cur[j * 34 + i - 34] + cur[j * 34 + i + 34]) * 0.5f
                    - prev[j * 34 + i];
                next[j * 34 + i] = height * damp;
                slope = (int)((cur[j * 34 + i] - cur[j * 34 + i - 1]) * 10.0f + 128.0f);
                if (slope >= 256)
                    slope = 255;
                else
                    slope = slope > -1 ? slope : 0;
                r = ((int (*)(unsigned))color_get_r)(w->col);
                g = color_get_g(w->col);
                b = color_get_b(w->col);
                sh = slope - 128;
                sh = base - sh;
                sh = sh > -1 ? sh : 0;
                sh = sh < 255 ? sh : 254;
                *pix = (((sh + 1) >> 1) << 24) | (((b + 1) >> 1 & 0xff) << 16) | ((g + 1) << 7 & 0xff00) | ((r + 1) >> 1 & 0xff);
                pix++;
            }
        }
        {
            float *map_prev = w->prev;

            w->prev = w->cur;
            w->cur = w->next;
            w->next = map_prev;
        }
    } else {
        int r, g, b;
        int sh;
        int *dst = scr_saver_pixels;
        float *prev = w->prev;
        float *cur = w->cur;

        for (j = 1; j < 33; j++) {
            for (i = 1; i < 33; i++) {
                mix = (float)w->mode * 0.5f;
                height = prev[j * 34 + i] + mix * (cur[j * 34 + i] - prev[j * 34 + i]);
                mix = prev[j * 34 + i - 1] + mix * (cur[j * 34 + i - 1] - prev[j * 34 + i - 1]);
                slope = (int)((height - mix) * 10.0f + 128.0f);
                if (slope >= 256)
                    slope = 255;
                else
                    slope = slope > -1 ? slope : 0;
                r = ((int (*)(unsigned))color_get_r)(w->col);
                g = color_get_g(w->col);
                b = color_get_b(w->col);
                sh = slope - 128;
                sh = base - sh;
                sh = sh > -1 ? sh : 0;
                *dst++ = ((((sh = sh < 255 ? sh : 254) + 1) >> 1) << 24) | (((b + 1) >> 1 & 0xff) << 16) | ((g + 1) << 7 & 0xff00) | ((r + 1) >> 1 & 0xff);
            }
        }
    }
    w->mode = (w->mode + 1) % 2;
    src = w->cur;
    dd = scr_saver_height;
    for (j = 0; j < 33; j++) {
        for (i = 32; i >= 0; i--) {
            *dd = src[0] + src[1] + src[34] + src[35];
            src++;
            dd++;
        }
        src++;
    }
    FlushCache(0);
    scr_saver_flush_req = 1;
    scr_saver_flush_buffer();
    w->flag = 0;
    w->count = w->count + 1;
    if ((float)(w->count - 1) > 60.0f) {
        cx = (int)(RAND_FLOAT_B() * 25.0f + 4.0f);
        cy = (int)(RAND_FLOAT_B() * 25.0f + 4.0f);
        scr_saver_add_ripple(w->cur, cx, cy);
        w->count = 0;
        fish_spawn((int)((float)(cx - 1) * (screen_layout[0] * 0.03125f)), (int)((float)(cy - 1) * (screen_layout[0] * 0.03125f)));
    }
}

/* Fish, if any, under fish_parent. */
/* 0x00162088 */
void *scr_saver_create(int fish_parent)
{
    return task_create_arg(scr_root, &scr_saver_desc, 40, fish_parent);
}

/* No fish swimming; runs while paused; display list 13. */
/* 0x001620b8 */
static void fish_init(FishRec *w)
{
    int i;

    fish_work = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    for (i = 2; i >= 0; i--)
        w[i].active = 0;
    task_set_dlist(w, 13);
}

/* 0x00162100 */
static void fish_release(void)
{
    fish_work = 0;
}

/*
 * Fish glide and slow; every 50 frames a quarter turn left or right with a new push of 10. Wrap
 * at the edges; the tail beats faster with speed.
 */
/* 0x00162110 */
static void fish_exec(FishRec *fish)
{
    float sc[2];
    float t;
    int i;

    for (i = 0; i < 3; i++, fish++) {
        if (fish->active) {
            if (fish->timer-- > 0) {
                fish->speed = fish->speed * 0.9f;
            } else {
                fish->timer = 50;
                fish->speed = 10.0f;
                if (RAND_FLOAT_B() > 0.5f)
                    fish->ang -= ANG_90;
                else
                    fish->ang += ANG_90;
            }
            ulSinCos(fish->ang, &sc[0], &sc[1]);
            fish->x += fish->speed * sc[1];
            fish->y += fish->speed * sc[0];
            while (fish->x < 0.0f)
                fish->x = fish->x + screen_layout[0];
            while (fish->x >= screen_layout[0])
                fish->x = fish->x - screen_layout[0];
            while (fish->y < 0.0f)
                fish->y = fish->y + screen_layout[1];
            while (fish->y >= screen_layout[1])
                fish->y = fish->y - screen_layout[1];
            fish->tail_phase = (int)((float)fish->tail_phase + fish->speed * ANG_QUARTER / 10.0f);
        }
    }
}

/* Two quads (body, tail) of a 6-point outline 20 pixels long; tail swings with tail_phase. */
/* 0x00162398 */
static void fish_draw_one(int color, float x, float y, float size, int ang, int tail_phase)
{
    float shape_x[6] = {1.0f, 0.5f, 0.5f, 0.0f, -0.5f, -0.5f};
    float shape_y[6] = {0.0f, 0.3f, -0.3f, 0.0f, 0.4f, -0.3f};
    float vtx[16];
    float sn;
    float cs;
    float iz;
    int i;

    ulSinCos(tail_phase, &sn, &cs);
    ulSinCos((int)(sn * 8192.0f), &sn, &cs);
    cs = fabsf(cs);
    for (i = 3; i < 6; i++) {
        float t;
        float u;

        t = shape_x[i] * cs - shape_y[i] * sn;
        u = shape_x[i] * sn + shape_y[i] * cs;
        shape_x[i] = t;
        shape_y[i] = u;
    }
    ulSinCos(ang, &sn, &cs);
    for (i = 0; i < 6; i++) {
        float t;
        float u;

        t = (shape_x[i] * cs - shape_y[i] * sn) * 20.0f;
        u = (shape_x[i] * sn + shape_y[i] * cs) * 20.0f;
        shape_x[i] = t;
        shape_y[i] = u;
    }
    ulGxBeginStrip(1);
    iz = 1.0f / size;
    vtx[14] = iz;
    vtx[10] = iz;
    vtx[6] = iz;
    vtx[2] = iz;
    vtx[0] = x + shape_x[0];
    vtx[1] = y + shape_y[0];
    vtx[4] = x + shape_x[1];
    vtx[5] = y + shape_y[1];
    vtx[8] = x + shape_x[2];
    vtx[9] = y + shape_y[2];
    vtx[12] = x + shape_x[3];
    vtx[13] = y + shape_y[3];
    ((int *)vtx)[15] = color;
    ((int *)vtx)[11] = color;
    ((int *)vtx)[7] = color;
    ((int *)vtx)[3] = color;
    ulGxAddStrip(vtx, 4);
    vtx[4] = x + shape_x[4];
    vtx[5] = y + shape_y[4];
    vtx[8] = x + shape_x[5];
    vtx[9] = y + shape_y[5];
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* Black silhouettes. */
/* 0x00162678 */
static void fish_disp(FishRec *w)
{
    int i;

    for (i = 2; i >= 0; i--) {
        if (w->active)
            fish_draw_one(0x80000000, w->x, w->y, 0.35f, w->ang, w->tail_phase);
        w++;
    }
}

/* Room for three fish. */
/* 0x001626e0 */
static void *fish_create(void *parent)
{
    return task_create(parent, fish_desc, 96);
}

/* Speed 10, random heading, if one of the three is free. */
/* 0x00162708 */
static void fish_spawn(int x, int y)
{
    FishRec *fish;
    int idx;
    int i;

    if (fish_work) {
        fish = fish_work;
        idx = -1;
        for (i = 0; i < 3; i++) {
            if (fish->active == 0) {
                idx = i;
                break;
            }
            fish++;
        }
        if (idx != -1) {
            fish = &fish_work[idx];
            fish->active = 1;
            fish->unk_04 = 0;
            fish->x = (float)x;
            fish->y = (float)y;
            fish->speed = 10.0f;
            fish->ang = (int)(RAND_FLOAT_B() * 360.0f * (ANG_TURN / 360.0f));
            fish->tail_phase = 0;
            fish->timer = 0;
        }
    }
}

/*
 * Captured frame as 20 strip columns over a 21x16 grid, depth from the height table; no fog or
 * depth writes; display list 12.
 */
/* 0x00162808 */
static void screen_wave_draw(void)
{
    ScreenVtx vtx[32];
    ScreenVtx *vert;
    float *row;
    float *next_row;
    ScreenWaveWork *w = screen_wave_work;
    int *obj;
    int prev_dlist;
    int prev_fog;
    int i;
    int j;
    float left;
    float right;
    float top;
    float y;
    float t;
    float dy;
    float *height;
    float cols;

    if (w) {
        prev_dlist = RenderSetCurrentDisplayList(12);
        psGsSetAlphaTest(0, 5, 128, 1);
        psGsSetDepthTest(1, 1);
        psGsSetZMask(1);
        obj = screen_get_tex_list();
        ulGxSetTextureList(obj);
        ulGxSelectTexture(ulGxGetTextureList(), 0);
        psGsWriteReg(6, *(unsigned long *)(*(int *)(*obj + 8)) & ~((unsigned long)1 << 34));
        prev_fog = fog_set_enable(0);
        vert = vtx;
        for (i = 0; i < 32; i++) {
            vert->c = COL_WHITE;
            vert++;
        }
        if (sys_state == 0) {
            top = 15.5f;
            dy = screen_layout[1] / 15.0f;
        } else {
            top = 0.5f;
            dy = 512.0f / 15.0f;
        }
        left = 0.0f;
        right = 0.0f;
        cols = 20.0f;
        for (i = 0; i < 21; i++) {
            ulGxBeginTexStrip(0);
            right += screen_layout[0] / cols;
            row = w->tab + i * 16;
            next_row = w->tab + (i * 16 + 16);
            y = top;
            t = 0.0f;
            vert = vtx;
            for (j = 0; j < 16; j++) {
                vert[j * 2].x = left;
                vert[j * 2].y = y;
                height = row + j;
                vert[j * 2].z = *height + 0.01f;
                vert[j * 2].u = left * (1.0f / 1024.0f);
                vert[j * 2].v = t * (1.0f / 512.0f);
                vert[j * 2 + 1].x = right;
                vert[j * 2 + 1].y = y;
                height = next_row + j;
                vert[j * 2 + 1].z = *height + 0.01f;
                vert[j * 2 + 1].u = right * (1.0f / 1024.0f);
                vert[j * 2 + 1].v = t * (1.0f / 512.0f);
                t += dy;
                y += screen_layout[1] / 15.0f;
            }
            ulGxAddTexStrip(vtx, 32);
            ulGxEndTexStrip();
            left = right;
            vert = vtx;
        }
        fog_set_enable(prev_fog);
        psGsSetZMask(0);
        psGsSetDepthTest(1, 2);
        psGsSetAlphaTest(1, 5, 128, 1);
        RenderSetCurrentDisplayList(prev_dlist);
    }
}

/* 20x15 grid; default strength, wavelengths, speeds. */
/* 0x00162b18 */
static void screen_wave_init(ScreenWaveWork *w)
{
    screen_wave_work = w;
    w->grid = primitive_grid_create(20, 15);
    w->strength = 0.25f;
    w->step_x = 12743;
    w->step_y = 17294;
    w->speed_x = 3640;
    w->phase_x = 0;
    w->phase_y = 0;
    w->speed_y = 2366;
}

/* Clears effect flag 4. */
/* 0x00162b88 */
static void screen_wave_release(void **w)
{
    screen_effect_flags &= ~4;
    screen_wave_work = 0;
    primitive_grid_free(*w);
}

/* Effect flag 4 while strength > 0; advances both phases; heights = sine along x + sine along y. */
/* 0x00162bb0 */
static void screen_wave_exec(ScreenWaveWork *w)
{
    float sc[4];
    float scale;
    float *tab;
    int i;
    int j;
    int ax;
    int ay;
    int x;
    int y;

    w->strength = screen_wave_strength;
    if (screen_wave_strength > 0.0f)
        screen_effect_flags |= 4;
    else
        screen_effect_flags &= ~4;
    if (screen_effect_flags & 4) {
        tab = w->tab;
        i = 0;
        scale = w->strength * 0.25f * 0.04f;
        x = (w->phase_x + w->speed_x) & 0xffff;
        y = (w->phase_y + w->speed_y) & 0xffff;
        w->phase_y = y;
        w->phase_x = x;
        ax = x;
        do {
            ulSinCos(ax, &sc[0], &sc[1]);
            ay = w->phase_y;
            j = 15;
            do {
                ulSinCos(ay, &sc[2], &sc[3]);
                j--;
                ay += w->step_y;
                *tab++ = (sc[0] + sc[2] + 2.0f) * scale;
            } while (j >= 0);
            ax += w->step_x;
            i++;
        } while (i < 21);
    }
}

/* 0x00162d40 */
static void screen_wave_disp(void)
{
}

/* The first request creates the task. */
/* 0x00162d48 */
void screen_wave_start(void)
{
    screen_wave_req_cnt = screen_wave_req_cnt + 1;
    if (screen_wave_req_cnt == 1 && screen_wave_work == 0)
        task_create(scr_root, &screen_wave_desc, 1376);
}

/* Ends the task even while other requests remain (the count only matters when starting). */
/* 0x00162d90 */
void screen_wave_stop(void)
{
    if (screen_wave_req_cnt > 0) {
        int *task;

        screen_wave_req_cnt = screen_wave_req_cnt - 1;
        task = screen_wave_work;
        do {
            if (task)
                TASK_OF(task)->flags |= TASK_F_END;
        } while (0);
    }
}

/* Clamped 0..1. */
/* 0x00162dc8 */
void screen_wave_set_strength(float x)
{
    screen_wave_strength = ((x >= 0.0f ? x : 0.0f) < 1.0f) ? (x >= 0.0f ? x : 0.0f) : 1.0f;
}

/* 0x00162e18 */
static float screen_wave_get_strength(void)
{
    return screen_wave_strength;
}

/*
 * Takes prm's grid buffers, renders through the capture texture (old setting kept), rows strips
 * of cols + 1 vertex pairs; display list 12.
 */
/* 0x00162e28 */
static void scr_wv_uv_init(ScrWvUvWork *w, ScrWvUvArg *prm)
{
    ScreenVtx *vtx;
    int cols;
    int rows;
    int i;
    int j;
    float sx;
    float sy;
    float x;
    float y;

    vtx = prm->vtx;
    w->slope_x = prm->slope_x;
    cols = prm->cols;
    w->cols = cols;
    w->slope_y = prm->slope_y;
    rows = prm->rows;
    w->rows = rows;
    w->vtx = vtx;
    w->height = prm->height;
    w->slope_scale = prm->slope_scale / 1000.0f;
    w->saved = screen_swap_render(1);
    sx = screen_layout[0] / (float)cols;
    sy = screen_layout[1] / (float)rows;
    y = 0.0f;
    for (j = 0; j < rows; j++) {
        x = 0.0f;
        for (i = 0; i < cols + 1; i++) {
            vtx->x = x;
            vtx->y = y;
            vtx->c = COL_WHITE;
            vtx->z = prm->z;
            vtx++;
            vtx->x = x;
            vtx->y = y + sy;
            x += sx;
            vtx->c = COL_WHITE;
            vtx->z = prm->z;
            vtx++;
        }
        y += sy;
    }
    task_set_dlist(w, 12);
}

/* 0x00162f98 */
static void scr_wv_uv_release(int *w)
{
    screen_swap_render(w[7]);
}

/*
 * Shifts each vertex's UV by the grid's x/y slopes (water refraction), kept inside the frame's
 * part of the capture texture (height by screen mode).
 */
/* 0x00162fa0 */
static void scr_wv_uv_exec(ScrWvUvWork *w)
{
    int cols = w->cols;
    int stride = cols + 1;
    int rows = w->rows;
    float *src = w->height;
    float *p;
    float *dv;
    float *du;
    ScreenVtx *vtx;
    float us;
    float vs;
    float voff;
    float vmax;
    float u;
    int i;
    int stride2;
    float *t;
    float *dst;
    int j;

    heap_bzero(w->slope_y, stride * 4);
    stride2 = stride * 2;
    heap_bzero((float *)w->slope_y + stride * rows, stride * 4);
    p = (float *)w->slope_y + cols + 1;
    for (j = 1; j < rows; j++) {
        for (i = 0; i < cols + 1; i++) {
            *p = (src[stride2] - src[0]) * w->slope_scale;
            p++;
            src++;
        }
    }
    dst = w->slope_x;
    t = w->height;
    for (j = 0; j < rows + 1; j++) {
        *dst++ = 0.0f;
        for (i = 1; i < cols; i++) {
            *dst = (t[2] - t[0]) * w->slope_scale;
            dst++;
            t++;
        }
        *dst++ = 0.0f;
        t += 2;
    }
    du = w->slope_x;
    dv = w->slope_y;
    vtx = w->vtx;
    us = 0.625f / (float)cols;
    if (sys_state == 0) {
        vs = 0.9375f / (float)rows;
        voff = -0.03125f;
        vmax = 0.873046875f;
    } else {
        vs = 1.0f / (float)rows;
        voff = 0.0f;
        vmax = 1.001953125f;
    }
    for (j = 0; j < rows; j++) {
        u = 0.0f;
        for (i = 0; i < cols + 1; i++) {
            if (i < cols)
                vtx->u = C15_MIN(C15_MAX(u + du[0], 0.0f), 0.62451171875f);
            else
                vtx->u = 0.62451171875f;
            vtx->v = C15_MIN(C15_MAX(voff + dv[0], 0.0f), vmax);
            vtx++;
            if (i < cols)
                vtx->u = C15_MIN(C15_MAX(u + du[cols + 1], 0.0f), 0.62451171875f);
            else
                vtx->u = 0.62451171875f;
            if (j < rows - 1)
                vtx->v = C15_MIN(C15_MAX(voff + vs + dv[cols + 1], 0.0f), vmax);
            else
                vtx->v = vmax;
            vtx++;
            u += us;
            du++;
            dv++;
        }
        voff += vs;
    }
}

/* Once capturing: the frame through the bent grid, filtered, no fog. */
/* 0x00163410 */
static void scr_wv_uv_disp(int *w)
{
    int vtx;
    int nv;
    int i;
    int *obj;
    int prev_filt;
    int prev_blend;
    int prev_fog;

    if (screen_get_pass_cnt() >= 2) {
        i = 0;
        vtx = w[0];
        nv = (w[3] + 1) * 2;
        obj = screen_get_tex_list();
        ulGxSetTextureList(obj);
        ulGxSelectTexture(ulGxGetTextureList(), 0);
        psGsWriteReg(6, *(unsigned long *)(*(int *)(*obj + 8)) & ~((unsigned long)1 << 34));
        prev_fog = fog_set_enable(0);
        prev_blend = draw_set_test_mode(0x18000);
        prev_filt = draw_set_fc_mode(8192);
        psGsSetAlphaTest(1, 5, 128, 1);
        ulGxBeginTexGouraudGif(0);
        for (i = 0; i < w[4]; i++) {
            ulGxAddTexGouraudGif(vtx, nv);
            vtx += nv * 24;
        }
        ulGxEndTexGouraudGif();
        draw_set_fc_mode(prev_filt);
        draw_set_test_mode(prev_blend);
        fog_set_enable(prev_fog);
    }
}

/* Under the fog task: vertex, slope, height buffers, slope scale scl, depth z. */
/* 0x00163598 */
static void *scr_wv_uv_create(float scl, float z, int cols, int rows, int vtx, int dx, int dy, int height)
{
    scr_wv_uv_arg.cols = cols;
    scr_wv_uv_arg.rows = rows;
    scr_wv_uv_arg.height = (void *)height;
    scr_wv_uv_arg.slope_scale = scl;
    scr_wv_uv_arg.z = z;
    scr_wv_uv_arg.vtx = (ScreenVtx *)vtx;
    scr_wv_uv_arg.slope_x = (void *)dx;
    scr_wv_uv_arg.slope_y = (void *)dy;
    return task_create_arg(fog_get_work(), &scr_wv_uv_desc, 32, &scr_wv_uv_arg);
}
