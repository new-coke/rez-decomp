/*
 * Break effects for stg04 objects: a stage model's shatter at half scale, with the type's own
 * break parameters or a fixed set.
 */

#include "types_common.h"
#include "color.h"

#include "types_overlays.h"

#include "debug.h"
#include "nj_system.h"
#include "player.h"
#include "en_break.h"
#include "enemy_shatter.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern void *stage_models;

extern const int en_break_types[];

/* 0x00afbac0 stg04, 0x00afa2a0 stg22 */
Shatter *en_break_spawn_plain(void *obj, Vec3 *pos, int *ang, int kind)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    v3_copy(&desc.pos, pos);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    desc.models = stage_models;
    desc.type = en_break_types[kind];
    i3_copy(desc.ang, ang);
    desc.kind = 0;
    v3_set(&desc.scale, 0.5f, 0.5f, 0.5f);
    return enemy_shatter_create(obj, &desc, 0, 0);
}

/* 0x00afbba8 stg04, 0x00afa388 stg22 */
Shatter *en_break_spawn_tuned(void *obj, Vec3 *pos, int *ang, int kind)
{
    SpawnDesc desc;
    ShatterParams prm;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    v3_copy(&desc.pos, pos);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    v3_set(&desc.scale, 0.5f, 0.5f, 0.5f);
    desc.models = stage_models;
    desc.type = en_break_types[kind];
    i3_copy(desc.ang, ang);
    desc.kind = 0;
    prm.Gravity = 10.0f;
    prm.PowVec = 0.9f;
    prm.PowAng = 6.7f;
    prm.RtoVecDec = 0.94f;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    prm.TimeAlphaEnd = 80.0f;
    prm.force_wire = 0;
    prm.pcol_wire = RGBA(0x34, 0x0f, 0, 0x80);
    prm.b_acc = 0;
    prm.frm_acc = 0.0f;
    return enemy_shatter_create(obj, &desc, (float *)&prm, 0);
}

#ifdef VERSION_US
/* kind 0 or 1; carried by the player's movement this frame */
/* 0x00afbd28 stg04, 0x00afa508 stg22 */
Shatter *en_break_spawn(void *obj, Vec3 *pos, int *ang, int kind)
{
    Vec3 wpos, org;

    player_get_frame_vel(&org);
    wpos.x = pos->x + org.x;
    wpos.y = pos->y + org.y;
    wpos.z = pos->z + org.z;
    switch (kind) {
    case 0:
        return en_break_spawn_plain(obj, &wpos, ang, 0);
    case 1:
        return en_break_spawn_tuned(obj, &wpos, ang, 1);
    }
    return 0;
}
#else
/* 0x00afbd28 stg04, 0x00afa508 stg22 */
Shatter *en_break_spawn(void *obj, Vec3 *pos, int *ang, int kind)
{
    switch (kind) {
    case 0:
        return en_break_spawn_plain(obj, pos, ang, 0);
    case 1:
        return en_break_spawn_tuned(obj, pos, ang, 1);
    }
    return 0;
}
#endif
