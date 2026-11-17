/*
 * Mars boss walls: the manager sends waves of walls from a per-level pattern table. Each wall
 * carries lock-on films stretched to points of its model; films break when shot or out of time.
 */

#include "types_gw.h"
#include "b_mars.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "lockon.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "player.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars_wall.h"
#include "mars.h"
#include "mars_core.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

extern char mars_wall_vardef[];
extern GwVarDef mars_wall_vardefs[];
extern struct GwVarNode *mars_wall_vars_group;
extern struct TaskDesc mars_w2_desc;
extern float mars_w2_break_sec;
extern int CMD_FILM_LIFE;
extern const float mars_w2_scl;
extern struct TaskDesc mars_wall_man_desc;
extern int mars_wall_man_debug;
extern int *mars_wall_man_levels[];
extern float cmWallLevelParam[][3];
extern float mars_w2_shake_decay;
extern float mars_w2_shake_min;
extern float mars_w2_shake_rate;
extern float CMD_CAMERA_FS;
extern float CMD_CAMERA_FR;
extern unsigned int cmWCol;
extern float mars_w2_fade_sec;
extern float mars_w2_hold_sec;
extern unsigned int cmWallWireCol[2];
extern float FILM_LEN;
extern float mars_wall_man_wall_sec;

/* spawn arguments built by mars_wall_man_exec */
typedef struct MarsSpawnArg {
    int id;                     /* 0x00 */
    Vec3 *pos;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *dir;                  /* 0x0c */
    struct MarsWallManWork *man;
    int n;                      /* 0x14 */
    int unk_18;                 /* 0x18: copied into the wall work by mars_w2_init, never read */
    int roll;                 /* 0x1c */
    int roll_speed;                 /* 0x20 */
    float speed;               /* 0x24 */
} MarsSpawnArg;

typedef struct MarsFilmTarget {
    char pad_00[4];
    int sub;                    /* 0x04: its sub-object, which points at the model point */
    int clip;
    char pad_0c[52];
    float film_time;            /* 0x40: frames the film holds before it breaks */
} MarsFilmTarget;

typedef struct MarsFilmRef {
    int obj;                    /* MarsFilmTarget * */
    int n;
} MarsFilmRef;

typedef struct MarsFilmSet {
    char pad_00[36];
    MarsFilmTarget *obj;        /* 0x24 */
    int n;                      /* 0x28 */
} MarsFilmSet;

/* wall film, 148 bytes */
typedef struct MarsFilm {
    LockObj lock;
    Vec3 pos;
    int vtx;                            /* 0x4c: float * to the model point followed */
    Vec3 lock_pos;
    char pad_5c[16];
    float fade;                         /* 0x6c */
    int state;
    int timer;
    int timer_len;
    int target;                         /* 0x7c: MarsFilmTarget * */
    int part;                           /* 0x80: ModelPart * */
    float time;
    float time_end;
    char pad_8c[4];
    float unk_90;                       /* 0x90: 64 - life * 8 for a single film, never read */
} MarsFilm;

typedef struct MarsFilmDrawView {
    Vec3 pos;
    char pad_0c[32];
    float fade;                         /* 0x2c */
    int state;
    char pad_34[8];
    MarsFilmTarget *target;             /* 0x3c */
    ModelPart *part;
    float time;
    char pad_48[76];
} MarsFilmDrawView;

/* 716 bytes */
typedef struct MarsWallWork {
    void *pack;
    void *textures;
    char *targets;
    Vec3 pos;
    Vec3 *base_pos;
    Vec3 base;
    Vec3 ofs;
    float dist;
    float speed;
    float scl;
    int ang[3];
    int *ang_src;
    int ang_base[3];
    MarsFilm film[4];
    int state;
    int leader;
    int film_cnt;
    int group;
    int roll;
    int roll_spd;
    struct MarsWallWork *next;
    struct MarsWallManWork *man;
} MarsWallWork;

/* 124 bytes */
typedef struct MarsWallManWork {
    void *pack;
    void *textures;
    Vec3 *pos;
    int *ang;
    int wait;
    int interval;
    int state;
    struct MarsWallWork *first;
    int roll;
    int tilt;
    int defs[12];
    char pad_58[16];
    int *cur;                           /* 0x68 */
    int **level_tab;
    void *boss;
    char pad_74[4];
    int group;                          /* 0x78 */
} MarsWallManWork;

/* 0x00caf560 */
void mars_w2_break_group(MarsWallWork *wall, MarsWallWork *ref)
{
    int i;
    MarsFilm *film;
    float k;

    if (wall == 0)
        return;
    k = mars_w2_break_sec;
    for (; wall; wall = wall->next) {
        if (ref->group != wall->group)
            continue;
        if (wall == ref)
            continue;
        for (i = 0, film = wall->film; i < wall->film_cnt; i++, film++) {
            if (film->state == 1) {
                film->state = 2;
                film->lock.flags &= ~LOCK_LOCKABLE;
                film->timer = film->timer_len = (int)(k * FPS);
            }
        }
    }
}

/* 0x00caf610 */
void mars_w2_locks_clear(MarsWallWork *wall, int mode)
{
    int i;
    MarsFilm *film;

    for (i = 0, film = wall->film; i < wall->film_cnt; i++, film++) {
        switch (mode) {
        case 1:
            if (film->state == 1)
                film->lock.flags |= LOCK_LOCKABLE;
            break;
        case 0:
            film->lock.flags &= ~LOCK_LOCKABLE;
            break;
        case -1:
            if (wall->film_cnt == 1)
                lock_remove_entry(3, &film->lock);
            else
                lock_remove_entry(2, &film->lock);
            break;
        }
    }
}

/* 0x00caf708 */
void mars_w2_films_init(MarsWallWork *w)
{
    int i;
    MarsFilm *e;

    for (i = 0, e = w->film; i < w->film_cnt; i++, e++) {
        int v = (int)w->targets;
        MarsFilmRef *t = (MarsFilmRef *)(i * 8 + v);
        Vec3 *pos = &w->pos;
        MarsFilmTarget *o;
        int k;
        int life;
        float film_param;
        int n;
        int sub;

        e->time = 0.0f;
        e->fade = 1.0f;
        v = t->obj;
        o = (MarsFilmTarget *)v;
        life = CMD_FILM_LIFE;
        film_param = o->film_time;
        sub = o->sub;
        e->time_end = film_param;
        e->target = (int)o;
        v3_copy(&e->pos, pos);
        k = *(int *)(sub + 44) + 8;
        n = t->n;
        e->state = 1;
        e->part = n;
        e->vtx = k;
        lock_init(2, &e->lock, w, &e->lock_pos, 0, life, 0);
        lockon_entry(&e->lock);
    }
}

/* 0x00caf808 */
void mars_w2_films_init_b(MarsWallWork *w)
{
    MarsFilm *e = w->film;
    MarsFilmSet *s = (MarsFilmSet *)w->targets;
    MarsFilmTarget *o;
    int k;
    int n;
    int sub;
    float film_param;

    e->fade = 1.0f;
    e->time = 0.0f;
    o = s->obj;
    film_param = o->film_time;
    sub = o->sub;
    e->time_end = film_param;
    e->target = (int)o;
    v3_copy(&e->pos, &w->pos);
    k = *(int *)(sub + 44) + 8;
    n = s->n;
    e->state = 1;
    e->part = n;
    e->vtx = k;
    lock_init(3, &e->lock, w, &e->lock_pos, 0, CMD_FILM_LIFE * 8, 0);
    lockon_entry(&e->lock);
}

/* 0x00caf8c8 */
void mars_w2_points_update(MarsWallWork *w)
{
    MarsFilm *film = w->film;
    int i;

    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(&w->pos);
    mars_wall_rotate_yxz(w->ang);
    vu0_matrix_scale(ulMatrixCur, w->scl, w->scl, w->scl);
    for (i = 0; i < w->film_cnt; i++, film++)
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)film->vtx, &film->lock_pos.x);
    ulMatrixPop();
}

/* 0x00caf988 */
void mars_w2_init(MarsWallWork *w, int *arg)
{
    MarsSpawnArg *spawn_arg = (MarsSpawnArg *)arg;
    Vec3 pl_vel;
    MarsWallManWork *man;
    MarsWallWork *tail;
    int n;

    w->base_pos = &w->base;
    w->ang_src = w->ang_base;
    w->targets = (char *)spawn_arg->id;
    player_get_frame_vel(&pl_vel);
    v3_sub(&w->base, spawn_arg->pos, &pl_vel);
    i3_copy(w->ang_base, spawn_arg->ang);
    w->roll = spawn_arg->roll;
    w->man = spawn_arg->man;
    w->roll_spd = spawn_arg->roll_speed;
    w->dist = spawn_arg->dir->z;
    w->group = spawn_arg->unk_18;
    w->speed = spawn_arg->speed;
    w->scl = mars_w2_scl;
    w->film_cnt = n = spawn_arg->n;
    if (n >= 2)
        mars_w2_films_init(w);
    else
        mars_w2_films_init_b(w);
    man = w->man;
    if (man->first) {
        tail = man->first;
        while (tail->next)
            tail = tail->next;
        tail->next = w;
        w->leader = 0;
        w->next = 0;
        mars_w2_locks_clear(w, 0);
    } else {
        *(int *)&w->next = 0; /* steer(type) */
        man->first = w;
        w->leader = 1;
        mars_w2_locks_clear(w, 0);
    }
}

/* 0x00cafb18 */
void mars_w2_release(MarsWallWork *w)
{
    mars_w2_locks_clear(w, -1);
}

/* 0x00cafb20 */
void mars_w2_move(MarsWallWork *w)
{
    int n;
    int i;
    MarsFilm *film;
    int *src_ang;

    n = enemy_status_get_ticks();
    if (n) {
        do {
            switch (w->state) {
            case 0:
                w->speed -= mars_w2_shake_decay;
                if (w->speed <= mars_w2_shake_min) {
                    w->speed = mars_w2_shake_min;
                    w->state = 1;
                }
                break;
            case 1:
                w->speed = mars_w2_shake_min;
                w->scl += mars_w2_shake_rate;
                if (1.0f <= w->scl) {
                    oscilate_create(CMD_CAMERA_FS, CMD_CAMERA_FR);
                    if (w->film_cnt == 1)
                        boss_se_play(13, 0, (int)&w->pos);
                    else
                        boss_se_play(10, 0, (int)&w->pos);
                    w->scl = 1.0f;
                    w->state += 1;
                }
                break;
            }
            w->dist += w->speed;
            w->roll += w->roll_spd;
        } while (--n);
    }
    if (w->leader && w->state > 0)
        mars_w2_locks_clear(w, 1);
    src_ang = w->ang_src;
    i3_copy(w->ang, src_ang);
    w->ofs.z = w->dist;
    w->ofs.x = w->ofs.y = 0.0f;
    w->ang[2] = w->roll;
    vec_rotate_zxy(&w->ofs, w->ang);
    v3_sum(&w->pos, w->base_pos, &w->ofs);
    for (i = 0, film = w->film; i < w->film_cnt; i++, film++)
        v3_copy(&film->pos, &w->pos);
}

/* 0x00cafd78 */
void mars_w2_films_step(MarsWallWork *w)
{
    int i;
    MarsFilm *film;

    for (i = 0, film = w->film; i < w->film_cnt; i++, film++) {
        switch (film->state) {
        case 1:
            if (w->film_cnt == 1) {
                float t = 64.0f - (float)film->lock.life * 8.0f;

                film->unk_90 = t;
                if (film->time < t)
                    film->time += 1.0f;
            }
            break;
        case 2:
            film->time += 1.0f;
            if (film->time_end <= film->time) {
                if (w->film_cnt >= 2) {
                    if (mars_debris_bounded())
                        cm_wire_break_set_cb(cm_wire_break_simple(w->man->boss, 9, *(ModelPart **)(w->targets + 32), &film->lock_pos, 0, w->ang, cmWCol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
                    else
                        cm_wire_break_simple(w->man->boss, 1, *(ModelPart **)(w->targets + 32), &film->lock_pos, 0, w->ang, cmWCol);
                } else {
                    if (mars_debris_bounded())
                        cm_wire_break_set_cb(cm_wire_break_simple(w->man->boss, 9, *(ModelPart **)(w->targets + 44), &film->lock_pos, 0, w->ang, cmWCol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
                    else
                        cm_wire_break_simple(w->man->boss, 1, *(ModelPart **)(w->targets + 44), &film->lock_pos, 0, w->ang, cmWCol);
                }
                film->state = 3;
                film->time = film->time_end;
            }
            break;
        case 4:
            if (--film->timer <= 0) {
                film->timer = film->timer_len = (int)(mars_w2_fade_sec * FPS);
                film->state = 5;
                film->fade = 1.0f;
            } else {
                film->fade = (float)film->timer / (float)film->timer_len;
            }
            break;
        case 5:
            if (--film->timer <= 0) {
                film->timer = film->timer_len = (int)(mars_w2_hold_sec * FPS);
                film->state += 1;
            }
            break;
        case 6:
            if (--film->timer <= 0) {
                film->state = 0;
                film->fade = 0.0f;
            } else {
                film->fade = (float)film->timer / (float)film->timer_len;
            }
            break;
        }
    }
}

/* 0x00cb0040 */
void mars_w2_exec(MarsWallWork *w)
{
    Vec3 pl_vel;
    MarsFilm *films;
    MarsFilm *film;
    int i, alive, done, st, v;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->base, &pl_vel);
    mars_w2_move(w);
    mars_w2_films_step(w);
    mars_w2_points_update(w);
    if (w->leader == 0)
        return;
    done = 0;
    alive = 0;
    for (i = 0, film = films = w->film; i < w->film_cnt; i++, film++) {
        st = film->state;
        switch (st) {
        case 1:
            alive++;
            if (film->lock.flags & LOCK_HIT) {
                if (film->lock.life <= 0) {
                    lock_remove_entry(2, &film->lock);
                    if (w->film_cnt == st) {
                        lock_score(3, &film->lock);
                        lock_remove_entry(3, &film->lock);
                    } else {
                        boss_se_play(11, (int)&film->lock, 0);
                        lock_score(2, &film->lock);
                        lock_remove_entry(2, &film->lock);
                    }
                    v = (int)(mars_w2_break_sec * FPS);
                    film->state = 2;
                    film->lock.flags &= ~LOCK_LOCKABLE;
                    film->timer = film->timer_len = v;
                } else if (w->film_cnt == st) {
                    boss_se_play(14, (int)&film->lock, 0);
                }
            } else if (0.0f < w->dist) {
                int frames;

                mars_player_damage();
                frames = (int)(mars_w2_break_sec * FPS);
                film->lock.flags &= ~LOCK_LOCKABLE;
                film->state = 2;
                film->timer = film->timer_len = frames;
                mars_w2_break_group(w->man->first, w);
            }
            break;
        case 2:
            alive++;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            done++;
            break;
        }
    }
    if (alive)
        return;
    if (done == 0) {
        FINISH_IF(w);
        return;
    }
    if (w->man->first != w)
        return;
    if (w->film_cnt == 1)
        boss_se_play(15, 0, (int)&w->pos);
    else
        boss_se_play(12, 0, (int)&w->pos);
    for (i = 0, film = films; i < w->film_cnt; i++, film++)
        film->state = 4;
    if (w->next) {
        w->next->leader = 1;
        w->man->first = w->next;
    } else {
        w->man->first = 0;
    }
}

/* steer(cast) */
/* steer(alias): films read through MarsFilmDrawView, indexed from w each time */
/* 0x00cb0370 */
void mars_w2_disp(MarsWallWork *w)
{
    float rgba[4];
    unsigned int col[2];
    int save;
    int list;
    int i;

    ulGxSetTextureList(w->textures);
    draw_push_strip_flags();
    draw_push_override_color();
    save = draw_set_override_flags(0);
    rgba[1] = rgba[2] = rgba[3] = 0.0f;
    for (i = 0; i < w->film_cnt; i++) {
        if (((MarsFilmDrawView *)&w->film[0].pos)[i].state == 1) {
            draw_set_override_flags(0);
        } else {
            draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        }
        if (((MarsFilmDrawView *)&w->film[0].pos)[i].state == 0)
            continue;
        ulMatrixPush();
        MTX_TRANSF(((MarsFilmDrawView *)&w->film[0].pos)[i].pos.x, ((MarsFilmDrawView *)&w->film[0].pos)[i].pos.y, ((MarsFilmDrawView *)&w->film[0].pos)[i].pos.z);
        mars_wall_rotate_yxz(w->ang);
        vu0_matrix_scale(ulMatrixCur, w->scl, w->scl, w->scl);
        if (((MarsFilmDrawView *)&w->film[0].pos)[i].state >= 5) {
            col[0] = (cmWallWireCol[0] & 0xffffff) | ((int)((float)color_get_a(cmWallWireCol[0]) * ((MarsFilmDrawView *)&w->film[0].pos)[i].fade) + 1) >> 1 << 24;
            col[1] = (cmWallWireCol[1] & 0xffffff) | ((int)((float)color_get_a(cmWallWireCol[1]) * ((MarsFilmDrawView *)&w->film[0].pos)[i].fade) + 1) >> 1 << 24;
        } else {
            if (((MarsFilmDrawView *)&w->film[0].pos)[i].state == 1)
                ((void (*)(WireObj *, int, int, int, unsigned int))tree_flags_depth)((WireObj *)((MarsFilmDrawView *)&w->film[0].pos)[i].target->sub, 1, 1, -9, 0);
            else
                ((void (*)(WireObj *, int, int, int, unsigned int))tree_flags_depth)((WireObj *)((MarsFilmDrawView *)&w->film[0].pos)[i].target->sub, 1, 1, -1, 8);
            rgba[0] = ((MarsFilmDrawView *)&w->film[0].pos)[i].fade - 1.0f;
            draw_set_override_color(rgba);
            tree_draw_motion((WireObj *)((MarsFilmDrawView *)&w->film[0].pos)[i].target->sub, (void *)((MarsFilmDrawView *)&w->film[0].pos)[i].target->clip, 0x80000000, ((MarsFilmDrawView *)&w->film[0].pos)[i].time);
            col[0] = cmWallWireCol[0];
            col[1] = cmWallWireCol[1];
        }
        list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->textures);
        draw_set_blend(4, 5);
        part_draw_beat_frame(((MarsFilmDrawView *)&w->film[0].pos)[i].part, ((MarsFilmDrawView *)&w->film[0].pos)[i].time, col[0], col[1], 2.0f);
        RenderSetCurrentDisplayList(list);
        ulMatrixPop();
    }
    draw_set_override_flags(save);
    draw_pop_strip_flags();
    draw_pop_override_color();
}

/* 0x00cb0638 */
void *mars_w2_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_w2_desc, 716, arg);
}

/* 0x00cb0660 */
void mars_wall_man_decode(int code, int *quarter, int *tilt, int *count, float x)
{
    switch (code & 0xf) {
    case 1:
        *quarter = 0;
        break;
    case 2:
        *quarter = ANG_90;
        break;
    case 4:
        *quarter = ANG_180;
        break;
    case 8:
        *quarter = 0xc000;
        break;
    default:
        *quarter = 0;
        break;
    }
    switch (code & 0xf0) {
    case 0x10:
        *tilt = (int)DEG_TO_ANG(x);
        break;
    case 0x20:
        *tilt = (int)DEG_TO_ANG(-x);
        break;
    default:
        *tilt = 0;
        break;
    }
    if (code & 0x40)
        *count = 1;
    else
        *count = 4;
}

/* 0x00cb0728 */
void mars_wall_man_exec(MarsWallManWork *w)
{
    int arg[10];
    Vec3 v;
    float sc[2];
    int n, k;
    char *child;

    n = enemy_status_get_ticks();
    switch (w->state) {
    case 0:
        for (; n; n--) {
            if (--w->wait < 0) {
                w->wait = (int)cmWallLevelParam[mars_get_level((int)w->boss)][0];
                w->state = 1;
            }
        }
        break;
    case 1:
        for (; n; n--) {
            if (--w->interval > 0)
                continue;
            k = mars_get_level((int)w->boss) * 3 + 2;
            v.x = 0.0f;
            v.z = FILM_LEN;
            v.y = 0.0f;
            w->interval = (int)(((float *)cmWallLevelParam)[k] * FPS);
            ROT_Z(w->roll, sc, v.x, v.y);
            arg[0] = (int)w->defs;
            arg[1] = (int)w->pos;
            arg[2] = (int)w->ang;
            arg[3] = (int)&v;
            arg[4] = (int)w;
            *(float *)&arg[9] = mars_wall_man_wall_sec;
            arg[6] = w->group;
            {
                int lv_idx = mars_get_level((int)w->boss) * 3 + 1;

                mars_wall_man_decode(*w->cur, &arg[7], &arg[8], &arg[5], ((float *)cmWallLevelParam)[lv_idx]);
            }
            mars_w2_create(w, arg);
            w->cur += 1;
            if (*w->cur != -1)
                continue;
            w->level_tab += 1;
            w->group += 1;
            w->cur = *w->level_tab;
            if (w->cur == 0) {
                w->state = 2;
                mars_request_next(w->boss);
                continue;
            }
            w->state = 0;
            if ((int)(RAND_FLOAT_A() * 2.0f) == 0) {
                int lv_idx = mars_get_level((int)w->boss) * 3 + 1;

                w->tilt = (int)DEG_TO_ANG(((float *)cmWallLevelParam)[lv_idx]);
            } else {
                int lv_idx = mars_get_level((int)w->boss) * 3 + 1;

                w->tilt = (int)DEG_TO_ANG(-((float *)cmWallLevelParam)[lv_idx]);
            }
        }
        break;
    case 2:
        child = ((char **)TASK_OF(w))[2];
        if (child == 0 || child + 64 == 0)
            FINISH_IF(w);
        break;
    }
}

/* 0x00cb0a58 */
void mars_wall_man_release(Earth1stDemoWork *w)
{
}

/* steer(cast) */
/* 0x00cb0a60 */
void *mars_wall_man_create(void *parent, Vec3 *pos, int *ang, MarsWallManArg *wall_arg, int k)
{
    MarsWallManWork *w = task_create_inherit(parent, &mars_wall_man_desc, 124);
    int *dst, *src, *defs;
    int i;

    ((struct { char _00[112]; void *parent; } *)w)->parent = parent;
    if (mars_wall_man_debug)
        w->level_tab = (int **)mars_wall_man_levels[0];
    else
        w->level_tab = (int **)mars_wall_man_levels[mars_get_level((int)parent)];
    w->cur = *w->level_tab;
    defs = w->defs;
    for (i = 0, dst = defs, src = wall_arg->params; i < 12; i++)
        dst[i] = src[i];
    for (i = 0, dst = defs; i < 4; dst += 2, i++)
        ((void (*)(WireObj *, int, int, int, unsigned int))tree_flags_depth)(*(WireObj **)(dst[0] + 4), 1, 1, -1, 0x80000000);
    w->pos = pos;
    w->ang = ang;
    if ((int)(RAND_FLOAT_A() * 2.0f) == 0) {
        int n = mars_get_level((int)w->boss) * 3 + 1;

        w->tilt = (int)DEG_TO_ANG(((float *)cmWallLevelParam)[n]);
    } else {
        int n = mars_get_level((int)w->boss) * 3 + 1;

        w->tilt = (int)DEG_TO_ANG(-((float *)cmWallLevelParam)[n]);
    }
    return w;
}

/* 0x00cb0c38 */
int mars_wall_man_is_active(MarsWallManWork *w)
{
    if (!w->first && w->state == 2)
        return 0;
    return 1;
}

/* 0x00cb0c58 */
void mars_wall_man_stop(MarsWallManWork *w)
{
    if (w->state != 2)
        w->state = 2;
}

/* 0x00cb0c70 */
void mars_wall_vars_add(void)
{
    mars_wall_vars_group = gw_varedit_add(&gw_varedit_root, 0, mars_wall_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_wall_vars_group, mars_wall_vardefs);
}

/* 0x00cb0cc0 */
void mars_wall_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_wall_vardef);
}

/* 0x00cb0cd8 */
void mars_wall_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
