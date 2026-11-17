/*
 * Perfect Wall manager (stg04): moves the two halves as one; they swing and charge, close in with
 * lightning between them, unite, then rotate, each phase on its own curve. Hurts on contact;
 * scores each half shot before uniting (or both after). Plays the hit and break sounds.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "nj_system.h"
#include "player.h"
#include "player_status.h"
#include "sound_midise.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "task.h"
#include "en_perfectwall_manager.h"
#include "en_lightning.h"
#include "en_perfectwall.h"
#include "enemy_vars.h"
#include "util.h"
#include "system.h"
#include "mathdef.h"

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

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_sum(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x + rhs->x;
    dst->y = lhs->y + rhs->y;
    dst->z = lhs->z + rhs->z;
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern struct GwVarNode *gw_varedit_root;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern struct TaskDesc en_perfectwall_manager_desc;
extern int en_perfectwall_manager_init_arg;
extern ParamRec en_perfectwall_manager_params;
extern VarDef en_perfectwall_manager_vardefs[14];
extern float en_perfectwall_manager_curve1_table[];
extern float en_perfectwall_manager_curve2_table[];
extern float en_perfectwall_manager_curve3_table[];
extern float en_perfectwall_manager_curve4_table[];
extern float en_perfectwall_manager_curve5_table[];
extern int en_perfectwall_manager_score_unite;
extern int en_perfectwall_manager_score_a;
extern int en_perfectwall_manager_score_b;
#define EN_PERFECTWALL_MANAGER_REC ((EnPerfectWallManagerRec *)&en_perfectwall_manager_params)
extern void (*en_perfectwall_manager_states[])(EnPerfectWallManagerWork *w);

/* 0x00b0eff8 stg04, 0x00b07dd0 stg22 */
void en_perfectwall_manager_vars_register(void)
{
    VarDef grp_def;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&grp_def, 0, sizeof grp_def);
    grp_def.name = en_perfectwall_manager_params.name;
    grp_def.fmt = en_perfectwall_manager_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&grp_def);
    def = en_perfectwall_manager_vardefs;
    val = en_perfectwall_manager_params.v;
    for (i = 0; i < 14; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_perfectwall_manager_vardefs);
    en_perfectwall_vars_register();
    en_lightning_vars_register();
}

/* 0x00b0f0b8 stg04, 0x00b07e90 stg22 */
void en_perfectwall_manager_vars_unregister(void)
{
    en_lightning_vars_unregister();
    en_perfectwall_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00b0f0e0 stg04, 0x00b07eb8 stg22 */
int en_perfectwall_manager_vars_init(int *id)
{
    en_perfectwall_manager_vars_register();
    en_perfectwall_manager_init_arg = *id;
    return 0;
}

/* 0x00b0f118 stg04, 0x00b07ef0 stg22 */
int en_perfectwall_manager_vars_release(void)
{
    en_perfectwall_manager_vars_unregister();
    return 0;
}

/* 0x00b0f138 stg04, 0x00b07f10 stg22 */
void en_perfectwall_manager_calc_points(EnPerfectWallManagerWork *w)
{
    Vec3 ofs;
    float sc[2];
    float x, y, z;

    z = 0.0f;
    x = w->gap;
    y = z;
    ROT_Z(-w->ang[2], sc, x, y);
    ofs.x = x;
    ofs.z = z;
    ofs.y = y;
    v3_sum(&w->pts[0], &w->pos, &ofs);
    v3_sub(&w->pts[1], &w->pos, &ofs);
}

/* 0x00b0f218 stg04, 0x00b07ff0 stg22 */
void en_perfectwall_manager_swing(EnPerfectWallManagerWork *w)
{
    Vec3 *pos = &w->pos;
    Vec3 *off = &w->wob;
    float t, r;

    t = w->swing_t += EN_PERFECTWALL_MANAGER_REC->spd_swing;
    r = EN_PERFECTWALL_MANAGER_REC->r_swing;
    v3_sub(pos, pos, off);
    v3_set(off, sinf(t * 3.0f) * r, cosf(t * 5.0f) * r, sinf(t * 4.0f) * r);
    v3_add(pos, off);
}

/* 0x00b0f338 stg04, 0x00b08110 stg22 */
void en_perfectwall_manager_lightning_create(EnPerfectWallManagerWork *w)
{
    EnLightningArg prm;

    prm.n = (w->gap_min + w->gap_min) / EN_PERFECTWALL_MANAGER_REC->frm_life;
    prm.m = 3;
    prm.a = &w->seg[0][0];
    prm.to = &w->seg[1][0];
    w->sub[0] = en_lightning_create(w, &prm);
    prm.a = &w->seg[0][1];
    prm.to = &w->seg[1][1];
    w->sub[1] = en_lightning_create(w, &prm);
    w->flags |= 4;
}

/* 0x00b0f3d0 stg04, 0x00b081a8 stg22 */
void en_perfectwall_manager_lightning_finish(EnPerfectWallManagerWork *w)
{
    int i;

    if (w->flags & 4) {
        for (i = 1; i >= 0; i--) {
            FINISH_IF(w->sub[i]);
            w->sub[i] = 0;
        }
        w->flags &= ~4;
    }
}

/* 0x00b0f430 stg04, 0x00b08208 stg22 */
void en_perfectwall_manager_lightning_check(EnPerfectWallManagerWork *w)
{
    int fl = w->flags;

    if (fl & 4) {
        if (!w->sub[0] || (TASK_OF(w->sub[0])->flags & TASK_F_END))
            w->sub[0] = 0;
        if (!w->sub[1] || (TASK_OF(w->sub[1])->flags & TASK_F_END))
            w->sub[1] = 0;
        if (!w->sub[0] && !w->sub[1])
            w->flags = fl & ~4;
    }
}

/* 0x00b0f4a8 stg04, 0x00b08280 stg22 */
void en_perfectwall_manager_collision(EnPerfectWallManagerWork *w)
{
    if ((w->flags & 0x300) != 0x300 && 0.0f <= w->turn) {
        if (util_player_near_axis(&w->pos, w->ang, EN_PERFECTWALL_MANAGER_REC->collision_r, EN_PERFECTWALL_MANAGER_REC->collision_z)) {
            player_status_add_damage(1.0f);
            w->result = 1;
        }
    }
}

/* 0x00b0f530 stg04, 0x00b08308 stg22 */
void en_perfectwall_manager_joint_timer(EnPerfectWallManagerWork *w)
{
    Vec3 rel;

    if (!(w->flags & 0x10)) {
        player_get_frame_pos(&rel);
        v3_sub(&rel, &w->pos, &rel);
        util_dir_to_player_frame(&rel, &rel);
        if (0.0f <= rel.z) {
            w->flags |= 0x10;
            w->timer = EN_PERFECTWALL_MANAGER_REC->dst_lightning_joint * FPS;
        }
    } else {
        w->timer -= 1.0f;
        if (w->timer < 0.0f) {
            en_perfectwall_manager_lightning_finish(w);
            w->result = 2;
        }
    }
}

/* 0x00b0f618 stg04, 0x00b083f0 stg22 */
float en_perfectwall_manager_close_gap_curve(float t)
{
    return util_curve_eval(en_perfectwall_manager_curve1_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0f678 stg04, 0x00b08450 stg22 */
float en_perfectwall_manager_close_rot_curve(float t)
{
    return util_curve_eval(en_perfectwall_manager_curve2_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0f6d8 stg04, 0x00b084b0 stg22 */
float en_perfectwall_manager_unite_gap_curve(float t)
{
    return util_curve_eval(en_perfectwall_manager_curve3_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0f738 stg04, 0x00b08510 stg22 */
float en_perfectwall_manager_unite_rot_curve(float t)
{
    return util_curve_eval(en_perfectwall_manager_curve4_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0f798 stg04, 0x00b08570 stg22 */
float en_perfectwall_manager_rotate_curve(float t)
{
    return util_curve_eval(en_perfectwall_manager_curve5_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0f7f8 stg04, 0x00b085d0 stg22 */
void en_perfectwall_manager_state_close(EnPerfectWallManagerWork *w)
{
    float end, rate;

    switch ((unsigned int)w->cnt) {
    case 0:
        w->dang = -10922;
        w->rt = 1.0f;
        w->cnt_next++;
        w->dgap = -w->speed;
        w->cnt++;
        w->rot = 0;
        w->sstep = 0;
        w->rt_max = (int)((w->gap - w->gap_min) / w->speed);
    case 1:
        switch (w->sstep) {
        case 0:
            w->gap += w->dgap;
            if (w->gap <= w->gap_min) {
                w->gap = w->gap_min;
                w->rt = 1.0f;
                w->rt_max = 42.0f;
                w->sstep = 1;
                w->dgap = 0;
            }
            break;
        case 1:
            end = w->rt_max;
            rate = w->rt / end;
            w->gap -= w->dgap;
            w->dgap = -w->gap_min * en_perfectwall_manager_close_gap_curve(rate);
            w->wall_ang_z -= w->rot;
            w->gap += w->dgap;
            w->rot = w->dang * en_perfectwall_manager_close_rot_curve(rate);
            w->wall_ang_z += w->rot;
            w->rt += 1.0f;
            if (end < w->rt) {
                en_perfectwall_manager_lightning_create(w);
                w->rt = 1.0f;
                w->rt_max = fabsf(w->turn);
                do {
                    w->cnt_next = w->cnt + 1;
                } while (0);
            }
            break;
        }
        if (w->phase == w->next && w->cnt == w->cnt_next)
            break;
        w->cnt++;
    case 2:
        w->rt += 1.0f;
        if (w->rt_max < w->rt) {
            if (w->turn < 0.0f)
                w->flags |= 2;
            if (0.0f < w->gap_spd) {
                do {
                    w->next = 1;
                    w->cnt_next = 0;
                } while (0);
            } else {
                w->flags |= 2;
                do {
                    w->next = 3;
                    w->cnt_next = 0;
                } while (0);
            }
        }
        break;
    }
}

/* 0x00b0fa98 stg04, 0x00b08870 stg22 */
void en_perfectwall_manager_state_unite(EnPerfectWallManagerWork *w)
{
    float frm, end, rate, gap;

    switch ((unsigned int)w->cnt) {
    case 0:
        w->cnt++;
        w->dang = -w->wall_ang_z;
        w->rt = 1.0f;
        w->dgap_unite = -w->gap_min;
        w->cnt_next++;
        w->dgap = 0;
        w->rot = 0;
        w->rt_max = (int)(w->gap_min / w->gap_spd);
    case 1:
        frm = w->rt;
        end = w->rt_max;
        gap = w->gap;
        rate = frm / end;
        gap -= w->dgap;
        w->dgap = w->dgap_unite * en_perfectwall_manager_unite_gap_curve(rate);
        w->wall_ang_z -= w->rot;
        w->gap = gap + w->dgap;
        w->rot = w->dang * en_perfectwall_manager_unite_rot_curve(rate);
        w->wall_ang_z += w->rot;
        if (!(w->flags & 8) && end * 0.6f < frm) {
            w->result = 3;
            w->flags |= 8;
            SoundRhythmSe_Enemy(en_perfectwall_manager_init_arg, 4);
        }
        w->rt += 1.0f;
        if (end < w->rt) {
            w->flags |= 1;
            en_perfectwall_manager_lightning_finish(w);
            w->result = 5;
            do {
                w->next = 2;
                w->cnt_next = 0;
            } while (0);
        }
        if (w->phase != w->next || w->cnt != w->cnt_next)
            w->cnt++;
        break;
    case 2:
        break;
    }
}

/* 0x00b0fc90 stg04, 0x00b08a68 stg22 */
void en_perfectwall_manager_state_rotate(EnPerfectWallManagerWork *w)
{
    float rate;

    switch ((unsigned int)w->cnt) {
    case 2:
        break;
    case 0:
        w->rt = 1.0f;
        w->cnt++;
        w->cnt_next++;
        w->rot = 0;
        w->rt_max = (int)(EN_PERFECTWALL_MANAGER_REC->frm_rotate * FPS);
    case 1:
        w->ang[2] -= w->rot;
        rate = en_perfectwall_manager_rotate_curve(w->rt / w->rt_max);
        w->rot = w->yaw * rate;
        w->ang[2] += w->rot;
        w->rt += 1.0f;
        if (w->rt_max < w->rt) {
            w->flags |= 2;
            do {
                w->next = 3;
                w->cnt_next = 0;
            } while (0);
        }
        if (w->phase != w->next || w->cnt != w->cnt_next)
            w->cnt++;
        break;
    }
}

/* 0x00b0fdc8 stg04, 0x00b08ba0 stg22 */
void en_perfectwall_manager_charge(EnPerfectWallManagerWork *w)
{
    Vec3 ofs;

    if (w->flags & 2) {
        float dis;
        dis = w->dist + EN_PERFECTWALL_MANAGER_REC->acc_charge * 0.00027777778f;
        dis = w->dist = dis < w->dist_max ? dis : w->dist_max;
        ofs.z = dis;
        ofs.x = 0.0f;
        ofs.y = 0.0f;
        util_dir_to_player_frame(&ofs, &ofs);
        v3_add(&w->pos, &ofs);
        en_perfectwall_manager_collision(w);
    }
}

/* 0x00b0fe88 stg04, 0x00b08c60 stg22 */
void en_perfectwall_manager_exec(EnPerfectWallManagerWork *w)
{
    int n;
    Vec3 pl_vel;
    Task *child;
    int fl;
    void (*fn)(EnPerfectWallManagerWork *);

    n = enemy_status_get_ticks();
    w->result = 0;
    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    child = (Task *)TASK_OF(w)->child;
    if ((child ? child + 1 : 0) == 0) {
        FINISH_IF(w);
        return;
    }
    en_perfectwall_manager_lightning_check(w);
    fl = w->flags;
    if ((fl & 0x300) == 0x300 && (w->flags & 1))
        w->result = 6;
    if ((w->flags & 0x300) && !(w->flags & 1)) {
        w->result = 4;
        return;
    }
    while (n--) {
        do {
            w->phase = w->next;
            w->cnt = w->cnt_next;
        } while (0);
        en_perfectwall_manager_swing(w);
        en_perfectwall_manager_charge(w);
        fn = en_perfectwall_manager_states[w->phase];
        if (fn)
            fn(w);
    }
    en_perfectwall_manager_calc_points(w);
    en_perfectwall_manager_joint_timer(w);
}

/* 0x00b0fff0 stg04, 0x00b08dc8 stg22 */
void en_perfectwall_manager_init(EnPerfectWallManagerWork *w, EnPerfectWallPlace *place)
{
    EnPerfectWallPlace wall;
    int pang[4];
    int *ang = w->ang;

    v3_copy(&w->pos, &place->pos);
    util_ang_from_player_frame(place->ang, pang);
    w->wall_ang_z = 10922;
    w->yaw = pang[0];
    pang[0] = 0;
    util_ang_to_player_frame(pang, ang);
    w->gap = EN_PERFECTWALL_MANAGER_REC->dst_wall;
    w->speed = place->param0 * FRAME_SEC;
    w->gap_min = place->param1;
    w->turn = place->param2;
    w->gap_spd = place->param3 * FRAME_SEC;
    w->dist_max = place->param4 * FRAME_SEC;
    nj_copy8(&wall, place, 64);
    wall.pos.x += w->gap;
    wall.ang[0] = ang[0];
    wall.ang[1] = ang[1];
    wall.ang[2] = ang[2];
    wall.param0 = 0.0f;
    en_perfectwall_create(w, &wall);
    nj_copy8(&wall, place, 64);
    wall.pos.x -= w->gap;
    wall.ang[0] = ang[0];
    wall.ang[1] = ang[1];
    wall.ang[2] = ang[2];
    wall.param0 = 1.0f;
    en_perfectwall_create(w, &wall);
}

/* 0x00b10168 stg04, 0x00b08f40 stg22 */
void en_perfectwall_manager_release(void)
{
}

/* 0x00b10170 stg04, 0x00b08f48 stg22 */
void *en_perfectwall_manager_create(void *parent, void *prm)
{
    return task_create_arg(parent, &en_perfectwall_manager_desc, 212, prm);
}

/* 0x00b10198 stg04, 0x00b08f70 stg22 */
void en_perfectwall_manager_set_ends(EnPerfectWallManagerWork *w, int kind, Vec3 *src)
{
    Vec3 *dst = w->seg[kind];
    int k;

    for (k = 2; k != 0; k--) {
        Vec3 *pt = dst++;

        v3_copy(pt, src++);
    }
}

/* 0x00b101e8 stg04, 0x00b08fc0 stg22 */
void en_perfectwall_manager_wall_gone(EnPerfectWallManagerWork *w, int bit)
{
    w->flags |= 256 << bit;
    en_perfectwall_manager_lightning_finish(w);
}

/* 0x00b10208 stg04, 0x00b08fe0 stg22 */
void en_perfectwall_manager_get_point(EnPerfectWallManagerWork *w, int kind, Vec3 *pos, int *ang)
{
    i3_copy(ang, w->ang);
    v3_copy(pos, &w->pts[kind]);
}

/* 0x00b10250 stg04, 0x00b09028 stg22 */
void en_perfectwall_manager_get_angles(EnPerfectWallManagerWork *w, int kind, int *ang)
{
    switch (kind) {
    case 0:
        i3_set(ang, 0, ANG_180, w->wall_ang_z);
        break;
    case 1:
        i3_set(ang, 0, 0, w->wall_ang_z);
        break;
    }
}

/* 0x00b10290 stg04, 0x00b09068 stg22 */
int en_perfectwall_manager_get_state(EnPerfectWallManagerWork *w)
{
    return w->result;
}

/* 0x00b10298 stg04, 0x00b09070 stg22 */
int en_perfectwall_manager_is_united(EnPerfectWallManagerWork *w)
{
    return w->flags & 1;
}

/* 0x00b102a8 stg04, 0x00b09080 stg22 */
int en_perfectwall_manager_has_lightning(EnPerfectWallManagerWork *w)
{
    return w->flags & 4;
}

/* 0x00b102b8 stg04, 0x00b09090 stg22 */
int en_perfectwall_manager_side_score(EnPerfectWallManagerWork *w, int kind)
{
    int score = 0;

    switch (kind) {
    case 0:
        if (w->flags & 1) {
            if (w->flags & 0x200)
                score = en_perfectwall_manager_score_unite;
        } else
            score = en_perfectwall_manager_score_a;
        break;
    case 1:
        if (w->flags & 1) {
            if (w->flags & 0x100)
                score = en_perfectwall_manager_score_unite;
        } else
            score = en_perfectwall_manager_score_b;
        break;
    }
    return score;
}

/* 0x00b10328 stg04, 0x00b09100 stg22 */
void en_perfectwall_manager_se_hit(EnPerfectWallManagerWork *w, Vec3 *pos)
{
    Vec3 scr;

    ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &scr.x);
    project_point(&scr, &scr);
    if (en_perfectwall_manager_is_united(w))
        SoundRhythmSe_EnemyPos(en_perfectwall_manager_init_arg, 2, &scr.x);
    else
        SoundRhythmSe_EnemyPos(en_perfectwall_manager_init_arg, 0, &scr.x);
}

/* 0x00b103a0 stg04, 0x00b09178 stg22 */
void en_perfectwall_manager_se_break(EnPerfectWallManagerWork *w, Vec3 *pos)
{
    Vec3 scr;

    ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &scr.x);
    project_point(&scr, &scr);
    if (en_perfectwall_manager_is_united(w))
        {
        if (w->flags & 0x300)
            SoundRhythmSe_EnemyPos(en_perfectwall_manager_init_arg, 3, &scr.x);
        else
            SoundRhythmSe_EnemyPos(en_perfectwall_manager_init_arg, 2, &scr.x);
    }
    else
        SoundRhythmSe_EnemyPos(en_perfectwall_manager_init_arg, 1, &scr.x);
}

/* 0x00b10430 stg04, 0x00b09208 stg22 */
void en_perfectwall_manager_se_reset(void)
{
    SoundRhythmSe_Enemy(en_perfectwall_manager_init_arg, 5);
}
