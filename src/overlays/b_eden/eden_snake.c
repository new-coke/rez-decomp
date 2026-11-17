/*
 * Eden boss snake form: a head with spring-following segments, each with a ring of piece cells;
 * the human variant poses a wire figure, the orbit variant spins rings around the boss. All fire
 * UB. Also the sound helpers and texture-animation wrappers.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "anim.h"
#include "debug.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "player.h"
#include "pnb.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_snake.h"
#include "eden.h"
#include "eden_bullet.h"
#include "eden_core.h"
#include "eden_map.h"
#include "eden_piece.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"

extern BossSe se_table;
extern char eden_snake_vardef[];
extern GwVarDef eden_snake_vardefs[];
extern struct GwVarNode *eden_snake_vars_group;
extern BossVolDef vol_table[];
extern int adx_files[2][2];
extern float eden_sound_tempos[];
extern char eden_sound_seq_files[][8];
extern char eden_sound_seq_file[];
extern struct TaskDesc eden_sound_beat_desc;
extern int eden_snake_orbit_ub_kinds[];
extern int eden_snake_ub_kinds[];
extern char eden_sound_pcm_file[];
extern int eden_sound_pcm_index[];
extern float cmd_st_len;
extern char *eden_tex_anime_files[];
extern int cmd_hm_node;

/* a ring of the orbit form, 56 bytes */
typedef struct EdenRing {
    Vec3 pos;
    char pad_0c[4];
    int ang;                    /* 0x10 */
    char pad_14[8];
    int ang_spd;                /* 0x1c: added to ang each frame (eden_snake_orbit_update) */
    char pad_20[4];
    float r;                    /* 0x24 */
    float r_to;                 /* 0x28: what r springs to */
    float r_vel;                /* 0x2c */
    int n;                      /* 0x30 */
    int timer;                  /* 0x34: frames to the next r_to */
} EdenRing;

/* a segment's ring of cells, 52 bytes */
typedef struct EdenSnakeSeg {
    EdenSnakeCell *cells;
    Vec3 pos;
    Vec3 dir;
    int ang[3];
    Vec3 vel;
} EdenSnakeSeg;

/* 92 bytes */
typedef struct EdenSnakeWork {
    struct PnbHead *pack;
    void *tex_list;
    ModelPart **parts;
    void *parent;
    Vec3 pos;
    Vec3 base;
    Vec3 drift;
    int ang[3];
    float phase;
    float swing;
    EdenSnakeSeg *segs;
    int nseg;
    int ncell;
    int fire_time;
    int fire_len;
} EdenSnakeWork;

/* the posed model eden_snake_human_model_draw draws, 64 bytes */
typedef struct EdenHumanModel {
    char pad_00[4];
    void *tree;
    void *mot;
    char pad_0c[48];
    float frm;                  /* 0x3c */
} EdenHumanModel;

/* the human form, 2032 bytes */
typedef struct EdenSnakeHumanWork {
    struct PnbHead *pack;
    void *tex_list;
    int parts;
    EdenHumanPt pts[150];
    EdenHumanModel model;
    char pad_752[28];
    void *mot_walk;             /* 0x770 */
    void *mot_jump;
    float walk_end;
    float jump_end;
    void *tree;
    int step;
    Vec3 pos;
    Vec3 core;
    int ang[3];
    int dir[3];
    float yaw;                  /* degrees around the player */
    int time;
    int len;
    int kind;                   /* 1 on the second pass */
    int damage;
    int damage_max;
    int side_set;
    char pad_7d4[4];
    float vy;                   /* 0x7d8 */
    float tilt;
    char pad_7e0[12];
    struct EdenPieceWork *piece; /* 0x7ec */
} EdenSnakeHumanWork;

/* the orbit form: three rings of cells, 3816 bytes */
typedef struct EdenSnakeOrbitWork {
    struct PnbHead *pack;
    void *tex_list;
    int parts;
    void *parent;
    Vec3 pts[150];
    int pt_ang[150][3];
    EdenRing rings[3];
    Vec3 pos;
    char pad_ed4[12];
    float phase;                /* 0xee0 */
    int end;
} EdenSnakeOrbitWork;

extern struct TaskDesc eden_snake_desc;
extern char str_eden_snake_c[];
extern int cmd_grp_num;
extern const int cmd_p_num;
extern float cmd_sn_st_w;
extern const float eden_cmd_sn_st_rw;

extern float cmd_p_len;
extern struct TaskDesc eden_snake_human_desc;
extern float cmd_hm_ry_t;
extern float cmd_hm_ry_t2;
extern const int cmd_hm_break_count1;
extern const int cmd_hm_break_count2;
extern struct TaskDesc eden_snake_orbit_desc;
extern int cmd_or_num_c;
extern int cmd_or_num;
extern float cmd_or_r;
extern float cmd_or_r_c;
extern float cmd_or_ry;
extern float cmd_or_rdy;
extern float eden_snake_do_mf;
extern float eden_snake_do_kf;
extern float eden_snake_swing_r;
extern float cmd_m_fd;
extern float cmd_mv;
extern float cmd_hm_bz;
extern float cmd_hm_by;
extern float cmd_hm_cx;
extern float cmd_or_h, cmd_or_dr, cmd_or_rcht, cmd_or_rchdt, cmd_or_mf, cmd_or_kf, cmd_or_pz, cmd_or_py, cmd_or_swing_r;
extern float cmd_hm_mv, cmd_hm_rdang, cmd_hm_mv_s, cmd_hm_rdang_s, cmd_hm_ry_st, cmd_hm_ry_fin, cmd_hm_fa_br, cmd_hm_fa_bz;
extern const float cmd_hm_fa_t;
extern const float cmd_hm_jfrm;
extern const float cmd_hm_sfrm;
extern const float cmd_hm_fa_nr;
extern const float cmd_hm_fa_v;
extern const float cmd_hm_fa_g;
extern const float cmd_hm_rlen;

/* plays the beat SE every second, 16 times; 12 bytes */
typedef struct EdenSoundBeatWork {
    int time;
    int len;
    int cnt;
} EdenSoundBeatWork;

/* 0x00cbe1c0 */
void eden_snake_vars_add(void)
{
    eden_snake_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_snake_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_snake_vars_group, eden_snake_vardefs);
}

/* 0x00cbe210 */
void eden_snake_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_snake_vardef);
}

/* 0x00cbe228 */
void eden_snake_ring_place(EdenSnakeWork *w, EdenSnakeSeg *src)
{
    int step = 0xffff / w->ncell;
    EdenSnakeCell *place = src->cells;
    int ang = step;
    int i;
    Vec3 v;

    for (i = 0; i < w->ncell; i++, place++) {
        vec_up_for_dir(&src->dir, &v, ang);
        ang += step;
        v3_scale(&v, cmd_p_len);
        v3_sum(&place->pos, &src->pos, &v);
        i3_copy(place->ang, src->ang);
        v3_copy(&place->dir, &src->dir);
    }
}

/* 0x00cbe378 */
void eden_snake_body_follow(EdenSnakeWork *w)
{
    Vec3 old, tgt;
    int n = w->nseg;
    EdenSnakeSeg *src = w->segs;

    v3_copy(&tgt, &w->pos);
    for (; n; src++, n--) {
        v3_copy(&old, &src->pos);
        spring_to_v(&src->vel, &src->pos, &tgt, eden_snake_do_mf, eden_snake_do_kf);
        v3_sub(&src->dir, &src->pos, &old);
        do {
            src->ang[1] = (int)RAD_TO_ANG(atan2f(-src->dir.x, -src->dir.z));
            src->ang[0] = (int)RAD_TO_ANG(atan2f(src->dir.y, sqrtf(src->dir.x * src->dir.x + src->dir.z * src->dir.z)));
        } while (0);
        v3_copy(&tgt, &src->pos);
        eden_snake_ring_place(w, src);
    }
}

/* steer(cast) */
/* 0x00cbe548 */
void eden_snake_head_move(EdenSnakeWork *snake)
{
    Vec3 old, amp;

    amp.x = amp.y = amp.z = 1.0f;
    v3_copy(&old, &snake->pos);
    amp.z = amp.y = amp.x = cmd_mv;
    wobble_step(&snake->drift, &amp, &snake->phase, cmd_m_fd);
    v3_sum(&snake->base, &snake->drift, &snake->base);
    wobble_step_slow(&amp, eden_snake_swing_r, &snake->swing);
    v3_sum(&snake->pos, &snake->base, &amp);
    v3_sub(&old, &snake->pos, &old);
    do {
        snake->ang[1] = (int)RAD_TO_ANG(atan2f(-old.x, -old.z));
        snake->ang[0] = (int)RAD_TO_ANG(atan2f(old.y, sqrtf(old.x * old.x + old.z * old.z)));
    } while (0);
    eden_snake_body_follow(snake);
}

/* 0x00cbe740 */
void eden_snake_place(EdenSnakeWork *w)
{
    Vec3 v;

    v.z = cmd_st_len;
    v.x = v.y = 0.0f;
    player_get_pos(&w->pos);
    v3_add(&w->pos, &v);
    eden_snake_head_move((char *)w);
}

/* 0x00cbe7c8 */
void eden_snake_exec(EdenSnakeWork *w)
{
    eden_snake_head_move((char *)w);
    if (w->fire_time)
        w->fire_time--;
}

/* 0x00cbe800 */
void eden_snake_release(EdenSnakeWork *w)
{
    EdenSnakeSeg *row = w->segs;
    int i;

    for (i = 0; i < w->nseg; i++, row++)
        heap_free(row->cells, 0);
    heap_free(w->segs, 0);
    eden_core_shine_reset(0);
}

/* 0x00cbe878 */
void eden_snake_disp(EdenSnakeWork *w)
{
    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    if (w->fire_len) {
        float t = (1.0f - (float)w->fire_time / (float)w->fire_len) * w->parts[4]->frames;

        model_object_draw(w->parts[4], t);
    }
    ulMatrixPop();
}

/* 0x00cbe918 */
int *eden_snake_create(void *parent, int *arg)
{
    EdenSnakeWork *w = task_create_inherit(parent, &eden_snake_desc, 92);
    int n;
    EdenSnakeSeg *p;
    int i;

    n = cmd_grp_num;
    w->parent = parent;
    w->ncell = cmd_p_num;
    w->parts = (ModelPart **)arg;
    w->nseg = n;
    i = n;
    p = w->segs = heap_block_tag(game_calloc(i * 52), str_eden_snake_c, 310);
    for (; i; i--, p++)
        p->cells = heap_block_tag(game_calloc(w->ncell * 36), str_eden_snake_c, 312);
    w->phase = cmd_sn_st_w * RAD_PER_DEG;
    w->swing = eden_cmd_sn_st_rw * RAD_PER_DEG;
    eden_snake_place(w);
    eden_core_shine_set_req(0, (int)&w->pos);
    return (int *)w;
}

/* 0x00cbea60 */
void eden_snake_get_dims(EdenSnakeWork *w, int *nrow, int *ncell)
{
    *nrow = w->nseg;
    *ncell = w->ncell;
}

/* 0x00cbea78 */
void eden_snake_cell_get(EdenSnakeWork *w, int i, int j, EdenSnakeCell **cell, int **vec)
{
    EdenSnakeSeg *row = &w->segs[i];
    EdenSnakeCell *entry = &row->cells[j];

    *cell = entry;
    *vec = entry->ang;
}

/* 0x00cbeaa8 */
void eden_snake_fire(EdenSnakeWork *w, int on, int cnt)
{
    if (!on) {
        w->fire_len = cnt;
        w->fire_time = cnt;
    } else {
        eden_bullet_ub_fire(w->parent, &w->pos, (int)w->parts, eden_snake_ub_kinds[boss_get_level()]);
        w->fire_len = 0;
        w->fire_time = 0;
    }
}

/* 0x00cbeb20 */
void eden_snake_human_tree_points(void *node, int mtx, int *cur, int *idx)
{
    WireObj *obj = node;

    ulMatrixPush();
    anim_apply_node((struct AnimNode *)obj);
    if (*idx == cmd_hm_node && mtx)
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)mtx);
    (*idx)++;
    if (obj->model && cur) {
        char *pts = *(char **)obj->model;
        int n = *(short *)(pts + 6);
        float *p = (float *)(pts + 8);

        for (; n; p += 6, n--) {
            ulMatrixApplyPoint((float *)ulMatrixCur, p, (float *)*cur);
            *cur += 12;
        }
    }
    if (obj->child)
        eden_snake_human_tree_points(obj->child, mtx, cur, idx);
    ulMatrixPop();
    if (obj->sibling)
        eden_snake_human_tree_points(obj->sibling, mtx, cur, idx);
}

/* 0x00cbec40 */
void eden_snake_human_model_draw(EdenHumanModel *w, Vec3 *pos, int *ang, int pts, int mtx)
{
    int cur[2];

    cur[0] = pts;
    cur[1] = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(pos);
    eden_snake_rotate_yxz(ang);
    anim_start(w->mot, w->frm);
    if (pts)
        eden_snake_human_tree_points(w->tree, mtx, &cur[0], &cur[1]);
    else
        eden_snake_human_tree_points(w->tree, mtx, 0, &cur[1]);
    ulMatrixPop();
}

/* 0x00cbed08 */
void eden_snake_human_circle_move(EdenSnakeHumanWork *w, float rad, float da, int ang, float k)
{
    Vec3 pl, dif;
    float sc[2];
    float old_x;
    float deg;

    w->yaw += da;
    if (360.0f <= w->yaw)
        w->yaw -= 360.0f;
    w->pos.x = 0.0f;
    w->pos.z = -rad;
    do {
        old_x = w->pos.x;
    } while (0);
    do {
        float ua = w->pos.x, ub = w->pos.z;
        deg = ANG_PER_DEG;
        ulSinCos((int)(w->yaw * deg), &sc[0], &sc[1]);
        w->pos.x = ua * sc[1] + ub * sc[0];
        w->pos.z = -ua * sc[0] + ub * sc[1];
    } while (0);
    w->ang[2] = (int)(cmd_hm_bz * deg) + ang;
    w->ang[1] = (int)((w->yaw + cmd_hm_by) * deg);
    w->ang[0] = 0;
    i3_copy(w->dir, w->ang);
    w->dir[0] += (int)(cmd_hm_cx * deg);
    if (!(fabsf(k) < FLT_EPSILON)) {
        Vec3 *mtx = &w->core;

        eden_snake_human_model_draw(&w->model, &w->pos, w->ang, 0, (int)mtx);
        player_get_pos(&pl);
        dif.x = pl.x - mtx->x;
        dif.y = old_x;
        dif.z = pl.z - mtx->z;
        if (1.0f < k)
            k = 1.0f;
        dif.x *= k;
        dif.y = k * dif.y;
        dif.z *= k;
        v3_add(&w->pos, &dif);
    }
}

/* 0x00cbef18 */
void eden_snake_human_exec(EdenSnakeHumanWork *w)
{
    float spd, k;
    int wrap;

    if (w->kind) {
        spd = cmd_hm_mv;
        k = cmd_hm_rdang;
    } else {
        spd = cmd_hm_mv_s;
        k = cmd_hm_rdang_s;
    }
    switch (w->step) {
    case 0:
        w->pos.y = cmd_hm_ry_st + (cmd_hm_ry_fin - cmd_hm_ry_st) * (1.0f - (float)w->time / (float)w->len);
        eden_snake_human_circle_move(w, cmd_hm_rlen, k, 0, 0.0f);
        w->model.frm += spd;
        w->model.mot = w->mot_walk;
        if (w->walk_end < w->model.frm) {
            w->model.frm -= w->walk_end;
            wrap = 1;
        } else
            wrap = 0;
        if (--w->time > 0)
            break;
        if (!wrap)
            break;
        *(int *)&w->model.frm = 0; /* steer(type) */
        w->step++;
        eden_piece_cells_command(w->piece, 2);
        eden_piece_clear_damage(w->piece);
        boss_se_play(50, 0, (int)&w->core);
    case 1:
        w->damage = eden_piece_get_damage(w->piece);
        if (cmd_hm_sfrm <= w->model.frm) {
            float t;

            t = (cmd_hm_jfrm - w->model.frm) / (cmd_hm_jfrm - cmd_hm_sfrm);
            eden_snake_human_circle_move(w, cmd_hm_rlen, k * (t * t), 0, 0.0f);
        } else
            eden_snake_human_circle_move(w, cmd_hm_rlen, k, 0, 0.0f);
        w->model.frm += spd;
        w->model.mot = w->mot_jump;
        if (cmd_hm_jfrm <= w->model.frm) {
            w->step++;
            w->vy = cmd_hm_fa_v;
            w->time = w->len = (int)(cmd_hm_fa_t * FPS);
            boss_se_play(49, 0, (int)&w->core);
            if (w->kind)
                eden_map_cam_preset_side(1);
        }
        break;
    case 2: {
        float t;

        w->damage = eden_piece_get_damage(w->piece);
        w->model.frm += spd;
        w->model.mot = w->mot_jump;
        if (w->jump_end < w->model.frm)
            w->model.frm = w->jump_end;
        if (!w->side_set && w->core.y < 0.0f) {
            if (w->kind)
                eden_map_cam_preset_side(-1);
            w->side_set = 1;
        }
        t = (float)w->time / (float)w->len;
        w->tilt = w->tilt * cmd_hm_fa_br + cmd_hm_fa_bz * (1.0f - cmd_hm_fa_br);
        w->vy += cmd_hm_fa_g;
        w->pos.y += w->vy;
        eden_snake_human_circle_move(w, cmd_hm_rlen * t, 0.0f, (int)DEG_TO_ANG(w->tilt), cmd_hm_fa_nr * (1.0f - t));
        if (--w->time < 2 || w->damage > w->damage_max) {
            eden_piece_cells_command(w->piece, -2);
            w->step++;
        }
        break;
    }
    case 3: {
        float ang = cmd_hm_fa_bz;
        float rate = *(float *)&cmd_hm_fa_nr;

        w->tilt = ang;
        eden_snake_human_circle_move(w, 0.0f, 0.0f, (int)DEG_TO_ANG(ang), rate);
        break;
    }
    }
    eden_snake_human_model_draw(&w->model, &w->pos, w->ang, (int)w->pts, (int)&w->core);
}

/* 0x00cbf390 */
void eden_snake_human_release(EdenSnakeHumanWork *w)
{
    if (w->kind)
        eden_map_cam_preset_side(0);
    eden_core_shine_reset(0);
}

/* 0x00cbf3c0 */
void eden_snake_human_disp(EdenSnakeHumanWork *w)
{
    ulGxSetTextureList(w->tex_list);
    debug_flags_test(4);
}

/* steer(cast) */
/* 0x00cbf3e0 */
char *eden_snake_human_create(void *parent, int arg, int kind)
{
    EdenSnakeHumanWork *w = task_create_inherit(parent, &eden_snake_human_desc, 2032);
    int mdl;

    w->piece = eden_get_piece(parent);
    w->parts = arg;
    w->mot_walk = pnb_get_entry(w->pack, 127)->src;
    w->mot_jump = pnb_get_entry(w->pack, 128)->src;
    w->walk_end = (unsigned int)(((int *)w->mot_walk)[1] - 1);
    w->jump_end = (unsigned int)(((int *)w->mot_jump)[1] - 1);
    mdl = (int)pnb_get_entry(w->pack, 126)->src;
    w->kind = kind;
    w->model.tree = w->tree = (void *)mdl;
    if (!kind) {
        w->time = w->len = (int)(cmd_hm_ry_t * FPS);
        w->damage_max = cmd_hm_break_count1;
    } else {
        w->time = w->len = (int)(cmd_hm_ry_t2 * FPS);
        w->damage_max = cmd_hm_break_count2;
    }
    eden_core_shine_set_req(0, (int)&w->core);
    return (char *)w;
}

/* 0x00cbf560 */
void eden_snake_human_point_get(EdenSnakeHumanWork *w, int i, EdenHumanPt **pt, char **vec)
{
    if (pt)
        *pt = &w->pts[i];
    if (vec)
        *vec = (char *)w->dir;
}

/* 0x00cbf590 */
int eden_snake_human_is_done(EdenSnakeHumanWork *w)
{
    return w->step == 3;
}

/* 0x00cbf5a0 */
void eden_snake_orbit_ring_points(EdenRing *ring, Vec3 *out, int *ang, int n)
{
    int step = 0xffff / n;
    int step_ang = 0;
    Vec3 v;
    float sc[2];

    for (; n; n--) {
        v.z = -ring->r;
        v.x = v.y = 0.0f;
        ROT_Y(ring->ang + step_ang, sc, v.x, v.z);
        out->x = ring->pos.x + v.x;
        ang[1] = ring->ang + step_ang;
        ang[0] = ANG_90;
        step_ang += step;
        out->y = ring->pos.y + v.y;
        ang += 3;
        out->z = ring->pos.z + v.z;
        out++;
    }
}

/* steer(cast) */
/* 0x00cbf6e8 */
void eden_snake_orbit_update(EdenSnakeOrbitWork *orbit)
{
    Vec3 wob;
    EdenRing *ring;
    int n;

    wobble_step_slow(&wob, cmd_or_swing_r, &orbit->phase);
    orbit->pos.z = cmd_or_pz;
    orbit->pos.y = cmd_or_py;
    orbit->pos.x = 0.0f;
    v3_add(&orbit->pos, &wob);
    ring = &orbit->rings[0];
    if (--ring->timer <= 0) {
        ring->r_to = cmd_or_r_c + RAND_FLOAT_A() * cmd_or_dr;
        ring->timer = (int)((cmd_or_rcht + RAND_FLOAT_A() * cmd_or_rchdt) * FPS);
    }
    spring_to(&orbit->rings[0].r_vel, &orbit->rings[0].r, ring->r_to, cmd_or_mf, cmd_or_kf);
    v3_copy(&ring->pos, &orbit->pos);
    ring->ang += ring->ang_spd;
    eden_snake_orbit_ring_points(ring, orbit->pts, orbit->pt_ang[0], ring->n);
    n = ring->n;
    ring = &orbit->rings[1];
    if (--ring->timer <= 0) {
        ring->r_to = cmd_or_r + RAND_FLOAT_A() * cmd_or_dr;
        ring->timer = (int)((cmd_or_rcht + RAND_FLOAT_A() * cmd_or_rchdt) * FPS);
    }
    spring_to(&orbit->rings[1].r_vel, &orbit->rings[1].r, ring->r_to, cmd_or_mf, cmd_or_kf);
    v3_copy(&ring->pos, &orbit->pos);
    ring->pos.y += cmd_or_h;
    ring->ang += ring->ang_spd;
    eden_snake_orbit_ring_points(ring, &orbit->pts[n], orbit->pt_ang[n], ring->n);
    n += ring->n;
    ring = &orbit->rings[2];
    if (--ring->timer <= 0) {
        ring->r_to = cmd_or_r + RAND_FLOAT_A() * cmd_or_dr;
        ring->timer = (int)((cmd_or_rcht + RAND_FLOAT_A() * cmd_or_rchdt) * FPS);
    }
    spring_to(&orbit->rings[2].r_vel, &orbit->rings[2].r, ring->r_to, cmd_or_mf, cmd_or_kf);
    v3_copy(&ring->pos, &orbit->pos);
    ring->ang += ring->ang_spd;
    ring->pos.y -= cmd_or_h;
    eden_snake_orbit_ring_points(ring, &orbit->pts[n], orbit->pt_ang[n], ring->n);
}

/* 0x00cbfac8 */
void eden_snake_orbit_exec(EdenSnakeOrbitWork *w)
{
    eden_snake_orbit_update((char *)w);
}

/* 0x00cbfad0 */
void eden_snake_orbit_release(void)
{
    eden_core_shine_reset(0);
}

/* 0x00cbfad8 */
void eden_snake_orbit_disp(EdenSnakeOrbitWork *w)
{
    ulGxSetTextureList(w->tex_list);
}

/* 0x00cbfae0 */
char *eden_snake_orbit_create(void *parent, int arg)
{
    EdenSnakeOrbitWork *w = task_create_inherit(parent, &eden_snake_orbit_desc, 3816);
    float r_c, r_s;
    int num_c, num;

    r_c = cmd_or_r_c;
    r_s = cmd_or_r;
    num_c = cmd_or_num_c;
    num = cmd_or_num;
    w->parent = parent;
    w->parts = arg;
    w->rings[0].r = w->rings[0].r_to = r_c;
    w->rings[1].r = w->rings[1].r_to = w->rings[2].r = w->rings[2].r_to = r_s;
    w->rings[0].n = num_c;
    w->rings[1].n = w->rings[2].n = num;
    w->rings[0].ang_spd = (int)DEG_TO_ANG(RAND_FLOAT_A() * ((cmd_or_ry + cmd_or_rdy) - cmd_or_ry) + cmd_or_ry);
    w->rings[1].ang_spd = (int)DEG_TO_ANG(RAND_FLOAT_A() * ((cmd_or_ry + cmd_or_rdy) - cmd_or_ry) + cmd_or_ry);
    w->rings[2].ang_spd = (int)DEG_TO_ANG(RAND_FLOAT_A() * ((cmd_or_ry + cmd_or_rdy) - cmd_or_ry) + cmd_or_ry);
    eden_core_shine_set_req(0, (int)&w->pos);
    return (char *)w;
}

/* 0x00cbfc78 */
void eden_snake_orbit_point_get(EdenSnakeOrbitWork *w, int i, char **pt, char **vec)
{
    if (pt)
        *pt = (char *)&w->pts[i];
    if (vec)
        *vec = (char *)w->pt_ang[i];
}

/* 0x00cbfcb0 */
void eden_snake_orbit_set_end(EdenSnakeOrbitWork *w)
{
    w->end = 1;
}

/* 0x00cbfcc0 */
void eden_snake_orbit_fire(EdenSnakeOrbitWork *w)
{
    eden_bullet_ub_fire(w->parent, &w->pos, w->parts, eden_snake_orbit_ub_kinds[boss_get_level()]);
}

/* 0x00cbfd18 */
void eden_snake_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00cbfd68 */
void boss_se_play(int id, int lock, int pos)
{
    se_table_play(0, &se_table, id, lock, pos);
}

/* 0x00cbfd88 */
void boss_vol_play(int id, int arg)
{
    vol_table_play(0, vol_table, id, arg);
}

/* 0x00cbfda0 */
void boss_vol_stop(int id)
{
    vol_table_stop(0, vol_table, id);
}

/* 0x00cbfdb8 */
void boss_bgm_exit_cb(BgmMan *bgm)
{
    se_table_stop(&se_table);
}

/* 0x00cbfdc8 */
BgmMan *boss_bgm_start(void *parent)
{
    se_table_reset(&se_table);
    return cm_bgm_sound_man_create(parent, boss_bgm_exit_cb);
}

/* 0x00cbfe08 */
void boss_adx_request(void)
{
    cm_bgm_sound_man_add_file(0, adx_files[0], (int)adx_files[1]);
}

/* 0x00cbfe20 */
void boss_bgm_switch(int cb, int cb_arg, int idx)
{
    cm_bgm_sound_sub_start(0, cb, cb_arg, 0, 0, idx, idx);
}

/* 0x00cbfe48 */
void eden_sound_bgm_start_cb(void)
{
    boss_vol_play(13, 0);
    pcm_start();
}

/* 0x00cbfe68 */
void eden_sound_bgm_start(int i)
{
    ((void (*)(BgmMan *, int, int, int, int, int))cm_bgm_sound_sub_create_idle)(0, (int)eden_sound_bgm_start_cb, 0, 0, 0, 12);
    pcm_load_index((void *)eden_sound_pcm_file, 1, eden_sound_pcm_index[i % 8]);
}

/* 0x00cbfee8 */
void eden_sound_seq_load_cb(int arg)
{
    sound_mpb_load_create_bpms(eden_sound_seq_files[0], 1);
    sound_mpb_load_create_bpms(eden_sound_seq_files[1], 2);
    music_change(8);
    midiseq_mute_request(40);
    midiseq_play();
}

/* 0x00cbff38 */
void eden_sound_seq_load(void)
{
    cm_se_laser_set_cb(cm_se_laser_create(eden_sound_seq_file), eden_sound_seq_load_cb, 0);
}

/* 0x00cbff68 */
void eden_sound_tempo_set(int i)
{
    beat_clock_request_tempo(eden_sound_tempos[i]);
}

/* 0x00cbff80 */
void eden_sound_beat_exec(EdenSoundBeatWork *w)
{
    if (--w->time <= 0) {
        SoundRhythmSe_Common(9);
        w->time = w->len;
        if (--w->cnt <= 0) {
            do { FINISH(w); } while (0);
        }
    }
}

/* 0x00cbffe0 */
int *eden_sound_beat_create(void *parent)
{
    EdenSoundBeatWork *w = task_create(parent, &eden_sound_beat_desc, 12);

    w->cnt = 16;
    w->time = w->len = 60;
    return (int *)w;
}

/* 0x00cc0020 */
CmTexAnimeWork *eden_tex_anime_create(int i)
{
    void *boss = boss_get_task();
    CmTexAnimeWork *w = cm_tex_anime_create(boss, 21, eden_tex_anime_files[i], boss_get_arg0() + eden_tex_offset(), 18432, 15, 4);

    tex_anime_set_visible(0);
    tex_anime_set_play(0);
    return w;
}

/* 0x00cc00b0 */
void eden_tex_anime_set_visible(int on)
{
    tex_anime_set_visible(on);
}

/* 0x00cc00b8 */
void eden_tex_anime_set_play(int on)
{
    tex_anime_set_play(on);
}

/* 0x00cc00c0 */
void eden_tex_anime_stop(void)
{
    tex_anime_stop();
}
