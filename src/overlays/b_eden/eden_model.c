/*
 * Eden boss final stage: the model manager brings up the body part by part (wire orbs of lockable
 * pieces), with the belts, core and MB drones; belts first, then the open core. Between parts it
 * plays the camera cut; after the last it starts the ending.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "camera.h"
#include "color.h"
#include "fade.h"
#include "gw_varedit.h"
#include "heap.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "screen.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_model.h"
#include "eden.h"
#include "eden_belt.h"
#include "eden_core.h"
#include "eden_dust.h"
#include "eden_effect.h"
#include "eden_enemy.h"
#include "eden_map.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"

/* 176 bytes */
typedef struct EdenModelWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    int arg;
    ModelPart *wire;
    Vec3 pos;
    Vec3 base;
    EdenOrb *orbs;
    int norb;
    int nadd;
    int state;
    int time;
    int len;
    int index;
    struct DcNode *obj;
    int burst;
    int compiled;
    ModelPart part;
    WireObj *compiled_obj;
} EdenModelWork;

/* model indices of one part (eden_model_part_defs), 36 bytes */
typedef struct EdenPartDef {
    int orb_mdl;
    int idx[5];
    Vec3 ofs;
} EdenPartDef;

/* 1496 bytes */
typedef struct EdenModelManWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    int arg;
    Vec3 pos;
    int part;
    int step;
    int time;
    int len;
    int seq;
    int seq_time;
    int next;
    EdenPartDef *defs;
    int ndefs;
    int model_state;
    ModelPart body[5];
    ModelPart wire[5];
    ModelPart burst[5];
    EdenModelWork *model;
    struct EdenMapWork *map;
    struct EdenBeltManWork *belt;
    char pad_5b0[4];
    BossCameraWork *cam;        /* 0x5b4 */
    char pad_5b8[4];
    struct EdenCoreWork *core;  /* 0x5bc */
    struct EdenEnemyMbManWork *mb;
    int pl_di[2];
    int unk_5cc;                /* 0x5cc: cleared at each part's end, never read */
    int level_set;
    int player_drawn;
} EdenModelManWork;

extern int modeltest_compile_obj;
extern char eden_model_vardef[];
extern GwVarDef eden_model_vardefs[];
extern struct GwVarNode *eden_model_vars_group;
extern ModelPart *eden_model_common_part;
extern float cmd_pass_r;
extern int cmd_tgt_hp_base;
extern float eden_model_font_st_alpha;
extern float eden_model_font_stf_alpha;
extern struct TaskDesc eden_model_man_desc;
extern EdenPartDef eden_model_part_defs[];
extern int cmd_seq_num;
extern float eden_model_orbs_fade_out[4];
extern float cmd_part_bscl;
extern float eden_model_orbs_fade[4];
extern struct TaskDesc eden_model_desc;
extern char str_eden_model_c[];
extern float cmd_model_len;
extern unsigned int eden_model_rgb_wire_col_r[3];
extern float cmd_core_lt;
extern float eden_model_mot_v, cmd_fade_t, cmd_open_wait_t;
extern Vec3 eden_model_part_ofs[];
extern float cmd_rgb_x, cmd_rgb_scl;
extern float cmd_fade_scl;
extern unsigned int eden_model_wire_col;
extern float eden_model_man_part_fade[4];
extern float cmd_part_dang;
extern float eden_model_bl_t, cmd_cam_ang_t, cmd_cam_demo_t, cmd_cam_demo_last_t, cmd_cam_demo_ft;
extern float cmd_cam_demo_t2;
extern float cmd_cam_def_ang, cmd_cam_zoom_ang, cmd_cam_off_z, cmd_cam_ry_ang, cmd_end_t, cmd_end_t_f;
extern int eden_model_bl_rate, eden_model_bl_rad, cmd_cam_demo_st, cmd_end_ft;
extern char eden_model_man_cam_targets[];
extern float eden_model_man_turn_start[], eden_model_man_turn_end[];

/* 0x00cb8d18 */
void eden_model_vars_add(void)
{
    eden_model_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_model_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_model_vars_group, eden_model_vardefs);
}

/* 0x00cb8d68 */
void eden_model_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_model_vardef);
}

/* 0x00cb8d80 */
int eden_model_node_count(WireObj *node)
{
    int n = 0;

    while (node) {
        if (node->model)
            n++;
        node = node->sibling;
    }
    return n;
}

/* 0x00cb8db0 */
void eden_model_orb_init(EdenModelWork *w, EdenOrb *item, WireObj *obj, WireObj *sub_obj, int src, int *ang, float t)
{
    char *it;
    int n;
    Vec3 pos;

    v3_copy(&item->ofs, (Vec3 *)obj->pos);
    obj = obj->child;
    item->ngrp = eden_model_node_count(obj);
    n = item->ngrp;
    item->grp = heap_block_tag(game_calloc(n * 220), str_eden_model_c, 307);
    it = (char *)item->grp;
    if (w->compiled)
        sub_obj = sub_obj->child;
    do {
        item->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        item->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        item->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
        item->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_part_dang + cmd_part_dang) - cmd_part_dang);
    } while (0);
    item->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_part_dang + cmd_part_dang) - cmd_part_dang);
    item->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_part_dang + cmd_part_dang) - cmd_part_dang);
    for (; n; n--) {
        if (w->compiled) {
            if (obj->model) {
                v3_sum(&pos, &item->ofs, &w->pos);
                eden_dust_grp_init(w, (EdenGrp *)it, &pos, (void **)obj, (int)sub_obj, (Vec3 *)obj->pos, *(int *)(*(char **)(src + 88) + 4), ang, 1, t);
                it += 220;
                sub_obj = sub_obj->sibling;
                obj = obj->sibling;
            }
        } else if (obj->model) {
            v3_sum(&pos, &item->ofs, &w->pos);
            eden_dust_grp_init(w, (EdenGrp *)it, &pos, (void **)obj, 0, (Vec3 *)obj->pos, *(int *)(*(char **)(src + 88) + 4), ang, 0, t);
            it += 220;
            obj = obj->sibling;
        }
    }
}

/* 0x00cb9118 */
int eden_model_orb_add(EdenModelWork *w)
{
    WireObj *obj, *sub_obj;
    int z[3];
    int i;

    if (w->nadd >= w->norb)
        return 0;
    sub_obj = 0;
    if (w->compiled) {
        sub_obj = w->compiled_obj->child;
        obj = ((WireObj *)w->obj)->child;
        for (i = w->nadd; i; i--) {
            obj = obj->sibling;
            sub_obj = sub_obj->sibling;
        }
    } else {
        obj = ((WireObj *)w->obj)->child;
        for (i = w->nadd; i; i--)
            obj = obj->sibling;
    }
    z[0] = 0;
    z[1] = 0;
    z[2] = 0;
    eden_model_orb_init(w, &w->orbs[w->nadd], obj, sub_obj, w->arg, z, cmd_pass_r);
    w->nadd++;
    return 1;
}

/* 0x00cb9228 */
void eden_model_orbs_free(EdenModelWork *w)
{
    int n = w->norb;
    EdenOrb *item = w->orbs;

    for (; n; item++, n--) {
        int ngrp;
        EdenGrp *grp;

        if (item->flags & 4)
            lock_remove_entry(18, &item->lock);
        ngrp = item->ngrp;
        grp = item->grp;
        for (; ngrp; ngrp--, grp++) {
            eden_dust_lock_remove(grp);
            heap_free(grp->items, 0);
        }
        heap_free(item->grp, 0);
    }
    heap_free(w->orbs, 0);
}

/* 0x00cb9308 */
int eden_model_orb_update(void *w, EdenOrb *orb, Vec3 *base, int *ang)
{
    if (orb->flags & 0x20) {
        orb->fade *= eden_model_font_st_alpha;
        if (orb->fade < eden_model_font_stf_alpha) {
            orb->fade = 0;
            orb->flags &= ~0x20;
        }
    }
    if (orb->flags & 4) {
        if (--orb->timer <= 0) {
            orb->lock.life = 0;
            goto off;
        }
        if (orb->lock.flags & LOCK_HIT) {
            eden_circle_spawn(w, &orb->pos, 0);
            orb->flags |= 1;
            if (orb->lock.life <= 0) {
                lock_score(18, &orb->lock);
            off:
                lock_remove_entry(18, &orb->lock);
                orb->flags &= ~4;
            }
        } else
            orb->flags &= ~1;
    }
    v3_sum(&orb->pos, base, &orb->ofs);
    i3_sum(orb->ang, orb->ang, orb->dang);
    if (orb->flags & 2)
        orb->flags |= 0x40;
    return 0;
}

/* 0x00cb94a8 */
void eden_model_orbs_update(EdenModelWork *w)
{
    int ang[3];
    int n;
    EdenOrb *orb;
    int lim;
    int hit;

    ang[0] = 0;
    ang[1] = 0;
    ang[2] = 0;
    n = w->nadd;
    orb = w->orbs;
    for (; n; n--, orb++) {
        eden_model_orb_update(w, orb, &w->pos, ang);
        lim = 0;
        if (orb->flags & 8) {
            if (orb->lock.life)
                lim = (orb->lock.life_max - orb->lock.life) * cmd_tgt_hp_base;
            else
                lim = orb->ngrp;
        }
        hit = eden_dust_grps_update(w, w->orbs, (void *)w->nadd, orb->grp, &orb->pos, orb->ngrp, lim);
        if (hit) {
            if (!(orb->flags & 8)) {
                orb->fade = 1.0f;
                orb->timer = (lim = 0) / cmd_tgt_hp_base;
                orb->flags |= 0x29;
            }
            if (hit == 2)
                orb->flags |= 2;
        }
    }
}

/* steer(cast) */
/* 0x00cb95c8 */
void eden_model_orbs_draw(EdenModelWork *w)
{
    int old;
    float t;
    int n;
    EdenOrb *orb;
    EdenGrp *grp;
    int k;

    ulGxSetTextureList(w->tex_list);
    old = draw_set_override_flags(0);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    t = beat_curve(0);
    for (n = w->norb, orb = w->orbs; n; n--) {
        if (orb->flags & 0x10) {
            orb++;
            continue;
        }
        k = orb->ngrp;
        grp = orb->grp;
        if (orb->flags & 4) {
            ulMatrixPush();
            MTX_TRANS(&orb->pos);
            eden_model_rotate_yxz(orb->ang);
            vu0_matrix_scale(ulMatrixCur, cmd_part_bscl * t + 1.0f, cmd_part_bscl * t + 1.0f, cmd_part_bscl * t + 1.0f);
            if (orb->flags & 0x20) {
                draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
                eden_model_orbs_fade[0] = -orb->fade;
                draw_set_override_color(eden_model_orbs_fade);
                draw_set_override_flags(0);
            }
            ulMatrixPop();
        }
        ((void (*)(void *, EdenGrp *, int, int, int))eden_dust_grps_draw)(w, grp, k, w->arg, eden_tex_offset());
        orb++;
    }
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(old);
}

/* steer(cast) */
/* 0x00cb9788 */
void eden_model_orbs_draw_fade(EdenModelWork *w, float alpha)
{
    int old;
    int n, k;
    EdenOrb *orb;
    EdenGrp *grp;

    ulGxSetTextureList(w->tex_list);
    old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    eden_model_orbs_fade_out[0] = alpha + -1.0f;
    draw_set_override_color(eden_model_orbs_fade_out);
    for (n = w->norb, orb = w->orbs; n; orb++, n--) {
        for (k = orb->ngrp, grp = orb->grp; k; k--, grp++) {
            ulMatrixPush();
            MTX_TRANS(&grp->pos);
            eden_model_rotate_yxz(grp->ang);
            nj_cnk_vu1_draw(grp->model[1]);
            ulMatrixPop();
        }
    }
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(old);
}

/* 0x00cb98d0 */
void eden_model_se_play(EdenModelWork *w)
{
    int kind = w->index;

    if (kind < 4)
        boss_vol_play(kind * 3 + 2, 0);
    else
        boss_vol_play(13, 0);
}

/* 0x00cb9900 */
void eden_model_exec(EdenModelWork *w)
{
    int z[3];
    float t, frm;
    ModelPart *part;
    int i, n;

    v3_set(&w->pos, 0.0f, 0.0f, 0.0f);
    v3_add(&w->pos, &w->base);
    w->pos.z += cmd_model_len;
    eden_model_orbs_update(w);
    if (eden_is_end())
        return;
    switch (w->state) {
    case 5:
        if (--w->time > 0)
            return;
        t = beat_clock_get_interval() * 4.0f * (1.0f - beat_clock_get_bar());
        frm = cmd_fade_t * FPS;
        if ((float)(int)t != frm)
            return;
        w->state = 1;
        w->time = w->len = (int)frm;
        if (w->index < 4)
            boss_bgm_switch((int)eden_model_se_play, (int)w, w->index + 8);
        else
            eden_sound_bgm_start(eden_ending_type());
        break;
    case 0:
        w->wire->frame = 0.0f;
        for (i = 0, n = 0; i < w->nadd; i++)
            if (w->orbs[i].flags & 0x40)
                n++;
        if (n != w->nadd)
            return;
        if (eden_model_orb_add(w))
            return;
        w->time = w->len = (int)(cmd_open_wait_t * FPS);
        w->state = 5;
        break;
    case 1:
        if (--w->time > 0)
            return;
        w->state = 2;
        w->time = w->len = (int)(beat_clock_get_interval() * 4.0f);
        z[0] = 0;
        z[1] = 0;
        z[2] = 0;
        boss_se_play(52, 0, (int)z);
        break;
    case 2:
        if (--w->time > 0)
            return;
        w->state = 3;
        break;
    case 3:
        part = w->wire;
        part->frame += eden_model_mot_v;
        if (part->frames <= part->frame) {
            part->frame = part->frames;
            w->state = 4;
        }
        break;
    case 4:
        break;
    }
}

/* 0x00cb9bc0 */
void eden_model_screen_offset(Vec3 *in, Vec3 *out, float dx, float dy)
{
    ScreenCfg *scr = &screen_cfg;
    Vec3 scr_pt;
    float k, neg, x, y;

    ulMatrixApplyPoint((float *)ulMatrixCur, &in->x, &scr_pt.x);
    neg = -1.0f;
    k = neg / scr_pt.z;
    x = scr_pt.x * k * scr->sx;
    y = scr_pt.y * k * scr->sy;
    {
        float *cam = camera_get_inv_mtx();

        out->z = neg / k;
        out->x = (x + dx) / (k * scr->sx);
        out->y = (y + dy) / (k * scr->sy);
        ulMatrixApplyPoint(cam, &out->x, &out->x);
    }
}

/* 0x00cb9ce0 */
void eden_model_draw_mirror(ModelPart *wire, Vec3 *pos, Vec3 *ctr, float alpha, float dist, float s)
{
    Vec3 n;
    Vec3 ofs;

    draw_set_blend(4, 1);
    ulMatrixPush();
    MTX_TRANS(pos);
    draw_set_color(eden_model_rgb_wire_col_r[0]);
    model_node_walk_list_anim(wire->root, wire->clip, wire->frame, wire->wire_list);
    ulMatrixPop();
    ulMatrixPush();
    n.x = -ctr->x;
    n.y = -ctr->y;
    n.z = -ctr->z;
    eden_model_screen_offset(pos, &ofs, dist, 0.0f);
    MTX_TRANS(&ofs);
    MTX_TRANS(&n);
    vu0_matrix_scale(ulMatrixCur, s, s, s);
    MTX_TRANS(ctr);
    draw_set_color((eden_model_rgb_wire_col_r[1] & 0xffffff) | ((int)(color_get_a(eden_model_rgb_wire_col_r[1]) * alpha) + 1) >> 1 << 24);
    model_node_walk_list_anim(wire->root, wire->clip, wire->frame, wire->wire_list);
    ulMatrixPop();
    ulMatrixPush();
    n.x = -ctr->x;
    n.y = -ctr->y;
    n.z = -ctr->z;
    eden_model_screen_offset(pos, &ofs, -dist, 0.0f);
    MTX_TRANS(&ofs);
    MTX_TRANS(&n);
    vu0_matrix_scale(ulMatrixCur, s, s, s);
    MTX_TRANS(ctr);
    draw_set_color((eden_model_rgb_wire_col_r[2] & 0xffffff) | ((int)(color_get_a(eden_model_rgb_wire_col_r[2]) * alpha) + 1) >> 1 << 24);
    model_node_walk_list_anim(wire->root, wire->clip, wire->frame, wire->wire_list);
    ulMatrixPop();
}

/* 0x00cb9f88 */
void eden_model_disp(EdenModelWork *w)
{
    float t;
    unsigned int col;
    Vec3 n;

    ulGxSetTextureList(w->tex_list);
    eden_wire_colors_set();
    switch (w->state) {
    case 0:
    case 5:
        eden_model_orbs_draw(w);
        break;
    case 1:
        t = (float)w->time / (float)w->len;
        eden_model_orbs_draw_fade(w, t);
        ulMatrixPush();
        MTX_TRANS(&w->pos);
        col = (eden_model_rgb_wire_col_r[0] & 0xffffff) | ((int)(color_get_a(eden_model_rgb_wire_col_r[0]) * (1.0f - t)) + 1) >> 1 << 24;
        draw_set_blend(4, 1);
        draw_set_color(col);
        {
            ModelPart *mdl = w->wire;

            model_node_walk_list_anim(mdl->root, mdl->clip, mdl->frame, mdl->wire_list);
        }
        ulMatrixPop();
        break;
    case 2: {
        float k = (float)w->time / (float)w->len;

        v3_neg(&n, &eden_model_part_ofs[w->index]);
        eden_model_draw_mirror(w->wire, &w->pos, &n, 1.0f - k, k * cmd_rgb_x, cmd_rgb_scl * k + 1.0f);
        break;
    }
    case 3:
        ulMatrixPush();
        MTX_TRANS(&w->pos);
        draw_set_blend(4, 5);
        {
            ModelPart *mdl = w->wire;

            wire_draw_color_c(mdl->root, mdl->clip, mdl->frame, mdl->wire_list);
        }
        {
            ModelPart *mdl = w->wire;
            int k = w->burst;

            eden_model_common_part->clip = mdl->clip;
            eden_model_common_part->root = k;
            model_object_draw(eden_model_common_part, mdl->frame);
        }
        ulMatrixPop();
        break;
    case 4:
        ulMatrixPush();
        MTX_TRANS(&w->pos);
        draw_set_blend(4, 5);
        {
            ModelPart *mdl = w->wire;

            wire_draw_color_c(mdl->root, mdl->clip, mdl->frame, mdl->wire_list);
        }
        {
            ModelPart *mdl = w->wire;
            int k = w->burst;

            eden_model_common_part->clip = mdl->clip;
            eden_model_common_part->root = k;
            model_object_draw(eden_model_common_part, mdl->frame);
        }
        ulMatrixPop();
        break;
    }
}

/* 0x00cba270 */
void eden_model_release(EdenModelWork *w)
{
    eden_model_orbs_free(w);
    dc_object_release(&w->part);
}

/* 0x00cba298 */
EdenModelWork *eden_model_create(void *parent, int arg, int mdl, int mdl_b, ModelPart *wire, Vec3 *pos, int idx)
{
    EdenModelWork *w = task_create_inherit(parent, &eden_model_desc, 176);
    WireObj *obj;
    int i;

    v3_copy(&w->base, pos);
    w->arg = arg;
    w->index = idx;
    w->wire = wire;
    v3_set(&w->pos, 0.0f, 0.0f, 0.0f);
    v3_add(&w->pos, &w->base);
    w->pos.z += cmd_model_len;
    w->obj = pnb_get_entry(w->pack, mdl)->src;
    dc_object_init(&w->part, w->obj, 0, 0);
    if (modeltest_compile_obj) {
        dc_object_rebuild(&w->part);
        w->compiled_obj = (WireObj *)dc_object_get_compiled(&w->part);
        if (w->compiled_obj)
            w->compiled = 1;
        else
            w->compiled = 0;
    } else {
        w->compiled = 0;
        w->compiled_obj = 0;
    }
    w->burst = (int)pnb_get_entry(w->pack, mdl_b)->src;
    for (obj = ((WireObj *)w->obj)->child; obj; obj = obj->sibling)
        w->norb++;
    w->orbs = heap_block_tag(game_calloc(w->norb * 188), str_eden_model_c, 916);
    for (i = 0; i < w->norb; i++)
        eden_model_orb_add(w);
    return w;
}

/* 0x00cba4a0 */
int eden_model_get_state(EdenModelWork *w)
{
    return w->state;
}

/* 0x00cba4a8 */
void eden_model_man_seq_step(EdenModelManWork *w)
{
    int z[3];

    switch (w->seq) {
    case 0:
        if (!w->belt)
            return;
        if (eden_belt_man_get_mode(w->belt))
            return;
        eden_set_flags(2);
        eden_belt_man_set_wait(w->belt, 0);
        eden_map_pass_change(w->map, EDEN_MAP_PASS, w->part);
        eden_core_open(w->core, 1);
        boss_vol_play(w->part * 3 + 1, 0);
        w->seq++;
        break;
    case 1:
        if (eden_is_end())
            return;
        if (!eden_map_pass_is_end(w->map) && eden_core_get_state(w->core))
            return;
        ((void (*)(struct EdenCoreWork *, int))eden_core_set_hits)(w->core, 0); /* steer(cast) */
        eden_set_flags(1);
        eden_set_lock_off(1);
        z[0] = 0;
        z[1] = 0;
        z[2] = 0;
        boss_se_play(51, 0, (int)z);
        w->seq++;
        break;
    case 2:
        if (eden_is_end())
            eden_set_flags(eden_get_flags() & ~1);
        break;
    case 3:
        if (w->part < 4) {
            peri_act_set_selection(w->part * 2 + 9);
            eden_set_flags(6);
            eden_belt_man_set_mode(w->belt, 1);
            eden_belt_man_set_wait(w->belt, 1);
            eden_enemy_mb_man_next_group(w->mb, 1);
            w->seq = 0;
        } else {
            peri_act_set_selection(w->part * 2 + 9);
            eden_enemy_mb_man_next_group(w->mb, 1);
            eden_set_flags(2);
            eden_core_open(w->core, 1);
            w->seq = 4;
            w->seq_time = 480;
        }
        break;
    case 4:
        if (--w->seq_time > 0 && eden_core_get_state(w->core))
            return;
        eden_map_pass_change(w->map, EDEN_MAP_PASS, w->part);
        w->seq_time = (int)(cmd_core_lt * FPS);
        w->seq++;
        break;
    case 5:
        if (w->seq_time && --w->seq_time <= 0)
            ((void (*)(struct EdenCoreWork *, int))eden_core_set_hits)(w->core, 0); /* steer(cast) */
        if (eden_core_get_state(w->core))
            return;
        eden_map_level_set(w->map, w->part);
        w->level_set = 1;
        eden_set_flags(1);
        eden_set_lock_off(1);
        eden_set_break_all(1);
        ((void (*)(int *))boss_fx_release)(boss_get_task());
        w->seq++;
        break;
    case 6:
        break;
    }
}

/* steer(cast) */
/* 0x00cba758 */
void eden_model_man_exec(EdenModelManWork *man)
{
    Vec3 v;
    EdenPartDef *def;
    PnbEntry *ent;
    int i;
    float ang_a, ang_b;
    int *cam;
    BossPlDiWork *pldi;

    v3_set(&man->pos, 0.0f, 0.0f, 0.0f);
    if (man->cam && (TASK_OF(man->cam)->flags & TASK_F_END))
        man->cam = 0;
    if (man->model)
        man->model_state = eden_model_get_state(man->model);
    else
        man->model_state = 0;
    man->pos.z += cmd_model_len;
    eden_model_man_seq_step(man);
    switch (man->step) {
    case 0:
        break;
    case 1:
        i = man->part;
        def = &man->defs[i];
        man->model = eden_model_create(man, man->arg, def->orb_mdl, def->idx[4], &man->wire[i], &def->ofs, i);
        man->step++;
        break;
    case 2:
        if (eden_is_end())
            return;
        if (man->model_state == 5) {
            if (man->level_set)
                return;
            eden_map_level_set(man->map, man->part);
            man->level_set = 1;
            break;
        }
        if (man->model_state != 1)
            return;
        eden_set_flags(0);
        eden_tex_anime_create(man->part);
        man->time = man->len = (int)(cmd_fade_t * FPS);
        man->step++;
        cam = eden_effect_screen_man_create(man);
        v.x = v.y = 0.0f;
        v.z = cmd_cam_off_z;
        if (man->part + 1 < man->ndefs) {
            ang_a = cmd_cam_demo_ft;
            ang_b = cmd_cam_demo_t + (cmd_fade_t + cmd_fade_t);
        } else {
            ang_a = 0.0f;
            ang_b = cmd_cam_demo_last_t;
            eden_effect_screen_man_end(cam);
        }
        man->cam = boss_camera_create(man, 1.0f, 0, 0, &v, 88, cmd_cam_demo_st, (int)(ang_a * FPS), 1);
        boss_camera_look(man->cam, (int)(ang_b * FPS));
        boss_camera_set_target(man->cam, (Vec3 *)(eden_model_man_cam_targets + man->part * 12));
        boss_camera_set_player_vec(man->cam, 0);
        player_set_draw(0);
        return;
    case 3:
        if (man->time)
            man->time--;
        if (man->model_state != 3)
            return;
        peri_act_set_selection(man->part * 2 + 10);
        eden_set_dalpha(0.0f);
        eden_tex_anime_set_visible(1);
        eden_tex_anime_set_play(1);
        man->next = man->part + 1;
        eden_map_set_turn(man->map, eden_model_man_turn_start[man->part] + cmd_cam_ry_ang, (eden_model_man_turn_end[man->part] - eden_model_man_turn_start[man->part]) / (cmd_cam_demo_t2 * FPS));
        eden_map_pass_change(man->map, EDEN_MAP_PATH, man->part);
        eden_map_set_target(man->map, 0);
        screen_set_margin(eden_model_bl_rad);
        screen_set_fade_alpha(eden_model_bl_rate);
        boss_cam_ang_create(man->cam, (int)DEG_TO_ANG(cmd_cam_def_ang), (int)DEG_TO_ANG(cmd_cam_zoom_ang), 0, (int)(cmd_cam_ang_t * FPS));
        man->time = man->len = 4;
        man->step++;
        break;
    case 4:
        screen_set_margin(eden_model_bl_rad);
        screen_set_fade_alpha(eden_model_bl_rate);
        if (man->model_state != 4)
            return;
        man->step++;
        if (man->model) {
            task_end_with_exit(man->model);
            man->model = 0;
        }
        man->time = man->len = (int)(cmd_fade_t * FPS);
        cm_tex_anime_color_create(0, COL_WHITE, 0x808080, 0, man->time);
        if (man->part + 1 < man->ndefs) {
            cm_sc_blur_set_curve(cm_sc_blur_create(man, (float)eden_model_bl_rad, (float)eden_model_bl_rate, 0.0f, eden_model_bl_t), 1);
            eden_map_set_target(man->map, 1);
            if (man->cam)
                boss_cam_ang_create(man->cam, (int)DEG_TO_ANG(cmd_cam_zoom_ang), (int)DEG_TO_ANG(cmd_cam_def_ang), 0, (int)(cmd_fade_t * FPS));
            boss_vol_play(man->part * 3 + 3, 0);
            return;
        }
        man->time = man->len = cmd_end_ft;
        man->step = 6;
        eden_bgm_flag_clear();
        eden_map_pass_change(man->map, EDEN_MAP_PATH_END, 0);
        if (man->cam)
            ((void (*)(BossCameraWork *, int, int))boss_camera_idle_after)(man->cam, 2, 0);
        fade_start(0, 0, (int)(cmd_end_t_f * FPS));
        pldi = boss_pl_di_create(man, 1, 0);
        ent = pnb_get_entry(man->pack, 134);
        man->pl_di[0] = (int)ent->src;
        man->pl_di[1] = 0;
        boss_pl_di_set(pldi, man->pl_di);
        return;
    case 5: {
        int j;

        player_set_draw(man->player_drawn);
        eden_set_dalpha(1.0f - (float)man->time / (float)man->len);
        if (--man->time > 0)
            break;
        if (!man->part)
            eden_end_request();
        eden_set_dalpha(1.0f);
        eden_tex_anime_stop();
        eden_set_lock_off(0);
        eden_map_set_default(man->map);
        j = man->part + 1;
        man->step = 0;
        man->seq++;
        man->unk_5cc = 0;
        man->level_set = 0;
        man->part = j;
        if (j < man->ndefs)
            man->step = 1;
        else
            man->step = -1;
        break;
    }
    case 6:
        screen_set_margin(eden_model_bl_rad);
        screen_set_fade_alpha(eden_model_bl_rate);
        if (--man->time > 0)
            break;
        man->time = man->len = (int)(cmd_end_t * FPS - (float)cmd_end_ft);
        man->step++;
        break;
    case 7:
        screen_set_margin(eden_model_bl_rad);
        screen_set_fade_alpha(eden_model_bl_rate);
        if (--man->time > 0)
            break;
        man->step = -1;
        break;
    }
}

/* steer(cast) */
/* 0x00cbaea0 */
void eden_model_man_part_draw(EdenModelManWork *w, int i, float t, int mode)
{
    float sc = cmd_fade_scl * (1.0f - t) + 1.0f;
    EdenPartDef *def = &w->defs[i];
    int old_blend = draw_set_override_flags(0);
    Vec3 n;
    unsigned int col;

    ulMatrixPush();
    MTX_TRANS(&w->pos);
    MTX_TRANS(&def->ofs);
    switch (mode) {
    case 0:
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        eden_model_man_part_fade[0] = t + -1.0f;
        draw_set_override_color(eden_model_man_part_fade);
        dc_object_draw(&w->body[i]);
        draw_pop_override_color();
        draw_pop_strip_flags();
        break;
    case 1:
        draw_set_override_flags(0);
        dc_object_draw(&w->body[i]);
        break;
    case 2:
        v3_neg(&n, &eden_model_part_ofs[i]);
        MTX_TRANS(&eden_model_part_ofs[i]);
        vu0_matrix_scale(ulMatrixCur, sc, sc, sc);
        MTX_TRANS(&n);
        draw_set_blend(4, 5);
        col = (eden_model_wire_col & 0xffffff) | ((int)(color_get_a(eden_model_wire_col) * t) + 1) >> 1 << 24;
        wire_colors_set(col, col, 2.0f);
        wire_draw_color_c(w->wire[i].root, w->wire[i].clip, w->wire[i].frames, w->wire[i].wire_list);
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        eden_model_man_part_fade[0] = t + -1.0f;
        draw_set_override_color(eden_model_man_part_fade);
        {
            ModelPart *part = eden_model_common_part;
            int mot = w->burst[i].clip;
            int mdl = w->burst[i].root;

            *(void **)&part->clip = (void *)mot; /* steer(alias) */
            *(void **)&part->root = (void *)mdl;
            model_object_draw(part, w->burst[i].frames);
        }
        draw_pop_override_color();
        draw_pop_strip_flags();
        break;
    }
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00cbb1d8 */
void eden_model_man_disp(EdenModelManWork *w)
{
    ulGxSetTextureList(w->tex_list);
    switch (w->step) {
    case 0:
    case 1:
    case 2:
        if (w->part)
            eden_model_man_part_draw(w, w->part - 1, 1.0f, 1);
        break;
    case 3:
        if (w->part)
            eden_model_man_part_draw(w, w->part - 1, (float)w->time / (float)w->len, 0);
        break;
    case 5:
    case 6:
        eden_model_man_part_draw(w, w->part, (float)w->time / (float)w->len, 2);
        eden_model_man_part_draw(w, w->part, 1.0f, 0);
        break;
    case -1:
    case 7:
        eden_model_man_part_draw(w, w->part, 1.0f, 1);
        break;
    }
    if (w->next) {
        int old_blend = draw_set_override_flags(0);
        int frm = w->next;
        ModelPart *part = ((ModelPart **)w->arg)[46];
        float lim = part->frames;

        frm = lim < (float)frm ? (int)lim : frm;
        model_object_draw(part, (float)frm);
        draw_set_override_flags(old_blend);
    }
}

/* steer(cast) */
/* 0x00cbb360 */
void eden_model_man_parts_init(EdenModelManWork *w)
{
    int i;
    EdenPartDef *def = w->defs;

    for (i = 0; i < w->ndefs; i++, def++) {
        dc_object_init(&w->wire[i], *((struct DcNode **)pnb_get_entry(w->pack, def->idx[1])), *((struct DcClip **)pnb_get_entry(w->pack, def->idx[3])), 0);
        {
            int wire_list = *((int *)pnb_get_entry(w->pack, def->idx[2]));
            int body = def->idx[0];

            w->wire[i].wire_list = wire_list;
            dc_object_init(&w->body[i], *((struct DcNode **)pnb_get_entry(w->pack, body)), 0, 0);
        }
        dc_object_init(&w->burst[i], *((struct DcNode **)pnb_get_entry(w->pack, def->idx[4])), *((struct DcClip **)pnb_get_entry(w->pack, def->idx[3])), 0);
    }
}

/* 0x00cbb498 */
void eden_model_man_parts_free(EdenModelManWork *w)
{
    int i;

    for (i = 0; i < w->ndefs; i++) {
        dc_object_release(&w->wire[i]);
        dc_object_release(&w->body[i]);
        dc_object_release(&w->burst[i]);
    }
}

/* 0x00cbb508 */
void eden_model_man_release(EdenModelManWork *w)
{
    player_set_draw(w->player_drawn);
    player_demo_motion_end();
    dc_object_release(eden_model_common_part);
    eden_model_man_parts_free(w);
}

/* 0x00cbb548 */
EdenModelManWork *eden_model_man_create(void *parent, struct EdenMapWork *map, int arg)
{
    EdenModelManWork *w = task_create_inherit(parent, &eden_model_man_desc, 1496);
    PnbEntry *obj, *mot;

    w->map = map;
    w->arg = arg;
    w->player_drawn = player_is_drawn();
    obj = pnb_get_entry(w->pack, 66);
    mot = pnb_get_entry(w->pack, 61);
    dc_object_init(eden_model_common_part, obj->src, mot->src, 1);
    part_radius_default(eden_model_common_part);
    eden_model_common_part->coll_lock = 1;
    w->defs = eden_model_part_defs;
    w->ndefs = 5;
    eden_model_man_parts_init(w);
    w->step++;
    w->part = cmd_seq_num;
    eden_set_flags(0);
    w->belt = (struct EdenBeltManWork *)eden_belt_man_create(w, w->arg);
    eden_belt_man_set_wait(w->belt, 1);
    eden_map_pass_change(w->map, EDEN_MAP_START, 0);
    w->core = (struct EdenCoreWork *)eden_core_create(w, (char *)w->arg);
    w->mb = (struct EdenEnemyMbManWork *)eden_enemy_mb_man_create(w, w->arg);
    return w;
}

/* 0x00cbb680 */
int eden_model_man_is_end(EdenModelManWork *w)
{
    return w->step == -1;
}

/* 0x00cbb690 */
int eden_model_man_get_part(EdenModelManWork *w)
{
    return w->part;
}

/* 0x00cbb698 */
void eden_model_man_set_visible(EdenModelManWork *w, int on)
{
    if (!on)
        TASK_OF(w)->disp = 0;
    else
        TASK_OF(w)->disp = eden_model_man_disp;
}

/* 0x00cbb6b8 */
void eden_model_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
