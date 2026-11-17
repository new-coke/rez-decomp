/*
 * Eden boss pieces: lockable cells covering the current form in rows (arm links, claw links,
 * mirror segments or snake rings). They charge up in rows and fire EB, UB or KB, shake when hit
 * and break off or shatter; the main task counts the cells left to end the phase.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "console.h"
#include "debug.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_piece.h"
#include "eden.h"
#include "eden_arm.h"
#include "eden_bullet.h"
#include "eden_claw.h"
#include "eden_core.h"
#include "eden_effect.h"
#include "eden_font.h"
#include "eden_snake.h"
#include "eden_wall.h"
#include "../common/boss_common.h"
#include "../common/boss_texture_anime.h"

/* 140 bytes */
typedef struct EdenPieceWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    int ncell;
    int step;
    int time;
    int len;
    int fire_time;
    int fire_cnt;
    EdenPieceCell *cells;
    int nrow;
    int ncol;
    int rows_left;
    int kb_mode;
    int attack;
    void *obj;
    void *font;
    void **obj_slot;
    void *parent;
    int kind;                   /* 0 arm, 1 claw, 2 wall, 3 snake */
    int snake;
    int row_break;              /* breaking one cell breaks its row */
    int live;
    int linked;
    int damage;
    int damage_frame;
    int hp;
    int hp_max;
    int hp_prev;
    float flare_scl;
    int frames;
    int alive;                  /* starts as the core hp; 0 once the gauge is empty */
    int core_hp_max;
    int se_req;
    int se_id;
} EdenPieceWork;

extern char eden_piece_vardef[];
extern GwVarDef eden_piece_vardefs[];
extern struct GwVarNode *eden_piece_vars_group;
extern float cmd_ch_lt;
extern int cmd_ch_n;
extern unsigned int eden_piece_cube_wire_col[];
extern Vec3 eden_piece_debris_vel;
extern float cmd_light_fv;
extern float cmd_earth_break_t;
extern float cmd_venus_break_t;
extern float cmd_eff_circle_r;
extern int cmd_eff_circle_frm;
extern Vec3 eden_piece_shatter_vel;
extern float cmd_piece_arm_at;
extern float eden_piece_arm_secs[];
extern int eden_piece_charge_rows[];
extern float cmd_pi_bv;
extern const float eden_piece_snake_sec;
extern float cmd_piece_ch_scl;
extern float cmd_ch_st;
extern float cmd_ch_mt;
extern float cmd_pi_scl;
extern Vec3 eden_piece_break_vel;
extern float cmd_piece_snake_at, cmd_or_break_rate;
extern const float cmd_piece_snake_st2;
extern const float cmd_piece_snake_st;
extern int cmd_piece_snake_at_num;
extern struct TaskDesc eden_piece_desc;
extern char str_eden_piece_c[];
extern int cmd_venus_piece_hp;
extern int cmd_uranus_piece_hp;
extern int cmd_venus_po_piece_hp;
extern int cmd_earth_piece_hp;
extern const int cmd_mars_core_hp, cmd_venus_core_hp;
extern const int cmd_uranus_core_hp;
extern const int cmd_earth_core_hp;
extern float cmd_piece_v, cmd_piece_dv, cmd_shine_rscl;
extern float cmd_shine_rscl_s;
extern const float cmd_piece_t;
extern float cmd_arm_hit_kf, cmd_arm_hit_mf, cmd_arm_hit_v, cmd_arm_hit_vlim, cmd_fall_vr, eden_piece_fall_g;
extern float cmd_pi_vmax, cmd_shine_rr, cmd_shine_rf;
extern const float cmd_shine_lim;

/* 0x00cbb708 */
void eden_piece_vars_add(void)
{
    eden_piece_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_piece_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_piece_vars_group, eden_piece_vardefs);
}

/* 0x00cbb758 */
void eden_piece_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_piece_vardef);
}

/* 0x00cbb770 */
void eden_piece_cell_break(EdenPieceWork *w, EdenPieceCell *mate)
{
    EdenPieceCell *cell = w->cells;
    int n = w->ncell;

    if (w->row_break) {
        for (; n; n--, cell++) {
            if (cell->row != mate->row || cell == mate)
                continue;
            if (!(cell->flags & 0x40))
                continue;
            if (cell->flags & 1) {
                Vec3 *pos = &cell->pos;

                cm_circle2_create(w->parent, pos, console_get_pcol(), 24, cmd_eff_circle_frm, cmd_eff_circle_r, 0.5f);
                eden_shatter_spawn(w, 2, w->parts[2], pos, &eden_piece_break_vel, cell->ang, eden_piece_cube_wire_col[w->kind]);
            }
            if (cell->flags & 2)
                lock_remove_entry(w->kind + 12, &cell->lock);
            if (cell->flags & 8)
                cell->flags = 265;
            else
                cell->flags = 0;
            cell->fire_time = 0;
        }
        w->rows_left--;
    }
    lock_remove_entry(w->kind + 12, &mate->lock);
    if (mate->flags & 8)
        mate->flags = 265;
    else
        mate->flags = 0;
    mate->fire_time = 0;
    cm_circle2_create(w->parent, &mate->pos, console_get_pcol(), 24, cmd_eff_circle_frm, cmd_eff_circle_r, 0.5f);
    eden_shatter_spawn(w, 2, w->parts[2], &mate->pos, &eden_piece_break_vel, mate->ang, eden_piece_cube_wire_col[w->kind]);
}

/* 0x00cbb9a8 */
void eden_piece_cells_shatter(EdenPieceWork *w)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;

    for (; n; n--, cell++) {
        if (cell->flags & 1) {
            Vec3 *pos = &cell->pos;

            cm_circle2_create(w->parent, pos, console_get_pcol(), 16, cmd_eff_circle_frm, cmd_eff_circle_r, 0.5f);
            eden_shatter_spawn(w, 2, w->parts[2], pos, &eden_piece_shatter_vel, cell->ang, eden_piece_cube_wire_col[w->kind]);
        }
        if (cell->flags & 2)
            lock_remove_entry(w->kind + 12, &cell->lock);
        cell->flags = 0;
    }
}

/* 0x00cbbac0 */
void eden_piece_cells_debris(EdenPieceWork *w)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;

    for (; n; n--, cell++) {
        if (cell->flags & 1)
            eden_shatter_model(w, 2, w->parts[2], &cell->pos, &eden_piece_debris_vel, cell->ang, eden_piece_cube_wire_col[w->kind]);
        if (cell->flags & 2)
            lock_remove_entry(w->kind + 12, &cell->lock);
        cell->flags = 0;
    }
}

/* 0x00cbbb80 */
EdenPieceCell *eden_piece_row_get(EdenPieceWork *w, EdenPieceCell *cell)
{
    int ofs;

    if ((unsigned int)(w->kind - 2) < 2)
        return 0;
    ofs = w->ncol * 220 - 220;
    return (EdenPieceCell *)((char *)cell - ofs);
}

/* 0x00cbbbb0 */
void eden_piece_charge_init(EdenPieceWork *w, EdenPieceCell *cell)
{
    cell->fire_time = cmd_ch_lt * FPS;
    cell->frame = 0;
    cell->frame_max = w->parts[4]->frames;
}

/* 0x00cbbbe8 */
void eden_piece_charge_step(EdenPieceWork *w, EdenPieceCell *row)
{
    EdenPieceCell *cell = eden_piece_row_get(w, row);
    EdenPieceCell *next;
    int n;
    float t;
    float s, c;

    if (!cell)
        return;
    if (cell->charge_cnt) {
        if (--cell->charge_time <= 0) {
            cell->charge_time = (int)(cmd_ch_st * FPS);
            cell->charge_spd = 1.5707964f / (cmd_ch_mt * FPS);
            cell->charge_cnt--;
            cell->charge = 0;
        }
    }
    for (n = w->ncol; n; n--, cell++) {
        next = n != 1 ? cell + 1 : 0;
        if (cell->charge_spd != 0.0f) {
            t = cell->charge;
            cell->charge += cell->charge_spd;
            if (1.5707964f <= cell->charge && t < 1.5707964f && next) {
                next->charge = 0;
                next->charge_spd = cell->charge_spd;
            }
            if (PI <= cell->charge) {
                cell->charge = cell->charge_spd = 0.0f;
            }
        }
        if (!(fabsf(cell->charge) < FLT_EPSILON)) {
            ulSinCosf(cell->charge, &s, &c);
            {
                float k = s;

                cell->scale.x = cell->scale.y = cell->scale.z = cmd_piece_ch_scl * k + 1.0f;
            }
        }
    }
}

/* 0x00cbbdc8 */
void eden_piece_cell_fire(EdenPieceWork *w, EdenPieceCell *cell)
{
    if (cell->fire_time) {
        cell->frame += cmd_light_fv;
        if (cell->frame_max <= cell->frame)
            cell->frame = cell->frame_max;
        if (--cell->fire_time <= 0) {
            if (w->snake == 1)
                eden_bullet_ub_fire(w, &cell->pos, (int)w->parts, 1);
            else {
                int *cell_data = cell->ang;

                if (!w->kb_mode)
                    eden_bullet_eb_create(w, &cell->pos, (int)w->parts, cell_data);
                else {
                    eden_bullet_kb_create(w, &cell->pos, &cell->vel, (char *)cell->ang + (((int)cell & 1) >> 1), (int)w->parts); /* steer(zero) */
                    w->se_req = 1;
                    w->se_id = 13;
                }
            }
        }
    }
}

/* 0x00cbbeb0 */
void eden_piece_charge_start(EdenPieceWork *w)
{
    int start;
    int i, k;
    int n = 0;
    EdenPieceCell *cell;

    start = (int)(RAND_FLOAT_A() * (float)w->nrow);
    for (i = 0; i < w->nrow; i++) {
        k = (start + i) % w->nrow;
        cell = &(w->cells)[(k + 1) * w->ncol - 1];
        if (!(cell->flags & 2))
            continue;
        if (eden_arm_man_get_chain(w->obj, k))
            continue;
        eden_piece_charge_init(w, cell);
        eden_arm_man_attack(w->obj, k, n++);
        w->se_req = 1;
        w->se_id = 9;
        eden_piece_row_get(w, cell)->charge_cnt = cmd_ch_n;
        if (!(n < eden_piece_charge_rows[boss_get_level()]))
            return;
    }
}

/* 0x00cbc040 */
void eden_piece_charge_reset(EdenPieceWork *w)
{
    int i, j;

    eden_arm_man_rotate_start(w->obj);
    for (i = 0; i < w->nrow; i = j) {
        EdenPieceCell *cell;

        j = i + 1;
        cell = &(w->cells)[j * w->ncol - 1];
        if (cell->flags & 2) {
            eden_piece_charge_init(w, cell);
            eden_piece_row_get(w, cell)->charge_cnt = cmd_ch_n;
        }
    }
}

/* 0x00cbc0f8 */
void eden_piece_arm_step(EdenPieceWork *w)
{
    int k;

    switch (w->step) {
    case 0:
        if (--w->time > 0)
            break;
        w->time = w->len = (int)(cmd_piece_arm_at * FPS);
        w->step = 1;
        break;
    case 1:
        if (--w->time > 0)
            break;
        if (!w->attack)
            k = (int)(RAND_FLOAT_A() * 2.0f) + 1;
        else {
            k = (int)(RAND_FLOAT_A() * 2.0f);
            if (k)
                k = w->attack == 1 ? 2 : 1;
        }
        w->attack = k;
        switch (k) {
        case 0:
            eden_arm_man_next_mode(w->obj);
            break;
        case 1:
            w->kb_mode = k;
            eden_piece_charge_reset(w);
            break;
        default:
            w->kb_mode = 0;
            eden_piece_charge_start(w);
            break;
        }
        k = boss_get_level();
        w->time = w->len = (int)(eden_piece_arm_secs[k] * FPS);
        break;
    }
}

/* 0x00cbc280 */
void eden_piece_obj_change(EdenPieceWork *w, int kind)
{
    int n;
    EdenPieceCell *cell;

    task_end_with_exit(w->obj);
    switch (kind) {
    case 0:
        w->obj = eden_snake_human_create(w->parent, (int)w->parts, 0);
        *w->obj_slot = w->obj;
        for (n = w->ncell, cell = w->cells; n; n--, cell++)
            eden_snake_human_point_get(w->obj, n - 1, (EdenHumanPt **)&cell->point, (char **)&cell->point_vec);
        break;
    case 1:
        w->obj = eden_snake_create(w->parent, (int *)w->parts);
        *w->obj_slot = w->obj;
        for (n = w->ncell, cell = w->cells; n; n--, cell++)
            eden_snake_cell_get(w->obj, cell->row, cell->col, (EdenSnakeCell **)&cell->point, (int **)&cell->point_vec);
        break;
    case 2:
        w->obj = eden_snake_orbit_create(w->parent, (int)w->parts);
        *w->obj_slot = w->obj;
        for (n = w->ncell, cell = w->cells; n; n--, cell++)
            eden_snake_orbit_point_get(w->obj, n - 1, (char **)&cell->point, (char **)&cell->point_vec);
        break;
    case 3:
        w->obj = eden_snake_human_create(w->parent, (int)w->parts, 1);
        *w->obj_slot = w->obj;
        for (n = w->ncell, cell = w->cells; n; n--, cell++)
            eden_snake_human_point_get(w->obj, n - 1, (EdenHumanPt **)&cell->point, (char **)&cell->point_vec);
        break;
    }
}

/* 0x00cbc448 */
void eden_piece_snake_step(EdenPieceWork *w)
{
    int y[3];
    int z[3];
    float k;

    switch (w->step) {
    case 0:
        if (--w->time > 0)
            break;
        w->step++;
        w->time = w->len = (int)(cmd_piece_snake_st * FPS);
        break;
    case 1:
        if (--w->time <= 0) {
            w->step++;
            w->time = w->len = (int)(cmd_piece_snake_st * FPS);
        } else if ((float)w->time == 120.0f)
            eden_snake_fire(w->obj, 0, 120);
        break;
    case 2:
        w->time = w->len = (int)(eden_piece_snake_sec * FPS);
        eden_piece_cells_shake(w, 1);
        w->step++;
        eden_snake_fire(w->obj, 1, 0);
        break;
    case 3:
        if (--w->time <= 0) {
            w->step++;
            eden_piece_cells_shake(w, 0);
        } else if (w->time == 1)
            eden_piece_obj_change(w, 0);
        break;
    case 4:
        if (!eden_snake_human_is_done(w->obj))
            break;
        y[0] = 0;
        y[1] = 0;
        y[2] = 0;
        boss_se_play(48, 0, (int)y);
        eden_piece_cells_shake(w, 1);
        w->step++;
        w->time = w->len = (int)(eden_piece_snake_sec * 0.5f * FPS);
        break;
    case 5:
        if (--w->time <= 0) {
            w->step = 6;
            eden_piece_cells_shake(w, 0);
            w->fire_cnt = cmd_piece_snake_at_num;
            w->fire_time = (int)(eden_piece_snake_sec * FPS);
            w->time = w->len = (int)(cmd_piece_snake_st2 * FPS);
        } else if (w->time == 1)
            eden_piece_obj_change(w, 2);
        break;
    case 6:
        if (--w->fire_time <= 0) {
            if (!w->kb_mode)
                eden_snake_orbit_fire(w->obj);
            w->fire_time = (int)(cmd_piece_snake_at * FPS);
        }
        k = (float)w->hp / (float)w->hp_max;
        if (--w->time > 0 && !(k <= cmd_or_break_rate))
            break;
        eden_piece_cells_shake(w, 1);
        ((void (*)(char *, int))eden_snake_orbit_set_end)(w->obj, 1);
        w->step++;
        w->time = w->len = (int)(eden_piece_snake_sec * 0.5f * FPS);
        break;
    case 7:
        if (--w->time <= 0) {
            w->step = 8;
            eden_piece_cells_shake(w, 0);
        } else if (w->time == 1) {
            eden_piece_obj_change(w, 3);
            eden_piece_cells_hide(w, 0);
        }
        break;
    case 8:
        if (eden_snake_human_is_done(w->obj)) {
            w->step++;
            z[0] = 0;
            z[1] = 0;
            z[2] = 0;
            eden_piece_cells_debris(w);
            boss_se_play(48, 0, (int)z);
        }
        break;
    }
}

/* 0x00cbc7e0 */
void eden_piece_exec(EdenPieceWork *w)
{
    Vec3 v;
    EdenPieceCell *p;
    int i, hits, n, left;
    float rate;

    switch (w->kind) {
    case 0:
        eden_piece_arm_step(w);
        break;
    case 1:
    case 2:
        break;
    case 3:
        eden_piece_snake_step(w);
        break;
    }
    w->frames++;
    w->hp_prev = w->hp;
    w->hp = 0;
    i = w->ncell;
    p = w->cells;
    w->live = 0;
    w->linked = 0;
    for (; i; p++, i--) {
        if (!(p->flags & 1))
            continue;
        w->live++;
        if (p->flags & 0x40)
            w->linked++;
        if (p->flags & 0x10) {
            p->flare = p->flare * cmd_shine_rr + (1.0f - cmd_shine_rr);
            p->flags |= 0x20;
            if (player_sphere_test(&p->pos, 1.0f) == 1)
                eden_player_damage();
        } else {
            p->flare *= cmd_shine_rf;
            if (p->flare < cmd_shine_lim) {
                p->flare = 0;
                p->flags &= ~0x20;
            }
        }
        if (p->flags & 4) {
            p->vel.x *= cmd_fall_vr;
            p->vel.z *= cmd_fall_vr;
            p->vel.y += eden_piece_fall_g;
            {
                float len = vec3_length(&p->vel);

                if (cmd_pi_vmax < len)
                    v3_scale(&p->vel, cmd_pi_vmax / len);
            }
            v3_add(&p->pos, &p->vel);
        } else {
            v3_copy(&p->vel, &p->pos);
            if (p->timer && --p->timer <= 0)
                eden_effect_bflash2_create(w, (int)&p->pos, (int)p->ang);
            point_approach(&p->rest, p->point, &p->pos, &p->ofs, 1.0f - (float)p->timer / (float)p->timer_max);
            v3_sub(&p->vel, &p->pos, &p->vel);
            i3_copy(p->ang, p->point_vec);
        }
        if (!(p->flags & 2))
            continue;
        spring_step_v(&p->spring_vel, &p->spring, cmd_arm_hit_mf, cmd_arm_hit_kf);
        v3_sum(&p->pos, &p->spring, &p->pos);
        eden_piece_cell_fire(w, p);
        eden_piece_charge_step(w, p);
        n = p->lock.life;
        w->hp += n;
        if (p->lock.flags & LOCK_HIT) {
            p->flags |= 0x400;
            if (n <= 0) {
                lock_score(w->kind + 12, &p->lock);
                if (w->kind == 2) {
                    if (p->row)
                        boss_se_play(45, (int)&p->lock, 0);
                } else
                    boss_se_play(w->kind + 43, (int)&p->lock, 0);
                eden_piece_cell_break(w, p);
                continue;
            }
            if (w->kind == 2) {
                if (!p->row)
                    boss_se_play(41, (int)&p->lock, 0);
            } else
                boss_se_play(w->kind + 39, (int)&p->lock, 0);
            {
                Vec3 *vel = &p->lock.imp.v;

                if (v3_is_zero(vel))
                    continue;
                v3_mul(&v, vel, cmd_arm_hit_v);
            }
            v3_add(&p->spring_vel, &v);
            if (vec3_length(&p->spring_vel) > cmd_arm_hit_vlim)
                vec_set_length(&p->spring_vel, cmd_arm_hit_vlim);
        } else
            p->flags &= ~0x400;
    }
    w->damage_frame = w->hp_prev - w->hp;
    w->damage += w->damage_frame;
    hits = eden_core_shine_get_hits(0);
    if (w->hp_max) {
        if (w->hp)
            rate = (float)w->hp / (float)w->hp_max;
        else
            rate = 0.0f;
    } else if (w->live)
        rate = (float)w->live / (float)w->ncell;
    else
        rate = 0.0f;
    left = w->core_hp_max - hits;
    rate *= (float)left / (float)w->core_hp_max;
    if (rate <= 0.0f || left <= 0) {
        console2_set_boss_rate(0.0f);
        w->alive = 0;
    } else {
        if (rate < 0.003921569f)
            console2_set_boss_rate(0.003921569f);
        else
            console2_set_boss_rate(rate);
        w->alive = 1;
    }
    if (w->se_req) {
        boss_se_play(w->se_id, 0, eden_core_shine_get_pos(0));
        w->se_req = 0;
    }
}

/* 0x00cbced0 */
void eden_piece_cells_draw(EdenPieceWork *w)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;
    float one = 1.0f;
    float sc = beat_curve(0) + one;
    WireObj *obj = (WireObj *)dc_object_get_compiled(w->parts[0]);

    ulGxSetTextureList(w->tex_list);
    boss_set_screen_scale(one, one);
    ((void (*)(float, float))draw_nop)(0.0f, 0.0f);
    draw_set_color(eden_piece_cube_wire_col[w->kind]);
    tex_anime_select();
    boss_set_screen_scale(one, one);
    for (; n; cell++, n--) {
        Vec3 *pos = &cell->pos;

        if ((cell->flags & 0x301) != 1)
            continue;
        if (!view_sphere_visible(pos, 10.0f))
            continue;
        ulMatrixPush();
        MTX_TRANS(pos);
        eden_piece_rotate_yxz(cell->ang);
        vu0_matrix_scale(ulMatrixCur, cmd_pi_scl, cmd_pi_scl, cmd_pi_scl);
        if (cell->flags & 0x80)
            vu0_matrix_scale(ulMatrixCur, sc, sc, sc);
        if (!(fabsf(cell->charge) < FLT_EPSILON)) {
            vu0_matrix_scale(ulMatrixCur, cell->scale.x, cell->scale.y, cell->scale.z);
            if (obj)
                tree_draw_culled_b_w(obj);
            else
                tree_draw_culled_a_w((WireObj *)w->parts[0]->root);
        } else if (obj)
            tree_draw_culled_b_w(obj);
        else
            tree_draw_culled_a_w((WireObj *)w->parts[0]->root);
        draw_set_blend(4, 5);
        dc_object_draw(w->parts[1]);
        ulMatrixPop();
    }
    ulGxSetTextureList(w->tex_list);
}

/* 0x00cbd0f0 */
void eden_piece_disp(EdenPieceWork *w)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;
    float one = 1.0f;
    float sc = beat_curve(0) + one;

    ulGxSetTextureList(w->tex_list);
    boss_set_screen_scale(one, one);
    ((void (*)(float, float))draw_nop)(0.0f, 0.0f);
    draw_set_color(eden_piece_cube_wire_col[w->kind]);
    for (; n; cell++, n--) {
        Vec3 *pos = &cell->pos;

        if (!(cell->flags & 1))
            continue;
        if (!view_sphere_visible(pos, 10.0f))
            continue;
        ulMatrixPush();
        MTX_TRANS(pos);
        do {
            do {
                eden_piece_rotate_yxz(cell->ang);
            } while (0);
        } while (0);
        vu0_matrix_scale(ulMatrixCur, cmd_pi_scl, cmd_pi_scl, cmd_pi_scl);
        if (cell->flags & 0x20) {
            int list = RenderSetCurrentDisplayList(6);
            int old_list;

            ulGxSetTextureList(w->tex_list);
            old_list = (int)fog_set_enable(0);
            draw_set_blend(4, 1);
            flare_draw_here(2, COL_WHITE, w->flare_scl * cell->flare);
            fog_set_enable((void *)old_list);
            RenderSetCurrentDisplayList(list);
        }
        if (!(cell->flags & 0x300)) {
            ulMatrixPop();
            do { } while (0);
            continue;
        }
        do {
            if (cell->flags & 0x80)
                vu0_matrix_scale(ulMatrixCur, sc, sc, sc);
            if (cell->fire_time) {
                ulMatrixPush();
                ulMatrixUnitRotation((volatile float *)ulMatrixCur);
                model_object_draw(w->parts[4], cell->frame);
                ulMatrixPop();
            }
        } while (0);
        if (!(fabsf(cell->charge) < FLT_EPSILON)) {
            vu0_matrix_scale(ulMatrixCur, cell->scale.x, cell->scale.y, cell->scale.z);
            if (!(cell->flags & 0x100)) {
                if (cell->flags & 0x200)
                    dc_object_draw(w->parts[3]);
            }
            draw_set_blend(4, 5);
            model_draw_kind_list(w->parts[1]->root, w->parts[1]->wire_list);
        } else {
            if (!(cell->flags & 0x100)) {
                if (cell->flags & 0x200)
                    dc_object_draw(w->parts[3]);
            }
            draw_set_blend(4, 5);
            model_draw_kind_list(w->parts[1]->root, w->parts[1]->wire_list);
        }
        ulMatrixPop();
    }
    eden_piece_cells_draw(w);
}

/* 0x00cbd3e0 */
void eden_piece_release(EdenPieceWork *w)
{
    EdenPieceCell *cell = w->cells;
    int i;

    for (i = 0; i < w->ncell; i++, cell++)
        if (cell->flags & 2)
            lock_remove_entry(w->kind + 12, &cell->lock);
    heap_free(w->cells, 0);
    console2_swap_boss_on(0);
}

/* 0x00cbd460 */
int eden_piece_create(void *w, int kind, int *tbl, int *arg, int k)
{
    int ang[3];
    EdenPieceWork *task = task_create_inherit(w, &eden_piece_desc, 140);
    EdenPieceCell *p;
    int i, n;

    *(int *)&task->parent = (int)w; /* steer(type) */
    task->parts = (ModelPart **)arg;
    task->obj = (void *)*tbl;
    task->font = (void *)kind;
    task->obj_slot = (void **)tbl;
    task->kind = k;
    switch (k) {
    case 0:
        eden_arm_man_get_counts(task->obj, &task->nrow, &task->ncol);
        {
            float scale = cmd_shine_rscl;
            task->row_break = 1;
            task->flare_scl = scale;
        }
        task->ncell = task->nrow * task->ncol;
        task->alive = task->core_hp_max = cmd_earth_core_hp;
        task->rows_left = task->nrow;
        break;
    case 1:
        eden_claw_get_counts(task->obj, &task->nrow, &task->ncol);
        task->flare_scl = cmd_shine_rscl;
        task->ncell = task->nrow * task->ncol;
        task->alive = task->core_hp_max = cmd_mars_core_hp;
        task->rows_left = task->nrow;
        break;
    case 2:
        eden_wall_get_dims(task->obj, &task->nrow, &task->ncol);
        {
            float scale = cmd_shine_rscl;
            task->row_break = 1;
            task->flare_scl = scale;
        }
        task->ncell = task->nrow * task->ncol;
        task->alive = task->core_hp_max = cmd_venus_core_hp;
        task->rows_left = task->nrow;
        break;
    default:
        eden_snake_get_dims(task->obj, &task->nrow, &task->ncol);
        {
            float scale = cmd_shine_rscl_s;
            task->snake = 1;
            task->flare_scl = scale;
        }
        task->ncell = task->nrow * task->ncol;
        task->alive = task->core_hp_max = cmd_uranus_core_hp;
        task->rows_left = task->nrow;
        break;
    }
    n = eden_font_count_live(task->font);
    task->cells = heap_block_tag(game_calloc(task->ncell * 220), str_eden_piece_c, 973);
    p = task->cells;
    task->live = task->ncell;
    for (i = 0; i < task->ncell; i++, p++) {
        eden_font_piece_point_random(task->font, (void *)(i % n), &p->pos, p->ang);
        v3_copy(&p->rest, &p->pos);
        p->row = i / task->ncol;
        p->col = i % task->ncol;
        p->flags = 1;
        switch (task->kind) {
        case 0:
            eden_arm_man_get_link(task->obj, p->row, p->col, (Vec3 **)&p->point, (int **)&p->point_vec);
            p->flags |= 0x40;
            if (p->col == task->ncol - 1) {
                p->flags |= 0x242;
                lock_init(task->kind + 12, &p->lock, task, &p->pos, 0, cmd_earth_piece_hp, 0);
                lockon_entry(&p->lock);
                task->hp += cmd_earth_piece_hp;
            }
            break;
        case 1:
            eden_claw_get_link(task->obj, p->row, p->col, (Vec3 **)&p->point, (Vec3 **)&p->point_vec);
            break;
        case 2:
            eden_wall_mirror_seg_get(task->obj, p->row, p->col, (Vec3 **)&p->point, (Vec3 **)&p->point_vec);
            if (!p->row) {
                if (p->col) {
                    int hp = cmd_venus_po_piece_hp;

                    p->flags |= 0x2c2;
                    lock_init(task->kind + 12, &p->lock, task, &p->pos, 0, hp, 0);
                    lockon_entry(&p->lock);
                    task->hp += cmd_venus_po_piece_hp;
                } else
                    p->flags |= 0x140;
            } else if (p->col)
                p->flags |= 0x40;
            else {
                p->flags |= 0x242;
                lock_init(task->kind + 12, &p->lock, task, &p->pos, 0, cmd_venus_piece_hp, 0);
                lockon_entry(&p->lock);
                p->lock.flags &= ~LOCK_LOCKABLE;
                task->hp += cmd_venus_piece_hp;
            }
            break;
        default: {
            int hp;

            eden_snake_cell_get(task->obj, p->row, p->col, (EdenSnakeCell **)&p->point, (int **)&p->point_vec);
            hp = cmd_uranus_piece_hp;
            p->flags |= 0xa;
            lock_init(task->kind + 12, &p->lock, task, &p->pos, 0, hp, 0);
            lockon_entry(&p->lock);
            task->hp += cmd_uranus_piece_hp;
            break;
        }
        }
        p->timer = p->timer_max = (int)(cmd_piece_t * FPS);
        p->ofs.z = cmd_piece_v + RAND_FLOAT_A() * cmd_piece_dv;
        do {
            ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        } while (0);
        ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        do { ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f); } while (0);
        vec_rotate_zxy(&p->ofs, ang);
    }
    if (task->kind == 2)
        eden_wall_mirrors_link(task, task->obj);
    task->hp_max = task->hp;
    task->time = task->len = (int)(cmd_piece_t * FPS);
    boss_gage_create(0);
    eden_core_shine_clear_hits(0);
    return (int)task;
}

/* 0x00cbda80 */
void eden_piece_shatter(EdenPieceWork *w)
{
    eden_piece_cells_shatter(w);
}

/* 0x00cbda88 */
int eden_piece_get_remain(EdenPieceWork *w)
{

    if (w->alive <= 0)
        return 0;
    switch (w->kind) {
    case 0:
        if (cmd_earth_break_t * FPS <= (float)w->frames)
            return 0;
        return w->rows_left;
    case 1:
        return eden_claw_is_active(w->obj);
    case 2:
        if (cmd_venus_break_t * FPS <= (float)w->frames)
            return 0;
        if (!w->live)
            return 0;
        break;
    case 3:
        if (!w->live || !w->hp)
            return 0;
        break;
    }
    return 1;
}

/* 0x00cbdb78 */
void eden_piece_cells_hide(EdenPieceWork *w, int on)
{
    EdenPieceCell *cell = w->cells;
    int n = w->ncell;

    if (on) {
        for (; n; n--, cell++)
            if (cell->flags & 1)
                cell->flags |= 8;
    } else {
        for (; n; n--, cell++)
            cell->flags &= ~8;
    }
}

/* 0x00cbdbe8 */
void eden_piece_cells_shake(EdenPieceWork *w, int flag)
{
    EdenPieceCell *cell = w->cells;
    int n = w->ncell;
    int frm;

    if (flag) {
        for (; n; n--, cell++) {
            if (!(cell->flags & 1))
                continue;
            cell->flags |= 4;
            cell->vel.x += RAND_FLOAT_A() * (cmd_pi_bv + cmd_pi_bv) - cmd_pi_bv;
            cell->vel.y += RAND_FLOAT_A() * (cmd_pi_bv + cmd_pi_bv) - cmd_pi_bv;
            cell->vel.z += RAND_FLOAT_A() * (cmd_pi_bv + cmd_pi_bv) - cmd_pi_bv;
        }
    } else {
        for (; n; n--, cell++) {
            if (!(cell->flags & 1))
                continue;
            frm = (int)(eden_piece_snake_sec * FPS);
            cell->flags &= ~4;
            v3_copy(&cell->ofs, &cell->vel);
            v3_copy(&cell->rest, &cell->pos);
            cell->timer = cell->timer_max = frm;
        }
    }
}

/* 0x00cbdd80 */
int eden_piece_cell_get_pose(EdenPieceWork *w, int row, int col, Vec3 *pos, Vec3 *dir, int *ang)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;

    for (; n; n--, cell++) {
        if (cell->row == row && cell->col == col) {
            if (cell->flags & 1) {
                if (pos)
                    v3_copy(pos, &cell->pos);
                if (dir)
                    v3_copy(dir, &cell->vel);
                if (ang)
                    i3_copy(ang, cell->ang);
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

/* 0x00cbde38 */
int eden_piece_cell_get_flags(EdenPieceCell *cell)
{
    return cell->flags;
}

/* 0x00cbde40 */
EdenPieceCell *eden_piece_cell_find(EdenPieceWork *w, int row, int col)
{
    int n = w->ncell;
    EdenPieceCell *cell = w->cells;

    for (; n; n--, cell++)
        if (cell->row == row && cell->col == col)
            return cell;
    return 0;
}

/* 0x00cbde80 */
void eden_piece_cell_command(EdenPieceWork *w, int row, int col, int cmd)
{
    EdenPieceCell *cell = w->cells;
    int n = w->ncell;

    switch (cmd) {
    case -1:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f1;
        break;
    case 2:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f2;
        break;
    case -2:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f3;
        break;
    case 3:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f4;
        break;
    case -3:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f5;
        break;
    f1:
        do {
            eden_piece_cell_break(w, cell);
        } while (0);
        break;
    f2:
        cell->flags |= 0x10;
        break;
    f3:
        cell->flags &= ~0x10;
        break;
    f4:
        cell->flags |= 0x80;
        break;
    f5:
        cell->flags &= ~0x80;
        break;
    f6:
        cell->flags &= ~0x90;
        break;
    case -4:
        for (; n; n--, cell++)
            if (cell->row == row && cell->col == col)
                goto f6;
        break;
    }
}

/* 0x00cbe030 */
void eden_piece_cells_command(EdenPieceWork *w, int mode)
{
    EdenPieceCell *cell = w->cells;
    int n = w->ncell;

    switch (mode) {
    case -1:
        for (; n; n--, cell++) {
            eden_piece_cell_break(w, cell);
            cell->flags = 0;
        }
        break;
    case 2:
        for (; n; n--, cell++)
            if (cell->flags & 2)
                cell->flags |= 0x10;
        break;
    case -2:
        for (; n; n--, cell++)
            cell->flags &= ~0x10;
        break;
    }
}

/* 0x00cbe130 */
void eden_piece_wall_cell_break(EdenPieceWork *w, EdenPieceCell *cell)
{
    eden_piece_cell_break(w, cell);
}

/* 0x00cbe138 */
void eden_piece_set_hold(EdenPieceCell *cell, int on)
{
    if (on)
        cell->lock.flags |= LOCK_LOCKABLE;
    else
        cell->lock.flags &= ~LOCK_LOCKABLE;
}

/* 0x00cbe160 */
int eden_piece_get_damage(EdenPieceWork *w)
{
    return w->damage;
}

/* 0x00cbe168 */
void eden_piece_clear_damage(EdenPieceWork *w)
{
    w->damage = 0;
}

/* 0x00cbe170 */
void eden_piece_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
