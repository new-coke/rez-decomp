/*
 * Earth boss bubbles and mines: the emitter releases bubbles in timed bursts (per level). In range,
 * a bubble bursts into debris and eight lockable mines that drift and swing until they blow up.
 * Also the bubbles the arm carries.
 */

#include "types_gw.h"
#include "b_earth.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "earth_bubble.h"
#include "earth.h"
#include "earth_cube.h"
#include "earth_demo.h"
#include "../common/boss_common.h"

extern BossBake earth_bubble_bakes[3];

/* "Earth_Bubble", 448 bytes; also its creation argument */
typedef struct BossBubble {
    char pad_00[4];
    void *textures;             /* 0x04: the texture list */
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 vel;                   /* 0x20 */
    Vec3 scale;                 /* 0x2c */
    int scale_cell;             /* 0x38 */
    int flags;                  /* 0x3c */
    int frames;                 /* 0x40 */
    int carrier;                /* 0x44 */
    Vec3 start;                 /* 0x48: the player's position at the start */
    int *cell;                  /* 0x54 */
    LockObj lock;                  /* 0x58 */
    float speed_scale;          /* 0x98 */
    ModelPart part;                /* 0x9c */
    ModelPart part2;               /* 0xf8: drawn at each of the eight points (flag 2) */
    ModelPart part3;               /* 0x154 */
    int state;                  /* 0x1b0 */
    void *parent;               /* 0x1b4 */
    int *end_flag;              /* 0x1b8: nonzero ends the bubble */
    int level;                  /* 0x1bc: the level (earth_bubble_levels) */
} BossBubble;

/* "Earth_Mine", 340 bytes */
typedef struct BossMine {
    char pad_00[4];
    void *tex;                  /* 0x04: texture list */
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    Vec3 vel;                   /* 0x20 */
    int frames;                 /* 0x2c */
    int state;                  /* 0x30 */
    ModelPart part;                /* 0x34 */
    ModelPart part2;               /* 0x90: drawn in state 2 */
    LockObj lock;                  /* 0xec */
    void *shatter_parent;       /* 0x12c: the debris' parent */
    int *end_flag;              /* 0x130: set to blow it up */
    float rgba[4];              /* 0x134 */
    float damp;                 /* 0x144: the drift's damping */
    float wobble_phase;         /* 0x148: the wobble's phase (wobble_step_slow) */
    float size;                 /* 0x14c: the glow's size */
    int hit;                    /* 0x150 */
} BossMine;

extern char earth_bubble_desc[], earth_mine_desc[];
extern BossPartList earth_bubble_parts;
extern Vec3 earth_bubble_points[8];
extern BossBake earth_bubble_bakes[3];
extern struct GwVarNode *earth_b_vars_group;
extern GwVarDef earth_b_vardefs[];

/* earth_bubble_levels entry */
typedef struct BossBubbleLevel {
    float wait;                 /* seconds before a burst */
    int count;                  /* 0x04 */
    float period;               /* 0x08: seconds between bubbles */
    float speed;                /* 0x0c: a bubble's speed (earth_bubble_exec) */
} BossBubbleLevel;

extern BossBubbleLevel earth_bubble_levels[];
extern int SW_BUBBLE;
extern float BUBBLE_SHOT_ANG, earth_bubble_up;
extern struct TaskDesc earth_bubble_manage_desc;

/* 48 bytes */
typedef struct BossDebris {
    Vec3 pos;
    Vec3 vel;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
    int dang[3];                /* 0x24 */
} BossDebris;

/* burst bubble's debris, 140 bytes; also its creation argument */
typedef struct BossDebrisWork {
    char pad_00[8];
    BossDebris *parts;          /* 0x08 */
    int count;                  /* 0x0c */
    int life;                   /* 0x10 */
    int life0;                  /* 0x14 */
    int unk_18;                 /* 0x18: set to 0 by earth_break2_init, never read */
    float speed;                /* 0x1c */
    Vec3 pos;                   /* 0x20 */
    Vec3 vel;                   /* 0x2c */
    Vec3 size;                  /* 0x38 */
    unsigned int col;           /* 0x44 */
    TrailVtx quad[4];           /* 0x48 */
    float scale;                /* 0x88 */
} BossDebrisWork;

extern char str_earth_bubble_c[], earth_break2_desc[];
extern float earth_break2_life, earth_break2_spin, earth_break2_out_speed;

/* "Earth_Mine" creation argument */
typedef struct BossMineArg {
    Vec3 pos;
    int ang[3];                 /* 0x0c */
    Vec3 vel;                   /* 0x18 */
    float scale;                /* 0x24: the model's scale */
    int frames;                 /* 0x28 */
    void *shatter_parent;       /* 0x2c */
    int *end_flag;              /* 0x30 */
} BossMineArg;

extern int HITPOINT_M;
extern BossCfKey earth_mine_fade_keys[];
extern float MINE_WAIT, MINE_STOP_RATE, MINE_SWING_R, mine_rr, mine_r;
extern float CMD_RIP_H;
extern unsigned int col_mine;
extern float earth_bubble_rgba[4];
extern float earth_bubble_fade_time;
extern float earth_bubble_return_speed, BUBBLE_SPEED2, BUBBLE_LIFE, BUBBLE_LIFE2;
extern float MISSILE_DMG, bullet_hit_rad;
extern unsigned int col_miss;
extern int HITPOINT;
extern float BUBBLE_TIME2, BUBBLE_TIME3, earth_bubble_mine_time_min, earth_bubble_mine_time_max, BUBBLE_MINE_SPEED, BUBBLE_MINE_LEN, BUBBLE_MINE_ANG;
extern float earth_bubble_break_speed, earth_bubble_break_w, earth_bubble_break_h;
extern int earth_bubble_break_count;

/* 0x00caaed8 */
void earth_bubble_init(BossBubble *w, BossBubble *arg)
{
    w->scale.x = w->scale.y = w->scale.z = 1.0f;
    scroll_relative(&arg->pos, &w->pos);
    v3_copy(&w->vel, &arg->vel);
    player_get_pos(&w->start);
    w->state = 0;
    w->end_flag = arg->end_flag;
    w->flags = arg->flags;
    w->parent = arg->parent;
    w->level = arg->level;
    if (w->flags & 0x10) {
        {
            int sc = arg->scale_cell, ca = arg->carrier, ce = arg->cell;

            w->scale_cell = sc;
            w->carrier = ca;
            w->cell = ce;
        }
        TASK_OF(w)->exec = (void (*)(void *))earth_bubble_exec_carried;
        TASK_OF(w)->disp = (void (*)(void *))earth_bubble_disp_carried;
        w->lock.flags &= ~LOCK_LOCKABLE;
        w->part = *earth_bubble_parts.p[4];
        w->part2 = *earth_bubble_parts.p[3];
        w->part3 = *earth_bubble_parts.p[5];
        ((struct { int w[17]; float speed; } *)&w->part)->speed *= arg->speed_scale;
        w->frames = w->part.frames / w->part.speed;
        lock_init(7, &w->lock, w, &w->pos, 0, HITPOINT, 128);
        lockon_entry(&w->lock);
        w->lock.flags &= ~LOCK_LOCKABLE;
    } else {
        w->part = *earth_bubble_parts.p[0];
        w->part2 = *earth_bubble_parts.p[1];
        w->frames = BUBBLE_TIME2 * FPS;
        lock_init(5, &w->lock, w, &w->pos, 0, HITPOINT, 128);
        lockon_entry(&w->lock);
    }
}

/* 0x00cab548 */
void earth_bubble_release(BossBubble *w)
{
    lock_remove_entry(5, &w->lock);
}

/* 0x00cab558 */
void earth_bubble_exec_carried(BossBubble *w)
{
    Vec3 pl_pos, pl_vec;
    int ang[3];
    int n;

    player_get_vec(&pl_vec);
    switch (w->state) {
    case 0:
        if (*w->cell == -1)
            {
                FINISH_IF(w);
                return;
            }
        v3_copy(&w->pos, (Vec3 *)w->carrier);
        for (n = enemy_status_get_ticks(); n; n--) {
            if (w->part.frame <= 32.0f)
                dc_object_advance_time(&w->part);
        }
        if (*w->cell)
            break;
        w->state++;
    case 1:
        for (n = enemy_status_get_ticks(); n; n--) {
            w->frames--;
            dc_object_advance_time(&w->part);
        }
        if (w->frames > 0) {
            if (*w->cell == -1)
                {
                FINISH_IF(w);
                return;
            }
            v3_copy(&w->pos, (Vec3 *)w->carrier);
            break;
        }
        w->state++;
    case 2:
        if (*w->cell == -1)
            {
                FINISH_IF(w);
                return;
            }
        w->flags &= ~0x10;
        w->lock.flags |= LOCK_LOCKABLE;
        if (w->flags & 0x20) {
            w->frames = BUBBLE_LIFE2 * FPS;
            v3_sub(&w->vel, (Vec3 *)w->carrier, &w->pos);
            v3_sub(&w->vel, &w->vel, &pl_vec);
            vec3_normalize(&w->vel, &w->vel);
            v3_scale(&w->vel, earth_bubble_return_speed);
            w->state++;
        } else {
            boss_se_play((int)w, 13, (int)&w->lock, 0);
            v3_copy(&w->pos, (Vec3 *)w->carrier);
            w->frames = BUBBLE_LIFE * FPS;
            player_get_pos(&pl_pos);
            v3_sub(&w->vel, &pl_pos, &w->pos);
            vec3_normalize(&w->vel, &w->vel);
            v3_scale(&w->vel, BUBBLE_SPEED2);
            w->state++;
            if (!(w->flags & 4))
                break;
            FINISH_IF(w);
            return;
        }
        break;
    case 3:
        v3_add(&w->pos, &pl_vec);
        for (n = enemy_status_get_ticks(); n; n--) {
            dc_object_advance_time(&w->part2);
            dc_object_advance_time(&w->part3);
            v3_add(&w->pos, &w->vel);
            w->frames--;
        }
        if (player_touch(&w->pos, bullet_hit_rad, MISSILE_DMG))
            goto blow;
        if (w->frames <= 0)
            {
                FINISH_IF(w);
                return;
            }
        break;
    }
    if (!(w->lock.flags & LOCK_HIT))
        return;
    if (w->lock.life <= 0) {
        lock_score(7, &w->lock);
blow:
        boss_se_play((int)w, 20, (int)&w->lock, (int)&w->pos);
        angles_turn_y(w->ang, ANG_180, ang);
        cm_wire_break_simple(w->parent, 5, earth_bubble_parts.p[7], &w->pos, 0, ang, col_miss);
        FINISH_IF(w);
    } else {
        boss_se_play((int)w, 19, (int)&w->lock, (int)&w->pos);
    }
}

/* steer(cast) */
/* 0x00caba88 */
void earth_bubble_disp_carried(BossBubble *w)
{
    int old_flags, old_list, old_fog;

    ulGxSetTextureList(w->textures);
    old_flags = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    ulMatrixPush();
    if (w->flags & 0x10) {
        if (view_sphere_visible(&w->pos, w->part.radius)) {
            MTX_TRANS(&w->pos);
            old_list = RenderSetCurrentDisplayList(6);
            ulGxSetTextureList(w->textures);
            earth_bubble_rgba[0] = 0.0f;
            draw_set_override_color(earth_bubble_rgba);
            ulMatrixUnitRotation((volatile float *)ulMatrixCur);
            old_fog = (int)fog_set_enable(0);
            dc_object_draw(&w->part);
            fog_set_enable((void *)old_fog);
            RenderSetCurrentDisplayList(old_list);
        }
    } else if (view_sphere_visible(&w->pos, w->part2.radius)) {
        if ((float)w->frames > earth_bubble_fade_time * FPS) {
            draw_set_override_flags(0);
        } else {
            earth_bubble_rgba[0] = (float)w->frames / (earth_bubble_fade_time * FPS) + -1.0f;
            draw_set_override_color(earth_bubble_rgba);
        }
        MTX_TRANS(&w->pos);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
            w->ang[0] = RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
        } while (0);
        vu0_matrix_rotate_y(ulMatrixCur, w->ang[1]);
        vu0_matrix_rotate_x(ulMatrixCur, w->ang[0]);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&w->part2);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        {
            int old_fog = (int)fog_set_enable(0);
            int old_list = RenderSetCurrentDisplayList(6);

            ulGxSetTextureList(w->textures);
            dc_object_draw(&w->part3);
            RenderSetCurrentDisplayList(old_list);
            fog_set_enable((void *)old_fog);
        }
    }
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old_flags);
}

/* 0x00cabd50 */
void earth_bubble_exec(BossBubble *w)
{
    BossMineArg arg;
    Vec3 dir, pl, burst_pos, size;
    int ang[3];
    float k, t, ratio;
    int n, i;

    player_get_pos(&pl);
    player_get_vec(&dir);
    v3_add(&w->pos, &dir);
    v3_add(&w->start, &dir);
    k = earth_bubble_levels[w->level].speed;
    for (n = enemy_status_get_ticks(); n; n--) {
        float speed;

        dc_object_advance_time(&w->part2);
        v3_sub(&dir, &w->start, &w->pos);
        vec3_normalize(&dir, &dir);
        {
            float frames = w->frames < 0 ? 0.0f : (float)w->frames;

            speed = BUBBLE_MINE_LEN * k;
            t = frames / (BUBBLE_TIME2 * FPS);
        }
        w->frames--;
        v3_scale(&dir, speed * (1.0f - t));
        v3_sum(&w->pos, &dir, &w->pos);
        v3_mul(&dir, &w->vel, t);
        v3_sum(&w->pos, &dir, &w->pos);
        if (w->frames <= -(BUBBLE_TIME3 * FPS))
            goto hit;
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
        lock_score(5, &w->lock);
    } else if (!*w->end_flag) {
        int range = player_range_check(&w->pos);

        if (range == -1)
            goto finish;
        if (range != 1)
            return;
    }
hit:
    boss_se_play((int)w, 21, (int)&w->lock, (int)&w->pos);
    if (w->flags & 4) {
        for (i = 0; i < 8; i++)
            ;
    } else if (w->flags & 2) {
        v3_sub(&dir, &pl, &w->pos);
        ratio = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        ratio *= BUBBLE_MINE_LEN;
        k = ratio / BUBBLE_MINE_SPEED - earth_bubble_mine_time_max * FPS;
        if (k != 0.0f) {
            ratio = earth_bubble_mine_time_max * FPS / k;
            if (ratio > 1.0f)
                ratio = 1.0f;
            else if (ratio < 0.0f)
                ratio = 0.0f;
        }
        v3_sub(&burst_pos, &w->pos, &dir);
        {
            int cn;

            size.x = earth_bubble_break_w;
            size.y = earth_bubble_break_h;
            cn = earth_bubble_break_count;
            {
                unsigned int *col = &col_mine;

                earth_break2_create(w->parent, &burst_pos, 0, &size, cn, earth_bubble_break_speed, col[2]);
            }
        }
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
        cm_wire_break_simple(w->parent, 5, earth_bubble_parts.p[6], &w->pos, 0, ang, col_mine);
        vec3_normalize(&dir, &dir);
        v3_scale(&dir, BUBBLE_MINE_SPEED);
        player_get_vec(&pl);
        for (i = 0; i < 8; i++) {
            float rnd;

            v3_mul(&arg.vel, &earth_bubble_points[i], BUBBLE_MINE_ANG);
            v3_add(&arg.vel, &dir);
            rnd = RAND_FLOAT_A();
            arg.scale = w->part2.frame;
            v3_copy(&arg.pos, &w->pos);
            arg.frames = (rnd * (earth_bubble_mine_time_max - earth_bubble_mine_time_min) + earth_bubble_mine_time_min) * FPS + k;
            v3_sub(&arg.pos, &arg.pos, &arg.vel);
            v3_sub(&arg.pos, &arg.pos, &pl);
            v3_add(&arg.pos, &earth_bubble_points[i]);
            arg.end_flag = w->end_flag;
            i3_set(arg.ang, 0, 0, 0);
            arg.shatter_parent = w->parent;
            earth_mine_create(w->parent, &arg);
        }
    }
finish:
    FINISH_IF(w);
}

/* steer(cast) */
/* 0x00cac3b8 */
void earth_bubble_disp(BossBubble *w)
{
    Vec3 pts[8];
    int z, i;

    ulGxSetTextureList(w->textures);
    z = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    earth_bubble_rotate_yxz(w->ang);
    dc_object_draw(&w->part);
    if (w->flags & 4) {
        for (i = 0; i < 8; i++)
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&earth_bubble_points[i], (float *)&pts[i]);
    } else if (w->flags & 2) {
        for (i = 0; i < 8; i++) {
            ulMatrixPush();
            MTX_TRANS(&earth_bubble_points[i]);
            dc_object_draw(&w->part2);
            ulMatrixPop();
        }
    }
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_set_override_flags(z);
    if (w->flags & 4) {
        for (i = 0; i < 8; i++)
            ;
    }
}

/* 0x00cac540 */
BossBubble *earth_bubble_create(void *parent, BossBubble *arg)
{
    return task_create_inherit_arg(parent, earth_bubble_desc, 448, arg);
}

/* 0x00cac568 */
void earth_bubbles_setup(BossPartList *parts)
{
    WireObj *obj, *n;
    Vec3 *v;
    int i;

    for (i = 0; i < 8; i++)
        earth_bubble_parts.p[i] = parts->p[i];
    obj = ((WireObj *)earth_bubble_parts.p[0]->root)->child->sibling;
    v3_copy(&earth_bubble_points[0], (Vec3 *)obj->pos);
    i = 0;
    n = obj->sibling;
    if (n) {
        obj = n;
        do {
            if (++i >= 8)
                break;
            v3_copy(&earth_bubble_points[i], (Vec3 *)obj->pos);
            obj = obj->sibling;
        } while (obj);
    }
    motion_bake(&earth_bubble_bakes[0], (WireObj *)earth_bubble_parts.p[3]->root, (int *)earth_bubble_parts.p[3]->clip);
    motion_bake(&earth_bubble_bakes[1], (WireObj *)earth_bubble_parts.p[4]->root, (int *)earth_bubble_parts.p[4]->clip);
    motion_bake(&earth_bubble_bakes[2], (WireObj *)earth_bubble_parts.p[5]->root, (int *)earth_bubble_parts.p[5]->clip);
    earth_b_vars_group = gw_varedit_add(&gw_varedit_root, 0, earth_b_vardef);
    gw_varedit_add_table(&gw_varedit_root, earth_b_vars_group, earth_b_vardefs);
}

/* 0x00cac6b8 */
void earth_bubbles_cleanup(void)
{
    motion_bake_free(&earth_bubble_bakes[0]);
    motion_bake_free(&earth_bubble_bakes[1]);
    motion_bake_free(&earth_bubble_bakes[2]);
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)earth_b_vardef);
}

/* 0x00cac708 */
int earth_bubble_spawn(void *parent, Vec3 *pos, Vec3 *vel, int flags, int end_flag, int level)
{
    BossBubble arg;

    v3_copy(&arg.pos, pos);
    v3_copy(&arg.vel, vel);
    arg.flags = flags;
    arg.end_flag = (int *)end_flag;
    arg.level = level;
    arg.parent = parent;
    return earth_bubble_create(parent, &arg) != 0;
}

/* 0x00cac770 */
void earth_bubble_manage_exec(BossBubbleGen *w)
{
    Vec3 v;
    float sc[2];
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        switch (w->state) {
        case 0:
            if (w->delay) {
                w->delay--;
                break;
            }
            if (--w->timer > 0)
                break;
            w->state = 1;
            w->timer = w->wait;
            break;
        case 1:
            if (!w->left)
                break;
            if (--w->timer2 > 0)
                break;
            w->timer2 = w->period;
            if (SW_BUBBLE) {
                boss_se_play((int)w, 23, 0, (int)w->pos);
                if (w->count >= 2) {
                    {
                        int ang = DEG_TO_ANG(-BUBBLE_SHOT_ANG + (w->side & 1) * 2 * BUBBLE_SHOT_ANG);

                        w->side ^= 1;
                        v.y = earth_bubble_up;
                        v.x = v.z = 0.0f;
                        ROT_Z(ang, sc, v.x, v.y);
                    }
                    earth_bubble_spawn(w->parent, w->pos, &v, 2, w->end_flag, w->level);
                } else {
                    earth_bubble_spawn(w->parent, w->pos, &w->dir, 2, w->end_flag, w->level);
                }
            }
            if (--w->left > 0)
                break;
            w->left = w->count;
            w->side = RAND_FLOAT_A() * 2.0f;
            w->state = 0;
            break;
        }
    }
    if (earth_get_attack_end(w->parent))
        FINISH_IF(w);
}

/* 0x00cac9a8 */
BossBubbleGen *earth_bubble_manage_create(BossMainWork *w, Vec3 *pos, int mode, int end_flag, int delay)
{
    BossBubbleGen *gen;
    BossBubbleLevel *lv;
    Vec3 v;
    float sc[4];
    int k;

    if (mode == 1) {
        boss_se_play((int)w, 23, 0, (int)pos);
        v.y = earth_bubble_up;
        v.x = v.z = 0.0f;
        ROT_Z(DEG_TO_ANG(BUBBLE_SHOT_ANG), (sc + 0), v.x, v.y);
        earth_bubble_spawn(w, pos, &v, 2, end_flag, earth_get_level(w));
        ROT_Z(DEG_TO_ANG(-(BUBBLE_SHOT_ANG * 2.0f)), (sc + 2), v.x, v.y);
        earth_bubble_spawn(w, pos, &v, 2, end_flag, earth_get_level(w));
        return 0;
    }
    gen = task_create_inherit(w, &earth_bubble_manage_desc, 76);
    gen->delay = delay;
    k = earth_get_level(w);
    gen->dir.y = earth_bubble_up;
    gen->dir.x = gen->dir.z = 0.0f;
    gen->level = k;
    lv = &earth_bubble_levels[k];
    gen->n = mode;
    gen->pos = pos;
    *(int **)&gen->end_flag = (int *)end_flag; /* a pointer passed as int */
    gen->parent = w;
    gen->timer = gen->wait = lv->wait * FPS;
    gen->timer2 = gen->period = earth_bubble_levels[k].period * FPS;
    gen->left = gen->count = earth_bubble_levels[k].count;
    gen->side = RAND_FLOAT_A() * 2.0f;
    return gen;
}

/* 0x00cacc20 */
int earth_bubble_spawn_carried(void *parent, int carrier, int *cell, int scale_cell, float speed_scale)
{
    BossBubble arg;

    arg.carrier = carrier;
    arg.scale_cell = scale_cell;
    arg.flags = *cell | 0x10;
    arg.speed_scale = speed_scale;
    arg.cell = cell;
    arg.parent = parent;
    return earth_bubble_create(parent, &arg) != 0;
}

/* 0x00cacc68 */
void earth_mine_init(BossMine *w, BossMineArg *arg)
{
    v3_copy(&w->pos, &arg->pos);
    i3_copy(w->ang, arg->ang);
    v3_copy(&w->vel, &arg->vel);
    w->end_flag = arg->end_flag;
    w->frames = arg->frames;
    w->damp = 1.0f;
    w->shatter_parent = arg->shatter_parent;
    w->state = 0;
    w->part = *earth_bubble_parts.p[1];
    w->part.frame = arg->scale;
    w->part2 = *earth_bubble_parts.p[2];
    lock_init(6, &w->lock, w, &w->pos, 0, HITPOINT_M, 128);
    lockon_entry(&w->lock);
    cm_cfchange_create(w, (BossCfKey *)w->rgba, earth_mine_fade_keys, 2, 1.0f, 0.0f);
}

/* steer(cast) */
/* 0x00cacf70 */
void earth_mine_exec(BossMine *w)
{
    Vec3 pl_vec;
    union {
        Vec3 e;
        int ang[3];
    } u;
    int n;

    player_get_vec(&pl_vec);
    v3_add(&w->pos, &pl_vec);
    n = enemy_status_get_ticks();
    switch (w->state) {
    case 0:
        for (; n; n--) {
            w->size = w->size * mine_rr + mine_r * (1.0f - mine_rr);
            dc_object_advance_time(&w->part);
            v3_add(&w->pos, &w->vel);
            w->frames--;
        }
        if (*w->end_flag || ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0)) {
blow:
            lock_score(6, &w->lock);
            boss_se_play((int)w, 22, (int)&w->lock, (int)&w->pos);
            u.ang[0] = 0;
            u.ang[2] = u.ang[1] = w->part.frame * ANG_TURN / w->part.frames + ANG_QUARTER;
            cm_wire_break_simple(w->shatter_parent, 5, earth_bubble_parts.p[6], &w->pos, 0, u.ang, col_mine);
            FINISH_IF(w);
            return;
        }
        if (w->frames > 0)
            return;
        w->frames = MINE_WAIT * FPS;
        w->state++;
        break;
    case 1:
        if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0)
            goto blow;
        for (; n; n--) {
            w->damp *= MINE_STOP_RATE;
            w->size = w->size * mine_rr + mine_r * (1.0f - mine_rr);
            v3_scale(&w->vel, w->damp);
            v3_add(&w->pos, &w->vel);
            {
                float k = MINE_SWING_R * (1.0f - w->damp);

                wobble_step_slow(&u.e, k, &w->wobble_phase);
            }
            v3_add(&w->pos, &u.e);
            dc_object_advance_time(&w->part);
            w->frames--;
        }
        if (w->frames > 0)
            return;
        boss_se_play((int)w, 22, (int)&w->lock, (int)&w->pos);
        w->frames = w->part2.frames / w->part2.speed;
        w->state++;
        break;
    case 2:
        dc_object_advance_time(&w->part2);
        if (!w->hit && player_touch_curve(&w->pos, 1, w->part2.frame, CMD_RIP_H))
            w->hit = 1;
        if (--w->frames > 0)
            return;
        FINISH_IF(w);
        break;
    }
}

/* 0x00cad3a8 */
void earth_mine_release(BossMine *w)
{
    lock_remove_entry(6, &w->lock);
}

/* steer(cast) */
/* 0x00cad3b8 */
void earth_mine_disp(BossMine *w)
{
    int z;

    ulGxSetTextureList(w->tex);
    draw_push_strip_flags();
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    switch (w->state) {
    case 0:
    case 1: {
        int dl;

        z = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(w->rgba);
        dc_object_draw(&w->part);
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->tex);
        draw_set_blend(4, 1);
        flare_draw_here(1, COL_WHITE, w->size);
        RenderSetCurrentDisplayList(dl);
        break;
    }
    case 2: {
        int dl = RenderSetCurrentDisplayList(6);

        ulGxSetTextureList(w->tex);
        z = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 6144);
        dc_object_draw(&w->part2);
        RenderSetCurrentDisplayList(dl);
        break;
    }
    default:
        z = draw_set_override_flags(0);
        break;
    }
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_set_override_flags(z);
}

/* 0x00cad510 */
BossMine *earth_mine_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, earth_mine_desc, 340, arg);
}

/* 0x00cad538 */
void earth_break2_init(BossDebrisWork *w, BossDebrisWork *arg)
{
    BossDebris *p;
    Vec3 dir;
    float sc[4];
    int i;
    float life = earth_break2_life;

    w->scale = 1.0f;
    w->count = arg->count;
    w->col = arg->col;
    w->quad[0].x = w->quad[1].x = -arg->size.x;
    w->quad[2].x = w->quad[3].x = arg->size.x;
    w->quad[0].y = w->quad[2].y = -arg->size.y;
    w->quad[1].y = w->quad[3].y = arg->size.y;
    w->life = w->life0 = life * FPS;
    w->quad[0].q = w->quad[1].q = w->quad[2].q = w->quad[3].q = 0.0f;
    w->unk_18 = 0;
    w->parts = heap_block_tag(game_malloc(w->count * sizeof(BossDebris)), str_earth_bubble_c, 1057);
    for (i = 0, p = w->parts; i < w->count; i++, p++) {
        dir.x = dir.y = 0.0f;
        dir.z = -arg->speed;
        p->ang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        p->ang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        p->ang[2] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        ROT_Y(p->ang[1], (sc + 0), dir.x, dir.z);
        ROT_X(p->ang[0], (sc + 2), dir.y, dir.z);
        v3_sum(&p->pos, &arg->pos, &dir);
        vec3_normalize(&p->vel, &dir);
        v3_scale(&p->vel, earth_break2_out_speed);
        v3_sum(&p->vel, &w->vel, &p->vel);   /* w's own vel, not the argument's */
        p->dang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * earth_break2_spin - earth_break2_spin);
        p->dang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * earth_break2_spin - earth_break2_spin);
        p->dang[2] = DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * earth_break2_spin - earth_break2_spin);
    }
}

/* 0x00cad948 */
void earth_break2_release(int *w)
{
    heap_free((void *)w[2], 0);
}

/* 0x00cad958 */
void earth_break2_exec(BossDebrisWork *w)
{
    BossDebris *p = w->parts;
    Vec3 pl_vel, pl_acc;
    int i;

    if (--w->life <= 0) {
        FINISH_IF(w);
        return;
    }
    w->scale *= 0.97f;
    player_get_frame_vel_old(&pl_vel);
    player_get_frame_accel(&pl_acc);
    v3_scale(&pl_acc, w->scale);
    v3_add(&pl_vel, &pl_acc);
    for (i = 0; i < w->count; i++, p++) {
        v3_sum(&p->pos, &pl_vel, &p->pos);
        v3_sum(&p->pos, &p->vel, &p->pos);
        {
            int *ang = p->ang;

            ang[0] = p->dang[0] + ang[0];
            ang[1] = p->dang[1] + ang[1];
            ang[2] = p->dang[2] + ang[2];
        }
    }
}

/* 0x00cadac8 */
void earth_break2_disp(BossDebrisWork *w)
{
    BossDebris *p = w->parts;
    unsigned int col;
    int i, old_flags;

    old_flags = draw_set_override_flags(0);
    draw_set_blend(4, 1);
    flat_strip_begin(1);
    for (i = 0; i < w->count; i++, p++) {
        ulMatrixPush();
        MTX_TRANS(&p->pos);
        earth_bubble_rotate_yxz(p->ang);
        col = (w->col & 0xffffff) | (((color_get_a(w->col) * w->life / w->life0 + 1) >> 1) << 24);
        w->quad[0].c = w->quad[1].c = w->quad[2].c = w->quad[3].c = col;
        poly_draw_flat(w->quad, 4);
        ulMatrixPop();
    }
    flat_strip_end();
    draw_set_override_flags(old_flags);
}

/* 0x00cadbf8 */
int earth_break2_create(void *parent, Vec3 *pos, Vec3 *vel, Vec3 *size, int count, float speed, unsigned int col)
{
    BossDebrisWork arg;
    BossDebrisWork *w;

    if (!count)
        return -1;
    scroll_relative(pos, &arg.pos);
    if (vel)
        v3_copy(&arg.vel, vel);
    else
        v3_set(&arg.vel, 0.0f, 0.0f, 0.0f);
    if (size)
        v3_copy(&arg.size, size);
    else
        arg.size.x = arg.size.y = arg.size.z = 1.0f;
    arg.speed = speed;
    arg.col = col;
    arg.count = count;
    w = task_create_inherit_arg(parent, earth_break2_desc, 140, &arg);
    task_set_dlist(w, 6);
    return w ? 0 : -1;
}

/* 0x00cadd10 */
void earth_bubble_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
