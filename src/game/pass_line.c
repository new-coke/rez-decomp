/*
 * Pass line: a travel line (point list, straight line from the player, or a test curve) giving
 * position, direction and heading; drawn via the "PassLineView" debug menu item.
 */

#include "types_game.h"
#include "color.h"
#include "system.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_math.h"
#include "pass_line.h"
#include "camera.h"
#include "debug.h"
#include "debugmenu.h"
#include "game.h"
#include "lockon.h"
#include "math_util.h"
#include "player.h"

extern PassLineState pass_line_state;
extern char str_pass_line_view[];
extern int pass_line_task;
extern PassVec3 pass_line_dir;
extern void *scr_root;
extern struct TaskDesc pass_line_view_desc;

static void pass_line_menu_toggle(void);

/* No points, kind 0. */
/* 0x00146f10 */
static void pass_line_state_clear(void)
{
    pass_line_state.total = 0;
    pass_line_state.pts = 0;
    pass_line_state.kind = 0;
}

/* total points, sampled every step; adds the "PassLineView" menu item. */
/* 0x00146f28 */
static void pass_line_menu_init(float step, PassVec3 *pts, int total)
{
    PassLineState *state = &pass_line_state;

    state->pts = pts;
    state->total = total;
    state->step = step;
    menu_item_add(info_menu_name, str_pass_line_view, pass_line_menu_toggle, 0, 0);
}

/* 0x00146f60 */
static void pass_line_reset(void)
{
    PassLineState *state = &pass_line_state;

    state->total = 0;
    state->pts = 0;
    state->step = 0;
    if (pass_line_task)
        pass_line_menu_toggle();
    menu_item_remove(info_menu_name, str_pass_line_view);
}

/* 0x00146fb0 */
static void pass_line_set_dir(PassVec3 *src)
{
    pass_line_dir.x = src->x;
    pass_line_dir.y = src->y;
    pass_line_dir.z = src->z;
}

/* 0x00146fd8 */
static void pass_line_get_dir(PassVec3 *dst)
{
    dst->x = pass_line_dir.x;
    dst->y = pass_line_dir.y;
    dst->z = pass_line_dir.z;
}

/* See pass_line_eval. */
/* 0x00147000 */
static void pass_line_set_kind(int kind)
{
    pass_line_state.kind = kind;
}

/* 0x00147010 */
static int pass_line_get_kind(void)
{
    return pass_line_state.kind;
}

/*
 * time: fractional point index, wraps. Direction blends the two segments around it; ang: pitch,
 * yaw. Nothing written without pos.
 */
/* 0x00147020 */
static void pass_line_eval_points(float time, PassVec3 *pos, PassVec3 *dir, int *ang)
{
    int i = (int)time;
    PassLineState *state = &pass_line_state;
    PassVec3 *next;
    PassVec3 *cur;
    float frac;
    PassVec3 seg_dir;
    PassVec3 next_dir;

    while (i >= state->total)
        i -= state->total - 1;
    cur = state->pts + i;
    i = 1 + i;
    if (!(i < state->total))
        i = state->total - 1;
    next = state->pts + i;
    i = i + 1;
    if (!(i < state->total))
        i = state->total - 1;
    if (pos) {
        pos->x = cur->x;
        pos->y = cur->y;
        pos->z = cur->z;
        frac = time - floorf(time);
        pos->x = pos->x + (next->x - cur->x) * frac;
        pos->y = pos->y + (next->y - cur->y) * frac;
        pos->z = pos->z + (next->z - cur->z) * frac;
        seg_dir.x = next->x - cur->x;
        seg_dir.y = next->y - cur->y;
        seg_dir.z = next->z - cur->z;
        cur = i + state->pts;
        next_dir.x = cur->x - next->x;
        next_dir.y = cur->y - next->y;
        next_dir.z = cur->z - next->z;
        seg_dir.x = seg_dir.x + (next_dir.x - seg_dir.x) * frac;
        seg_dir.y = seg_dir.y + (next_dir.y - seg_dir.y) * frac;
        seg_dir.z = seg_dir.z + (next_dir.z - seg_dir.z) * frac;
        if (dir) {
            dir->x = seg_dir.x;
            dir->y = seg_dir.y;
            dir->z = seg_dir.z;
        }
        if (ang) {
            ang[1] = (int)RAD_TO_ANG(atan2f(-seg_dir.x, -seg_dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(seg_dir.y, sqrtf(seg_dir.x * seg_dir.x + seg_dir.z * seg_dir.z)));
        }
    }
}

/*
 * 10 units down -Z per unit time, swinging 120 sideways and 160 along (period 360), bobbing 40
 * around height 180 (period 360 / 1.3).
 */
/* 0x001472e0 */
static void pass_line_orbit_point(float time, float *out)
{
    float sc[2];
    float x = 0.0f;
    float y = 180.0f;
    float z = -(time * 10.0f);

    ulSinCos((int)DEG_TO_ANG(time), &sc[0], &sc[1]);
    x = sc[1] * 120.0f + x;
    z += sc[0] * 160.0f;
    ulSinCos((int)DEG_TO_ANG(time * 1.3f), &sc[0], &sc[1]);
    y = sc[0] * 40.0f + y;
    out[2] = z;
    out[0] = x;
    out[1] = y;
}

/* Direction toward the point one step on; outputs optional. */
/* 0x001473e8 */
static void pass_line_orbit_eval(float time, PassVec3 *pos, PassVec3 *dir, int *ang)
{
    PassVec3 cur;
    PassVec3 next;
    PassVec3 delta;
    PassLineState *state = &pass_line_state;

    pass_line_orbit_point(time, &cur);
    pass_line_orbit_point(time + state->step, &next);
    if (pos) {
        pos->x = cur.x;
        pos->y = cur.y;
        pos->z = cur.z;
    }
    delta.x = next.x - cur.x;
    delta.y = next.y - cur.y;
    delta.z = next.z - cur.z;
    if (dir) {
        dir->x = delta.x;
        dir->y = delta.y;
        dir->z = delta.z;
    }
    if (ang) {
        delta.x = delta.x * 10.0f;
        delta.y = delta.y * 10.0f;
        delta.z = delta.z * 10.0f;
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-delta.x, -delta.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(delta.y, sqrtf(delta.x * delta.x + delta.z * delta.z)));
        } while (0);
    }
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

static inline void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/*
 * Outputs optional. Kind 0: point list (zero if none); 1: straight along pass_line_state.dir from
 * the player, advanced by frames elapsed; 2: test curve.
 */
/* 0x00147580 */
static void pass_line_eval(int count, PassVec3 *pos, PassVec3 *dir, int *ang)
{
    float time = (float)count * pass_line_state.step;

    if (pass_line_state.kind == 1)
        goto kind1;
    if (pass_line_state.kind == 0)
        goto kind0;
    if (pass_line_state.kind == 2)
        goto kind2;
    return;
kind0:
    if (pass_line_state.pts == 0) {
        if (pos) {
            pos->x = 0;
            pos->y = 0;
            pos->z = 0;
        }
        if (dir) {
            dir->x = 0;
            dir->y = 0;
            dir->z = 0;
        }
        if (ang) {
            ang[0] = 0;
            ang[1] = 0;
            ang[2] = 0;
        }
    } else {
        pass_line_eval_points(time, pos, dir, ang);
    }
    return;
kind1:
    if (pos) {
        float elapsed;
        PassVec3 ofs;
        PassVec3 player_pos;

        elapsed = game_time_get();
        elapsed = elapsed - game_prev_time_get();
        vscale(&ofs.x, &pass_line_state.dir.x, (float)(int)elapsed);
        player_get_pos(&player_pos);
        vadd(&pos->x, &player_pos.x, &ofs.x);
    }
    if (dir)
        vcopy(&dir->x, &pass_line_state.dir.x);
    if (ang) {
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-pass_line_state.dir.x, -pass_line_state.dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(pass_line_state.dir.y,
                                        sqrtf(pass_line_state.dir.x * pass_line_state.dir.x +
                                              pass_line_state.dir.z * pass_line_state.dir.z)));
        } while (0);
    }
    return;
kind2:
    pass_line_orbit_eval(time, pos, dir, ang);
}

/* Grey, every 10 steps; skips samples past the far clip or off screen. */
/* 0x001477d8 */
static void pass_line_draw(void)
{
    PassVec3 cam;
    PassVec3 prev;
    PassVec3 cur;
    PassVec3 dir;
    int ang[3];
    float far_sq;
    int n;
    int i;

    if (pass_line_state.pts) {
        n = (int)((float)pass_line_state.total / pass_line_state.step);
        far_sq = camera_get_far();
        far_sq = far_sq * far_sq;
        camera_get_eye(&cam);
        i = 0;
        pass_line_eval(0, &prev, &dir, ang);
        if (n > 0) {
            do {
                i += 10;
                pass_line_eval(i, &cur, &dir, ang);
                if (vec3_distance_squared(&cam, &cur) <= far_sq) {
                    if (view_sphere_visible(&cur, 1.0f))
                        lockon_draw_line_3d(&prev, &cur, COL_WHITE, COL_WHITE);
                }
                prev = cur;
            } while (i < n);
        }
    }
}

/* 0x00147920 */
static void pass_line_task_disp(void)
{
    pass_line_draw();
}

/* 0x00147928 */
static void pass_line_menu_toggle(void)
{
    if (pass_line_task) {
        TASK_END(pass_line_task);
        pass_line_task = 0;
        menu_item_set_value(info_menu_name, str_pass_line_view, 0);
    } else {
        pass_line_task = task_create(scr_root, &pass_line_view_desc, 0);
        menu_item_set_value(info_menu_name, str_pass_line_view, 1);
    }
}
