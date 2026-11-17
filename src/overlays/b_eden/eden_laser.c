/*
 * Eden boss mirror lasers: a wide beam between two points that sweeps end to end and hurts on
 * contact. Also the score table setup and debug-editor callbacks for the map fog keys and passes.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "gw_varedit.h"
#include "lens.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "eden_laser.h"
#include "eden.h"
#include "eden_map.h"
#include "../common/boss_common.h"

extern char eden_laser_vardef[];
extern GwVarDef eden_laser_vardefs[];
extern struct GwVarNode *eden_laser_vars_group;
extern char eden_score_vardef[];
extern int cmd_ch_map_num;
extern int cmd_ch_map_pass_type_num;
extern int cmd_ch_map_pass_seq_num;
extern int cmd_ch_map_pass_sw;
extern int eden_score_vars_group;
extern GwVarDef eden_score_vardefs[];
extern BossLockDef eden_score_lock_defs[];
extern int cmd_emap_num;
extern unsigned int cmd_map_col;
extern int cmd_map_m1_col, cmd_map_m2_col, cmd_map_m3_col;
extern int cmd_map_m1_bcol, cmd_map_m2_bcol, cmd_map_m3_bcol;
extern float cmd_map_near, cmd_map_far;
extern unsigned int cm_laser_color0[3], cm_laser_color10[3];
extern float cmd_beat_w, eden_laser_strip_h;
typedef struct EdenBar { unsigned int col[3]; char pad_0c[4]; float width, h; } EdenBar;
extern struct TaskDesc eden_laser_desc;
extern float cmd_laser_w;
extern float cmd_laser_wt;
extern float cmd_laser_mt;
extern float cmd_laser_wt_f;
extern float cmd_laser_mt_f;
extern float cmd_laser2_w;
extern float cmd_laser_dw, cmd_laser_wcy;

/* a mirror's laser between two points, 148 bytes */
typedef struct EdenLaserWork {
    struct PnbHead *pack;
    void *tex_list;
    void *parent;
    int parts;
    char pad_10[4];
    int end_time;               /* 0x14: the far end sweeps from from to to */
    int end_len;
    int start_time;             /* then the near end follows */
    int start_len;
    int wait;
    int hit;                    /* the player was hit once already */
    int fixed;                  /* the near end stays on from */
    int beat;
    int beat_time;
    char pad_38[4];
    Vec3 *from;                 /* 0x3c */
    Vec3 *to;
    Vec3 pts[2];
    Vec3 player;
    char pad_68[24];
    int ang;                    /* 0x80 */
    float width;
    float cur_width;
    float phase;
    float shimmer;
} EdenLaserWork;

/* 0x00cb6058 */
void eden_laser_vars_add(void)
{
    eden_laser_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_laser_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_laser_vars_group, eden_laser_vardefs);
}

/* 0x00cb60a8 */
void eden_laser_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_laser_vardef);
}

/* 0x00cb60c0 */
void eden_laser_exec(EdenLaserWork *w)
{
    Vec3 hit;
    float sc[2];

    player_get_pos(&w->player);
    if (!w->fixed) {
        if (w->wait) {
            w->wait--;
            v3_copy(w->pts, w->from);
        } else {
            if (w->start_time)
                w->start_time--;
            v3_lerp(w->pts, w->from, w->to, (float)w->start_time / (float)w->start_len);
        }
    } else
        v3_copy(w->pts, w->from);
    if (w->end_time) {
        w->end_time--;
        v3_lerp(&w->pts[1], w->from, w->to, (float)w->end_time / (float)w->end_len);
    } else
        v3_copy(&w->pts[1], w->to);
    w->phase += cmd_laser_wcy;
    ulSinCosf(w->phase, &sc[0], &sc[1]);
    w->cur_width = w->width + cmd_laser_dw * sc[0];
    w->shimmer = sc[0];
    if (w->beat && w->beat_time)
        w->beat_time--;
    if (!w->hit && segment_point_near(w->pts, &w->pts[1], &w->player, &hit, w->cur_width)) {
        eden_player_damage();
        w->hit = 1;
    }
    if (!w->wait && !w->start_time && !w->end_time)
        FINISH_IF(w);
}

/* 0x00cb6370 */
void eden_laser_disp(EdenLaserWork *w)
{
    EdenBar bar;
    float old_swap;

    ulGxSetTextureList(w->tex_list);
    old_swap = polyline_set_width_min(0.0f);
    draw_set_blend(4, 1);
    bar.col[0] = color_lerp(cm_laser_color0[0], cm_laser_color10[0], w->shimmer);
    bar.col[1] = color_lerp(cm_laser_color0[1], cm_laser_color10[1], w->shimmer);
    bar.col[2] = color_lerp(cm_laser_color0[2], cm_laser_color10[2], w->shimmer);
    if (w->beat)
        bar.width = w->cur_width * (cmd_beat_w * beat_curve(1) + 1.0f);
    else
        bar.width = w->cur_width;
    bar.h = eden_laser_strip_h;
    draw_polyline_wide_capped((int)&bar, w->pts, 1);
    polyline_set_width_min(old_swap);
}

/* 0x00cb6478 */
void eden_laser_release(Earth1stDemoWork *w)
{
}

/* 0x00cb6480 */
EdenLaserWork *eden_laser_create(void *parent, int sub, int tgt, int owner, int arg, int kind)
{
    EdenLaserWork *w = task_create_inherit(parent, &eden_laser_desc, 148);

    w->parts = arg;
    w->parent = parent;
    w->from = (Vec3 *)sub;
    if (owner)
        w->to = (Vec3 *)owner;
    else
        w->to = &w->player;
    w->ang = tgt;
    switch (kind) {
    case 0: {
        int frames, u;
        float width;

        frames = (int)(cmd_laser_mt_f * FPS);
        width = cmd_laser_w;
        u = (int)(cmd_laser_wt_f * FPS);
        w->hit = 1;
        w->fixed = 0;
        w->width = width;
        w->wait = u;
        w->start_time = w->start_len = w->end_time = w->end_len = frames;
        break;
    }
    case 1: {
        int frames, u;
        float width;

        frames = (int)(cmd_laser_mt * FPS);
        width = cmd_laser2_w;
        u = (int)(cmd_laser_wt * FPS);
        w->fixed = 0;
        w->width = width;
        w->wait = u;
        w->start_time = w->start_len = w->end_time = w->end_len = frames;
        break;
    }
    case 2: {
        int frames, u;
        float width;

        frames = (int)(cmd_laser_mt * FPS);
        width = cmd_laser_w;
        u = (int)(cmd_laser_wt * FPS);
        w->fixed = 1;
        w->width = width;
        w->wait = u;
        w->start_time = w->start_len = w->end_time = w->end_len = frames;
        break;
    }
    }
    return w;
}

/* 0x00cb6618 */
void eden_laser_stop(EdenLaserWork *w)
{
    w->fixed = 0;
    w->wait = 0;
}

/* 0x00cb6628 */
void eden_laser_fire(EdenLaserWork *w)
{
    w->beat = 1;
    w->beat_time = 60;
}

/* steer(cast) */
/* 0x00cb6640 */
void eden_score_vars_add(void)
{
    lock_table_set(eden_score_lock_defs, 19);
    eden_score_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, eden_score_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_score_vars_group, eden_score_vardefs);
}

/* 0x00cb66a0 */
void eden_score_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_score_vardef);
}

/* 0x00cb66b8 */
void eden_map_edit_fog_load(void)
{
    int i = cmd_emap_num;

    cmd_map_near = FOGKEY[i * 3].near;
    cmd_map_far = FOGKEY[i * 3].far;
    cmd_map_col = FOGKEY[i * 3].col;
    cmd_map_m1_col = FOGKEY[i * 3].m_col;
    cmd_map_m1_bcol = FOGKEY[i * 3].m_bcol;
    cmd_map_m2_col = FOGKEY[i * 3 + 1].m_col;
    cmd_map_m2_bcol = FOGKEY[i * 3 + 1].m_bcol;
    cmd_map_m3_col = FOGKEY[i * 3 + 2].m_col;
    cmd_map_m3_bcol = FOGKEY[i * 3 + 2].m_bcol;
}

/* 0x00cb67b8 */
void eden_map_edit_fog_store(void)
{
    int i = cmd_emap_num;

    FOGKEY[i * 3].col = cmd_map_col;
    FOGKEY[i * 3].m_col = cmd_map_m1_col;
    FOGKEY[i * 3].near = cmd_map_near;
    FOGKEY[i * 3].far = cmd_map_far;
    FOGKEY[i * 3].m_bcol = cmd_map_m1_bcol;
    FOGKEY[i * 3 + 1].m_col = cmd_map_m2_col;
    FOGKEY[i * 3 + 1].m_bcol = cmd_map_m2_bcol;
    FOGKEY[i * 3 + 2].m_col = cmd_map_m3_col;
    FOGKEY[i * 3 + 2].m_bcol = cmd_map_m3_bcol;
}

/* 0x00cb6888 */
void eden_map_edit_fog_cb(void)
{
    eden_map_fog_change(eden_map_task, cmd_ch_map_num);
}

/* 0x00cb68a0 */
void eden_map_edit_pass_cb(void)
{
    cmd_ch_map_pass_sw = 0;
    eden_map_pass_change(eden_map_task, cmd_ch_map_pass_type_num, cmd_ch_map_pass_seq_num);
}
