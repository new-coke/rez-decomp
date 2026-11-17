/* Tutorial 'Missile': fired by a 'Nul' launcher straight at its aim point; can be shot down. */

#include "types_gw.h"
#include "color.h"
#include "stg20.h"
#include "mathdef.h"
#include "debug.h"
#include "debugdraw.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_camera.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "missile.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

/* 300 bytes: lockable shot, two model parts */
typedef struct MissileWork {
    char pad_00[8];
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 vel;                   /* 0x20 */
    ModelPart body;                /* 0x2c */
    ModelPart glow;                /* 0x88: lit with the model's light */
    LockObj lock;                  /* 0xe4 */
    int fx;                     /* 0x124: effect slot */
    int fx_op;                  /* 0x128 */
} MissileWork;

extern void *missile_models;

extern void *missile_textures;

extern int missile_rot;
extern char missile_lock_name[];
extern char missile_pnb[];

extern char missile_ptm[];

extern GwVarDef missile_vardef[];
extern GwVarDef missile_vardefs[];
extern struct TaskDesc missile_desc;

/* shots fired */
extern int missile_count;

extern int missile_kind;

extern float missile_spd;

/* aim point distance from the player */
extern float missile_dst;

/* Aims missile_dst from the player, turned further around the player each shot. */
/* 0x00af7178 */
void missile_fire(NulWork *w, Vec3 *from)
{
    ShotArg sarg;
    Vec3 aim_pos;
    int ang[3];
    Vec3 aim_dir;
    float sc[4][2];
    float x, y, z;
    int aim_rot;
    Task *ptask;

    sarg.pos.x = from->x;
    sarg.pos.y = from->y;
    sarg.pos.z = from->z;
    sarg.kind = missile_kind;
    sarg.fx_arg = -1;
    sarg.unk_01 = 0;
    player_camera_get_angles(ang);
    aim_rot = missile_count * 6371;
    missile_count++;
    z = 0.0f;
    x = missile_dst;
    y = z;
    ROT_Z(aim_rot, sc[0], x, y);
    ROT_Z(ang[2], sc[1], x, y);
    ROT_X(ang[0], sc[2], y, z);
    ROT_Y(ang[1], sc[3], x, z);
    player_get_pos(&aim_pos);
    aim_pos.x += x;
    aim_pos.y += y;
    aim_pos.z += z;
    aim_dir.x = aim_pos.x - sarg.pos.x;
    aim_dir.y = aim_pos.y - sarg.pos.y;
    aim_dir.z = aim_pos.z - sarg.pos.z;
    vec3_normalize(&sarg.vel, &aim_dir);
    v3_scale(&sarg.vel, missile_spd);
    do {
        sarg.ang[1] = (int)RAD_TO_ANG(atan2f(-sarg.vel.x, -sarg.vel.z));
        sarg.ang[0] = (int)RAD_TO_ANG(atan2f(sarg.vel.y, sqrtf(sarg.vel.x * sarg.vel.x + sarg.vel.z * sarg.vel.z)));
    } while (0);
    sarg.ang[2] = 0;
    ptask = TASK_OF(w)->parent;
    missile_create(ptask ? (void *)(ptask + 1) : 0, &sarg);
}

/* 0x00af7410 */
void missile_shatter(MissileWork *w)
{
    SpawnDesc sdesc;

    nj_fill8(&sdesc, 0, sizeof sdesc);
    v3_copy(&sdesc.pos, &w->pos);
    v3_set(&sdesc.scale, 1.0f, 1.0f, 1.0f);
    sdesc.kind = 0;
    sdesc.type = 0;
    sdesc.models = missile_models;
    sdesc.lock = &w->lock;
    i3_copy(sdesc.ang, w->ang);
    v3_copy(&sdesc.v, &w->lock.imp.v);
    enemy_shatter_create(w, &sdesc, 0, 0);
}

/* steer(cast) */
/* 0x00af74e0 */
void missile_init(MissileWork *w, ShotArg *prm)
{
    void *models;
    int root;
    int gwire;

    v3_copy(&w->pos, &prm->pos);
    v3_copy(&w->vel, &prm->vel);
    i3_copy(w->ang, prm->ang);
    w->fx = enemy_status_ent_create(prm, w);
    enemy_status_event(w->fx, 2, &w->pos);
    models = missile_models;
    w->fx_op = 3;
    root = *((int *)pnb_get_entry(models, 0));
    dc_object_init(&w->body, root, 0, 0);
    root = *((int *)pnb_get_entry(models, 1));
    gwire = model_pnb_get_wire(models, 1);
    dc_object_init(&w->glow, root, 0, 0);
    dc_object_set_wire(&w->glow, gwire);
    lockon_obj_init(&w->lock, w, &w->pos, 0, missile_lock_name, w->fx, 1, 1, 8);
    lockon_entry(&w->lock);
}

/* 0x00af7658 */
void missile_release(MissileWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->glow);
    lockon_remove(&w->lock);
    enemy_status_event(w->fx, w->fx_op, &w->pos);
}

/* pos += vel */
static inline void v3_step(Vec3 *vel, Vec3 *pos)
{
    pos->x += vel->x;
    pos->y += vel->y;
    pos->z += vel->z;
}

/* 0x00af76a0 */
void missile_exec(MissileWork *w)
{
    int n = enemy_status_get_ticks();

    if (n > 0) {
        do {
            n--;
            v3_step(&w->vel, &w->pos);
            dc_object_advance_time(&w->body);
            dc_object_advance_time(&w->glow);
            w->ang[2] += missile_rot;
        } while (n);
    }
    if (!view_sphere_visible(&w->pos, w->body.radius))
        FINISH_IF(w);
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
        missile_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00af77d8 */
void missile_disp(MissileWork *w)
{
    Vec3 *pos = &w->pos;

    ulGxSetTextureList(missile_textures);
    ulMatrixPush();
    if (view_sphere_visible(pos, w->body.radius)) {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        missile_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->body);
        draw_set_color(((int)(beat_clock_get_phase() * 255.0f) + 1) >> 1 << 24 | 0x80);
        draw_set_blend(4, 5);
        dc_object_draw(&w->glow);
    }
    ulMatrixPop();
}

/* 0x00af78a8 */
void *missile_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &missile_desc, 300, prm);
}

/* Box at the placement. Original bug: the visibility test reads unset v, not prm->pos. */
/* 0x00af78d0 */
int missile_marker_draw(void **w, ShotArg *prm)
{
    float r = 10.0f;
    Vec3 v;
    Vec3 *pos;

    ulGxSetTextureList(w[1]);
    ulMatrixPush();
    if (view_sphere_visible(&v, r)) {
        pos = &prm->pos;
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        missile_rotate_yxz(prm->ang);
        debug_draw_box(COL_WHITE, r);
    }
    ulMatrixPop();
    return 0;
}

/* steer(cast) */
/* 0x00af7960 */
int missile_vars_init(int *prm)
{
    missile_kind = prm[0];
    missile_textures = ((void *(*)(void *, int))model_create_timed)(missile_ptm, prm[1]);
    missile_models = ((void *(*)(void *))model_pnb_register)(missile_pnb);
    missile_count = 0;
    gw_varedit_add_table(&gw_varedit_root,
                         gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), missile_vardef),
                         missile_vardefs);
    return 0;
}

/* 0x00af79f8 */
int missile_vars_release(void)
{
    model_list_remove(missile_textures);
    model_free_all_buffers_checked(missile_models);
    missile_textures = 0;
    missile_models = 0;
    enemy_vars_group_release();
    return 0;
}

/* 0x00af7a48 */
void missile_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
