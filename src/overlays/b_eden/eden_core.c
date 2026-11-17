/*
 * Eden boss core: opens to show two lockable halves, fires FB bullets while open and closes once
 * a share of its hits is taken; its hits drive the boss gauge. The core shine is a lockable light
 * that springs after the point the current form gives it.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "console.h"
#include "fog.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_core.h"
#include "eden.h"
#include "eden_bullet.h"
#include "eden_effect.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_wire.h"

/* the open core, two lockable halves; 236 bytes */
typedef struct EdenCoreWork {
    struct PnbHead *pack;
    void *tex_list;
    ModelPart **parts;
    void *parent;
    int state;                  /* 0 shut, 1 opening, 2 open, 3 closing */
    int stage;
    int opened;
    int time;
    int len;
    int dead;
    int hp[2];
    Vec3 pts[2];
    Vec3 vel;
    int ang[3];
    float frm;
    float frm_end;
    float glow;
    LockObj lock[2];
} EdenCoreWork;

/* the shine that follows the current form, 192 bytes */
typedef struct EdenCoreShineWork {
    struct PnbHead *pack;
    void *tex_list;
    ModelPart **parts;
    void *parent;
    int mode;
    int hits;
    int visible;
    int time;
    int len;
    int stage;
    int active;
    Vec3 pos;
    Vec3 *req;
    Vec3 home;
    char pad_48[12];
    Vec3 vel;                   /* 0x54 */
    char pad_60[20];
    float scl;                  /* 0x74 */
    float rate;
    float phase;
    LockObj lock;
} EdenCoreShineWork;

extern char eden_core_vardef[];
extern GwVarDef eden_core_vardefs[];
extern struct GwVarNode *eden_core_vars_group;
extern int cmd_core_hp;
extern struct TaskDesc eden_core_shine_desc;
extern int cmd_score_hp;
extern unsigned int eden_core_shot_kinds[];
extern struct TaskDesc eden_core_desc;
extern float eden_core_fade[4];
extern int eden_core_wire_cols[2];
extern float cmd_lcore_scl;
extern int cmd_core_score, cmd_core_score_fin;
extern const unsigned int cm_core_col;
extern float cmd_core_mot_v, cmd_core_mot_rv, cmd_camera_fs, cmd_camera_fr, cmd_fl_mv, cmd_fb_next_t;
extern float cmd_s_core_kf, cmd_s_core_mf, cmd_s_core_rr, cmd_s_core_rlim, cmd_s_core_hr;
extern float cmd_s_core_elen, cmd_s_core_ed_slen, cmd_s_core_ed_len;
extern const float cmd_s_core_ed_t;
extern const float cmd_s_core_et;
extern float cmd_s_core_ed_wait, cmd_s_core_ed_wait_sr2;
extern const float cmd_s_core_ed_wait_sr1;

/* 0x00cad170 */
void eden_core_vars_add(void)
{
    eden_core_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_core_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_core_vars_group, eden_core_vardefs);
}

/* 0x00cad1c0 */
void eden_core_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_core_vardef);
}

/* 0x00cad1d8 */
void eden_core_shot_fire(EdenCoreWork *w)
{
    unsigned int mask;

    if (w->opened)
        return;
    if (!eden_get_flags())
        return;
    mask = eden_core_shot_kinds[w->stage];
    if (mask & 1)
        eden_effect_dshine_scale((struct EdenShineWork *)eden_effect_dshine2_create(w, (int)&w->pts[0]), 8.0f);
    if (mask & 2)
        eden_effect_dshine_scale((struct EdenShineWork *)eden_effect_dshine2_create(w, (int)&w->pts[1]), 8.0f);
    eden_bullet_fb_spawn(w, w->parts, eden_core_shot_kinds[w->stage]);
}

/* 0x00cad2b8 */
void eden_core_exec(EdenCoreWork *w)
{
    int i, n, mul, hp;

    switch (w->state) {
    case 0:
        w->glow = 0.0f;
        break;
    case 1:
        w->frm += cmd_core_mot_v;
        if (w->frm_end <= w->frm) {
            w->frm = w->frm_end;
            w->state = 2;
            eden_core_shot_fire(w);
            w->time = w->len = (int)(cmd_fb_next_t * FPS);
        }
        w->glow = w->frm / w->frm_end;
        if (w->dead)
            w->state++;
        break;
    case 2:
        if (w->time) {
            w->glow = 1.0f - (float)w->time / (float)w->len;
            if (--w->time <= 0)
                eden_core_shot_fire(w);
        } else
            w->glow *= 0.5f;
        if (!w->opened)
            break;
        w->state++;
        peri_act_play_boss_break();
        boss_se_play(1, (int)w->lock, 0);
        oscilate_create(cmd_camera_fs, cmd_camera_fr);
        cm_sc_blur_create(w, 20.0f, 200.0f, 0.0f, 2.0f);
        w->parts[47]->speed = cmd_fl_mv;
        for (i = 0; i < 2; i++)
            cm_obj_mot_create(w, &w->pts[i], w->ang, w->parts[47], 0, 0.0f, 0.1f);
        break;
    case 3:
        w->frm -= cmd_core_mot_rv;
        w->glow *= 0.5f;
        if (w->frm <= 0.0f) {
            (w->lock)[0].flags &= ~0x100;
            (w->lock)[1].flags &= ~0x100;
            hp = cmd_core_hp;
            mul = (4 - w->stage) * 2;
            w->frm = 0.0f;
            w->state = 0;
            w->opened = 0;
            n = hp * mul / 5;
            w->stage++;
            if (n <= 1)
                n = 0;
            (w->lock)[0].life = n / 2;
            (w->lock)[1].life = n - n / 2;
        }
        break;
    }
}

/* 0x00cad5a0 */
void eden_core_hit_check(EdenCoreWork *w)
{
    LockObj *lock;
    Vec3 *pts;
    int *hp;
    int i;
    int sum = 0;
    int mask = 0;
    int n, k;
    Vec3 z;
    Vec3 *v;
    Vec3 *pt;

    v = &w->pts[1];
    pts = w->pts;
    tree_node_pose((WireObj *)w->parts[58]->root, (void *)w->parts[58]->clip, v, 0, 0, 1, w->frm);
    pts->x = -v->x;
    pts->y = -v->y;
    pts->z = -v->z;
    eden_core_exec(w);
    for (i = 0, hp = w->hp, lock = w->lock, pt = pts; i < 2; i++, pt++, hp++, lock++) {
        *hp = lock->life;
        sum += lock->life;
        if (lock->flags & LOCK_HIT) {
            boss_se_play(0, (int)lock, 0);
            eden_circle_spawn(w, pt, 0);
            mask |= 1 << i;
        }
    }
    if (sum <= 1)
        sum = 0;
    (w->lock)[0].life = sum / 2;
    (w->lock)[1].life = sum - sum / 2;
    k = (4 - w->stage) * 2;
    n = cmd_core_hp * k / 5;
    if (sum <= n && !w->opened) {
        w->opened = 1;
        (w->lock)[0].flags &= ~0x100;
        (w->lock)[1].flags &= ~0x100;
        if (sum <= 0) {
            peri_act_play_boss_finish();
            w->dead = 1;
            w->frm = 0;
            z.x = 0;
            z.y = 0;
            z.z = 0;
            cm_wire_break_simple(w, 2, w->parts[61], &z, &w->vel, w->ang, cm_core_col);
            cm_wire_break_simple(w, 2, w->parts[62], &z, &w->vel, w->ang, cm_core_col);
            {
                int hit = boss_get_level();
                if (!hit)
                    hit = 1;
                else
                    hit *= 2;
                hit *= cmd_core_score_fin;
                if (mask & 2)
                    score_at(w->lock[1].pos, hit);
                else
                    score_at(w->lock[0].pos, hit);
            }
        } else if (mask & 2)
            score_at(w->lock[1].pos, cmd_core_score);
        else
            score_at(w->lock[0].pos, cmd_core_score);
    }
    console2_set_boss_rate((float)sum / (float)(cmd_core_hp * 2));
}

/* 0x00cad878 */
void eden_core_glow_draw(EdenCoreWork *w)
{
    int old;
    unsigned int mask;
    ModelPart *part;

    if (!(fabsf(w->glow) < FLT_EPSILON)) {
        old = (int)fog_set_enable(0);
        mask = eden_core_shot_kinds[w->stage];
        if (mask & 1) {
            ulMatrixPush();
            MTX_TRANS(w->pts);
            ulMatrixUnitRotation((volatile float *)ulMatrixCur);
            vu0_matrix_scale(ulMatrixCur, cmd_lcore_scl, cmd_lcore_scl, cmd_lcore_scl);
            part = w->parts[4];
            model_object_draw(part, part->frames * w->glow);
            ulMatrixPop();
        }
        if (mask & 2) {
            ulMatrixPush();
            MTX_TRANS(&w->pts[1]);
            ulMatrixUnitRotation((volatile float *)ulMatrixCur);
            vu0_matrix_scale(ulMatrixCur, cmd_lcore_scl, cmd_lcore_scl, cmd_lcore_scl);
            part = w->parts[4];
            model_object_draw(part, part->frames * w->glow);
            ulMatrixPop();
        }
        fog_set_enable((void *)old);
    }
}

/* steer(cast) */
/* 0x00cad9f0 */
void eden_core_disp(EdenCoreWork *w)
{
    float t;
    int old;

    if (w->hp[0] || w->hp[1]) {
        ulGxSetTextureList(w->tex_list);
        t = eden_get_dalpha();
        if (!(fabsf(t) < FLT_EPSILON)) {
            old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_override_color();
            draw_push_strip_flags();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            eden_core_fade[0] = t + -1.0f;
            draw_set_override_color(eden_core_fade);
            ulMatrixPush();
            model_object_draw(w->parts[58], w->frm);
            model_object_draw(w->parts[60], w->frm);
            part_draw_beat_frame(w->parts[59], w->frm, eden_core_wire_cols[0], eden_core_wire_cols[1], 2.0f);
            ulMatrixPop();
            eden_core_glow_draw(w);
            draw_push_override_color();
            draw_push_strip_flags();
            draw_set_override_flags(old);
        }
    }
}

/* 0x00cadb38 */
void eden_core_release(EdenCoreWork *w)
{
    LockObj *lock;
    int i;

    console2_swap_boss_on(0);
    lock = w->lock;
    for (i = 0; i < 2; i++, lock++)
        lock_remove_entry(0, lock);
}

/* 0x00cadb90 */
int *eden_core_create(void *parent, char *src)
{
    EdenCoreWork *w = task_create_inherit(parent, &eden_core_desc, 236);
    LockObj *lock;
    Vec3 *p;
    int *hp;
    int i;

    w->parent = parent;
    w->parts = (ModelPart **)src;
    w->frm_end = ((ModelPart **)src)[58]->frames;
    for (i = 0, lock = w->lock, hp = w->hp, p = w->pts; i < 2; p++, i++, lock++, hp++) {
        lock_init(0, lock, w, p, 0, cmd_core_hp, 0);
        lockon_entry(lock);
        lock->flags &= ~LOCK_LOCKABLE;
        *hp = cmd_core_hp;
    }
    boss_gage_create(0);
    return (int *)w;
}

/* 0x00cadc88 */
void eden_core_open(EdenCoreWork *w, int mode)
{
    if (mode == 1) {
        if (!w->state) {
            w->state = mode;
            w->lock[0].flags |= LOCK_LOCKABLE;
            w->lock[1].flags |= LOCK_LOCKABLE;
            boss_se_play(2, (int)w->lock, 0);
        }
    } else if (w->state == 2)
        w->state = 3;
}

/* 0x00cadce8 */
int eden_core_get_state(EdenCoreWork *w)
{
    return w->state;
}

/* 0x00cadcf0 */
void eden_core_set_hits(EdenCoreWork *w)
{
    int n;
    int hits;

    if (w->state == 2 && !w->opened) {
        int hp, mul;

        hp = cmd_core_hp;
        mul = (4 - w->stage) * 2;
        w->opened = 1;
        n = hp * mul / 5;
        if (n <= 1)
            n = 0;
        hits = n / 2;
        (w->lock)[0].life = hits;
        (w->lock)[1].life = n - hits;
    }
}

/* steer(cast) */
/* 0x00cadd70 */
void eden_core_shine_exec(EdenCoreShineWork *shine)
{
    Vec3 tgt;
    Vec3 wob;
    Vec3 wob_a;
    Vec3 wob_b;
    float cs[2];

    switch (shine->mode) {
    case 0:
        spring_to_v(&shine->vel, &shine->pos, &shine->pos, cmd_s_core_mf, cmd_s_core_kf * shine->rate);
        {
            float damp_r = cmd_s_core_rr, damp_h = cmd_s_core_hr;

            shine->scl *= damp_r;
            shine->rate *= damp_h;
        }
        shine->active = 1;
        break;
    case 1:
        if (shine->req) {
            float damp_r, damp_h;

            spring_to_v(&shine->vel, &shine->pos, shine->req, cmd_s_core_mf, cmd_s_core_kf * shine->rate);
            damp_r = cmd_s_core_rr;
            damp_h = cmd_s_core_hr;
            shine->scl = shine->scl * damp_r + (1.0f - damp_r);
            shine->rate = shine->rate * damp_h + (1.0f - damp_h);
        } else {
            spring_to_v(&shine->vel, &shine->pos, &shine->home, cmd_s_core_mf, cmd_s_core_kf * shine->rate);
            shine->scl *= cmd_s_core_rr;
        }
        break;
    case 2:
        shine->mode++;
        shine->time = shine->len = (int)(cmd_s_core_et * FPS);
    case 3:
        ulSinCosf((1.0f - (float)shine->time / (float)shine->len) * PI, &cs[0], &cs[1]);
        {
            float z = 0.0f;

            tgt.y = z;
            tgt.x = z;
            tgt.z = cmd_s_core_elen * (cs[1] * -0.5f + 0.5f);
        }
        spring_to_v(&shine->vel, &shine->pos, &tgt, cmd_s_core_mf, cmd_s_core_kf * shine->rate);
        shine->rate = shine->rate * cmd_s_core_hr + (1.0f - cmd_s_core_hr);
        if (--shine->time <= 0)
            shine->mode = 0;
        break;
    case 4:
        break;
    case 5:
        shine->mode++;
        shine->time = shine->len = (int)(cmd_s_core_ed_t * FPS);
        shine->rate = 0.0f;
        shine->scl = 0.0f;
    case 6:
        player_get_pos(&shine->pos);
        shine->pos.z += cmd_s_core_ed_slen + cmd_s_core_ed_len * (1.0f - (float)shine->time / (float)shine->len);
        wobble_step_slow(&wob, cmd_s_core_ed_wait_sr1, &shine->phase);
        v3_sum(&shine->pos, &wob, &shine->pos);
        shine->scl = shine->scl * cmd_s_core_rr + (1.0f - cmd_s_core_rr);
        if (--shine->time <= 0) {
            shine->time = shine->len = (int)(cmd_s_core_ed_wait * FPS);
            shine->mode = 7;
        }
        break;
    case 7:
        v3_scale(&shine->pos, cmd_s_core_hr);
        wobble_step_slow(&wob_a, cmd_s_core_ed_wait_sr2, &shine->phase);
        v3_sum(&shine->pos, &wob_a, &shine->pos);
        if (--shine->time <= 0)
            shine->mode++;
        break;
    case 8:
        v3_scale(&shine->pos, cmd_s_core_hr);
        wobble_step_slow(&wob_b, cmd_s_core_ed_wait_sr2, &shine->phase);
        v3_sum(&shine->pos, &wob_b, &shine->pos);
        shine->scl *= cmd_s_core_rr;
        break;
    }
    if (shine->scl < cmd_s_core_rlim)
        shine->visible = 0;
    else
        shine->visible = 1;
    if (shine->visible && shine->mode == 1 && shine->active)
        shine->lock.flags |= LOCK_LOCKABLE;
    else
        shine->lock.flags &= ~LOCK_LOCKABLE;
    dc_object_advance_time(shine->parts[28]);
    if (shine->lock.flags & LOCK_HIT) {
        eden_circle_spawn(shine, &shine->pos, 1);
        boss_se_play(shine->stage % 4 + 5, 0, (int)&shine->pos);
    }
    {
        unsigned short n = cmd_score_hp;

        shine->hits += cmd_score_hp - shine->lock.life;
        shine->lock.life = n;
    }
}

/* 0x00cae300 */
void eden_core_shine_disp(EdenCoreShineWork *w)
{
    int old_blend;
    float k;

    if (!w->visible)
        return;
    old_blend = draw_set_override_flags(0);
    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    ulMatrixUnitRotation((volatile float *)ulMatrixCur);
    k = w->scl;
    vu0_matrix_scale(ulMatrixCur, k, k, k);
    dc_object_draw(w->parts[28]);
    dc_object_draw(w->parts[29]);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00cae3c8 */
void eden_core_shine_release(EdenCoreShineWork *w)
{
    lock_remove_entry(16, &w->lock);
}

/* 0x00cae3d8 */
int *eden_core_shine_create(void *parent, int arg)
{
    EdenCoreShineWork *w = task_create_inherit(parent, &eden_core_shine_desc, 192);
    LockObj *lock;

    w->parent = parent;
    w->parts = (ModelPart **)arg;
    lock = &w->lock;
    lock_init(16, lock, w, &w->pos, 0, cmd_score_hp, 0);
    lockon_entry(lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    task_set_dlist(w, 6);
    return (int *)w;
}

/* 0x00cae480 */
void eden_core_shine_set_req(int *shine, int req)
{
    EdenCoreShineWork *core;

    if (shine)
        core = (EdenCoreShineWork *)shine;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core)
        core->req = (Vec3 *)req;
}

/* 0x00cae4c0 */
int eden_core_shine_get_pos(int core)
{
    if (!core)
        core = eden_get_core((struct EdenWork *)boss_get_task());
    return core + 44;
}

/* 0x00cae4f0 */
void eden_core_shine_reset(int phase)
{
    EdenCoreShineWork *core;

    if (phase)
        core = (EdenCoreShineWork *)phase;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core) {
        core->req = 0;
        core->rate = 0;
    }
}

/* 0x00cae528 */
void eden_core_shine_set_mode(int *shine, int mode, int sub)
{
    EdenCoreShineWork *core;

    if (shine)
        core = (EdenCoreShineWork *)shine;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core) {
        core->mode = sub;
        core->stage = mode;
    }
}

/* 0x00cae578 */
int eden_core_shine_get_mode(int *shine)
{
    EdenCoreShineWork *core;

    if (shine)
        core = (EdenCoreShineWork *)shine;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core)
        return core->mode;
    return -1;
}

/* 0x00cae5b0 */
int eden_core_shine_get_hits(int *shine)
{
    EdenCoreShineWork *core;

    if (shine)
        core = (EdenCoreShineWork *)shine;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core)
        return core->hits;
    return -1;
}

/* 0x00cae5e8 */
void eden_core_shine_clear_hits(int *shine)
{
    EdenCoreShineWork *core;

    if (shine)
        core = (EdenCoreShineWork *)shine;
    else
        core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());
    if (core)
        core->hits = 0;
}

/* 0x00cae620 */
void eden_core_shine_set_active(int on)
{
    EdenCoreShineWork *core = (EdenCoreShineWork *)eden_get_core((struct EdenWork *)boss_get_task());

    if (core) {
        if (on)
            core->active = 1;
        else
            core->active = 0;
    }
}
