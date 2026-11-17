/*
 * Fog and background colour, re-applied to the renderer on change. FogAnim fades both to a new
 * colour over n frames; FogMap thickens the fog around stage fog points.
 */

#include "types_game.h"
#include "mathdef.h"
#include "types_gw.h"
#include "task.h"
#include "fog.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "callback.h"
#include "camera.h"
#include "color.h"
#include "heap.h"
#include "render.h"
#include "system.h"

typedef struct FogMapEase {
    char pad_00[28];
    float start;
    float end;
    float amp;
    int type;
} FogMapEase;

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

typedef struct {
    float x, y, z;
} FmVec;

extern int *fog_work;
extern float fog_table[];
extern int debug_disp_level;
extern int fog_log_id0;
extern int fog_log_id1;
extern char str_fog_update_data[];

extern char str_fog_update_back_col[];

extern FogReq fog_req;
extern char gw_varedit_root[];
extern GwVarDef fog_vardef[];
extern GwVarDef fog_vardefs[];
extern struct TaskDesc fog_desc;
extern char str_fog_req[];

extern void *fog_enable;
extern int fog_back_col;
extern char str_fog_back_col_req[];
extern void *system1_work;
extern struct TaskDesc fog_anim_desc;
extern struct TaskDesc fog_map_desc;
extern char str_draw_frame[];

/* Applied on the next fog_exec. */
/* 0x00118be0 */
static void fog_set_dirty(void)
{
    fog_work[1] = 1;
}

/* near = far - 1 unless below far. */
/* 0x00118bf8 */
static void fog_clamp_near(void)
{
    if (!((*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).lo < (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).hi)) /* steer(cast) */
        (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).lo = (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).hi - 1.0f; /* steer(cast) */
    fog_set_dirty();
}

/* far = near + 1 unless beyond near. */
/* 0x00118c30 */
static void fog_clamp_far(void)
{
    if (!((*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).lo < (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).hi)) /* steer(cast) */
        (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).hi = (*(struct { int pad[2] ; float lo ; float hi ; } *)&fog_req).lo + 1.0f; /* steer(cast) */
    fog_set_dirty();
}

/* Applied on the next fog_exec. */
/* 0x00118c68 */
static void fog_set_back_dirty(void)
{
    fog_work[2] = 1;
}

/* Inverse of fog_table_scale: index 0..127, 127 past every step. */
/* 0x00118c80 */
static int fog_table_lookup(float depth, float far)
{
    int i;
    float scale;

    depth = -depth;
    far = -far;
    scale = far / fog_table[127];
    for (i = 0; i < 128; i++) {
        if (depth < fog_table[127 - i] * scale)
            return i;
    }
    return 127;
}

/* Index 0 is the far end; steps get exponentially finer towards 127, next to the camera. */
/* 0x00118ce0 */
static float fog_table_scale(int idx, float x)
{
    return fog_table[127 - idx] * (-x / fog_table[127]);
}

/* Mode 4 starts the fog 30% from near to far. Logs with debug flag 8. */
/* steer(cast) */
/* 0x00118d18 */
static void UpdateFogData(FogEnt *slot)
{
    float r;
    float g;
    float b;

    switch (slot->mode) {
    case 0:
        ulGxSetFogRange(slot->near, slot->far, 0.0f, 0.0f);
        break;
    case 4:
        ulGxSetFogRange((slot->far - slot->near) * 0.3f + slot->near, slot->far, 0.0f, 1.0f);
        break;
    default:
        ulGxSetFogRange(slot->near, slot->far, 0.0f, 1.0f);
        break;
    }
    r = (float)((int (*)(int))color_get_r)(slot->color) / 255.0f;
    g = (float)color_get_g(slot->color) / 255.0f;
    b = (float)color_get_b(slot->color) / 255.0f;
    ulGxSetFogColor(r, g, b);
    if (debug_flags_test(8)) {
        if (debug_disp_level == 0)
            DEBUG_PRINTF(str_fog_update_data, fog_log_id0, fog_log_id1, slot->mode, slot->near, slot->far, slot->color);
    }
}

/* Logs with debug flag 8. */
/* steer(cast) */
/* 0x00118ee0 */
static void UpdateBackCol(FogEnt *slot)
{
    int col = slot->back;
    int b = color_get_b(col);
    int r = ((int (*)(int))color_get_r)(col);
    int g = color_get_g(col);

    ulGxSetClearColor(0x80000000 | ((b & 0xff) << 16) | ((g & 0xff) << 8) | (r & 0xff));
    if (debug_flags_test(8)) {
        if (debug_disp_level == 0)
            DEBUG_PRINTF(str_fog_update_back_col, fog_log_id0, fog_log_id1, slot->back);
    }
}

/* Current slot and fog_req. */
/* 0x00118fb8 */
static int fog_request(int mode, int color, float near, float far)
{
    FogEnt *slot = &fog_slots[fog_cur];

    fog_req.col = mode;
    fog_req.color = color;
    fog_req.near = near;
    fog_req.far = far;
    slot->color = color;
    slot->near = near;
    slot->mode = mode;
    slot->far = far;
    return 0;
}

/* 0x00119008 */
static void fog_nop(void)
{
}

/* Listener channel 2: slot 0, display list 0. */
/* 0x00119010 */
static void fog_listener(void)
{
    int old = RenderSetCurrentDisplayList(0);

    UpdateFogData(fog_slots);
    UpdateBackCol(fog_slots);
    RenderSetCurrentDisplayList(old);
}

/* fog_table: 128 steps of 2^(i / 16) * (16 + i % 16) / 16 (piecewise-linear exponential). */
/* 0x00119060 */
static void fog_init(int *w)
{
    int i;
    float *t = fog_table;
    struct GwVarNode *group;

    for (i = 0; i < 128; i++)
        *t++ = safe_powf(2.0f, (float)(i >> 4)) * ((float)(i & 15) + 16.0f) * 0.0625f;
    w[0] = listener_add(2, fog_listener);
    fog_enable = 0;
    ulGxEnableFog(0);
    heap_bzero(fog_slots, 4288);
    w[1] = 0;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    w[2] = 0;
    (*(int *)&fog_work) = (int)w; /* steer(cast) */
    group = gw_varedit_add(gw_varedit_root, 0, fog_vardef);
    gw_varedit_add_table(gw_varedit_root, group, fog_vardefs);
}

/* 0x00119180 */
static void fog_release(int *w)
{
    fog_work = 0;
    ulGxEnableFog(0);
    listener_remove(2, w[0]);
    gw_varedit_remove(gw_varedit_root, 0, fog_vardef);
}

/* Re-issues what fog_set_dirty / fog_set_back_dirty marked; back colour is fog_req word 5. */
/* 0x001191d0 */
static void fog_exec(int *w)
{
    if (w[1]) {
        w[1] = 0;
        fog_set((*(struct { int mode ; int color ; float near ; float far ; int bg ; } *)&fog_req).color, (*(struct { int mode ; int color ; float near ; float far ; int bg ; } *)&fog_req).mode, (*(struct { int mode ; int color ; float near ; float far ; int bg ; } *)&fog_req).near, (*(struct { int mode ; int color ; float near ; float far ; int bg ; } *)&fog_req).far); /* steer(cast) */
    }
    if (w[2]) {
        w[2] = 0;
        BackColReq((*(struct { int mode ; int color ; float near ; float far ; int bg ; } *)&fog_req).bg); /* steer(cast) */
    }
}

/* 0x00119238 */
static void fog_disp(void)
{
}

/* 0x00119240 */
int fog_create(void *parent)
{
    return task_create(parent, &fog_desc, 12);
}

/* 0x00119268 */
void fog_set(int color, int mode, float near, float far)
{
    fog_request(mode, color, near, far);
    UpdateFogData(fog_slots);
}

/* 128-entry density table (fog map). */
/* 0x00119298 */
static void fog_req_table(int color, float near, float far, void *table)
{
    FogEnt *slot = &fog_slots[fog_cur];

    fog_req.color = color;
    fog_req.near = near;
    fog_req.far = far;
    slot->color = color;
    slot->near = near;
    slot->far = far;
    nj_copy8(slot->table, table, 512);
    slot->flags |= 1;
    if (debug_flags_test(8)) {
        if (debug_disp_level == 0)
            DEBUG_PRINTF(str_fog_req, fog_log_id0, fog_log_id1, slot->mode, slot->near, slot->far, slot->color);
    }
    UpdateFogData(fog_slots);
}

/* 0x00119398 */
static void *fog_get_enable(void)
{
    return fog_enable;
}

/* Returns the previous setting. */
/* 0x001193a8 */
void *fog_set_enable(void *enable)
{
    void *old = fog_enable;

    ulGxEnableFog(enable);
    fog_enable = enable;
    return old;
}

/* Range kept. */
/* steer(cast) */
/* 0x001193f0 */
void fog_req_color(int color)
{
    FogEnt *slot = &fog_slots[fog_cur];
    float r;
    float g;
    float b;

    fog_req.color = color;
    slot->color = color;
    slot->flags |= 1;
    if (debug_flags_test(8)) {
        if (debug_disp_level == 0)
            DEBUG_PRINTF(str_fog_req, fog_log_id0, fog_log_id1, slot->mode, slot->near, slot->far, slot->color);
    }
    r = (float)((int (*)(int))color_get_r)(slot->color) / 255.0f;
    g = (float)color_get_g(slot->color) / 255.0f;
    b = (float)color_get_b(slot->color) / 255.0f;
    ulGxSetFogColor(r, g, b);
}

/* Screen clear colour. */
/* 0x00119540 */
void BackColReq(int color)
{
    FogEnt *slot = &fog_slots[fog_cur];

    fog_back_col = color;
    slot->back = color;
    slot->flags |= 2;
    if (debug_flags_test(8)) {
        if (debug_disp_level == 0)
            DEBUG_PRINTF(str_fog_back_col_req, fog_log_id0, fog_log_id1, slot->back);
    }
    UpdateBackCol(fog_slots);
}

/* Returns the mode. */
/* 0x001195e0 */
int fog_get_current(int *color, int *back, float *near, float *far)
{
    FogSlot *slot = &fog_slots[fog_cur];

    int cur_col, cur_back;
    float cur_near, cur_far;

    do {
        cur_col = slot->color;
        cur_near = slot->near;
    } while (0);
    *color = cur_col;
    *near = cur_near;
    do {
        cur_back = slot->back;
        cur_far = slot->far;
    } while (0);
    *back = cur_back;
    *far = cur_far;
    return slot->mode;
}

/* 0x00119628 */
static int fog_get_mode(void)
{
    FogSlot *slot = &fog_slots[fog_cur];

    return slot->mode;
}

/* 0x00119650 */
int fog_get_color(void)
{
    FogSlot *slot = &fog_slots[fog_cur];

    return slot->color;
}

/* 0x00119678 */
int fog_get_back_color(void)
{
    FogSlot *slot = &fog_slots[fog_cur];

    return slot->back;
}

/* 0x001196a0 */
float fog_get_near(void)
{
    FogSlot *slot = &fog_slots[fog_cur];

    return slot->near;
}

/* 0x001196c8 */
float fog_get_far(void)
{
    FogSlot *slot = &fog_slots[fog_cur];

    return slot->far;
}

/* 0x001196f0 */
static void fog_get_table(void *dst)
{
    FogSlot *slot = &fog_slots[fog_cur];

    nj_copy8(dst, slot->table, 512);
}

/* 0x00119718 */
int fog_get_work(void)
{
    return fog_work;
}

/* Start colour from the request if its bit 0, else the current fog colour. */
/* steer(cast) */
/* 0x00119728 */
static void fog_anim_init(int *fade, int *req)
{
    int start;

    if (req[0] & 1)
        start = req[1];
    else
        start = fog_get_color();
    fade[0] = ((int (*)(int))color_get_r)(start);
    fade[1] = color_get_g(start);
    fade[2] = color_get_b(start);
    fade[3] = ((int (*)(int))color_get_r)(req[2]);
    fade[4] = color_get_g(req[2]);
    fade[5] = color_get_b(req[2]);
    fade[6] = req[3];
}

/* 0x001197d0 */
static void fog_anim_release(void)
{
}

/* 1/remaining of the way per frame, as fog and back colour; ends when the count runs out. */
/* 0x001197d8 */
static void fog_anim_exec(FogAnimWork *fade)
{
    float inv;
    int cur_r, cur_g, cur_b;
    int r, g, b;
    int col;

    if (fade->count <= 0) {
        if (fade != 0)
            TASK_OF(fade)->flags |= TASK_F_END;
        return;
    }
    cur_b = fade->cur[2];
    inv = 1.0f / (float)fade->count;
    cur_r = fade->cur[0];
    cur_g = fade->cur[1];
    b = (int)((float)cur_b + (float)(fade->dst[2] - cur_b) * inv);
    r = (int)((float)cur_r + (float)(fade->dst[0] - cur_r) * inv);
    g = (int)((float)cur_g + (float)(fade->dst[1] - cur_g) * inv);
    fade->cur[2] = b;
    col = 0x80000000 | (((b + 1) >> 1) & 0xff) << 16 | (((g + 1) >> 1) & 0xff) << 8 | (((r + 1) >> 1) & 0xff);
    fade->cur[0] = r;
    fade->cur[1] = g;
    fog_req_color(col);
    BackColReq(col);
    fade->count--;
}

/* Under system1_work. */
/* 0x00119920 */
static void fog_anim_create(int prm)
{
    task_create_arg(system1_work, &fog_anim_desc, 28, prm);
}

/* Becomes fog_map_cur; current fog colour, range and table as its base. */
/* 0x00119940 */
static void fog_map_init(FogMapObj *w)
{
    unsigned col[2];

    fog_map_cur = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    fog_get_current(&col[0], &col[1], &w->near, &w->far);
    w->a = col[0] >> 24;
    w->r = (col[0] >> 16) & 0xff;
    w->g = (col[0] >> 8) & 0xff;
    w->b = col[0] & 0xff;
    fog_get_table(w->table);
    fog_map_cur->pt = 0;
    fog_map_cur->npt = 0;
}

/* 0x001199d0 */
static void fog_map_release(void)
{
    fog_map_cur->pt = 0;
    fog_map_cur->npt = 0;
}

/*
 * pt->amp before start, then falls to end: type 0 linear, 1 circular, 2 exponential, 3 S-curve
 * (cube root); 0 for unknown types.
 */
/* 0x001199e8 */
static float fog_ease(FogMapEase *pt, float t)
{
    float dur;
    float u;
    float r;
    float a;
    float b;

    t = t - pt->start;
    dur = pt->end - pt->start;
    r = pt->amp;
    if (!(t < 0.0f)) {
        switch (pt->type) {
        case 0:
            r *= (dur - t) / dur;
            break;
        case 1:
            u = t / dur;
            r *= sqrtf(1.0f - safe_powf(u, 2.0f));
            break;
        case 2:
            {
                float a = expf(1.0f - t / dur);
                float b = expf(1.0f);

                r *= (a - 1.0f) / (b - 1.0f);
            }
            break;
        case 3:
            {
                float v = (t - dur * 0.5f) / dur;

                r = v + v;
            }
            if (r < 0.0f)
                r = -safe_powf(-r, 0.33333334f);
            else
                r = safe_powf(r, 0.33333334f);
            r = pt->amp * (1.0f - (r + 1.0f) * 0.5f);
            break;
        default:
            r = 0.0f;
            break;
        }
    }
    return r;
}

/*
 * Points ahead raise the table to their density (fog_ease); points reaching the camera tint the
 * colour by density there. Table made monotonic with depth; set only if any point contributed.
 */
/* 0x00119bd8 */
static void fog_map_exec(FogMapWork *w)
{
    FmVec eye;
    float tbl[128];
    float view_pos[3];
    float sr, sg, sb, sw;
    float far, dist, rad, density;
    FogMapPt *p;
    int i, j, end;
    float *t;
    int *col;
    int hit;

    sr = sg = sb = sw = 0.0f;
    far = w->far;
    p = w->pt;
    hit = 0;
    camera_get_eye(&eye);
    nj_copy8(tbl, fog_map_cur->table, 512);
    for (i = 0; i < fog_map_cur->npt; i++, p++) {
        rad = p->radius;
        ulMatrixApplyPoint(ulMatrixCur, p->pos, view_pos);
        dist = fabsf(view_pos[2]);
        if (view_pos[2] < 0.0f) {
            if (rad < dist)
                continue;
            j = fog_table_lookup(-(rad - dist), far);
            density = fog_ease(p, fabsf(dist + fog_table_scale(127, far)));
            sr += p->r * density;
            sg += p->g * density;
            sb += p->b * density;
            sw += density;
            for (; j < 128; j++)
                tbl[j] = MAX(fog_ease(p, fabsf(dist + fog_table_scale(j, far))), tbl[j]);
            hit = 1;
        } else {
            float far_edge, near_edge;

            if (-far + rad < dist)
                continue;
            far_edge = dist + rad;
            near_edge = dist - rad;
            if (-far < far_edge)
                far_edge = -far;
            if (near_edge < 0.0f)
                near_edge = 0.0f;
            j = fog_table_lookup(-far_edge, far);
            end = fog_table_lookup(-near_edge, far);
            if (j < 128 && end >= 128) {
                density = fog_ease(p, fabsf(dist + fog_table_scale(127, far)));
                sr += p->r * density;
                sg += p->g * density;
                sb += p->b * density;
                sw += density;
            }
            for (; j <= end; j++)
                tbl[j] = MAX(fog_ease(p, fabsf(dist + fog_table_scale(j, far))), tbl[j]);
            hit = 1;
        }
    }
    t = &tbl[127];
    for (i = 127; i >= 0; i--, t--)
        t[0] = MAX(t[0], t[1]);
    col = w->col;
    sw += 1.0f;
    sr = (sr + col[1]) / sw;
    sg = (sg + col[2]) / sw;
    sb = (sb + col[3]) / sw;
    if (hit)
        fog_req_table(((((int)sb + 1) >> 1) & 0xff) << 16 | ((((int)sg + 1) >> 1) & 0xff) << 8 |
                    ((((int)sr + 1) >> 1) & 0xff), w->near, far, tbl);
}

/* 0x0011a080 */
static void fog_map_disp(void)
{
}

/* 0x0011a088 */
static void *fog_map_create(void *parent)
{
    return task_create(parent, &fog_map_desc, 544);
}

/* Colour bytes in memory order b, g, r, a. NULL pointers skipped. */
/* 0x0011a0b0 */
static void fog_map_set(float *near, float *far, unsigned char *col, void *table)
{
    if (near != 0)
        fog_map_cur->near = *near;
    if (far != 0)
        fog_map_cur->far = *far;
    if (col != 0) {
        fog_map_cur->a = col[3];
        fog_map_cur->r = col[2];
        fog_map_cur->g = col[1];
        fog_map_cur->b = col[0];
    }
    if (table != 0)
        nj_copy8(fog_map_cur->table, table, 512);
}

/* 0x0011a128 */
static void fog_map_set_points(int pt, int npt)
{
    fog_map_cur->pt = pt;
    fog_map_cur->npt = npt;
}

/* 0x0011a140 */
static void DrawFrame(void)
{
    printf(str_draw_frame);
}
