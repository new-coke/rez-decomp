/*
 * Ramses' ground rings: where Ramses meets the ground, rings spread out, one every 6 frames, each
 * growing over 60 frames as it fades in and out. Carried with the player.
 */

#include "types_common.h"
#include "mathdef.h"

#include "types_overlays.h"

#include "debug.h"
#include "color.h"
#include "enemy_status.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "ramses_circle.h"
#include "system.h"

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern struct TaskDesc ramses_circle_class;

/* 0x00b05378 stg01, 0x00b0c7d8 stg22 */
void ramses_circle_init(RingWork *w, RingArg *prm)
{
    Vec3 org;

    player_get_frame_vel(&org);
    v3_sub(&w->pos, prm->pos, &org);
    i3_copy(w->ang, prm->ang);
    w->count = 0;
}

/* 0x00b05408 stg01, 0x00b0c868 stg22 */
void ramses_circle_release(void)
{
}

/* 0x00b05410 stg01, 0x00b0c870 stg22 */
void ramses_circle_exec(RingWork *w)
{
    Vec3 org;

    w->count += enemy_status_get_ticks();
    if (w->count > 66) {
        w->count = 66;
        FINISH_IF(w);
    }
    player_get_frame_vel(&org);
    v3_add(&w->pos, &org);
}

/* 0x00b054a0 stg01, 0x00b0c900 stg22 */
void ramses_circle_disp(RingWork *w)
{
    float sc[2];
    Vec3 *p;
    unsigned int col;
    float rad, alpha;
    int i, n;

    for (i = 0, n = w->count; i != -1; i--) {
        if (n < 60.0f) {
            rad = n / 60.0f;
            ulSinCos((int)(rad * ANG_QUARTER), &sc[0], &sc[1]);
            ulSinCos((int)(sc[0] * ANG_TURN - ANG_QUARTER), &sc[0], &sc[1]);
            alpha = (sc[0] + 1.0f) * 0.5f * 255.0f;
            col = ((int)MIN(MAX_GE(alpha, 0.0f), 255.0f) + 1) >> 1 << 24 | 0xff;
            rad *= 30.0f;
            draw_set_blend(4, 5);
            ulMatrixPush();
            p = &w->pos;
            do {
                Vec3 *p_ = p;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            ramses_circle_rotate_yxz(w->ang);
            draw_ring_xz(rad, col, 16);
            ulMatrixPop();
        }
        n -= 6.0f;
        if (n < 0)
            break;
    }
}

/* 0x00b05680 stg01, 0x00b0cae0 stg22 */
void *ramses_circle_create(void *obj, RingArg *prm)
{
    return task_create_sibling_inherit_arg(obj, &ramses_circle_class, 36, prm);
}

/* 0x00b056a8 stg01, 0x00b0cb08 stg22 */
void ramses_circle_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
