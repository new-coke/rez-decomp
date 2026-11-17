/*
 * Eden boss piece text: a line of text (eden_font_texts) in wire glyphs, one lockable cluster per
 * character, that flies in front of the player. A glyph shot or out of time scores, sends a shock
 * wave through its neighbours and seeds the next piece group.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "bs_glyph.h"
#include "debug.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_font.h"
#include "eden_dust.h"
#include "eden_effect.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_texture_anime.h"

extern char eden_font_vardef[];
extern GwVarDef eden_font_vardefs[];
extern struct GwVarNode *eden_font_vars_group;
extern float cmd_font_px;
extern float cmd_font_scl;
extern float eden_font_bcircle_r;
extern int eden_font_bcircle_frm;
extern float cmd_set_part_px;
extern float cmd_set_part_py;
extern float cmd_set_part_pz;
extern float cmd_dust_z;
extern float cmd_dust_y;
extern float cmd_at_piece_sz;
extern float cmd_at_piece_sy;
extern float cmd_at_piece_x;
extern float cmd_at_piece_y;
extern float cmd_at_piece_z;
extern char eden_font_desc[], str_eden_font_c[];
extern char *eden_font_texts[];
extern float cmd_font_z;
extern int cmd_font_num_max;
extern float cmd_font_st_py, cmd_font_py, cmd_font_ry;
extern int cmd_test_disp;
extern float cmd_font_bscl;
extern float eden_font_fade[4];
extern float eden_font_do_mf, eden_font_do_kf, eden_font_do_kr, eden_font_do_pmf, eden_font_do_pkf;
extern float eden_font_do_atlen, cmd_do_atlen2, eden_font_st_alpha;
extern const float eden_font_rang_rate;
extern const float eden_font_stf_alpha;

/* 0x00cb4360 */
void eden_font_vars_add(void)
{
    eden_font_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_font_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_font_vars_group, eden_font_vardefs);
}

/* 0x00cb43b0 */
void eden_font_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_font_vardef);
}

/* 0x00cb43c8 */
int eden_font_glyph_vertex(void *owner, Vec3 *out, int i, char ch)
{
    Vec3 *verts;
    int count;

    glyph_lookup(ch, (void **)&verts, &count);
    if (out) {
        float scale = cmd_font_scl * 16.0f;

        v3_copy(out, (Vec3 *)((float *)verts + i * 6));
        v3_mul(out, (Vec3 *)((float *)verts + i * 6), scale);
    }
    return count;
}

/* 0x00cb4470 */
int eden_font_count_chars(char *str)
{
    int n = 0;

    while (*str) {
        if ((unsigned char)(*str - 33) <= 93)
            n++;
        str++;
    }
    return n;
}

/* 0x00cb44b8 */
float eden_font_line_offset(char *str)
{
    int n = 0;

    while (*str != '\n' && *str) {
        n++;
        str++;
    }
    return (float)(n - 1) * -0.5f * cmd_font_px;
}

/* 0x00cb4520 */
void eden_font_text_build(void *work, char *str)
{
    EdenText *w = work;
    float x = eden_font_line_offset(str);
    float y = cmd_font_st_py;
    int n;
    char *item;
    float yaw_sin, yaw_cos;

    n = w->n = eden_font_count_chars(str);
    item = (char *)(w->g = (EdenOrb *)heap_block_tag(game_calloc(n * 188), str_eden_font_c, 188));
    while (n) {
        int k, j, yaw;
        char *it;
        WireObj *obj;
        float rx, rz;

        while (*str == ' ' || *str == '\n') {
            if (*str == ' ')
                x += cmd_font_px;
            else if (*str == '\n') {
                x = eden_font_line_offset(str + 1);
                y += cmd_font_py;
            }
            str++;
        }
        ((Vec3 *)(item + 24))->z = 0.0f;
        rz = cmd_font_z;
        rx = ((Vec3 *)(item + 24))->z;
        ((Vec3 *)(item + 24))->x = x;
        ((Vec3 *)(item + 24))->y = y;
        yaw = (int)DEG_TO_ANG(x * cmd_font_ry / cmd_font_px);
        *(int *)(item + 88) = yaw;
        ROTS_Y(yaw, yaw_sin, yaw_cos, rx, rz);
        ((Vec3 *)(item + 24))->z = rz - cmd_font_z;
        ((Vec3 *)(item + 24))->x = rx;
        k = *(int *)(item + 4) = eden_font_glyph_vertex(w, 0, 0, *str);
        it = *(char **)item = heap_block_tag(game_calloc(k * 220), str_eden_font_c, 223);
        obj = dc_object_get_compiled(*(ModelPart **)((char *)w->arg + 84));
        for (j = 0; k; j++, k--, it += 220) {
            eden_font_glyph_vertex(w, (Vec3 *)(it + 88), j, *str);
            i3_copy((int *)(it + 160), (int *)(item + 84));
            if (obj) {
                int owner_arg = w->arg;
                *(int *)(it + 216) = (int)obj;
                *(void **)(it + 212) = (void *)1;
                *(int *)(it + 188) = *(int *)(*(char **)((char *)owner_arg + 84) + 4);
            } else {
                *(float *)(it + 212) = 0.0f;
                *(int *)(it + 188) = *(int *)(*(char **)((char *)w->arg + 84) + 4);
            }
        }
        str++;
        x += cmd_font_px;
        item += 188;
        n--;
    }
}

/* 0x00cb4858 */
EdenOrb *eden_font_next_rec(int *w)
{
    if (w[11] >= w[10])
        return 0;
    return &((EdenOrb *)w[3])[((int *)w[12])[w[11]++]];
}

/* 0x00cb48a0 */
int eden_font_glyph_step(void *w, EdenOrb *item, Vec3 *pos, Vec3 *move)
{
    Vec3 v;
    EdenOrb *rec;
    float len_a, len_b;
    int ret;
    int timer;

    if (*(int *)((char *)item + 12) & 0x20) {
        if (eden_font_stf_alpha > (*(float *)((char *)item + 180) *= eden_font_st_alpha)) {
            *(int *)((char *)item + 180) = 0;
            *(int *)((char *)item + 12) &= ~0x20;
        }
    }
    if (*(int *)((char *)item + 12) & 4) {
        if (--*(int *)((char *)item + 20) > 0) {
            if (!(((LockObj *)((char *)item + 108))->flags & LOCK_HIT))
                goto skip;
            if (((LockObj *)((char *)item + 108))->life > 0)
                goto skip;
            lock_score(18, (LockObj *)((char *)item + 108));
        }
        eden_font_shock_wave(w, (Vec3 *)((char *)item + 36));
        lock_remove_entry(18, (LockObj *)((char *)item + 108));
        *(int *)((char *)item + 12) = (*(int *)((char *)item + 12) | 1) & ~4;
        rec = eden_font_next_rec(w);
        if (rec) {
            v.x = cmd_set_part_px;
            v.y = cmd_set_part_py;
            v.z = cmd_set_part_pz;
            eden_dust_grp_seed(w, rec, (Vec3 *)((char *)w + 28), &v, *(int *)((char *)w + 8));
        }
    }
skip:
    if (*(int *)((char *)item + 12) & 0x80) {
        float k;

        v3_sum((Vec3 *)((char *)item + 36), pos, (Vec3 *)((char *)item + 24));
        i3_sum((int *)((char *)item + 60), (int *)((char *)item + 72), (int *)((char *)item + 60));
        do {
            *(int *)((char *)item + 60) = ANG_WRAP(*(int *)((char *)item + 60));
            *(int *)((char *)item + 64) = ANG_WRAP(*(int *)((char *)item + 64));
            *(int *)((char *)item + 68) = ANG_WRAP(*(int *)((char *)item + 68));
        } while (0);
        *(float *)((char *)item + 176) = *(float *)((char *)item + 176) * eden_font_rang_rate + (1.0f - eden_font_rang_rate);
        k = 1.0f - *(float *)((char *)item + 176);
        i3_mul((int *)((char *)item + 72), (int *)((char *)item + 72), k);
        angles_blend((int *)((char *)item + 60), (int *)((char *)item + 84), (int *)((char *)item + 60), k);
        return 0;
    }
    if (*(int *)((char *)item + 12) & 2) {
        float k;

        *(float *)((char *)item + 172) = *(float *)((char *)item + 172) * eden_font_do_kr + eden_font_do_kf * (1.0f - eden_font_do_kr);
        i3_sum((int *)((char *)item + 60), (int *)((char *)item + 72), (int *)((char *)item + 60));
        v3_add((Vec3 *)((char *)item + 36), move);
        v3_add((Vec3 *)((char *)item + 48), move);
        do {
            *(int *)((char *)item + 60) = ANG_WRAP(*(int *)((char *)item + 60));
            *(int *)((char *)item + 64) = ANG_WRAP(*(int *)((char *)item + 64));
            *(int *)((char *)item + 68) = ANG_WRAP(*(int *)((char *)item + 68));
        } while (0);
        *(float *)((char *)item + 176) = *(float *)((char *)item + 176) * eden_font_rang_rate + (1.0f - eden_font_rang_rate);
        k = 1.0f - *(float *)((char *)item + 176);
        i3_mul((int *)((char *)item + 72), (int *)((char *)item + 72), k);
        angles_blend((int *)((char *)item + 60), (int *)((char *)item + 84), (int *)((char *)item + 60), k);
        v3_sum(&v, pos, (Vec3 *)((char *)item + 24));
        spring_to_v((Vec3 *)((char *)item + 96), (Vec3 *)((char *)item + 36), &v, eden_font_do_mf, *(float *)((char *)item + 172));
        v3_sub(&v, (Vec3 *)((char *)item + 36), &v);
        len_a = vec3_length(&v);
        len_b = vec3_length((Vec3 *)((char *)item + 96));
        if (len_a < eden_font_do_atlen && len_b < eden_font_do_atlen)
            *(int *)((char *)item + 12) |= 0x40;
        if (len_a < cmd_do_atlen2 && len_b < cmd_do_atlen2) {
            *(int *)((char *)item + 12) |= 0x80;
            if (!*(int *)((char *)w + 52)) {
                *(int *)((char *)w + 52) = 1;
                boss_se_play(4, 0, (int)((char *)item + 36));
            }
        }
        return 0;
    }
    v3_add((Vec3 *)((char *)item + 36), move);
    i3_sum((int *)((char *)item + 60), (int *)((char *)item + 72), (int *)((char *)item + 60));
    v3_add((Vec3 *)((char *)item + 48), move);
    do {
        *(int *)((char *)item + 60) = ANG_WRAP(*(int *)((char *)item + 60));
        *(int *)((char *)item + 64) = ANG_WRAP(*(int *)((char *)item + 64));
        *(int *)((char *)item + 68) = ANG_WRAP(*(int *)((char *)item + 68));
    } while (0);
    spring_to_v((Vec3 *)((char *)item + 96), (Vec3 *)((char *)item + 36), (Vec3 *)((char *)item + 48), eden_font_do_pmf, eden_font_do_pkf);
    timer = *(int *)((char *)item + 16);
    if (timer) {
        ret = 1;
        *(int *)((char *)item + 16) = --timer;
        if (timer <= 0)
            goto done;
    }
    ret = 0;
done:
    return ret;
}

/* 0x00cb4f48 */
void eden_font_exec(void *work)
{
    EdenText *w = work;
    Vec3 hit;
    Vec3 ofs;
    Vec3 target_grp, target_piece, target_item;
    float z = 0.0f;
    int n;
    EdenOrb *item;

    ofs.z = cmd_font_z;
    ofs.y = z;
    ofs.x = z;
    player_get_pos(&w->pos);
    v3_add(&w->pos, &ofs);
    w->ofs.x = z;
    w->ofs.y = cmd_dust_y;
    w->ofs.z = cmd_dust_z;
    player_get_frame_vel(&hit);
    for (n = w->n, item = w->g; n;) {
        int live, rem, k;
        char *it;

        if (item->flags & 0x10) {
            item++;
            n--;
            continue;
        }
        if (eden_font_glyph_step(w, item, &w->pos, &hit)) {
            target_grp.x = cmd_set_part_px;
            target_grp.y = cmd_set_part_py;
            target_grp.z = cmd_set_part_pz;
            eden_dust_grp_retarget(item, &w->ofs, &target_grp);
        }
        live = rem = item->ngrp;
        it = *(char **)item;
        for (k = item->ngrp; k; it += 220, k--) {
            int flags, ngrp, j;
            char *sub;

            if (eden_dust_item_step(w, it, (Vec3 *)((char *)item + 36), (int *)((char *)item + 60), &hit, item->flags & 1)) {
                target_piece.x = cmd_set_part_px;
                target_piece.y = cmd_set_part_py;
                target_piece.z = cmd_set_part_pz;
                eden_dust_piece_retarget(it, &w->ofs, &target_piece);
            }
            flags = *(int *)(it + 8);
            live -= flags & 1;
            if (flags & 2) {
                if (flags & 0x40)
                    rem--;
                continue;
            }
            ngrp = *(int *)(it + 4);
            sub = *(char **)it;
            for (j = ngrp; j; sub += 96, j--) {
                if (eden_dust_shard_step(sub, (Vec3 *)(it + 100), (int *)(it + 136), &hit, *(int *)(it + 8) & 1)) {
                    target_item.x = cmd_set_part_px;
                    target_item.y = cmd_set_part_py;
                    target_item.z = cmd_set_part_pz;
                    eden_dust_item_retarget(sub, &w->ofs, &target_item);
                }
                if (*(int *)(sub + 72) & 2)
                    ngrp--;
            }
            if (!ngrp) {
                *(int *)(it + 8) |= 2;
                heap_free(*(void **)it, 0);
                *(void **)it = 0;
                *(int *)(it + 4) = 0;
            }
        }
        if (!live) {
            if (!(item->flags & 8)) {
                item->timer = 0;
                item->flags |= 0x29;
            } else if (!rem)
                item->flags |= 2;
        }
        item++;
        n--;
    }
    *(float *)((char *)w + 56) = beat_curve(0);
}

/* 0x00cb52d8 */
void eden_font_debug_box_draw(void)
{
    Vec3 line[2];
    Vec3 ofs;

    ofs.x = 0;
    ofs.z = cmd_dust_z;
    ofs.y = cmd_dust_y;
    ulMatrixPush();
    MTX_TRANS(&ofs);
    lines_begin(0.5f, RGBA(0, 0, 0x80, 0x80), 0);
    line[0].x = -cmd_set_part_px;
    line[1].x = cmd_set_part_px;
    line[0].y = line[1].y = cmd_set_part_py;
    line[0].z = line[1].z = -cmd_set_part_pz;
    lines_add_pairs(line, 2);
    line[0].x = -cmd_set_part_px;
    line[0].y = line[1].y = cmd_set_part_py;
    line[1].x = cmd_set_part_px;
    line[0].z = line[1].z = cmd_set_part_pz;
    lines_add_pairs(line, 2);
    line[0].x = -cmd_set_part_px;
    line[1].x = cmd_set_part_px;
    line[0].y = line[1].y = -cmd_set_part_py;
    line[0].z = line[1].z = -cmd_set_part_pz;
    lines_add_pairs(line, 2);
    line[0].x = -cmd_set_part_px;
    line[1].x = cmd_set_part_px;
    line[0].y = line[1].y = -cmd_set_part_py;
    line[0].z = line[1].z = cmd_set_part_pz;
    lines_add_pairs(line, 2);
    lines_end();
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00cb5450 */
void eden_font_disp(void *work)
{
    EdenText *w = work;
    int old_blend, n, k, j;
    EdenOrb *item;
    char *it, *sub;
    float sc;

    if (cmd_test_disp)
        eden_font_debug_box_draw();
    ulGxSetTextureList(*(void **)((char *)w + 4));
    boss_set_screen_scale(1.0f, 1.0f);
    ((void (*)(float, float))draw_nop)(0.0f, 0.0f);
    old_blend = draw_set_override_flags(0);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    tex_anime_select();
    for (n = w->n, item = w->g; n;) {
        if (item->flags & 0x10) {
            item++;
            n--;
            continue;
        }
        k = item->ngrp;
        it = *(char **)item;
        if (item->flags & 4) {
            ulMatrixPush();
            MTX_TRANS((Vec3 *)((char *)item + 36));
            eden_font_rotate_yxz((int *)((char *)item + 60));
            sc = cmd_font_bscl * w->bounce + 1.0f;
            vu0_matrix_scale(ulMatrixCur, sc, sc, sc);
            if (item->flags & 0x20) {
                draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
                eden_font_fade[0] = -*(float *)((char *)item + 180);
                draw_set_override_color(eden_font_fade);
                draw_set_override_flags(0);
            }
            ulMatrixPop();
        }
        for (; k; it += 220, k--) {
            if (view_sphere_visible((Vec3 *)(it + 100), 10.0f)) {
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(it + 100));
                eden_font_rotate_yxz((int *)(it + 136));
                if (*(int *)(it + 8) & 4) {
                    if (item->flags & 0x20) {
                        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
                        eden_font_fade[0] = -*(float *)((char *)item + 180);
                        draw_set_override_color(eden_font_fade);
                        draw_set_override_flags(0);
                    }
                } else if (*(int *)(it + 8) & 2) {
                    if (*(int *)(it + 212))
                        tree_draw_culled_b_w(*(WireObj **)(it + 216));
                    else
                        tree_draw_culled_a_w(*(WireObj **)(it + 188));
                }
                ulMatrixPop();
            }
            for (j = *(int *)(it + 4), sub = *(char **)it; j; j--, sub += 96) {
                if (*(int *)(it + 8) & 2)
                    continue;
                ulMatrixPush();
                MTX_TRANS((Vec3 *)(sub + 12));
                eden_font_rotate_yxz((int *)(sub + 48));
                if (item->flags & 0x20) {
                    draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
                    eden_font_fade[0] = -*(float *)((char *)item + 180);
                    draw_set_override_color(eden_font_fade);
                    nj_cnk_vu1_draw(*(void **)(*(char **)(sub + 92) + 4));
                    draw_set_override_flags(0);
                } else {
                    nj_cnk_vu1_draw(*(void **)(*(char **)(sub + 92) + 4));
                }
                ulMatrixPop();
            }
        }
        item++;
        n--;
    }
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(old_blend);
    ulGxSetTextureList(*(void **)((char *)w + 4));
}

/* 0x00cb57d0 */
void eden_font_grps_free(EdenText *w)
{
    int n = w->n;
    EdenOrb *item = w->g;

    for (; n; item++, n--) {
        int ngrp;
        EdenGrp *grp;

        if (item->flags & 4)
            lock_remove_entry(18, &item->lock);
        ngrp = item->ngrp;
        grp = item->grp;
        for (; ngrp; ngrp--, grp++) {
            eden_dust_lock_remove(grp);
            if (grp->items)
                heap_free(grp->items, 0);
        }
        heap_free(item->grp, 0);
    }
    heap_free(w->g, 0);
}

/* 0x00cb58b0 */
void eden_font_release(EdenText *w)
{
    eden_font_grps_free(w);
    heap_free(w->order, 0);
}

/* 0x00cb58e0 */
EdenText *eden_font_create(void *parent, int arg, int k)
{
    EdenText *w = task_create_inherit(parent, eden_font_desc, 60);
    float z = 0.0f;
    Vec3 ofs;
    Vec3 box;
    int i;

    w->arg = arg;
    ofs.z = cmd_font_z;
    ofs.y = z;
    ofs.x = z;
    player_get_pos(&w->pos);
    v3_add(&w->pos, &ofs);
    w->ofs.x = z;
    w->ofs.y = cmd_dust_y;
    w->ofs.z = cmd_dust_z;
    eden_font_text_build(w, eden_font_texts[k]);
    w->order = heap_block_tag(game_malloc(w->n * 4), str_eden_font_c, 678);
    for (i = 0; i < w->n; i++) {
        w->order[i] = i;
        w->g[i].flags |= 0x10;
    }
    shuffle_ints(w->order, w->n);
    for (i = 0; i < cmd_font_num_max; i++) {
        EdenOrb *item = eden_font_next_rec((int *)w);

        if (item) {
            box.x = cmd_set_part_px;
            box.y = cmd_set_part_py;
            box.z = cmd_set_part_pz;
            eden_dust_grp_seed(w, item, &w->ofs, &box, w->arg);
        }
    }
    box.x = 0;
    box.y = 0;
    box.z = 0;
    boss_se_play(3, 0, (int)&box);
    return w;
}

/* 0x00cb5b08 */
int eden_font_is_done(EdenText *w)
{
    int n = w->next;
    EdenOrb *item;
    int i;

    if (n != w->n)
        return 0;
    item = w->g;
    for (i = 0; i < n; i++, item++)
        if (!(item->flags & 0x40))
            return 0;
    return 1;
}

/* 0x00cb5b50 */
int eden_font_count_live(EdenText *w)
{
    EdenOrb *item = w->g;
    int n = 0;
    int i;

    for (i = 0; i < w->n; i++, item++)
        n += item->ngrp;
    return n;
}

/* 0x00cb5b90 */
void eden_font_piece_point_random(void *w, void *slot, Vec3 *pos, int *ang)
{
    pos->z = cmd_at_piece_sz + (RAND_FLOAT_A() * (cmd_at_piece_z + cmd_at_piece_z) - cmd_at_piece_z);
    pos->x = RAND_FLOAT_A() * (cmd_at_piece_x + cmd_at_piece_x) - cmd_at_piece_x;
    pos->y = cmd_at_piece_sy + (RAND_FLOAT_A() * (cmd_at_piece_y + cmd_at_piece_y) - cmd_at_piece_y);
    do {
        ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
}

/* 0x00cb5d30 */
void eden_font_get_item_pose(EdenText *w, int idx, Vec3 *pos, int *ang)
{
    EdenOrb *item;
    int sum;
    int i;

    i = 0;
    item = w->g;
    sum = 0;

    if (i < *(volatile int *)&w->n) do {
        sum += item->ngrp;
        if (idx <= sum - 1) {
            char *it = *(char **)item + (idx - (sum - item->ngrp)) * 220;
            Vec3 *item_pos = (Vec3 *)(it + 100);
            int *src = (int *)(it + 136);

            pos->x = item_pos->x;
            ang[0] = src[0];
            pos->y = item_pos->y;
            ang[1] = src[1];
            pos->z = item_pos->z;
            ang[2] = src[2];
            return;
        }
    } while (i++, item++, i < w->n);
    pos->z = 0;
    ang[2] = 0;
    pos->x = 0;
    do {
        pos->y = 0;
    } while (0);
    ang[0] = 0;
    ang[1] = 0;
}

/* 0x00cb5de0 */
void eden_font_end(EdenText *w)
{
    int n = w->n;
    EdenOrb *item = w->g;

    for (; n; item++, n--) {
        int ngrp;
        char *p;

        if (item->flags & 4) {
            lock_remove_entry(18, (LockObj *)((char *)item + 108));
            item->flags &= ~4;
        }
        ngrp = item->ngrp;
        p = *(char **)item;
        for (; ngrp; ngrp--, p += 220)
            eden_dust_lock_remove((int *)p);
    }
}

/* 0x00cb5e98 */
void eden_font_shine_create(void *w, int arg)
{
    eden_effect_dshine_create(w, arg);
}

/* 0x00cb5ea0 */
void eden_font_shock_wave(EdenText *w, Vec3 *pos)
{
    EdenOrb *orb;
    int i;
    int k, delay;
    EdenGrp *grp;
    float lim;
    float dsq;

    eden_font_shine_create(w, (int)pos);
    lim = eden_font_bcircle_r * eden_font_bcircle_r;
    i = 0;
    orb = w->g;
    if (0 < *(volatile int *)&w->n) do {
        if (orb->flags & 0x14)
            continue;
        for (k = orb->ngrp, grp = orb->grp; k; k--, grp++) {
            if (!(grp->flags & 4))
                continue;
            dsq = vec3_distance_squared(pos, &grp->pos);
            if (!(dsq < lim))
                continue;
            dsq = sqrtf(dsq);
            delay = (int)((float)eden_font_bcircle_frm * dsq / eden_font_bcircle_r);
            if (!grp->delay || delay < grp->delay)
                grp->delay = delay;
        }
    } while (i++, orb++, i < w->n);
}

/* 0x00cb6008 */
void eden_font_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
