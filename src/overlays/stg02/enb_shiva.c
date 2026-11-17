/*
 * Shiva's missiles, Area 2: launched up from a hatch, fall, turn to the player and dive along a
 * curve trailing smoke. Shatter when shot, on contact, or out of view.
 */

#include "stg02.h"
#include "mathdef.h"
#include "anim.h"
#include "debug.h"
#include "enemy_status.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enb_shiva.h"
#include "enb_shiva_manager.h"
#include "../common/air_mng.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/s_cos.h"
#include "../common/s_sin.h"

extern int enb_shiva_init_arg;

typedef struct EnbShivaSet {
    char *name;
    char *label;                /* 0x04 */
    float gravity;                  /* 0x08 */
    float up_y1;                  /* 0x0c */
    float spd_rotate;                  /* 0x10 */
    float spd_attack;                  /* 0x14 */
    float ang_clip;                  /* 0x18: degrees */
    float collision_r;                  /* 0x1c */
} EnbShivaSet;

extern int enb_shiva_level;
extern EnbShivaSet enb_shiva_sets[];
extern struct TaskDesc enb_shiva_desc;
extern int enb_shiva_lock_mtx;
extern VarDef enb_shiva_vardef;
extern VarDef enb_shiva_vardefs[6];
extern char str_enb_shiva_pos[];
extern char str_enb_shiva_ang[];
extern char enb_shiva_name[];

/* enb_shiva_desc, 312 bytes */
typedef struct EnbShivaWork {
    void *models;
    void *textures;             /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 vel;                   /* 0x20 */
    int phase;                  /* 0x2c: index into enb_shiva_states */
    int next;                   /* 0x30: the phase for the next frame */
    int step;                   /* 0x34: 0 starts the phase */
    int next_step;              /* 0x38 */
    Vec3 prev;                  /* 0x3c: last frame's position */
    Vec3 ofs;                   /* 0x48: start - player, for the dive */
    char pad_54[12];
    int roll;                   /* 0x60 */
    ModelPart obj;              /* 0x64 */
    LockObj lock;               /* 0xc0 */
    Vec3 tail;                  /* 0x100: the lock-on point, also where the smoke comes from */
    Vec3 fire_scl;              /* 0x10c: backfire scale */
    int fx_op;                  /* 0x118 */
    int fx;                     /* 0x11c */
    char pad_120[8];
    float t;                    /* 0x128: frames into the phase */
    float t_end;                /* 0x12c */
    float fall_t;               /* 0x130 */
    int slot;                   /* 0x134: enb_shiva_manager slot */
} EnbShivaWork;

extern void (*enb_shiva_states[])(EnbShivaWork *w);

/* sign-extends 16 bits; evaluates a more than once */
#define ANG16(a) ((a) & 0x8000 ? (int)((a) | 0xffff0000) : (a) & 0xffff)

/* 0x00b12d60 */
void enb_shiva_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&enb_shiva_vardef);
    for (i = 0; i < 2; i++) {
        def = enb_shiva_vardefs;
        prm = (int *)&enb_shiva_sets[i].gravity;
        enb_shiva_vardef.name = enb_shiva_sets[i].name;
        enb_shiva_vardef.fmt = enb_shiva_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&enb_shiva_vardef);
        for (j = 0; j < 6; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)enb_shiva_vardefs);
    }
}

/* 0x00b12e78 */
void enb_shiva_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b12e80 */
int enb_shiva_vars_init(int *prm)
{
    enb_shiva_vars_register();
    enb_shiva_init_arg = *prm;
    return 0;
}

/* 0x00b12eb8 */
int enb_shiva_vars_release(void)
{
    enb_shiva_vars_unregister();
    return 0;
}

/* 0x00b12ed8 */
void enb_shiva_calc_lock_point(EnbShivaWork *w)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    enb_shiva_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&enb_shiva_lock_mtx);
    ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&w->tail);
    ulMatrixPop();
}

/* 0x00b12f60 */
void enb_shiva_shatter(EnbShivaWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.type = 38;
    desc.models = stage_models;
    desc.kind = 0;
    v3_copy(&desc.pos, &w->pos);
    i3_copy(desc.ang, w->ang);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00b13018 */
void enb_shiva_slot_cb(EnbShivaWork *w, int *ev)
{
    if (ev[0] == 0) {
        enb_shiva_manager_burst(w->slot);
        w->fx_op = 3;
        enb_shiva_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00b13070 */
void enb_shiva_hit_check(EnbShivaWork *w)
{
    LockObj *lock = &w->lock;

    if (lock->flags & LOCK_HIT && lock->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_status_score_popup(w->fx, &w->pos, 10, lockon_hits_max_shot_cnt(lock));
        w->fx_op = 3;
        enb_shiva_manager_burst(w->slot);
        enb_shiva_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00b13110 */
void enb_shiva_hit_player(EnbShivaWork *w)
{
    if (player_sphere_test(&w->pos, enb_shiva_sets[enb_shiva_level].collision_r) == 1) {
        player_status_add_damage(1.0f);
        w->fx_op = 5;
        enb_shiva_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00b13198 */
void enb_shiva_clip_check(EnbShivaWork *w)
{
    if (!yuki_camera_relative(&w->pos, (int)DEG_TO_ANG(enb_shiva_sets[enb_shiva_level].ang_clip),
                              w->obj.radius)) {
        enb_shiva_shatter(w);
        FINISH_IF(w);
    }
}

/* 0x00b13220 */
void enb_shiva_fall_init(EnbShivaWork *w)
{
    w->fall_t = 1.0f;
}

/* 0x00b13230 */
void enb_shiva_fall(EnbShivaWork *w)
{
    w->pos.y += enb_shiva_sets[enb_shiva_level].gravity * 0.00027777778f * (0.5f - w->fall_t);
    w->fall_t += 1.0f;
}

/* 0x00b13290 */
float enb_shiva_fall_spd(EnbShivaWork *w, float dt)
{
    return enb_shiva_sets[enb_shiva_level].gravity * 0.00027777778f * (w->fall_t + dt);
}

/* 0x00b132d0 */
void enb_shiva_state_rise(EnbShivaWork *w)
{
    int step = w->step;

    switch ((unsigned int)step) {
    case 0:
        v3_set(&w->prev, 0.0f, 0.0f, 0.0f);
        w->t = 1.0f;
        w->t_end = w->vel.y / (enb_shiva_sets[enb_shiva_level].gravity * 0.00027777778f);
        w->step = step + 1;
        w->next_step++;
    case 1:
        v3_add(&w->pos, &w->vel);
        enb_shiva_fall(w);
        w->t += 1.0f;
        if (w->t_end < w->t) {
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b13418 */
void enb_shiva_state_fall(EnbShivaWork *w)
{

    switch ((unsigned int)w->step) {
    case 0:
        w->t = 1.0f;
        w->t_end = 20.0f;
        w->step++;
        w->next_step++;
    case 1:
        v3_add(&w->pos, &w->vel);
        enb_shiva_fall(w);
        w->t += 1.0f;
        if (w->t_end < w->t) {
            do {
                w->next_step = 0;
                w->next = 2;
            } while (0);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b13520 */
void enb_shiva_state_turn(EnbShivaWork *w)
{

    if (w->step != 1) {
        BackFireLink lnk;

        if (w->step != 0)
            return;
        {
        w->t = 1.0f;
        w->t_end = 60.0f;
        nj_fill8(&lnk, 0, sizeof(lnk));
        lnk.pos = &w->tail;
        lnk.ang = w->ang;
        lnk.gfx = 0;
        lnk.obj_scale = &w->fire_scl;
        en_backfire_create(w, &lnk, 1);
        v3_set(&w->fire_scl, 3.0f, 3.0f, 3.0f);
        w->step++;
        w->next_step++;
        }
    }
    {
        Vec3 pl;
        int ang[3];
        Vec3 dir;
        float sc[2];
        float t = w->t;
        float end = w->t_end;
        float rem = end - t;
        float nfrm, scl;
        int da;

        w->vel.y += (enb_shiva_fall_spd(w, rem) - w->vel.y) / (nfrm = rem + 1.0f);
        v3_add(&w->pos, &w->vel);
        scl = 3.0f - 2.0f / nfrm;
        v3_set(&w->fire_scl, scl, scl, scl);
        player_get_pos(&pl);
        v3_sub(&dir, &pl, &w->pos);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        w->ang[0] = (int)(w->ang[0] + (float)ANG16(-w->ang[0]) / (rem + 1.0f));
        w->ang[1] = (int)(w->ang[1] + (float)ANG16(ang[1] - w->ang[1]) / (rem + 1.0f));
        ulSinCos((int)(t * ANG_QUARTER / end), &sc[0], &sc[1]);
        w->roll = (int)(w->roll + (int)DEG_TO_ANG(enb_shiva_sets[enb_shiva_level].spd_rotate * FRAME_SEC) * sc[0]);
        enb_shiva_fall(w);
        {
            ModelPart *gfx = &w->obj;

            gfx->frame = gfx->frames * t / end;
        }
        w->t += 1.0f;
        if (end < w->t) {
            do {
                w->next_step = 0;
                w->next = 3;
            } while (0);
        }
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b13958 */
void enb_shiva_state_attack(EnbShivaWork *w)
{
    Vec3 pl, pl2, v;
    float rate, inv;

    switch ((unsigned int)w->step) {
    case 0:
        player_get_pos(&pl);
        {
            Vec3 *pos = &w->pos;
            v3_sub(&w->ofs, pos, &pl);
        }
        w->t = 1.0f;
        w->t_end = (int)(vec3_distance_xz((float *)&pl, (float *)&w->pos)
                      / (enb_shiva_sets[enb_shiva_level].spd_attack * FRAME_SEC));
        w->step++;
        w->next_step++;
    case 1:
        rate = w->t / w->t_end;
        rate = sin(rate * 1.5707964f);
        v3_copy(&w->prev, &w->pos);
        player_get_pos(&pl2);
        inv = 1.0f - rate;
        v3_mul(&v, &w->ofs, inv);
        (&w->pos)->x = pl2.x + v.x;
        {
            double qy = pl2.y, wy = w->ofs.y;
            (&w->pos)->y = qy + wy * cos(rate * 1.5707964f);
        }
        (&w->pos)->z = pl2.z + v.z;
        v3_sub(&v, &w->pos, &w->prev);
        do {
            w->ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            w->ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        w->ang[2] = 0;
        {
            int spin = (int)DEG_TO_ANG(enb_shiva_sets[enb_shiva_level].spd_rotate * FRAME_SEC);

            w->t += 1.0f;
            w->roll += spin;
        }
        if (w->t_end < w->t)
            do { w->next_step = w->step + 1; } while (0);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        else
            break;
    case 2:
        player_status_add_damage(1.0f);
        w->fx_op = 5;
        enb_shiva_shatter(w);
        FINISH_IF(w);
        break;
    }
}

/* 0x00b13d40 */
void enb_shiva_exec(EnbShivaWork *w)
{
    Vec3 pl_vec;
    int n = enemy_status_get_ticks();

    enb_shiva_hit_check(w);
    player_get_vec(&pl_vec);
    v3_add(&w->pos, &pl_vec);
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (n > 0) {
        do {
            do {
                w->phase = w->next;
                w->step = w->next_step;
            } while (0);
            enb_shiva_hit_player(w);
            enb_shiva_clip_check(w);
            if (enb_shiva_states[w->phase])
                enb_shiva_states[w->phase](w);
        } while (--n);
    } else {
        enb_shiva_hit_player(w);
    }
    enb_shiva_manager_set_pos(w->slot, &w->pos);
    enb_shiva_calc_lock_point(w);
}

/* 0x00b13e58 */
void enb_shiva_disp(EnbShivaWork *w)
{
    Vec3 *p = &w->pos;
    Vec3 pos;
    int ang[3];

    if (view_sphere_visible(p, (&w->obj)->radius)) {
        ulGxSetTextureList(w->textures);
        do {
            ulMatrixPush();
        } while (0);
        vu0_matrix_translate(ulMatrixCur, w->pos.x, p->y, p->z);
        enb_shiva_rotate_yxz(w->ang);
        vu0_matrix_rotate_z(ulMatrixCur, w->roll);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw((struct MapPart *)&w->obj);
        ulMatrixPop();
    }
    if (debug_flags_test(16)) {
        debug_set_origin(10, 15);
        v3_copy(&pos, p);
        debug_printf(str_enb_shiva_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        i3_copy(ang, w->ang);
        debug_printf(str_enb_shiva_ang, (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 0x10000 * DEG_PER_ANG));
        debug_newline();
    }
}

/* 0x00b13ff0 */
void enb_shiva_init(EnbShivaWork *w, Vec3 *pos, int *ang)
{
    int *wang = w->ang;
    float one = 1.0f;
    Vec3 v;
    Vec3 pl;
    TrailArg trail;
    int *mdl, *mot;

    v3_copy(&w->pos, pos);
    i3_copy(wang, ang);
    pos = &w->pos;
    player_get_vec(&v);
    v3_sub(pos, pos, &v);
     v3_set(&v, 0.0f, 0.0f, one);
    ulMatrixPush();
    matrix_unit_set(0);
    enb_shiva_rotate_yxz(wang);
     ulMatrixApplyVector((float *)ulMatrixCur, (float *)&v, (float *)&v);
    do { w->ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
    w->ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z))); } while (0);
    w->ang[2] = 0;
    ulMatrixPop();
    w->fx = enemy_status_ent_alloc(enb_shiva_init_arg, -1, (int)w);
    enemy_status_event(w->fx, 2, pos);
    w->fx_op = 3;
    mdl = pnb_get_entry(w->models, 38);
    mot = pnb_get_entry(w->models, 39);
    dc_object_init((struct MapPart *)&w->obj, *mdl, *mot, 0);
    dc_object_rebuild((struct MapPart *)&w->obj);
    lockon_obj_init(&w->lock, w, pos, 0, enb_shiva_name, w->fx, 1, 1, 0);
    lockon_entry(&w->lock);
    w->slot = enb_shiva_manager_add((int)w, pos, (int)enb_shiva_slot_cb);
    enb_shiva_fall_init(w);
    player_get_pos(&pl);
    v3_set(&w->vel, 0.0f, 0.0f,
           sqrtf(enb_shiva_sets[enb_shiva_level].gravity * 0.00027777778f * 2.0f *
                 (pl.y - (&w->pos)->y + enb_shiva_sets[enb_shiva_level].up_y1)));
    ulMatrixPush();
    matrix_unit_set(0);
    enb_shiva_rotate_yxz(wang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    ulMatrixApplyVector((float *)ulMatrixCur, (float *)&w->vel, (float *)&w->vel);
    ulMatrixPop();
    nj_fill8(&trail, 0, sizeof(trail));
    trail.body = 0;
    trail.pos = &w->tail;
    trail.tail_seg = 1;
    trail.ang = wang;
    trail.frames = 60;
    trail.tail_tex = 5;
    trail.tail_mask = 0x1000000;
    trail.one = 1;
    trail.tail_width = one;
    trail.tail_skip = 10;
    air_mng_create(w, &trail);
}

/* 0x00b14390 */
void enb_shiva_release(EnbShivaWork *w)
{
    enemy_status_event(w->fx, w->fx_op, (float *)&w->pos);
    lockon_remove(&w->lock);
    dc_object_release((struct MapPart *)&w->obj);
    enb_shiva_manager_remove((void *)w->slot);
}

/* 0x00b143d8 */
void *enb_shiva_create(void *parent, int pos, int ang)
{
    void *w = task_create_sibling_inherit(parent, &enb_shiva_desc, 312);

    enb_shiva_init(w, (Vec3 *)pos, (int *)ang);
    return w;
}

/* steer(cast) */
/* 0x00b14438 */
void enb_shiva_nodes_init(void)
{
    int mdl = *((int *)pnb_get_entry(stage_models, 38));
    int *src = (int *)*((int *)pnb_get_entry(stage_models, 39));

    anim_start(src, (float)(unsigned int)src[1] - 1.0f);
    node_tree_draw_matching_at(mdl, 0x1000000, 1, (int)&enb_shiva_lock_mtx, 1);
    enb_shiva_level = game_type_get() == 3;
}

/* 0x00b144f0 */
void enb_shiva_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
