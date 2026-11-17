/*
 * Shiva missile manager, Area 2: a slot per live missile; draws their glow and wire. A slot plays
 * its burst and fades when its missile is gone, and tells the missile through its callback.
 */

#include "stg02.h"
#include "debug.h"
#include "anim.h"
#include "fog.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enb_shiva_manager.h"

/* 44 bytes */
typedef struct EnbShivaManagerSlot {
    void *owner;                /* passed back to cb */
    void *cb;                   /* 0x04: void (*)(int owner, int *ev), ev = {event, slot} */
    Vec3 pos;                   /* 0x08 */
    int alpha;                  /* 0x14 */
    float mdl_scl;                   /* 0x18: scale of the model's node */
    float size;                 /* 0x1c: scale of the wire's node */
    float t;                    /* 0x20: animation time */
    float age;                  /* 0x24: frames */
    int flags;                  /* 0x28: 1 used, 2 animated, 4 fading, 8 follows the scroll */
} EnbShivaManagerSlot;

/* 5744 bytes */
typedef struct EnbShivaManagerWork {
    int mdl;                     /* pnb 64 */
    int *motion;                    /* 0x04: its motion (pnb 65; [1] = frames) */
    int wire;                   /* 0x08: pnb 66 */
    ModelPart part;                /* 0x0c */
    char pad_68[4];
    int count;                  /* 0x6c: the highest slot in use */
    EnbShivaManagerSlot slot[1];             /* 0x70 */
} EnbShivaManagerWork;

extern EnbShivaManagerWork *enb_shiva_manager_work;
extern struct TaskDesc enb_shiva_manager_desc;
extern char str_enb_shiva_manager_exists[];

/* 0x00b14540 */
int enb_shiva_manager_find_free(void)
{
    EnbShivaManagerSlot *slot = enb_shiva_manager_work->slot;
    int i;

    for (i = 0; i < 128; i++, slot++)
        if (!(slot->flags & 1))
            return i;
    return -1;
}

/* 0x00b14580 */
int enb_shiva_manager_find_used(int i)
{
    EnbShivaManagerSlot *slot = enb_shiva_manager_work->slot + i;

    do {
        if (slot->flags & 1)
            return i;
        i--;
        slot--;
    } while (i >= 0);
    return -1;
}

/* 0x00b145c8 */
void enb_shiva_manager_notify(int i, int code)
{
    EnbShivaManagerSlot *slot = enb_shiva_manager_work->slot + i;
    void (*fn)(int, int *);
    int ev[2];

    if (!(slot->flags & 8)) {
        fn = (void (*)(int, int *))slot->cb;
        if (fn) {
            int arg = (int)slot->owner;

            ev[0] = code;
            ev[1] = i;
            fn(arg, ev);
        }
    }
}

/* 0x00b14628 */
int enb_shiva_manager_add(int owner, Vec3 *pos, int cb)
{
    int i = enb_shiva_manager_find_free();
    EnbShivaManagerSlot *slot;
    EnbShivaManagerWork *mng;

    if (i != -1) {
        mng = enb_shiva_manager_work;
        slot = mng->slot + i;
        v3_copy(&slot->pos, pos);
        slot->flags = 1;
        slot->alpha = 254;
        slot->owner = (void *)owner;
        slot->cb = (void *)cb;
        slot->age = 1.0f;
        slot->t = 0;
        slot->mdl_scl = 0;
        if (mng->count < i)
            mng->count = i;
    }
    return i;
}

/* 0x00b146e8 */
void enb_shiva_manager_set_pos(int i, Vec3 *pos)
{
    v3_copy(&enb_shiva_manager_work->slot[i].pos, pos);
}

/* 0x00b14720 */
void enb_shiva_manager_burst(int i)
{
    (enb_shiva_manager_work->slot + i)->flags |= 2;
}

/* 0x00b14748 */
void enb_shiva_manager_remove(void *idx)
{
    EnbShivaManagerSlot *slot = enb_shiva_manager_work->slot + (int)idx;
    Vec3 pl_vel;

    if (slot->flags & 2) {
        player_get_frame_vel(&pl_vel);
        slot->flags |= 8;
        v3_sub(&slot->pos, &slot->pos, &pl_vel);
    } else
        slot->flags = 0;
}

/* 0x00b147d8 */
void enb_shiva_manager_exec(EnbShivaManagerWork *w)
{
    Vec3 pl_vel;
    Vec3 scl;
    EnbShivaManagerSlot *slot, *other;
    float frames, t;
    int n, i, j;

    n = w->count;
    frames = (unsigned int)(w->motion[1] - 1);
    player_get_frame_vel(&pl_vel);
    for (i = 0, slot = enb_shiva_manager_work->slot; i <= n; i++, slot++) {
        if (!(slot->flags & 1))
            continue;
        if (slot->flags & 2) {
            t = (slot->flags & 4 ? slot->age + 60.0f : slot->age) / 120.0f;
            t *= frames;
            slot->t = t;
            ulMatrixPush();
            matrix_unit_set(0);
            anim_start(w->motion, t);
            anim_apply_node((struct AnimNode *)w->mdl);
            yuki_matrix_get_scale(&scl);
            slot->mdl_scl = scl.x;
            matrix_unit_set(0);
            anim_start((void *)w->wire, t);
            anim_apply_node((unsigned int *)system_pnb_entry_get(0));
            yuki_matrix_get_scale(&scl);
            slot->size = scl.x;
            ulMatrixPop();
            if (slot->flags & 4) {
                slot->alpha = (1.0f - slot->age / 60.0f) * 255.0f;
                slot->age += 1.0f;
                if (60.0f < slot->age)
                    slot->flags = 0;
            } else {
                slot->age += 1.0f;
                if (60.0f < slot->age) {
                    slot->flags |= 4;
                    slot->age = 1.0f;
                    if (i == enb_shiva_manager_work->count)
                        enb_shiva_manager_work->count = enb_shiva_manager_find_used(i);
                }
            }
        }
        if (slot->flags & 8)
            v3_add(&slot->pos, &pl_vel);
    }
    for (i = 0, slot = enb_shiva_manager_work->slot; i <= n; i++, slot++) {
        if (!(slot->flags & 2))
            continue;
        for (j = 0, other = enb_shiva_manager_work->slot; j <= n; j++, other++) {
            if ((other->flags & 3) == 1) {
                t = slot->mdl_scl;
                t += other->mdl_scl;
                if (vec3_distance_squared(&slot->pos, &other->pos) < (t *= t, t *= 12.0f, t *= 12.0f)) {
                    enb_shiva_manager_burst(j);
                    enb_shiva_manager_notify(j, 0);
                }
            }
        }
    }
}

/* 0x00b14b30 */
void enb_shiva_manager_disp(EnbShivaManagerWork *w)
{
    EnbShivaManagerSlot *slot;
    void *fog;
    unsigned int alpha;
    int n, i;

    n = w->count;
    ulGxSetTextureList(stage_textures);
    draw_set_blend(4, 1);
    fog = fog_set_enable(0);
    for (i = 0, slot = enb_shiva_manager_work->slot; i <= n; i++, slot++) {
        if ((slot->flags & 3) == 3) {
            alpha = (slot->alpha + 1) >> 1 << 24;
            yuki_sprite2_draw3d(0, alpha | 0x808080, 6, &slot->pos, slot->size * 12.0f, slot->size * 12.0f);
            ulMatrixPush();
            do {
                Vec3 *p_ = &slot->pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            vu0_matrix_scale(ulMatrixCur, 12.0f, 12.0f, 12.0f);
            draw_set_color(alpha | 0x5b3300);
            model_object_draw(&w->part, slot->t);
            ulMatrixPop();
        }
    }
    fog_set_enable(fog);
}

/* steer(cast) */
/* 0x00b14c90 */
void enb_shiva_manager_init(EnbShivaManagerWork *w)
{
    if (debug_flags_test(16) && enb_shiva_manager_work) {
        ERROR_PRINTF(str_enb_shiva_manager_exists);
        FINISH_IF(w);
        return;
    }
    enb_shiva_manager_work = w;
    w->mdl = *((int *)pnb_get_entry(stage_models, 64));
    w->motion = (int *)*((int *)pnb_get_entry(stage_models, 65));
    w->wire = *((int *)pnb_get_entry(stage_models, 66));
    dc_object_init(&w->part, w->mdl, (struct DcClip *)w->motion, 0);
    dc_object_set_wire(&w->part, model_pnb_get_wire(stage_models, 64));
}

/* 0x00b14d88 */
void enb_shiva_manager_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 12));
    enb_shiva_manager_work = 0;
}

/* 0x00b14db0 */
void *enb_shiva_manager_create(void *parent)
{
    return task_create(parent, &enb_shiva_manager_desc, 5744);
}
