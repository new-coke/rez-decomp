/*
 * Eden boss mirror wall form: mirrors of 13 segments carrying piece cells, moving between a ring
 * and a grid wall. Boss lasers step along a mirror; a fully charged mirror fires back at the
 * player, and on a timer a random mirror flashes and launches an RB bullet.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "player.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_vu0.h"
#include "eden_wall.h"
#include "eden.h"
#include "eden_bullet.h"
#include "eden_core.h"
#include "eden_effect.h"
#include "eden_laser.h"
#include "eden_piece.h"
#include "eden_snake.h"
#include "../common/boss_common.h"

extern char eden_wall_vardef[];
extern GwVarDef eden_wall_vardefs[];
extern struct GwVarNode *eden_wall_vars_group;
typedef struct EdenMirrorSeg {
    int from_pos[3];
    Vec3 pos;                   /* 0x0c */
    Vec3 home_pos;              /* 0x18: restored into pos; the blend target */
    int home_ang[3];            /* 0x24: angles_blend from it */
    Vec3 ang;                   /* 0x30 */
    int ang_to[3];              /* 0x3c: angles_blend target, restored */
    Vec3 vel;                   /* 0x48: (0, 0, random cmd_mi_bv..+bdv) turned by ROT_X and ROT_Y */
    void *cell;                 /* 0x54: eden_piece_cell_find result; its flags break the mirror */
    char pad_58[4];
} EdenMirrorSeg;
typedef struct EdenMirror {
    Vec3 pos;
    Vec3 pos_to;                /* 0x0c: pos springs to it (eden_wall_mirrors_step); the blend target */
    Vec3 pos_from;              /* 0x18: the blend start */
    int ang[3];                 /* 0x24 */
    int ang_to[3];              /* 0x30 */
    int ang_from[3];            /* 0x3c */
    char pad_48[12];
    Vec3 vel;                   /* 0x54: pos's spring velocity */
    int timer;                  /* 0x60: frames to the next random target (cmd_mi2_t) */
    char pad_64[4];
    int state;                  /* 0x68: 0 idle, 1 armed, 2 firing */
    int charge;                 /* 0x6c: cmd_mi_cht frames */
    int shot;                   /* 0x70: the segment the laser has reached (0..13) */
    int wait;                   /* 0x74: frames before the next laser (cmd_mi_lwt) */
    char pad_78[20];
    EdenMirrorSeg sub[13];      /* 0x8c */
    void *task;
    int grid[2];
} EdenMirror;

/* the mirror wall, 204 bytes */
typedef struct EdenWallWork {
    struct PnbHead *pack;
    void *tex_list;
    int parts;
    void *parent;
    Vec3 pos;
    Vec3 core;
    char pad_28[12];
    EdenMirror *mirrors;        /* 0x34 */
    int laser;                  /* 0 waiting, 1 firing on the beat */
    int laser_cnt;
    int step;                   /* 0 wall, 1 opening, 2 lasers */
    int nmirror;
    int time;
    int len;
    float phase;
    int laser_wait;
    int flash_time;
    int break_time;
    char grid[6][18];           /* cells the mirrors stand on */
} EdenWallWork;
extern int cmd_ls_num;
extern float cmd_ls_st, cmd_ls_sdt;
extern float cmd_wall_sh, cmd_wall_h, cmd_wall_ay;
extern float cmd_mi_set_len, cmd_mi_set_len_nd, cmd_mi_set_py, cmd_mi_set_pdy, cmd_mi_set_ang_ylim2;
extern char eden_wall_desc[], str_eden_wall_c[];
extern int cmd_m_num;
extern float cmd_mirror_et, cmd_ls_st_st, cmd_mi_bsh_st, cmd_mode_ch_t;
extern float cmd_flash_t;
extern float cmd_flash_r;
extern float eden_wall_flash_secs[];
extern float cmd_mi_len;
extern float cmd_wall_r;
extern float cmd_mi_lwt, cmd_mi_cht;
extern float eden_wall_swing_r, cmd_wall_len, cmd_wall_y, cmd_core_y, cmd_change_t;
extern int eden_wall_restart_hits;
extern float cmd_mi_bv, cmd_mi_bdv;
extern float cmd_mi2_t, cmd_mi2_dt, cmd_mi_do_kf, cmd_mi_do_mf;

/* 0x00cc0ca8 */
void eden_wall_vars_add(void)
{
    eden_wall_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_wall_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_wall_vars_group, eden_wall_vardefs);
}

/* 0x00cc0cf8 */
void eden_wall_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_wall_vardef);
}

/* 0x00cc0d10 */
int eden_wall_grid_get(EdenWallWork *w, int i, int j)
{
    return w->grid[j][i];
}

/* 0x00cc0d28 */
void eden_wall_grid_set(EdenWallWork *w, int x, int y, int val)
{
    char *cell = (char *)w + (x + y * 18) % 108; /* steer(cast) */

    cell[96] = val;
}

/* 0x00cc0d58 */
void eden_wall_mirror_grid_step(EdenWallWork *w, EdenMirror *mirror)
{
    int i, k, x, y, n;

    k = (int)(RAND_FLOAT_A() * 4.0f);
    x = mirror->grid[0];
    y = mirror->grid[1];
    for (i = 0; i < 4; i++) {
        switch ((k + i) % 4) {
        case 0:
            if (y > 0) {
                n = y - 1;
                if (!eden_wall_grid_get(w, x, n)) {
                    mirror->grid[1] = n;
                    mirror->grid[0] = x;
                    goto found;
                }
            }
            break;
        case 1:
            if (y < 5) {
                n = y + 1;
                if (!eden_wall_grid_get(w, x, n)) {
                    mirror->grid[1] = n;
                    mirror->grid[0] = x;
                    goto found;
                }
            }
            break;
        case 2:
            if (x > 0) {
                n = x - 1;
                if (!eden_wall_grid_get(w, n, y)) {
                    mirror->grid[0] = n;
                    mirror->grid[1] = y;
                    goto found;
                }
            }
            break;
        case 3:
            if (x < 17) {
                n = x + 1;
                if (!eden_wall_grid_get(w, n, y)) {
                    mirror->grid[0] = n;
                    mirror->grid[1] = y;
                    goto found;
                }
            }
            break;
        }
    }
    return;
found:
    eden_wall_grid_set(w, x, y, 0);
    eden_wall_grid_set(w, mirror->grid[0], mirror->grid[1], 1);
}

/* 0x00cc0f50 */
void eden_wall_mirror_grid_pos(EdenWallWork *w, EdenMirror *mirror)
{
    Vec3 v;
    float sc[2];

    v.x = 0.0f;
    v.z = -cmd_mi_set_len + cmd_mi_set_len_nd * mirror->shot;
    v.y = cmd_mi_set_py + cmd_mi_set_pdy * mirror->grid[1];
    ROT_Y((int)DEG_TO_ANG(cmd_mi_set_ang_ylim2 * mirror->grid[0] / 18.0f - cmd_mi_set_ang_ylim2 * 0.5f), sc, v.x, v.z);
    v3_copy(&mirror->pos_to, &v);
}

/* 0x00cc1078 */
void eden_wall_mirrors_place(EdenWallWork *w)
{
    int n = w->nmirror;
    EdenMirror *mirror = w->mirrors;
    int size = 108;
    int i, j;
    char *grid, *cell;

    for (; n; n--) {
        do {
            i = (int)(RAND_FLOAT_A() * size);
            j = 0;
        } while (0);
        grid = (char *)w->grid;
    again:
        cell = grid + i;
        if (!*cell)
            goto found;
        i = (i + 1) % size;
        j++;
        if (j < 108)
            goto again;
        goto next;
    found:
        *cell = 1;
        mirror->grid[0] = i % 18;
        mirror->grid[1] = i / 18;
        eden_wall_mirror_grid_pos(w, mirror);
        v3_copy((Vec3 *)mirror, &mirror->pos_to);
    next:
        mirror++;
        w = (EdenWallWork *)((char *)w + (((int)mirror & 1) >> 1)); /* steer(zero) */
    }
}

/* 0x00cc11b8 */
void eden_wall_mirror_form_ring(EdenMirror *mirror)
{
    Vec3 v;
    float sc[2];
    int i, ang;
    EdenMirrorSeg *seg;

    seg = mirror->sub;
    i3_copy((int *)&seg->ang, mirror->ang);
    v3_copy(&seg->pos, (Vec3 *)mirror);
    *(int *)&seg->ang.x += ANG_90;
    for (i = 0, ang = 0, seg++; i < 12; i++, seg++) {
        v.y = cmd_mi_len;
        v.x = v.z = 0.0f;
        ROT_Z(ang, sc, v.x, v.y);
        vec_rotate_zxy(&v, mirror->ang);
        ang += 5461;
        v3_sum(&seg->pos, (Vec3 *)mirror, &v);
        i3_copy((int *)&seg->ang, mirror->ang);
        *(int *)&seg->ang.x += ANG_90;
    }
}

/* 0x00cc1340 */
void eden_wall_mirror_form_wall(EdenMirror *mirror)
{
    Vec3 v;
    float sc[2];
    int i, ang;
    EdenMirrorSeg *seg;

    seg = mirror->sub;
    i3_copy((int *)&seg->ang, mirror->ang);
    v3_copy(&seg->pos, (Vec3 *)mirror);
    *(int *)&seg->ang.x = 0;
    for (i = 0, ang = mirror->ang[1], seg++; i < 12; i++, seg++) {
        v.z = cmd_wall_r;
        v.x = v.y = 0.0f;
        ROT_Y(ang, sc, v.x, v.z);
        v3_sum(&seg->pos, (Vec3 *)mirror, &v);
        do {
            float tau, full;

            full = ANG_TURN;
            tau = TWO_PI;
            ((int *)&seg->ang)[1] = (int)(atan2f(-v.x, -v.z) * full / tau);
            ((int *)&seg->ang)[0] = (int)(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)) * full / tau);
        } while (0);
        ((int *)&seg->ang)[0] = ANG_90;
        ((int *)&seg->ang)[2] = 0;
        ang += 5461;
    }
}

/* 0x00cc1530 */
void eden_wall_mirrors_face(EdenWallWork *w)
{
    Vec3 pl, to_pl, to_ctr;
    EdenMirror *mirror = w->mirrors;
    Vec3 *dir;
    int i;
    float z;

    player_get_pos(&pl);
    for (i = 0; i < w->nmirror; i++, mirror++) {
        dir = (Vec3 *)((char *)mirror + 72);
        v3_sub(&to_pl, &pl, (Vec3 *)mirror);
        v3_sub(&to_ctr, &w->pos, (Vec3 *)mirror);
        vec3_normalize(&to_pl, &to_pl);
        vec3_normalize(&to_ctr, &to_ctr);
        v3_sum(dir, &to_pl, &to_ctr);
        vec3_normalize(dir, dir);
        do {
            z = 0.0f;
            to_pl.z = z;
            to_pl.y = 1.0f;
            to_pl.x = z;
        } while (0);
        angles_from_dirs(dir, &to_pl, mirror->ang);
        eden_wall_mirror_form_ring(mirror);
    }
}

/* 0x00cc16a8 */
void eden_wall_mirrors_step(EdenWallWork *w)
{
    int i, j, k;
    EdenMirror *mirror;
    void *piece;

    for (i = 0, mirror = w->mirrors; i < w->nmirror; i++, mirror++) {
        if (--mirror->timer <= 0) {
            eden_wall_mirror_grid_step(w, mirror);
            eden_wall_mirror_grid_pos(w, mirror);
            mirror->timer = (int)((RAND_FLOAT_A() * ((cmd_mi2_t + cmd_mi2_dt) - cmd_mi2_t) + cmd_mi2_t) * FPS);
        }
        spring_to_v(&mirror->vel, (Vec3 *)mirror, &mirror->pos_to, cmd_mi_do_mf, cmd_mi_do_kf);
        if (!mirror->state)
            continue;
        if (!mirror->sub[0].cell)
            continue;
        k = eden_piece_cell_get_flags(mirror->sub[0].cell);
        if (k) {
            if (k & 0x400) {
                mirror->wait = 0;
                if (mirror->shot) {
                    if (mirror->shot == 12) {
                        piece = eden_get_piece(w->parent);
                        for (j = 0; j < 13; j++)
                            eden_piece_cell_command(piece, i % w->nmirror, j, -3);
                    }
                    eden_piece_cell_command(eden_get_piece(w->parent), i % w->nmirror, mirror->shot % 13, -4);
                    mirror->shot--;
                    boss_se_play(47, 0, (int)&mirror->sub[0].pos);
                }
            }
            if (mirror->wait && --mirror->wait <= 0) {
                mirror->state = 1;
                boss_se_play(33, 0, (int)&mirror->sub[0].pos);
                eden_laser_create(w, (int)&mirror->sub[0].pos, (int)mirror->ang, 0, w->parts, 1);
                mirror->shot = 0;
                {
                    void *piece2;
                    int cell;

                    piece2 = eden_get_piece(w->parent);
                    for (cell = 0; cell < 13; cell++)
                        eden_piece_cell_command(piece2, i % w->nmirror, cell, -4);
                }
            }
        } else {
            if (mirror->task) {
                do {
                    FINISH(mirror->task);
                } while (0);
                mirror->task = 0;
            }
            mirror->state = 0;
        }
    }
}

/* 0x00cc1990 */
void eden_wall_mirrors_link(void *piece, EdenWallWork *w)
{
    EdenMirror *mirror = w->mirrors;
    int i, j;

    for (i = 0; i < w->nmirror; i++, mirror++) {
        EdenMirrorSeg *seg = mirror->sub;

        mirror->state = 1;
        for (j = 0; j < 13; j++, seg++)
            seg->cell = eden_piece_cell_find(piece, i, j);
    }
}

/* 0x00cc1a58 */
void eden_wall_mirror_shoot(EdenWallWork *w)
{
    int k = (int)(RAND_FLOAT_A() * w->nmirror);
    int n = w->nmirror;
    int i;

    for (i = 0; i < n; i++) {
        int j = k + i;
        EdenMirror *mirror = &(w->mirrors)[j % n];

        if (mirror->state == 1) {
            mirror->charge = (int)(cmd_mi_cht * FPS);
            boss_se_play(32, 0, (int)&w->core);
            boss_se_play(34, 0, (int)&mirror->sub[(mirror->shot + 1) % 13].pos);
            mirror->state = 2;
            eden_laser_create(w, (int)&w->core, (int)mirror->ang, (int)&mirror->sub[(mirror->shot + 1) % 13].pos, w->parts, 0);
            eden_piece_cell_command(eden_get_piece(w->parent), j % w->nmirror, (mirror->shot + 1) % 13, 2);
            if (mirror->shot >= 13) {
                mirror->wait = (int)(cmd_mi_lwt * FPS);
                mirror->shot = 13;
            } else if (mirror->shot == 12) {
                void *piece = eden_get_piece(w->parent);
                int seg;

                for (seg = 0; seg < 13; seg++)
                    eden_piece_cell_command(piece, j % w->nmirror, seg, 3);
            }
            mirror->shot++;
            return;
        }
    }
}

/* 0x00cc1cb0 */
void eden_wall_mirrors_check(EdenWallWork *w)
{
    int i;

    for (i = 0; i < w->nmirror; i++) {
        EdenMirror *mirror = &(w->mirrors)[i];

        if (mirror->task && (TASK_OF(mirror->task)->flags & TASK_F_END))
            mirror->task = 0;
        if (mirror->state == 2 && !w->laser)
            mirror->state = 1;
    }
}

/* 0x00cc1d28 */
void eden_wall_laser_step(EdenWallWork *w)
{
    eden_wall_mirrors_step(w);
    eden_wall_mirrors_face(w);
    switch (w->laser) {
    case 0:
        if (--w->laser_wait <= 0) {
            w->laser_cnt = cmd_ls_num;
            w->laser = 1;
        }
        break;
    case 1:
        if (beat_grid_hit(1)) {
            eden_wall_mirror_shoot(w);
            if (--w->laser_cnt < 0) {
                w->laser = 0;
                w->laser_wait = (RAND_FLOAT_A() * ((cmd_ls_st + cmd_ls_sdt) - cmd_ls_st) + cmd_ls_st) * FPS;
            }
        }
        break;
    }
    eden_wall_mirrors_check(w);
}

/* 0x00cc1e20 */
void eden_wall_mirrors_stack(EdenWallWork *w)
{
    EdenMirror *mirror = w->mirrors;
    Vec3 ofs;
    int i;

    for (i = 0; i < w->nmirror; i++, mirror++) {
        ofs.x = ofs.z = 0.0f;
        ofs.y = cmd_wall_sh + cmd_wall_h * i;
        v3_sum((Vec3 *)mirror, &w->pos, &ofs);
        mirror->ang[1] += (int)DEG_TO_ANG(cmd_wall_ay);
        mirror->ang[2] = 0;
        mirror->ang[0] = 0;
        eden_wall_mirror_form_wall(mirror);
    }
}

/* 0x00cc1f30 */
void eden_wall_flash_step(EdenWallWork *w)
{
    EdenMirrorSeg *seg;
    int k, ang;
    Vec3 *pos;
    Vec3 v;

    if (--w->flash_time > 0)
        return;
    seg = (EdenMirrorSeg *)w->mirrors;
    k = (int)(RAND_FLOAT_A() * 12.0f);
    ang = (int)DEG_TO_ANG(cmd_wall_ay);
    seg = (EdenMirrorSeg *)((char *)seg + 232) + k; /* steer(cast): the segment past the mirror's first */
    v.y = seg->pos.y;
    v.x = w->pos.x;
    pos = &seg->pos;
    v.z = w->pos.z;
    eden_bullet_rb_create(w, w->parts, pos, &v, ang);
    eden_effect_dshine_ex_create(w, (int)pos, (int)(cmd_flash_t * FPS), cmd_flash_r);
    boss_se_play(21, 0, (int)pos);
    w->flash_time = (int)(eden_wall_flash_secs[boss_get_level()] * FPS);
}

/* 0x00cc2078 */
void eden_wall_mirrors_reset(EdenWallWork *w)
{
    int i, j;
    EdenMirror *mirror;
    float sc[4];
    int ang;

    for (i = 0, mirror = w->mirrors; i < w->nmirror; i++, mirror++) {
        EdenMirrorSeg *seg;

        v3_copy(&mirror->pos_from, (Vec3 *)mirror);
        i3_copy(mirror->ang_from, mirror->ang);
        for (j = 0, seg = mirror->sub; j < 13; j++, seg++) {
            v3_copy((Vec3 *)seg, &seg->pos);
            {
                int *src = (int *)&seg->ang;
                int *dst = seg->home_ang;
                i3_copy(dst, src);
            }
            seg->vel.z = RAND_FLOAT_A() * ((cmd_mi_bv + cmd_mi_bdv) - cmd_mi_bv) + cmd_mi_bv;
            ang = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
            ROT_X(ang, sc, seg->vel.y, seg->vel.z);
            ang = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
            ROT_Y(ang, (sc + 2), seg->vel.x, seg->vel.z);
        }
    }
}

/* 0x00cc2310 */
void eden_wall_mirrors_restore(EdenWallWork *w)
{
    EdenMirror *mirror = w->mirrors;
    int i, j;

    for (i = 0; i < w->nmirror; i++, mirror++) {
        EdenMirrorSeg *seg;

        i3_copy(mirror->ang_to, mirror->ang);
        v3_copy(&mirror->pos_to, &mirror->pos);
        i3_set(mirror->ang, 0, 0, 0);
        for (j = 0, seg = mirror->sub; j < 13; j++, seg++) {
            v3_copy(&seg->home_pos, &seg->pos);
            {
                int *src = (int *)&seg->ang;
                int *dst = seg->ang_to;
                i3_copy(dst, src);
            }
        }
    }
}

static inline void v3_lerp_to(Vec3 *src_a, Vec3 *src_b, float k, Vec3 *dst)
{
    dst->x = src_a->x * k + src_b->x * (1.0f - k);
    dst->y = src_a->y * k + src_b->y * (1.0f - k);
    dst->z = src_a->z * k + src_b->z * (1.0f - k);
}

/* 0x00cc23d8 */
void eden_wall_mirrors_blend(EdenWallWork *w, float t)
{
    int i, k;
    EdenMirror *mirror;
    EdenMirrorSeg *seg;

    for (i = 0, mirror = w->mirrors; i < w->nmirror; i++, mirror++) {
        v3_lerp((Vec3 *)mirror, &mirror->pos_from, &mirror->pos_to, t);
        angles_blend(mirror->ang_from, mirror->ang_to, mirror->ang, t);
        for (k = 0, seg = mirror->sub; k < 13; k++, seg++) {
            v3_lerp_to((Vec3 *)seg->from_pos, &seg->home_pos, t, &seg->pos);
            angles_blend(seg->home_ang, seg->ang_to, (int *)&seg->ang, t);
        }
    }
}

/* 0x00cc2568 */
void eden_wall_mirrors_blend_step(EdenWallWork *w)
{
    eden_wall_mirrors_blend(w, (float)w->time / (float)w->len);
}

/* steer(cast) */
/* 0x00cc2590 */
void eden_wall_exec(EdenWallWork *wall)
{
    Vec3 wob;
    Vec3 *pos = &wall->pos;
    EdenMirror *mirror;
    int i;

    player_get_pos(pos);
    wob.x = 0.0f;
    wob.y = cmd_wall_y;
    wob.z = -cmd_wall_len;
    v3_add(pos, &wob);
    wobble_step_slow(&wob, eden_wall_swing_r, &wall->phase);
    v3_add(pos, &wob);
    v3_copy(&wall->core, pos);
    wall->core.y += cmd_core_y;
    switch (wall->step) {
    case 0:
        mirror = wall->mirrors;
        eden_wall_mirrors_stack(wall);
        eden_wall_flash_step(wall);
        if (--wall->break_time <= 0) {
            if (eden_piece_cell_get_flags(mirror->sub[0].cell))
                eden_piece_wall_cell_break(eden_get_piece(wall->parent), mirror->sub[0].cell);
        } else if (eden_core_shine_get_hits(0) >= eden_wall_restart_hits)
            eden_piece_wall_cell_break(eden_get_piece(wall->parent), mirror->sub[0].cell);
        if (!eden_piece_cell_get_flags(mirror->sub[0].cell)) {
            int frm;

            boss_se_play(31, 0, (int)&wall->core);
            eden_wall_mirrors_reset(wall);
            frm = (int)(cmd_change_t * FPS);

            mirror->state = 0;
            mirror++;
            wall->step++;
            wall->len = frm;
            wall->time = frm;
            for (i = 0; i < wall->nmirror - 1; i++, mirror++)
                eden_piece_set_hold(mirror->sub[0].cell, 1);
        }
        break;
    case 1:
        eden_wall_mirrors_blend_step(wall);
        if (--wall->time <= 0)
            wall->step++;
        break;
    case 2:
        eden_wall_laser_step(wall);
        break;
    }
}

/* 0x00cc2818 */
void eden_wall_release(EdenWallWork *w)
{
    EdenMirror *mirror = w->mirrors;
    int i;

    for (i = 0; i < w->nmirror; i++, mirror++)
        do {
            FINISH_IF(mirror->task);
        } while (0);
    heap_free(w->mirrors, 0);
    eden_core_shine_reset(0);
}

/* 0x00cc2878 */
void eden_wall_disp(Earth1stDemoWork *w)
{
}

/* 0x00cc2880 */
int *eden_wall_create(void *parent, int arg)
{
    EdenWallWork *w = task_create_inherit(parent, eden_wall_desc, 204);
    float fps = FPS;

    w->parent = parent;
    w->nmirror = cmd_m_num;
    w->parts = arg;
    w->time = w->len = cmd_mirror_et * fps;
    w->mirrors = heap_block_tag(game_calloc(w->nmirror * 1348), str_eden_wall_c, 664);
    eden_wall_mirrors_place(w);
    eden_wall_mirrors_face(w);
    eden_wall_mirrors_restore(w);
    w->break_time = cmd_mode_ch_t * fps;
    w->laser_wait = cmd_ls_st_st * fps;
    w->flash_time = cmd_mi_bsh_st * fps;
    eden_core_shine_set_req(0, (int)&w->core);
    boss_se_play(30, 0, (int)&w->core);
    return (int *)w;
}

/* 0x00cc29a8 */
void eden_wall_mirror_seg_get(EdenWallWork *w, int i, int j, Vec3 **pos, Vec3 **ang)
{
    EdenMirror *mirror = &(w->mirrors)[i];
    EdenMirrorSeg *seg = &mirror->sub[j];

    *ang = &seg->ang;
    *pos = &seg->pos;
}

/* 0x00cc29e0 */
void eden_wall_get_dims(EdenWallWork *w, int *nmirror, int *nseg)
{
    *nmirror = w->nmirror;
    *nseg = 13;
}
