/*
 * Mars boss bullets: lock-on shots (MB in a sweeping stream from the MBM task; NB, also in rings;
 * SB, homing, from the SBM task in the last phase) and shot-break effects (sparks, debris).
 */

#include "types_gw.h"
#include "b_mars.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars_bullet.h"
#include "mars.h"
#include "mars_color.h"
#include "mars_core.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

/* 140 bytes */
typedef struct MarsMbWork {
    void *pack;
    void *textures;
    ModelPart *mdl;
    ModelPart *break_mdl;
    Vec3 *src;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    int roll_spd;
    LockObj lock;
    char pad_7c[4];
    int timer;                          /* 0x80 */
    int timer_len;
    int *stop;
} MarsMbWork;

/* 56 bytes */
typedef struct MarsMbmWork {
    void *pack;
    void *textures;
    int mdl;
    int break_mdl;
    Vec3 *src;
    void *parent;
    int sweep;
    int sweep_len;
    int wait;
    int wait_len;
    int step;
    int step_len;
    int *stop;
    int up_ang;
} MarsMbmWork;

/* 184 bytes */
typedef struct MarsNbWork {
    void *pack;
    void *textures;
    ModelPart *mdl;
    ModelPart *break_mdl;
    char pad_10[4];
    Vec3 pos;                           /* 0x14 */
    Vec3 vel;
    Vec3 dir;
    Vec3 aim_vel;
    LockObj lock;
    int ang[3];
    int dang[3];
    int state;
    int timer;
    int timer_len;
    int kind;
    float spd_v;
    float spd;
    void *parent;
} MarsNbWork;

/* 336 bytes */
typedef struct MarsSbWork {
    void *pack;
    void *textures;
    ModelPart body;
    ModelPart shell;
    int break_mdl;
    Vec3 *src;
    Vec3 pos;
    Vec3 vel;
    LockObj lock;
    int ang[3];
    int dang[3];
    int state;
    int timer;
    int timer_len;
    int *stop;
    float wob_ph;
    void *owner;
} MarsSbWork;

/* 80 bytes */
typedef struct MarsSbmWork {
    void *pack;
    void *textures;
    int mdl;
    int mdl_b;
    int break_mdl;
    Vec3 *src;
    Vec3 last;
    Vec3 move;
    char *boss;
    int state;
    int left;
    int wait;
    int *stop;
    int boost_wait;
    int boost;
    int level;
} MarsSbmWork;

/* 132 bytes */
typedef struct MarsDebrisWork {
    void *pack;
    void *textures;
    ModelPart part;
    Vec3 pos;
    char pad_70[12];
    short fade;                         /* 0x7c */
    short fade_len;
    int end_frm;
} MarsDebrisWork;

extern char mars_bullet_vardef[];
extern GwVarDef mars_bullet_vardefs[];
extern struct GwVarNode *mars_bullet_vars_group;
extern BossV4 mars_b_br_verts[24];
extern struct TaskDesc mars_b_br_desc;
extern struct TaskDesc mars_br_l_desc;
extern struct TaskDesc mars_b_br_d_desc;
extern Vec3 mars_b_br_vtx_table[];
extern int mars_b_br_vtx_index[24];

typedef struct MarsBrLArg {      /* mars_br_l_create argument */
    Vec3 start;
    Vec3 span;                     /* 0x0c */
    char pad_18[24];
    int color;                      /* 0x30 */
    int k;                      /* 0x34 */
} MarsBrLArg;

extern float mars_br_l_sec;
extern int cmSBcol[2];
extern float CMD_NB_SCL;
extern Vec3 mars_br_l_line[2];

typedef struct MarsSparkWork {     /* short-lived spark, 64 bytes */
    Vec3 pos;                   /* 0x00 */
    Vec3 vel;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
    int dang[3];                /* 0x24 */
    unsigned int color;         /* 0x30 */
    int timer;                  /* 0x34: frames left */
    int total;                  /* 0x38 */
    float len;                  /* 0x3c */
} MarsSparkWork;

typedef struct MarsBurstWork {     /* up to six sparks moving together, 316 bytes */
    char pad_00[8];
    Vec3 pos[6];                /* 0x08 */
    Vec3 vel[6];                /* 0x50 */
    int ang[6][3];              /* 0x98 */
    int dang[6][3];             /* 0xe0 */
    unsigned int color;         /* 0x128 */
    int timer;                  /* 0x12c: frames left */
    int total;                  /* 0x130 */
    int n;                      /* 0x134 */
    float scale;                /* 0x138 */
} MarsBurstWork;

extern struct TaskDesc mars_mbm_desc;
extern struct TaskDesc mars_sbm_desc;
extern float CMD_MB_ST;
extern float CMD_MB_DANG;
extern int CMD_MB_NUM;
extern int CMD_MB_DNUM;
extern int mars_sb_counts[];
extern float CMD_SB_WT;
extern float CMD_SB_BS_TIME;
extern struct TaskDesc mars_nb_desc;
extern struct TaskDesc mars_mb_desc;
extern float CMD_NB_T;
extern float CMD_NB_RZ;
extern float CMD_MB_RZ;
extern float CMD_DAMAGE_FADE_TIME;
extern float CMD_SB_NV;
extern float CMD_MB_W;
extern float CMD_MB_T;
extern float CMD_NB_ATR;
extern float cmNbLevelParam[];
extern int CMD_NB_NUM;
extern float CMD_MB_R;
extern float CMD_MB_FT;
extern unsigned int cmMBcol;
extern float CMD_SB_W;
extern float CMD_SB_V;
extern float CMD_SB_T;
extern float CMD_SB_T1;
extern float cmSbLevelParam[];
extern struct TaskDesc mars_sb_desc;
extern float CMD_SB_RANG;
extern float CMD_SB_FR;
extern float CMD_SB_FT;
extern float CMD_SB_BT;
extern float CMD_SB_R;
extern float CMD_SB_SR;
extern unsigned int cmSBcol3;
extern float mars_b_br_spin;
extern float mars_br_ofs_z;
extern float mars_br_l_spin;
extern float CMD_NB_FR;
extern float CMD_NB_T1;
extern float CMD_NB_T2;
extern float CMD_NB_T3;
extern float CMD_NB_FT;
extern float CMD_NB_RZR;
extern float CMD_NB_R;
extern unsigned int cmNBcol;

/* 0x00ca4300 */
void mars_bullet_vars_add(void)
{
    mars_bullet_vars_group = gw_varedit_add(&gw_varedit_root, 0, mars_bullet_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_bullet_vars_group, mars_bullet_vardefs);
}

/* 0x00ca4350 */
void mars_bullet_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_bullet_vardef);
}

/* 0x00ca4368 */
void mars_mb_move(MarsMbWork *w)
{
    Vec3 pl_vec;

    player_get_vec(&pl_vec);
    v3_sum(&w->pos, &pl_vec, &w->pos);
    v3_sum(&w->pos, &w->vel, &w->pos);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-(&w->vel)->x, -(&w->vel)->z));
        w->ang[0] = RAD_TO_ANG(atan2f((&w->vel)->y, sqrtf((&w->vel)->x * (&w->vel)->x + (&w->vel)->z * (&w->vel)->z)));
    } while (0);
    w->ang[2] += w->roll_spd;
    if (--w->timer <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca44d8 */
void mars_mb_disp_fade(MarsMbWork *w)
{
    int save;
    float rgba[4];

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    ulMatrixPush();
    rgba[0] = (float)w->timer / (float)w->timer_len + -1.0f;
    rgba[1] = rgba[2] = rgba[3] = 0.0f;
    draw_set_override_color(rgba);
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(w->mdl);
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(save);
}

/* 0x00ca45d8 */
void mars_mb_exec(MarsMbWork *w)
{
    Vec3 pl_vec;

    int n;
    int v;

    player_get_vec(&pl_vec);
    n = enemy_status_get_ticks();
    v3_sum(&w->pos, &pl_vec, &w->pos);
    for (; n; n--) {
        v3_sum(&w->pos, &w->vel, &w->pos);
        w->ang[2] += w->roll_spd;
        if (w->timer)
            w->timer -= 1;
    }
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-(&w->vel)->x, -(&w->vel)->z));
        w->ang[0] = RAD_TO_ANG(atan2f((&w->vel)->y, sqrtf((&w->vel)->x * (&w->vel)->x + (&w->vel)->z * (&w->vel)->z)));
    } while (0);
    if (player_sphere_test(&w->pos, CMD_MB_R) & 1) {
        mars_player_damage();
        goto kill;
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
    kill:
        lock_score(5, &w->lock);
        v = (int)(CMD_MB_FT * FPS);
        w->timer = w->timer_len = v;
        boss_se_play(6, (int)&w->lock, (int)&w->pos);
        cm_wire_break_simple(w, 6, w->break_mdl, &w->pos, 0, w->ang, cmMBcol);
        FINISH_IF(w);
    } else if (w->timer <= 0 || *w->stop != 0) {
        TASK_OF(w)->exec = (void (*)(void *))mars_mb_move;
        v = (int)(CMD_MB_FT * FPS);
        w->timer = w->timer_len = v;
        TASK_OF(w)->disp = (void (*)(void *))mars_mb_disp_fade;
    }
}

/* 0x00ca4888 */
void mars_mb_disp(MarsMbWork *w)
{
    int save;
    void *fog;

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(0);
    fog = fog_set_enable(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(w->mdl);
    fog_set_enable(fog);
    ulMatrixPop();
    draw_set_override_flags(save);
}

/* 0x00ca4930 */
void mars_mb_release(MarsMbWork *w)
{
    lock_remove_entry(5, &w->lock);
}

/* 0x00ca4940 */
int *mars_mb_create(void *parent, Vec3 *src_pos, Vec3 *v, int mdl, int break_mdl, int end_flag, float t)
{
    MarsMbWork *w = task_create_inherit(parent, &mars_mb_desc, 140);
    Vec3 pl_vel;

    w->mdl = (ModelPart *)mdl;
    w->break_mdl = (ModelPart *)break_mdl;
    w->stop = (int *)end_flag;
    w->src = src_pos;
    w->roll_spd = (int)DEG_TO_ANG(CMD_MB_RZ);
    player_get_frame_vel(&pl_vel);
    v3_sub(&w->pos, src_pos, &pl_vel);
    w->timer = w->timer_len = (int)(t * FPS);
    v3_copy(&w->vel, v);
    lock_init(5, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    return (int *)w;
}

/* 0x00ca4a90 */
void mars_mbm_exec(MarsMbmWork *w)
{
    Vec3 pl_pos;
    Vec3 to_pl;
    Vec3 dir;
    Vec3 v;
    int n;

    for (n = enemy_status_get_ticks(); n != 0; n--) {
        if (--w->wait <= 0) {
            float k, inv_frames;

            w->wait = w->wait_len;
            player_get_pos(&pl_pos);
            v3_sub(&to_pl, &pl_pos, w->src);
            vec_up_for_dir(&to_pl, &dir, w->up_ang);
            k = (float)(w->sweep - 1) / (float)(w->sweep_len - 1) * -2.0f;
            v3_scale(&dir, CMD_MB_W);
            inv_frames = 1.0f / (CMD_MB_T * FPS);
            v.x = (to_pl.x + dir.x * k) * inv_frames;
            v.y = (to_pl.y + dir.y * k) * inv_frames;
            v.z = (to_pl.z + dir.z * k) * inv_frames;
            mars_mb_create(w->parent, w->src, &v, w->mdl, w->break_mdl, (int)w->stop, CMD_MB_T);
            if (--w->step <= 0) {
                w->step = w->step_len;
                if (--w->sweep <= 0)
                    w->sweep = w->sweep_len;
            }
        }
    }
    if (*w->stop)
        FINISH_IF(w);
}

/* 0x00ca4c78 */
int *mars_mbm_create(void *parent, int src_pos, int mdl, int break_mdl, int end_flag)
{
    MarsMbmWork *w = task_create_inherit(parent, &mars_mbm_desc, 56);
    int v;

    w->parent = parent;
    w->src = (Vec3 *)src_pos;
    w->mdl = mdl;
    w->break_mdl = break_mdl;
    v = (int)(CMD_MB_ST * FPS);
    w->wait = w->wait_len = v;
    w->sweep = w->sweep_len = CMD_MB_NUM;
    w->step = w->step_len = CMD_MB_DNUM;
    w->up_ang = (int)DEG_TO_ANG(CMD_MB_DANG * RAND_FLOAT_A() + 90.0f);
    w->stop = (int *)end_flag;
    return (int *)w;
}

/* 0x00ca4d98 */
void mars_nb_move(MarsNbWork *w)
{
    Vec3 pl_vec;

    player_get_vec(&pl_vec);
    v3_sum(&w->pos, &pl_vec, &w->pos);
    i3_sum(w->ang, w->ang, w->dang);
    v3_sum(&w->pos, &w->vel, &w->pos);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-(&w->vel)->x, -(&w->vel)->z));
        w->ang[0] = RAD_TO_ANG(atan2f((&w->vel)->y, sqrtf((&w->vel)->x * (&w->vel)->x + (&w->vel)->z * (&w->vel)->z)));
    } while (0);
    if (--w->timer <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca4f30 */
void mars_nb_disp_fade(MarsNbWork *w)
{
    int save;
    float rgba[4];

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    fog_set_enable(0);
    ulMatrixPush();
    rgba[0] = (float)w->timer / (float)w->timer_len + -1.0f;
    rgba[1] = rgba[2] = rgba[3] = 0.0f;
    draw_set_override_color(rgba);
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, CMD_NB_SCL, CMD_NB_SCL, CMD_NB_SCL);
    dc_object_draw(w->mdl);
    ulMatrixPop();
    fog_set_enable((void *)1);
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(save);
}

/* 0x00ca5058 */
void mars_nb_exec(MarsNbWork *w)
{
    Vec3 pl_vec;
    Vec3 aim;
    Vec3 pl;
    Vec3 step;
    float one;
    float ratio;
    int n;

    player_get_vec(&pl_vec);
    n = enemy_status_get_ticks();
    v3_sum(&w->pos, &pl_vec, &w->pos);
    switch (w->state) {
    case 0:
        for (; n; n--) {
            v3_sum(&w->pos, &w->vel, &w->pos);
            v3_scale(&w->vel, CMD_NB_FR);
            w->dang[2] = (float)w->dang[2] * CMD_NB_RZR;
            i3_sum(w->ang, w->ang, w->dang);
            if (w->timer)
                w->timer -= 1;
        }
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-(&w->vel)->x, -(&w->vel)->z));
            w->ang[0] = RAD_TO_ANG(atan2f((&w->vel)->y, sqrtf((&w->vel)->x * (&w->vel)->x + (&w->vel)->z * (&w->vel)->z)));
        } while (0);
        if (w->timer <= 0) {
            w->timer = w->timer_len = (int)(CMD_NB_T1 * FPS);
            w->state = 1;
        }
        break;
    case 1:
        for (; n; n--) {
            v3_sum(&w->pos, &w->vel, &w->pos);
            v3_scale(&w->vel, CMD_NB_FR);
            i3_sum(w->ang, w->ang, w->dang);
            if (w->timer)
                w->timer -= 1;
        }
        one = 1.0f;
        player_get_pos(&aim);
        v3_sub(&aim, &aim, &w->pos);
        vec3_normalize(&aim, &aim);
        ratio = (float)w->timer / (float)w->timer_len;
        v3_lerp(&aim, &w->dir, &aim, ratio);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-aim.x, -aim.z));
            w->ang[0] = RAD_TO_ANG(atan2f(aim.y, sqrtf(aim.x * aim.x + aim.z * aim.z)));
        } while (0);
        if (w->timer <= 0) {
            w->timer = w->timer_len = (int)(CMD_NB_T3 * FPS);
            w->state = 2;
            player_get_pos(&pl);
            v3_sub(&w->aim_vel, &pl, &w->pos);
            v3_scale(&w->aim_vel, one / (CMD_NB_T2 * FPS));
            w->spd_v = 0.208f;
        }
        break;
    case 2:
        for (; n; n--) {
            v3_sum(&w->pos, &w->vel, &w->pos);
            v3_scale(&w->vel, CMD_NB_FR);
            spring_to(&w->spd_v, &w->spd, 1.0f, 0.12f, 0.014f);
            v3_mul(&step, &w->aim_vel, w->spd);
            v3_sum(&w->pos, &step, &w->pos);
            i3_sum(w->ang, w->ang, w->dang);
            if (w->timer)
                w->timer -= 1;
        }
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-(&w->aim_vel)->x, -(&w->aim_vel)->z));
            w->ang[0] = RAD_TO_ANG(atan2f((&w->aim_vel)->y, sqrtf((&w->aim_vel)->x * (&w->aim_vel)->x + (&w->aim_vel)->z * (&w->aim_vel)->z)));
        } while (0);
        if (w->timer <= 0) {
            v3_copy(&w->vel, &w->aim_vel);
            TASK_OF(w)->exec = (void (*)(void *))mars_nb_move;
            TASK_OF(w)->disp = (void (*)(void *))mars_nb_disp_fade;
            w->timer = w->timer_len = (int)(CMD_NB_FT * FPS);
        }
        break;
    }
    if (player_sphere_test(&w->pos, CMD_NB_R) & 1) {
        mars_player_damage();
        goto kill;
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
    kill:
        lock_score(7, &w->lock);
        if (mars_debris_bounded())
            cm_wire_break_set_cb(cm_wire_break_simple(w->parent, 13, w->break_mdl, &w->pos, 0, w->ang, cmNBcol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
        else
            cm_wire_break_simple(w->parent, 5, w->break_mdl, &w->pos, 0, w->ang, cmNBcol);
        FINISH_IF(w);
        boss_se_play(21, (int)&w->lock, (int)&w->pos);
    }
}

/* 0x00ca5900 */
void mars_nb_disp(MarsNbWork *w)
{
    int save;

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(0);
    fog_set_enable(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, CMD_NB_SCL, CMD_NB_SCL, CMD_NB_SCL);
    dc_object_draw(w->mdl);
    ulMatrixPop();
    fog_set_enable((void *)1);
    draw_set_override_flags(save);
}

/* 0x00ca59b8 */
void mars_nb_release(MarsNbWork *w)
{
    lock_remove_entry(7, &w->lock);
}

/* 0x00ca59c8 */
int *mars_nb_create(void *parent, Vec3 *pos, Vec3 *dir, int mdl, int break_mdl, int kind)
{
    MarsNbWork *w = task_create_inherit(parent, &mars_nb_desc, 184);

    v3_copy(&w->pos, pos);
    w->mdl = (ModelPart *)mdl;
    w->break_mdl = (ModelPart *)break_mdl;
    w->parent = parent;
    w->kind = kind;
    v3_copy(&w->vel, dir);
    vec3_normalize(&w->dir, dir);
    w->timer = w->timer_len = (int)(CMD_NB_T * FPS);
    w->dang[2] = (int)DEG_TO_ANG(CMD_NB_RZ);
    lock_init(7, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    return (int *)w;
}

/* 0x00ca5b10 */
void mars_nb_fire_ring(void *parent, Vec3 *pos, int mdl, int break_mdl, int kind)
{
    Vec3 pl_pos;
    Vec3 pl_vec;
    Vec3 to_pl;
    Vec3 v;
    float k;
    int ang_ofs;
    int i;

    player_get_pos(&pl_pos);
    player_get_vec(&pl_vec);
    v3_sub(&to_pl, &pl_pos, pos);
    if (to_pl.x * to_pl.x + to_pl.z * to_pl.z < CMD_NB_ATR * CMD_NB_ATR)
        return;
    k = cmNbLevelParam[mars_get_level((int)parent)];
    v3_sub(&pl_pos, pos, &pl_vec);
    ang_ofs = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f);
    for (i = 0; i < CMD_NB_NUM; i++) {
        vec_up_for_dir(&to_pl, &v, (int)DEG_TO_ANG((float)i * 360.0f / (float)CMD_NB_NUM) + ang_ofs);
        v3_scale(&v, k / (CMD_NB_T * FPS));
        mars_nb_create(parent, &pl_pos, &v, mdl, break_mdl, kind);
    }
    boss_se_play(20, 0, (int)&pl_pos);
}

/* 0x00ca5d70 */
void mars_sb_home(MarsSbWork *w)
{
    Vec3 pl_pos;
    Vec3 pl_vec;
    Vec3 to_pl;

    player_get_vec(&pl_vec);
    player_get_pos(&pl_pos);
    v3_sum(&w->pos, &pl_vec, &w->pos);
    i3_sum(w->ang, w->ang, w->dang);
    v3_sum(&w->pos, &w->vel, &w->pos);
    v3_sub(&to_pl, &pl_pos, &w->pos);
    vec_set_length(&to_pl, CMD_SB_NV);
    v3_add(&w->pos, &to_pl);
    if (--w->timer <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca5ee0 */
void mars_sb_disp_fade(MarsSbWork *w)
{
    int save;
    float rgba[4];
    unsigned int col[2];

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    fog_set_enable(0);
    ulMatrixPush();
    rgba[0] = (float)w->timer / (float)w->timer_len + -1.0f;
    rgba[1] = rgba[2] = rgba[3] = 0.0f;
    draw_set_override_color(rgba);
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    dc_object_draw((struct MapPart *)&w->body);
    col[0] = (cmSBcol[0] & 0xffffff) | (color_get_a(cmSBcol[0]) * w->timer / w->timer_len + 1) >> 1 << 24;
    col[1] = (cmSBcol[1] & 0xffffff) | (color_get_a(cmSBcol[1]) * w->timer / w->timer_len + 1) >> 1 << 24;
    part_draw_beat_frame(&w->shell, w->shell.frame, col[0], col[1], 2.0f);
    ulMatrixPop();
    fog_set_enable((void *)1);
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(save);
}

/* 0x00ca60a0 */
void mars_sb_exec(MarsSbWork *w)
{
    Vec3 pl_pos;
    Vec3 pl_vec;
    Vec3 wob;
    Vec3 to_pl;
    int n, v;

    player_get_vec(&pl_vec);
    player_get_pos(&pl_pos);
    v3_sum(&w->pos, &pl_vec, &w->pos);
    for (n = enemy_status_get_ticks(); n != 0; n--) {
        wobble_step_slow(&wob, CMD_SB_SR, &w->wob_ph);
        v3_add(&w->pos, &wob);
        v3_sub(&to_pl, &pl_pos, &w->pos);
        vec_set_length(&to_pl, CMD_SB_NV);
        switch (w->state) {
        case 0:
            v3_sum(&w->pos, &w->vel, &w->pos);
            v3_scale(&w->vel, CMD_SB_FR);
            i3_sum(w->ang, w->ang, w->dang);
            v3_add(&w->pos, &to_pl);
            if (--w->timer <= 0) {
                boss_se_play(23, (int)&w->lock, (int)&w->pos);
                v = (int)(CMD_SB_BT * FPS);
                w->state = 1;
                w->timer = w->timer_len = v;
            }
            break;
        case 1:
            i3_sum(w->ang, w->ang, w->dang);
            dc_object_advance_time((struct MapPart *)&w->body);
            dc_object_advance_time((struct MapPart *)&w->shell);
            v3_sum(&w->pos, &w->vel, &w->pos);
            v3_scale(&w->vel, CMD_SB_FR);
            w->timer -= 1;
            v3_add(&w->pos, &to_pl);
            if (w->timer <= 0) {
                TASK_OF(w)->exec = (void (*)(void *))mars_sb_home;
                TASK_OF(w)->disp = (void (*)(void *))mars_sb_disp_fade;
                v = (int)(CMD_SB_FT * FPS);
                w->timer = w->timer_len = v;
            }
            break;
        }
    }
    if (player_sphere_test(&w->pos, CMD_SB_R) & 1) {
        mars_player_damage();
        goto kill;
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
    kill:
        lock_score(6, &w->lock);
        if (w->state == 0)
            boss_se_play(24, (int)&w->lock, (int)&w->pos);
        else
            boss_se_play(25, (int)&w->lock, (int)&w->pos);
        cm_wire_break_simple(w->owner, 13, ((ModelPart *)w->break_mdl), &w->pos, 0, w->ang, cmSBcol3);
        FINISH_IF(w);
    } else if (*w->stop) {
        void (*home_fn)(void *) = (void (*)(void *))mars_sb_home;

        TASK_OF(w)->disp = (void (*)(void *))mars_sb_disp_fade;
        TASK_OF(w)->exec = home_fn;
        v = (int)(CMD_SB_FT * FPS);
        w->timer = w->timer_len = v;
    }
}

/* 0x00ca6540 */
void mars_sb_disp(MarsSbWork *w)
{
    int save;

    ulGxSetTextureList(w->textures);
    save = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    mars_bullet_rotate_yxz(w->ang);
    dc_object_draw((struct MapPart *)&w->body);
    part_draw_beat_frame(&w->shell, w->shell.frame, cmSBcol[0], cmSBcol[1], 2.0f);
    ulMatrixPop();
    draw_set_override_flags(save);
}

/* 0x00ca65e0 */
void mars_sb_release(MarsSbWork *w)
{
    lock_remove_entry(6, &w->lock);
}

/* 0x00ca65f0 */
void *mars_sb_create(void *parent, void *owner, Vec3 *pos, Vec3 *v, ModelPart *mdl, ModelPart *mdl_b, int break_mdl, int *end_flag, float spd)
{
    MarsSbWork *w = task_create_inherit(parent, &mars_sb_desc, 336);
    Vec3 pl_vel;
    int k;

    w->owner = owner;
    w->body = *mdl;
    w->shell = *mdl_b;
    w->break_mdl = break_mdl;
    w->src = pos;
    w->stop = end_flag;
    player_get_frame_vel(&pl_vel);
    v3_sub(&w->pos, pos, &pl_vel);
    v3_copy(&w->vel, v);
    do {
        w->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (CMD_SB_RANG + CMD_SB_RANG) - CMD_SB_RANG);
        w->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (CMD_SB_RANG + CMD_SB_RANG) - CMD_SB_RANG);
    } while (0);
    do {
        w->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (CMD_SB_RANG + CMD_SB_RANG) - CMD_SB_RANG);
    } while (0);
    do {
        do {
            w->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        } while (0);
    } while (0);
    w->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    w->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    w->wob_ph = (RAND_FLOAT_A() * 2.0f - 1.0f) * 2.0f * PI;
    k = (int)(spd * FPS);
    w->timer = w->timer_len = k;
    lock_init(6, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    return w;
}

/* 0x00ca6ac8 */
void mars_sbm_exec(MarsSbmWork *w)
{
    Vec3 pl_pos;
    Vec3 shot_vel;
    Vec3 dir;
    int n;
    float spd;
    MarsWork *boss;

    player_get_pos(&pl_pos);
    v3_sub(&w->move, w->src, &w->last);
    v3_copy(&w->last, w->src);
    for (n = enemy_status_get_ticks(); n != 0; n--) {
        if (w->boost_wait && --w->boost_wait <= 0)
            w->boost = 1;
        switch (w->state) {
        case 0:
            if (--w->wait > 0)
                break;
            if (w->left & 1) {
                boss = (MarsWork *)w->boss;
                mars_c_pulse(boss->core);
                mars_vcc_start(boss->vcc, mars_c_get_pos(boss->core), 0);
                w->wait = (int)(CMD_SB_T * FPS / (float)(mars_sb_counts[mars_get_level((int)boss)] - 1));
            }
            spd = CMD_SB_T1 + CMD_SB_T1 * 0.2 * (RAND_FLOAT_A() * 2.0f - 1.0f);
            v3_sub(&shot_vel, &pl_pos, w->src);
            vec_up_for_dir(&shot_vel, &dir, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f));
            v3_scale(&dir, RAND_FLOAT_A() * CMD_SB_W);
            shot_vel.x = (shot_vel.x + dir.x) * CMD_SB_V;
            shot_vel.y = (shot_vel.y + dir.y) * CMD_SB_V;
            shot_vel.z = (shot_vel.z + dir.z) * CMD_SB_V;
            mars_sb_create(w, w->boss, w->src, &shot_vel, (ModelPart *)w->mdl, (ModelPart *)w->mdl_b, w->break_mdl, w->stop, spd);
            boss_se_play(22, 0, (int)w->src);
            if (--w->left <= 0) {
                int i = mars_get_level((int)w->boss);

                w->left = mars_sb_counts[i];
                w->state = 1;
                w->wait = (int)(cmSbLevelParam[w->level + w->boost] * FPS);
            }
            break;
        case 1:
            if (--w->wait <= 0)
                w->state = 0;
            break;
        }
    }
    if (*w->stop)
        FINISH_IF(w);
}

/* 0x00ca6ee8 */
int *mars_sbm_create(void *parent, Vec3 *pos, int mdl, int mdl_b, int break_mdl, int end_flag)
{
    MarsSbmWork *w = task_create_inherit(parent, &mars_sbm_desc, 80);

    w->mdl = mdl;
    w->break_mdl = break_mdl;
    w->mdl_b = mdl_b;
    w->boss = parent;
    w->src = pos;
    v3_copy(&w->last, pos);
    w->left = mars_sb_counts[mars_get_level((int)parent)];
    w->state = 1;
    w->wait = (int)(CMD_SB_WT * FPS);
    w->level = mars_get_level((int)parent);
    w->stop = (int *)end_flag;
    w->boost_wait = (int)(CMD_SB_BS_TIME * FPS);
    return (int *)w;
}

/* 0x00ca7008 */
void mars_b_br_verts_build(BossV4 *dst)
{
    int i;

    for (i = 0; i < 24; i++, dst++) {
        Vec3 *src = &mars_b_br_vtx_table[mars_b_br_vtx_index[i]];

        dst->x = src->x;
        dst->y = src->y;
        dst->z = src->z;
    }
}

/* 0x00ca7060 */
void mars_b_br_verts_init(void)
{
    mars_b_br_verts_build(mars_b_br_verts);
}

/* 0x00ca7070 */
void mars_b_br_init(MarsBurstWork *w, int *src)
{
    Vec3 ofs;
    float sc[2][2];
    int i;

    w->timer = w->total = ((short *)src)[13];
    w->n = ((short *)src)[12] % 7;
    w->scale = ((float *)src)[5];
    for (i = 0; i < w->n; i++) {
        if (src[0]) {
            v3_copy(&w->pos[i], (Vec3 *)src[0]);
        } else {
            v3_set(&w->pos[i], 0.0f, 0.0f, 0.0f);
        }
        if (src[1]) {
            v3_copy(&w->vel[i], (Vec3 *)src[1]);
        } else {
            v3_set(&w->vel[i], 0.0f, 0.0f, 0.0f);
        }
        if (src[2])
            i3_copy(w->ang[i], (int *)src[2]);
        else
            i3_set(w->ang[i], 0, 0, 0);
        w->color = src[4];
        ofs.z = mars_br_ofs_z;
        ofs.x = ofs.y = 0.0f;
        ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc[0], ofs.y, ofs.z);
        ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc[1], ofs.x, ofs.z);
        do {
            v3_sum(&w->vel[i], &w->vel[i], &ofs);
        } while (0);
        w->dang[i][0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_b_br_spin - mars_b_br_spin);
        w->dang[i][1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_b_br_spin - mars_b_br_spin);
        w->dang[i][2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_b_br_spin - mars_b_br_spin);
    }
}

/* 0x00ca7478 */
void mars_b_br_exec(MarsBurstWork *w)
{
    Vec3 pl_vec;
    int i;

    if (--w->timer <= 0) {
        FINISH_IF(w);
    } else {
        player_get_vec(&pl_vec);
        for (i = 0; i < w->n; i++) {
            v3_sum(&w->pos[i], &pl_vec, &w->pos[i]);
            v3_sum(&w->pos[i], &w->vel[i], &w->pos[i]);
            i3_sum(w->ang[i], w->dang[i], w->ang[i]);
        }
    }
}

/* 0x00ca7570 */
void mars_b_br_disp(MarsBurstWork *w)
{
    int save = draw_set_override_flags(0);
    int i;
    unsigned int col;
    TrailVtx *v;

    draw_set_blend(4, 1);
    flat_strip_begin(1);
    for (i = 0; i < w->n; i++) {
        ulMatrixPush();
        MTX_TRANS(&w->pos[i]);
        mars_bullet_rotate_yxz(w->ang[i]);
        vu0_matrix_scale(ulMatrixCur, w->scale, w->scale, w->scale);
        v = (TrailVtx *)&mars_b_br_verts[i * 4];
        col = w->color & 0xffffff;
        col |= color_get_a(w->color) * w->timer / w->total << 24;
        v[0].c = v[1].c = v[2].c = v[3].c = col;
        poly_draw_flat(v, 4);
        ulMatrixPop();
    }
    flat_strip_end();
    draw_set_override_flags(save);
}

/* 0x00ca76d0 */
void *mars_b_br_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_b_br_desc, 316, arg);
}

/* 0x00ca76f8 */
void mars_br_l_init(MarsSparkWork *w, MarsBrLArg *src)
{
    Vec3 ofs;
    float sc[2][2];

    v3_scale(&src->span, 0.5f);
    w->timer = w->total = src->k;
    w->color = src->color;
    w->len = vec3_length(&src->span);
    v3_sum(&w->pos, &src->span, &src->start);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-src->span.x, -src->span.z));
        w->ang[0] = RAD_TO_ANG(atan2f(src->span.y, sqrtf(src->span.x * src->span.x + src->span.z * src->span.z)));
    } while (0);
    ofs.z = mars_br_ofs_z;
    ofs.x = ofs.y = 0.0f;
    ROT_X((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc[0], ofs.y, ofs.z);
    ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc[1], ofs.x, ofs.z);
    v3_add(&w->vel, &ofs);
    w->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_br_l_spin - mars_br_l_spin);
    w->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_br_l_spin - mars_br_l_spin);
    w->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 2.0f * mars_br_l_spin - mars_br_l_spin);
}

/* 0x00ca7a58 */
void mars_br_l_exec(MarsSparkWork *w)
{
    Vec3 pl_vec;

    if (--w->timer <= 0) {
        FINISH_IF(w);
    } else {
        player_get_vec(&pl_vec);
        v3_sum(&w->pos, &w->vel, &w->pos);
        i3_sum(w->ang, w->dang, w->ang);
        v3_sum(&w->pos, &pl_vec, &w->pos);
    }
}

/* 0x00ca7b38 */
void mars_br_l_disp(MarsSparkWork *w)
{
    unsigned int col;

    draw_set_blend(4, 1);
    col = w->color & 0xffffff;
    col |= color_get_a(w->color) * w->timer / w->total << 24;
    ulMatrixPush();
    MTX_TRANSF(w->pos.x, w->pos.y, w->pos.z);
    mars_bullet_rotate_yxz(w->ang);
    vu0_matrix_scale(ulMatrixCur, 1.0f, 1.0f, w->len);
    lines_begin(0.5f, col, 1);
    lines_add_pairs(mars_br_l_line, 2);
    lines_end();
    ulMatrixPop();
}

/* 0x00ca7c18 */
void *mars_br_l_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_br_l_desc, 64, arg);
}

/* 0x00ca7c40 */
void mars_br_l_spawn(void *parent, Vec3 *start, Vec3 *span, int col)
{
    MarsBrLArg arg;

    arg.k = (int)(mars_br_l_sec * FPS);
    arg.start.x = start->x;
    arg.start.y = start->y;
    arg.start.z = start->z;
    arg.span.x = span->x;
    arg.span.y = span->y;
    arg.span.z = span->z;
    arg.color = col;
    mars_br_l_create(parent, &arg);
}

/* 0x00ca7cb0 */
void mars_b_br_d_init(MarsDebrisWork *w, void **arg)
{
    int v;

    v3_copy(&w->pos, (Vec3 *)arg[0]);
    w->part = *(ModelPart *)arg[3];
    v = (int)(CMD_DAMAGE_FADE_TIME * FPS);
    w->end_frm = (int)w->part.frames;
    w->part.speed *= 0.5f;
    w->fade_len = w->fade = v;
}

/* 0x00ca7e08 */
void mars_b_br_d_exec(MarsDebrisWork *w)
{
    Vec3 pl_vec;

    player_get_vec(&pl_vec);
    v3_add(&w->pos, &pl_vec);
    if (w->part.frame < (float)w->end_frm)
        dc_object_advance_time((struct MapPart *)&w->part);
    else if (--w->fade <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00ca7eb8 */
void mars_b_br_d_disp(MarsDebrisWork *w)
{
    int save;
    float rgba[4];

    if (view_sphere_visible(&w->pos, 10.0f)) {
        ulMatrixPush();
        save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
        rgba[0] = (float)w->fade / (float)w->fade_len;
        rgba[1] = rgba[2] = rgba[3] = 1.0f;
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(rgba);
        MTX_TRANS(&w->pos);
        ulMatrixUnitRotation((volatile float *)ulMatrixCur);
        dc_object_draw((struct MapPart *)&w->part);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(save);
        ulMatrixPop();
    }
}

/* 0x00ca7fc8 */
void *mars_b_br_d_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_b_br_d_desc, 132, arg);
}

/* 0x00ca7ff0 */
void mars_bullet_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
