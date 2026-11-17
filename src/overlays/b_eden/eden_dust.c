/*
 * Eden boss piece motion: the lockable pieces of the boss's text and orbs scatter, spring into
 * place around their owner's point and orbit it. A piece shot or out of time scores and sends a
 * shock wave that sets off the groups around it by distance.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_dust.h"
#include "eden.h"
#include "eden_effect.h"
#include "eden_font.h"
#include "../common/boss_common.h"

extern char eden_dust_vardef[];
extern GwVarDef eden_dust_vardefs[];
extern struct GwVarNode *eden_dust_vars_group;
extern float cmd_piece_fv, cmd_piece_sr, cmd_piece_sd;
extern float eden_dust_bcircle_r;
extern int eden_dust_bcircle_frm;
extern float cmd_set_plen;
extern float cmd_dust_npt;
extern float cmd_dust_dang, cmd_piece_lt, cmd_piece_ldt;
extern float eden_dust_do_kr, cmd_do_pmf2, cmd_do_pkf2, eden_dust_do_atlen, eden_dust_font_st_alpha;
extern float eden_dust_rang_rate;
extern const float eden_dust_font_stf_alpha;

extern float eden_dust_do_mf, eden_dust_do_kf, eden_dust_do_pmf;
extern const float eden_dust_do_pkf;
extern float cmd_dust_sr;

/* 0x00cae988 */
void eden_dust_vars_add(void)
{
    eden_dust_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_dust_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_dust_vars_group, eden_dust_vardefs);
}

/* 0x00cae9d8 */
void eden_dust_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_dust_vardef);
}

/* 0x00cae9f0 */
void eden_dust_item_seed(EdenGrpItem *w, Vec3 *base, char *src, Vec3 *range, int k)
{
    Vec3 ofs;
    WireObj *obj;

    ofs.z = RAND_FLOAT_A() * (range->z + range->z) - range->z;
    ofs.x = RAND_FLOAT_A() * (range->x + range->x) - range->x;
    ofs.y = RAND_FLOAT_A() * (range->y + range->y) - range->y;
    v3_sum(&w->pos, base, &ofs);
    do {
        w->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
        w->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    w->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    do {
        w->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    eden_dust_item_retarget(w, base, range);
    for (obj = (*(WireObj **)(src + 4))->child; k; k--)
        obj = obj->sibling;
    w->flags |= 0x20;
    w->obj = obj;
    w->scl = 1.0f;
    v3_copy(&w->home, (Vec3 *)obj->pos);
}

/* 0x00caece8 */
void eden_dust_piece_seed(void *parent, EdenGrp *w, Vec3 *base, Vec3 *range)
{
    Vec3 ofs;

    ofs.z = RAND_FLOAT_A() * (range->z + range->z) - range->z;
    ofs.x = RAND_FLOAT_A() * (range->x + range->x) - range->x;
    ofs.y = RAND_FLOAT_A() * (range->y + range->y) - range->y;
    v3_sum(&w->pos, base, &ofs);
    do {
        w->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        w->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
        w->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    w->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    do {
        w->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    eden_dust_piece_retarget(w, base, range);
    w->flags |= 0x21;
    w->alpha = 1.0f;
    w->life = (int)((cmd_piece_lt + RAND_FLOAT_A() * cmd_piece_ldt) * FPS);
}

/* 0x00caefb8 */
void eden_dust_lock_remove(EdenGrp *w)
{
    if (w->flags & 4) {
        lock_remove_entry(17, &w->lock);
        w->flags &= ~4;
    }
}

/* 0x00caf000 */
void eden_dust_item_retarget(EdenGrpItem *w, Vec3 *base, Vec3 *range)
{
    Vec3 ofs;
    float len, lim;

    ofs.z = RAND_FLOAT_A() * (range->z + range->z) - range->z;
    ofs.x = RAND_FLOAT_A() * (range->x + range->x) - range->x;
    ofs.y = RAND_FLOAT_A() * (range->y + range->y) - range->y;
    v3_sum(&w->target, base, &ofs);
    v3_sub(&ofs, &w->target, &w->pos);
    len = vec3_length(&ofs);
    lim = cmd_set_plen;
    if (lim < len) {
        vec_set_length(&ofs, lim);
        v3_sum(&w->target, &w->pos, &ofs);
    }
    w->timer = (int)((RAND_FLOAT_A() * (cmd_dust_npt - 1.0f) + 1.0f) * FPS);
}

/* 0x00caf1e8 */
void eden_dust_piece_retarget(EdenGrp *w, Vec3 *base, Vec3 *range)
{
    Vec3 ofs;
    float len, lim;

    ofs.z = RAND_FLOAT_A() * (range->z + range->z) - range->z;
    ofs.x = RAND_FLOAT_A() * (range->x + range->x) - range->x;
    ofs.y = RAND_FLOAT_A() * (range->y + range->y) - range->y;
    v3_sum(&w->target, base, &ofs);
    v3_sub(&ofs, &w->target, &w->pos);
    len = vec3_length(&ofs);
    lim = cmd_set_plen;
    if (lim < len) {
        vec_set_length(&ofs, lim);
        v3_sum(&w->target, &w->pos, &ofs);
    }
    w->timer = (int)((RAND_FLOAT_A() * (cmd_dust_npt - 1.0f) + 1.0f) * FPS);
}

/* 0x00caf3d0 */
int eden_dust_item_step(void *w, EdenGrp *it, Vec3 *pos, int *ang, Vec3 *move, int on)
{
    Vec3 v;
    Vec3 rot;
    float len_a, len_b, inv_blend;

    if (it->flags & 0x20) {
        it->alpha *= eden_dust_font_st_alpha;
        if (it->alpha < eden_dust_font_stf_alpha) {
            it->alpha = 0;
            it->flags &= ~0x20;
        }
    }
    if (it->flags & 4) {
        if (it->delay && --it->delay <= 0)
            lock_score(17, &it->lock);
        else if (--it->life > 0) {
            if (!((&it->lock)->flags & LOCK_HIT))
                goto skip;
            if ((&it->lock)->life > 0)
                goto skip;
            lock_score(17, &it->lock);
        }
        eden_font_shock_wave(w, &it->pos);
        boss_get_parts();
        lock_remove_entry(17, &it->lock);
        it->flags = (it->flags | 1) & ~4;
    }
skip:
    if (on) {
        if (it->flags & 0x40) {
            v3_copy(&v, &it->org);
            vec_rotate_zxy(&v, ang);
            i3_copy(it->ang, ang);
            v3_sum(&it->pos, pos, &v);
        } else {
            float k;

            ((Vec3 *)&it->spring_k)->x = it->spring_k * eden_dust_do_kr + eden_dust_do_kf * (1.0f - eden_dust_do_kr);
            v3_add(&it->pos, move);
            v3_add(&it->target, move);
            k = eden_dust_rang_rate;
            i3_sum(it->ang, it->dang, it->ang);
            it->blend = it->blend * k + (1.0f - k);
            inv_blend = 1.0f - it->blend;
            i3_mul(it->dang, it->dang, inv_blend);
            angles_blend(it->ang, ang, it->ang, inv_blend);
            v3_copy(&rot, &it->org);
            vec_rotate_zxy(&rot, ang);
            v3_sum(&rot, pos, &rot);
            spring_to_v(&it->vel, &it->pos, &rot, eden_dust_do_mf, it->spring_k);
            v3_sub(&rot, &it->pos, &rot);
            len_a = vec3_length(&rot);
            len_b = vec3_length(&it->vel);
            if (len_a < eden_dust_do_atlen && len_b < eden_dust_do_atlen) {
                it->flags |= 0x40;
                eden_effect_bflash_create(w, (int)&it->pos, (int)it->ang);
            }
        }
    } else {
        v3_add(&it->pos, move);
        i3_sum(it->ang, it->dang, it->ang);
        v3_add(&it->target, move);
        do {
            it->ang[0] = ANG_WRAP(it->ang[0]);
            it->ang[1] = ANG_WRAP(it->ang[1]);
            it->ang[2] = ANG_WRAP(it->ang[2]);
        } while (0);
        spring_to_v(&it->vel, &it->pos, &it->target, eden_dust_do_pmf, eden_dust_do_pkf);
        if (it->timer && --it->timer <= 0)
            return 1;
    }
    return 0;
}

/* 0x00caf940 */
int eden_dust_shard_step(char *shard, Vec3 *pos, int *ang, Vec3 *move, int on)
{
    int dang[3];
    Vec3 v;
    float inv_blend;

    if (*(int *)(shard + 72) & 0x20) {
        *(float *)(shard + 88) *= eden_dust_font_st_alpha;
        if (*(float *)(shard + 88) < eden_dust_font_stf_alpha) {
            *(int *)(shard + 88) = 0;
            *(int *)(shard + 72) &= ~0x20;
        }
    }
    if (on) {
        *(float *)(shard + 80) = *(float *)(shard + 80) * eden_dust_do_kr + eden_dust_do_kf * (1.0f - eden_dust_do_kr);
        i3_sum((int *)(shard + 48), (int *)(shard + 60), (int *)(shard + 48));
        v3_add((Vec3 *)(shard + 12), move);
        v3_add((Vec3 *)(shard + 24), move);
        do {
            *(int *)(shard + 48) = ANG_WRAP(*(int *)(shard + 48));
            *(int *)(shard + 52) = ANG_WRAP(*(int *)(shard + 52));
            *(int *)(shard + 56) = ANG_WRAP(*(int *)(shard + 56));
        } while (0);
        *(float *)(shard + 84) = *(float *)(shard + 84) * eden_dust_rang_rate + (1.0f - eden_dust_rang_rate);
        inv_blend = 1.0f - *(float *)(shard + 84);
        i3_mul((int *)(shard + 60), (int *)(shard + 60), inv_blend);
        i3_mul((int *)(shard + 48), (int *)(shard + 48), inv_blend);
        i3_mul(dang, ang, 1.0f - inv_blend);
        i3_add((int *)(shard + 48), dang);
        v3_copy(&v, (Vec3 *)shard);
        vec_rotate_zxy(&v, ang);    /* target angle, not the shard's own */
        v3_sum(&v, pos, &v);
        spring_to_v((Vec3 *)(shard + 36), (Vec3 *)(shard + 12), &v, eden_dust_do_mf, *(float *)(shard + 80));
        v3_sub(&v, (Vec3 *)(shard + 12), &v);
        if (vec3_length(&v) < eden_dust_do_atlen)
            *(int *)(shard + 72) |= 2;
        return 0;
    }
    i3_sum((int *)(shard + 48), (int *)(shard + 60), (int *)(shard + 48));
    v3_add((Vec3 *)(shard + 12), move);
    v3_add((Vec3 *)(shard + 24), move);
    do {
        *(int *)(shard + 48) = ANG_WRAP(*(int *)(shard + 48));
        *(int *)(shard + 52) = ANG_WRAP(*(int *)(shard + 52));
        *(int *)(shard + 56) = ANG_WRAP(*(int *)(shard + 56));
    } while (0);
    spring_to_v((Vec3 *)(shard + 36), (Vec3 *)(shard + 12), (Vec3 *)(shard + 24), eden_dust_do_pmf, eden_dust_do_pkf);
    if (*(int *)(shard + 76)) {
        if (--*(int *)(shard + 76) <= 0)
            return 1;
    } else {
        do { } while (0);
    }
    return 0;
}

/* 0x00cafe00 */
void eden_dust_grp_seed(void *w, EdenOrb *item, Vec3 *base, Vec3 *rr, int arg)
{
    Vec3 ofs;
    int n;
    EdenGrp *it;

    ofs.z = RAND_FLOAT_A() * (rr->z + rr->z) - rr->z;
    ofs.x = RAND_FLOAT_A() * (rr->x + rr->x) - rr->x;
    ofs.y = RAND_FLOAT_A() * (rr->y + rr->y) - rr->y;
    v3_sum(&item->pos, base, &ofs);
    do {
        item->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        item->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        item->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
        item->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    do {
        item->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
        item->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    eden_dust_grp_retarget(item, base, rr);
    item->flags = (item->flags & ~0x10) | 0x20;
    item->fade = 1.0f;
    for (n = item->ngrp, it = item->grp; n; n--, it++) {
        eden_dust_piece_seed(w, it, base, rr);
        it->count = 0;
        it->items = 0;
        it->flags |= 2;
    }
}

/* 0x00cb00e0 */
void eden_dust_grp_retarget(EdenOrb *w, Vec3 *base, Vec3 *range)
{
    Vec3 ofs;
    float len, lim;

    ofs.z = RAND_FLOAT_A() * (range->z + range->z) - range->z;
    ofs.x = RAND_FLOAT_A() * (range->x + range->x) - range->x;
    ofs.y = RAND_FLOAT_A() * (range->y + range->y) - range->y;
    v3_sum(&w->target, base, &ofs);
    v3_sub(&ofs, &w->target, &w->pos);
    len = vec3_length(&ofs);
    lim = cmd_set_plen;
    if (lim < len) {
        vec_set_length(&ofs, lim);
        v3_sum(&w->target, &w->pos, &ofs);
    }
    w->move_time = (int)((RAND_FLOAT_A() * (cmd_dust_npt - 1.0f) + 1.0f) * FPS);
}

/* 0x00cb02c8 */
void eden_dust_orbit_point(Vec3 *out, int *ang, float dist, float rad)
{
    float sc[2];

    out->z = -dist;
    out->x = out->y = 0.0f;
    ROT_Y((int)(rad * ANG_PER_RAD), sc, out->x, out->z);
    vec_rotate_zxy(out, ang);
}

/* 0x00cb0378 */
void eden_dust_orbit_step(EdenGrp *w, Vec3 *base, Vec3 *out)
{
    Vec3 wob;
    Vec3 k;

    w->phase2 += cmd_piece_fv;
    if (w->phase2 >= TWO_PI)
        w->phase2 -= TWO_PI;
    eden_dust_orbit_point(out, w->ang0, w->scl, w->phase2);
    v3_sum(out, base, out);
    k.x = k.y = k.z = cmd_piece_sr;
    wobble_step(&wob, &k, &w->phase, cmd_piece_sd);
    v3_add(out, &wob);
}

/* 0x00cb0488 */
void eden_dust_grp_step(EdenGrp *grp, Vec3 *pos, int on)
{
    Vec3 v;
    Vec3 rot;
    float len, k, inv_blend;
    float keep, goal, speed;

    if (grp->flags & 0x20) {
        grp->alpha *= eden_dust_font_st_alpha;
        if (grp->alpha < eden_dust_font_stf_alpha) {
            grp->alpha = 0.0f;
            grp->flags &= ~0x20;
        }
    }
    if (!on) {
        eden_dust_orbit_step(grp, pos, &grp->pos);
        i3_sum(grp->ang, grp->dang, grp->ang);
    } else if (grp->flags & 0x40) {
        v3_copy(&v, &grp->org);
        vec_rotate_zxy(&v, grp->ang_to);
        i3_copy(grp->ang, grp->ang_to);
        v3_sum(&grp->pos, pos, &v);
    } else {
        keep = eden_dust_do_kr;
        goal = cmd_do_pkf2;
        i3_sum(grp->ang, grp->dang, grp->ang);
        k = eden_dust_rang_rate;
        grp->spring_k = grp->spring_k * keep + goal * (1.0f - keep);
        grp->blend = grp->blend * k + (1.0f - k);
        inv_blend = 1.0f - grp->blend;
        i3_mul(grp->dang, grp->dang, inv_blend);
        angles_blend(grp->ang, grp->ang_to, grp->ang, inv_blend);
        v3_copy(&rot, &grp->org);
        vec_rotate_zxy(&rot, grp->ang_to);
        v3_sum(&rot, pos, &rot);
        spring_to_v(&grp->vel, &grp->pos, &rot, cmd_do_pmf2, grp->spring_k);
        v3_sub(&rot, &grp->pos, &rot);
        len = vec3_length(&rot);
        speed = vec3_length(&grp->vel);
        if (len < eden_dust_do_atlen && speed < eden_dust_do_atlen)
            grp->flags |= 0x40;
    }
}

/* 0x00cb07d0 */
void eden_dust_grp_piece_step(EdenGrp *grp, EdenGrpItem *it, Vec3 *move, int on)
{
    char *shard = (char *)it;
    Vec3 v;
    float inv_blend;

    if (it->flags & 0x20) {
        if ((*(float *)(shard + 88) *= eden_dust_font_st_alpha) < eden_dust_font_stf_alpha) {
            *(int *)(shard + 88) = 0;
            it->flags &= ~0x20;
        }
    }
    if (!on) {
        if (move) {
            v3_add((Vec3 *)(shard + 12), move);
            v3_add((Vec3 *)(shard + 24), move);
        }
        i3_sum((int *)(shard + 48), (int *)(shard + 60), (int *)(shard + 48));
        do {
            *(int *)(shard + 48) = ANG_WRAP(*(int *)(shard + 48));
            *(int *)(shard + 52) = ANG_WRAP(*(int *)(shard + 52));
            *(int *)(shard + 56) = ANG_WRAP(*(int *)(shard + 56));
        } while (0);
        spring_to_v((Vec3 *)(shard + 36), (Vec3 *)(shard + 12), (Vec3 *)(shard + 24), eden_dust_do_pmf, eden_dust_do_pkf);
        i3_sum((int *)(shard + 48), (int *)(shard + 60), (int *)(shard + 48));
        if (*(int *)(shard + 76) && --*(int *)(shard + 76) <= 0) {
            v.x = RAND_FLOAT_A() * (cmd_dust_sr + cmd_dust_sr) - cmd_dust_sr;
            v.y = RAND_FLOAT_A() * (cmd_dust_sr + cmd_dust_sr) - cmd_dust_sr;
            v.z = RAND_FLOAT_A() * (cmd_dust_sr + cmd_dust_sr) - cmd_dust_sr;
            v3_sum((Vec3 *)(shard + 24), &grp->pos, &v);
            *(int *)(shard + 76) = (int)((1.0f + RAND_FLOAT_A() * (cmd_dust_npt - 1.0f)) * FPS);
        }
    } else {
        *(float *)(shard + 80) = *(float *)(shard + 80) * eden_dust_do_kr + eden_dust_do_kf * (1.0f - eden_dust_do_kr);
        if (move) {
            v3_add((Vec3 *)(shard + 12), move);
            v3_add((Vec3 *)(shard + 24), move);
        }
        i3_sum((int *)(shard + 48), (int *)(shard + 60), (int *)(shard + 48));
        do {
            *(int *)(shard + 48) = ANG_WRAP(*(int *)(shard + 48));
            *(int *)(shard + 52) = ANG_WRAP(*(int *)(shard + 52));
            *(int *)(shard + 56) = ANG_WRAP(*(int *)(shard + 56));
        } while (0);
        *(float *)(shard + 84) = *(float *)(shard + 84) * eden_dust_rang_rate + (1.0f - eden_dust_rang_rate);
        v3_copy(&v, (Vec3 *)shard);
        inv_blend = 1.0f - *(float *)(shard + 84);
        i3_mul((int *)(shard + 60), (int *)(shard + 60), inv_blend);
        i3_mul((int *)(shard + 48), (int *)(shard + 48), inv_blend);
        vec_rotate_zxy(&v, grp->ang);
        v3_sum(&v, &grp->pos, &v);
        spring_to_v((Vec3 *)(shard + 36), (Vec3 *)(shard + 12), &v, eden_dust_do_mf, *(float *)(shard + 80));
        v3_sub(&v, (Vec3 *)(shard + 12), &v);
        if (vec3_length(&v) < eden_dust_do_atlen)
            it->flags |= 2;
    }
}

/* 0x00cb0d80 */
void eden_dust_grp_init(void *owner, EdenGrp *grp, Vec3 *pos, void **model, int tgt, Vec3 *org, int count, int *ang, int kind, float scl)
{
    Vec3 ofs;                     /* unused */

    grp->phase2 = RAND_FLOAT_A() * 2.0f * PI;
    grp->phase = RAND_FLOAT_A() * 2.0f * PI;
    grp->model = model;
    i3_copy(grp->ang0, ang);
    v3_copy(&grp->org, org);
    grp->scl = scl;
    do {
        grp->ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        grp->ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        grp->ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    } while (0);
    do {
        grp->dang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    do {
        grp->dang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
        grp->dang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_dang + cmd_dust_dang) - cmd_dust_dang);
    } while (0);
    eden_dust_grp_step(grp, pos, 0);
    grp->flags |= 0x21;
    grp->alpha = 1.0f;
    grp->life = (int)((cmd_piece_lt + RAND_FLOAT_A() * cmd_piece_ldt) * FPS);
    grp->kind = kind;
    grp->tgt = tgt;
    grp->count = 0;
    grp->items = 0;
}

/* 0x00cb1068 */
void eden_dust_grp_check(void *w, void *orbs, void *norb, char *grp)
{
    if (!(*(int *)(grp + 8) & 4))
        return;
    if (*(int *)(grp + 16) && --*(int *)(grp + 16) <= 0)
        lock_score(17, (LockObj *)(grp + 24));
    else if (--*(int *)(grp + 20) > 0) {
        if (!(*(int *)(grp + 40) & 0x2000) || *(short *)(grp + 30) > 0)
            return;
        lock_score(17, (LockObj *)(grp + 24));
    }
    eden_dust_shock_wave(w, orbs, norb, grp + 100);
    boss_get_parts();
    lock_remove_entry(17, (LockObj *)(grp + 24));
    *(int *)(grp + 8) = (*(int *)(grp + 8) | 1) & ~4;
}

/* 0x00cb1170 */
int eden_dust_grps_update(void *w, void *orbs, void *norb, EdenGrp *grp, Vec3 *pos, int n, int lim)
{
    int i = 0;
    int live = n;
    int busy = n;
    int k, j;
    EdenGrpItem *it;

    for (; n; grp++, i++, n--) {
        eden_dust_grp_check(w, orbs, norb, (char *)grp);
        if (eden_test_flags(1)) {
            if (i < lim)
                eden_dust_grp_step(grp, pos, 1);
            else
                eden_dust_grp_step(grp, pos, 0);
        }
        if (grp->flags & 2) {
            if (grp->flags & 0x40)
                live--;
            busy--;
        } else {
            k = grp->count;
            it = grp->items;
            for (j = k; j; j--, it++) {
                eden_dust_grp_piece_step(grp, it, 0, grp->flags & 1);
                if (it->flags & 2)
                    k--;
            }
            if (!k) {
                grp->flags |= 2;
                heap_free(grp->items, 0);
                grp->items = 0;
                grp->count = 0;
            }
        }

    }
    if (!busy)
        return live ? 1 : 2;
    return 0;
}

/* 0x00cb1320 */
void eden_dust_grps_draw(void *owner, char *grp, int n)
{
    for (; n; n--) {
        ulMatrixPush();
        MTX_TRANS((Vec3 *)(grp + 100));
        eden_dust_rotate_yxz((int *)(grp + 136));
        if (*(int *)(grp + 8) & 2) {
            if (*(int *)(grp + 212))
                model_draw_box_culled(*(void **)(*(char **)(grp + 216) + 4));
            else
                model_draw_culled(*(void **)(*(char **)(grp + 188) + 4));
        }
        ulMatrixPop();
        grp += 220;
    }
}

/* 0x00cb13d8 */
void eden_dust_shock_wave(void *w, void *orbs, void *norb, void *pos)
{
    EdenOrb *orb = orbs;
    int n = (int)norb;
    int i;
    int k, delay;
    EdenGrp *grp;
    float lim;
    float dsq;

    eden_effect_dshine_create(w, (int)pos);
    lim = eden_dust_bcircle_r * eden_dust_bcircle_r;
    for (i = 0; i < n; i++, orb++) {
        if (orb->flags & 0x14)
            continue;
        for (k = orb->ngrp, grp = orb->grp; k; k--, grp++) {
            if (!(grp->flags & 4))
                continue;
            dsq = vec3_distance_squared(pos, &grp->pos);
            if (!(dsq < lim))
                continue;
            dsq = sqrtf(dsq);
            delay = (int)((float)eden_dust_bcircle_frm * dsq / eden_dust_bcircle_r);
            if (!grp->delay || delay < grp->delay)
                grp->delay = delay;
        }
    }
}

/* 0x00cb1538 */
void eden_dust_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
