/* Piece shot off Hanuman's shield: a wire shard that flies off, slows, spins and fades. */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "color.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_hanuman_shield_break.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

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

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);

extern void *stage_textures;
extern void *stage_models;

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

extern struct TaskDesc en_hanuman_shield_break_desc;

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define EN_HANUMAN_SHIELD_BREAK_MODEL(n) ((n) + 81)
#else
#define EN_HANUMAN_SHIELD_BREAK_MODEL(n) (n)
#endif

/* 0x00b07678 stg02, 0x00b019c0 stg22 */
void en_hanuman_shield_break_exec(EnHanumanShieldBreakWork *w)
{
    Vec3 step;
    float unit = 1.0f;
    float fps = FPS;

    w->t += unit;
    if (fps < w->t) {
        FINISH_IF(w);
        return;
    }
    player_get_frame_vel(&step);
    v3_add(&w->pos, &step);
    w->alpha = unit - w->t / fps;
    v3_scale(&w->vel, 0.94f);
    v3_add(&w->pos, &w->vel);
    i3_add(w->ang, w->spin);
    w->pos.y += (0.5f - w->t) * 0.0027777778f;
}

/* 0x00b07800 stg02, 0x00b01b48 stg22 */
void en_hanuman_shield_break_disp(EnHanumanShieldBreakWork *w)
{
    if (view_sphere_visible(&w->pos, 40.0f)) {
        draw_set_blend(4, 1);
        ulGxSetTextureList(stage_textures);
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, w->pos.x, w->pos.y, w->pos.z);
        } while (0);
        en_hanuman_shield_break_rotate_yxz(w->ang);
        draw_set_color(((int)(color_get_a(RGBA(0x34, 0x0f, 0, 0x80)) * w->alpha) + 1) >> 1 << 24 | 0xf34);
        dc_object_draw(&w->body);
        ulMatrixPop();
    }
}

/* 0x00b078c8 stg02, 0x00b01c10 stg22 */
/* steer(cast) */
void en_hanuman_shield_break_init(EnHanumanShieldBreakWork *w)
{
    Vec3 d;

    player_get_frame_vel(&d);
    v3_sub(&w->pos, &w->pos, &d);
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HANUMAN_SHIELD_BREAK_MODEL(37)), 0, 0);
    dc_object_set_wire(&w->body, model_pnb_get_wire(stage_models, EN_HANUMAN_SHIELD_BREAK_MODEL(37)));
    vec3_scale_to_length(6.666667f, &w->vel.x, &w->vel.x);
    w->spin[0] = DEG_TO_ANG((RAND_FLOAT_A() * 120.0f + 120.0f) * FRAME_SEC);
    w->spin[1] = DEG_TO_ANG((RAND_FLOAT_A() * 60.0f + 60.0f) * FRAME_SEC);
    w->spin[2] = DEG_TO_ANG((RAND_FLOAT_A() * 60.0f + 60.0f) * FRAME_SEC);
}

/* 0x00b07a70 stg02, 0x00b01db8 stg22 */
void en_hanuman_shield_break_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 0x30));
}

/* 0x00b07a78 stg02, 0x00b01dc0 stg22 */
EnHanumanShieldBreakWork *en_hanuman_shield_break_create(void *parent, Vec3 *pos, int *ang, Vec3 *v, int part)
{
    EnHanumanShieldBreakWork *w = task_create_sibling(parent, &en_hanuman_shield_break_desc, 152);

    v3_copy(&w->pos, pos);
    i3_copy(w->ang, ang);
    w->part = part;
    v3_copy(&w->vel, v);
    en_hanuman_shield_break_init(w);
    return w;
}

/* 0x00b07b38 stg02, 0x00b01e80 stg22 */
void en_hanuman_shield_break_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
