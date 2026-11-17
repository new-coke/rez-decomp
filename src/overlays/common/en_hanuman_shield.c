/*
 * Hanuman's shield: rides in front of its Hanuman; parts and cores are shot off one at a time.
 * Once released it turns and accelerates at the player and crashes into them unless it passes.
 */

#include "types_common.h"
#include "task.h"
#include "color.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_draw.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_hanuman_shield.h"
#include "en_hanuman_shield_break.h"
#include "enemy_shatter.h"
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

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *stage_textures;
extern void *stage_models;

extern int en_hanuman_shield_init_arg;
extern void (*en_hanuman_shield_exec_table[])(void *o, void *w);
extern void (*en_hanuman_shield_disp_table[])(void *o, void *w);
extern ParamRec en_hanuman_shield_params;
extern VarDef en_hanuman_shield_vardefs[14];
extern int en_hanuman_shield_node_mtx;
#define EN_HANUMAN_SHIELD_REC ((EnHanumanShieldRec *)&en_hanuman_shield_params)
extern void (*en_hanuman_shield_states[])(void *o, EnHanumanShieldWork *w);
extern char en_hanuman_shield_lock_name[];
extern char en_hanuman_shield_name[];
extern float en_hanuman_shield_on_frms[2][8];
extern float en_hanuman_shield_node_frms[2][8];
extern float en_hanuman_shield_fade_color[4];

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define EN_HANUMAN_SHIELD_MODEL(n) ((n) + 81)
#else
#define EN_HANUMAN_SHIELD_MODEL(n) (n)
#endif

#if defined(OVERLAY_STG22)
extern float en_hanuman_shield_on_frms0[7];
extern float en_hanuman_shield_on_frms1[7];
extern float en_hanuman_shield_node_frms0[7];
extern float en_hanuman_shield_node_frms1[7];
#endif

/* 0x00b05c80 stg02, 0x00afffc8 stg22 */
void en_hanuman_shield_vars_register(void)
{
    VarDef d;
    VarDef *e;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&d, 0, sizeof d);
    d.name = en_hanuman_shield_params.name;
    d.fmt = en_hanuman_shield_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&d);
    e = en_hanuman_shield_vardefs;
    val = en_hanuman_shield_params.v;
    for (i = 0; i < 14; i++) {
        e->ptr = val;
        val++;
        e++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_hanuman_shield_vardefs);
}

/* 0x00b05d30 stg02, 0x00b00078 stg22 */
void en_hanuman_shield_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b05d38 stg02, 0x00b00080 stg22 */
int en_hanuman_shield_vars_init(int *fx_id)
{
    en_hanuman_shield_vars_register();
    en_hanuman_shield_init_arg = *fx_id;
    return 0;
}

/* 0x00b05d70 stg02, 0x00b000b8 stg22 */
int en_hanuman_shield_vars_release(void)
{
    en_hanuman_shield_vars_unregister();
    return 0;
}

/* 0x00b05d90 stg02, 0x00b000d8 stg22 */
void en_hanuman_shield_calc_parts(void *obj, int *work)
{
    EnHanumanShieldWork *w = (EnHanumanShieldWork *)work;
    Vec3 *p;
    int i;

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hanuman_shield_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    for (i = 6; i > 0; i--) {
        if (w->flags & (128 << i) && !(w->flags & (0x4000 << i))) {
            ulMatrixPush();
            vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_hanuman_shield_node_mtx + i * 16);
            ulMatrixGetTranslation((float *)ulMatrixCur, &w->pts[i].x);
            if (w->phase == 1)
                model_camera_angles(w->angs[i]);
            ulMatrixPop();
        }
    }
    if ((w->flags & 0x4080) == 0x80) {
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_hanuman_shield_node_mtx);
        ulMatrixGetTranslation((float *)ulMatrixCur, &w->pts[0].x);
    }
    ulMatrixPop();
    w->flags |= 0x200000;
}

/* 0x00b05f20 stg02, 0x00b00268 stg22 */
void en_hanuman_shield_calc_parts_once(void *obj, int *w)
{
    if (!(w[12] & 0x200000))
        en_hanuman_shield_calc_parts(obj, w);
}

/* 0x00b05f48 stg02, 0x00b00290 stg22 */
void en_hanuman_shield_uv_scroll(void *obj, int *work)
{
    EnHanumanShieldWork *w = (EnHanumanShieldWork *)work;
    float sc[2];
    float rate, time, end;

    if (w->flags & 0x400000) {
        time = w->timer += 1.0f;
        end = EN_HANUMAN_SHIELD_REC->frm_mirroring * FPS;
        if (time <= end) {
            rate = time / end;
            w->u = (1.0f - rate) * 0.5f;
            w->v = (rate - 1.0f) * 0.5f;
        } else {
            ulSinCos((int)((int)DEG_TO_ANG(EN_HANUMAN_SHIELD_REC->spd_ang_uvscroll * FRAME_SEC) * enemy_task_get_time(obj)),
                          &sc[0], &sc[1]);
            w->u = EN_HANUMAN_SHIELD_REC->rng_uvscroll * sc[1];
            w->flags = (w->flags | 0x1000000) & ~0x800000;
            w->v = EN_HANUMAN_SHIELD_REC->rng_uvscroll * sc[0];
        }
    }
}

/* 0x00b06078 stg02, 0x00b003c0 stg22 */
#if defined(OVERLAY_STG22)
void en_hanuman_shield_part_shatter(void *obj, int *work, int i)
{
    EnHanumanShieldWork *w = (EnHanumanShieldWork *)work;
    SpawnDesc desc;
    float frm;

    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x40000;
    desc.models = stage_models;
    desc.type = 116;
    desc.motion = 117;
    if (w->flags & (128 << i))
        desc.time = 9.0f;
    else {
        frm = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        desc.time = frm;
        while (10.0f <= desc.time)
            desc.time -= 10.0f;
    }
    v3_copy(&desc.pos, &w->pts[i]);
    i3_copy(desc.ang, w->angs[i]);
    enemy_task_get_scl(obj, &desc.scale);
    desc.v.x = 0.0f;
    desc.v.y = 0.0f;
    desc.v.z = 0.0f;
    enemy_shatter_create(obj, &desc, 0, 0);
}
#else
void en_hanuman_shield_part_shatter(void *obj, int *work, int i)
{
    EnHanumanShieldWork *w = (EnHanumanShieldWork *)work;
    SpawnDesc desc;
    float frm;

    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x40000;
    desc.models = stage_models;
    desc.type = 35;
    desc.motion = 36;
    if (w->flags & (128 << i))
        desc.time = 9.0f;
    else {
        frm = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        desc.time = frm;
        while (10.0f <= desc.time)
            desc.time -= 10.0f;
    }
    v3_copy(&desc.pos, &w->pts[i]);
    i3_copy(desc.ang, w->angs[i]);
    enemy_task_get_scl(obj, &desc.scale);
    desc.v.x = 0.0f;
    desc.v.y = 0.0f;
    desc.v.z = 0.0f;
    enemy_shatter_create(obj, &desc, 0, 0);
}
#endif

/* 0x00b061d0 stg02, 0x00b00518 stg22 */
void en_hanuman_shield_shatter_all(void *obj, int *w)
{
    int i;

    for (i = 6; i >= 0; i--)
        if ((w[12] >> i) & 1 && !(w[12] & (0x4000 << i)))
            en_hanuman_shield_part_shatter(obj, w, i);
}

/* 0x00b06258 stg02, 0x00b005a0 stg22 */
void en_hanuman_shield_spawn_piece(void *obj, EnHanumanShieldWork *w, int k, Vec3 *v)
{
    Vec3 p;
    int ang[4];

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hanuman_shield_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)&en_hanuman_shield_node_mtx + k * 16);
    ulMatrixGetTranslation((float *)ulMatrixCur, &p.x);
    model_camera_angles(ang);
    ulMatrixPop();
    en_hanuman_shield_break_create(obj, &p, ang, v, 1);
}

/* 0x00b06338 stg02, 0x00b00680 stg22 */
void en_hanuman_shield_hit_check(void *obj, EnHanumanShieldWork *w)
{
    int slot = enemy_task_get_status_slot(obj);
    LockObj *lock = w->lock;
    Vec3 *p = w->pts;
    int i;

    for (i = 0; i < 7; i++, lock++, p++) {
        if (!(lock->flags & LOCK_HIT))
            continue;
        if (lock->life > 0)
            continue;
        enemy_status_score_popup(slot, p, 10, lockon_hits_max_shot_cnt(lock));
        lock->flags &= ~LOCK_LOCKABLE;
        en_hanuman_shield_spawn_piece(obj, w, i, &lock->imp.v);
        w->flags |= 0x4000 << i;
    }
    if ((w->flags & 0x1fc000) == 0x1fc000)
        enemy_task_set_state(obj, 3);
}

/* 0x00b06478 stg02, 0x00b007c0 stg22 */
void en_hanuman_shield_crash_check(void *obj, EnHanumanShieldWork *w)
{
    Vec3 to_pl;
    Vec3 n;
    float dis;
    int i, bit;

    for (i = 0; i < 7; i++) {
        bit = 0x4000 << i;
        if (w->flags & bit)
            continue;
        if (player_sphere_test(&w->pts[i], EN_HANUMAN_SHIELD_REC->collision_r) != 1)
            continue;
        player_get_pos(&to_pl);
        v3_sub(&to_pl, &to_pl, &w->pts[i]);
        v3_set(&n, 0.0f, 0.0f, 1.0f);
        ulMatrixPush();
        matrix_unit_set(0);
        en_hanuman_shield_rotate_yxz(w->angs[i]);
        ulMatrixApplyVector((float *)ulMatrixCur, &n.x, &n.x);
        ulMatrixPop();
        dis = vec3_length(&to_pl);
        vec3_normalize(&to_pl, &to_pl);
        vec3_normalize(&n, &n);
        dis *= ulVec3Dot(&to_pl, &n);
        if (dis < EN_HANUMAN_SHIELD_REC->collision_z) {
            player_status_add_damage(1.0f);
            n.x = -n.x;
            n.y = -n.y;
            n.z = -n.z;
            en_hanuman_shield_spawn_piece(obj, w, i, &n);
            w->flags |= bit;
        }
    }
}

/* 0x00b06678 stg02, 0x00b009c0 stg22 */
#if defined(OVERLAY_STG22)
void en_hanuman_shield_state_appear(void *obj, EnHanumanShieldWork *w)
{
    float frm;
    float *on_frms;
    float *node_frms;
    int i;

    switch (w->step) {
    case 0:
        w->flags |= 0x800000;
        w->u = 0.5f;
        w->v = -0.5f;
        w->step++;
        w->next_step++;
    case 1:
        frm = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        if (w->k) {
            on_frms = en_hanuman_shield_on_frms1;
            node_frms = en_hanuman_shield_node_frms1;
        } else {
            on_frms = en_hanuman_shield_on_frms0;
            node_frms = en_hanuman_shield_node_frms0;
        }
        for (i = 6; i >= 0; i--) {
            if (!((w->flags >> i) & 1) && on_frms[i] <= frm) {
                w->flags |= 1 << i;
                SoundRhythmSe_Enemy(en_hanuman_shield_init_arg, 2);
            }
        }
        for (i = 6; i >= 0; i--) {
            if (!(w->flags & (128 << i)) && node_frms[i] <= frm) {
                lockon_entry(&w->lock[i]);
                w->flags |= 128 << i;
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}
#else
void en_hanuman_shield_state_appear(void *obj, EnHanumanShieldWork *w)
{
    float frm;
    float *on_frms, *node_frms;
    int i;

    switch (w->step) {
    case 0:
        w->flags |= 0x800000;
        w->u = 0.5f;
        w->v = -0.5f;
        w->step++;
        w->next_step++;
    case 1:
        frm = ((ModelPart *)enemy_task_gfx_ptr(obj))->frame;
        if (w->k) {
            on_frms = en_hanuman_shield_on_frms[1];
            node_frms = en_hanuman_shield_node_frms[1];
        } else {
            on_frms = en_hanuman_shield_on_frms[0];
            node_frms = en_hanuman_shield_node_frms[0];
        }
        for (i = 6; i >= 0; i--) {
            if (!((w->flags >> i) & 1) && on_frms[i] <= frm) {
                w->flags |= 1 << i;
                SoundRhythmSe_Enemy(en_hanuman_shield_init_arg, 2);
            }
        }
        for (i = 6; i >= 0; i--) {
            if (!(w->flags & (128 << i)) && node_frms[i] <= frm) {
                lockon_entry(&w->lock[i]);
                w->flags |= 128 << i;
            }
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}
#endif

/* 0x00b06840 stg02, 0x00b00b88 stg22 */
void en_hanuman_shield_state_attack(void *obj, EnHanumanShieldWork *w)
{
    Vec3 pl;
    Vec3 p;
    Vec3 pos;
    int cur[3];
    Vec3 step;
    Vec3 cur_pos;
    Vec3 pl2;
    Vec3 to_me;
    Vec3 fwd;
    float z, dot;

    switch (w->step) {
    case 0:
        player_get_pos(&pl);
        enemy_task_get_pos(obj, &p);
        if (pl.x > p.x)
            w->tang[1] = (int)DEG_TO_ANG(EN_HANUMAN_SHIELD_REC->ang_rotate_y_limit) + ANG_180;
        else
            w->tang[1] = ANG_180 - (int)DEG_TO_ANG(EN_HANUMAN_SHIELD_REC->ang_rotate_y_limit);
        if (pl.y > p.y)
            w->tang[0] = (int)DEG_TO_ANG(EN_HANUMAN_SHIELD_REC->ang_rotate_x_limit);
        else
            w->tang[0] = -(int)DEG_TO_ANG(EN_HANUMAN_SHIELD_REC->ang_rotate_x_limit);
        w->tang[2] = 0;
        w->step++;
        w->next_step++;
        w->owner = 0;
    case 1:
        w->spd = w->spd + EN_HANUMAN_SHIELD_REC->acc_attack * 0.00027777778f < EN_HANUMAN_SHIELD_REC->spd_attack_max * FRAME_SEC
                     ? w->spd + EN_HANUMAN_SHIELD_REC->acc_attack * 0.00027777778f
                     : EN_HANUMAN_SHIELD_REC->spd_attack_max * FRAME_SEC;
        player_get_frame_accel(&step);
        vec3_scale_to_length(-w->spd, &step.x, &step.x);
        enemy_task_get_pos(obj, &pos);
        v3_add(&pos, &step);
        enemy_task_set_pos(obj, &pos);
        enemy_task_get_rot(obj, cur);
        ang3_approach(w->tang, cur, EN_HANUMAN_SHIELD_REC->rto_ang_rotate);
        enemy_task_set_rot(obj, cur);
        en_hanuman_shield_crash_check(obj, w);
        enemy_task_get_pos(obj, &cur_pos);
        player_get_frame_pos(&pl2);
        player_get_frame_accel(&fwd);
        v3_sub(&to_me, &cur_pos, &pl2);
        if (ulVec3Dot(&to_me, &fwd) < 0.0f) {
            float k;
            vec3_normalize(&to_me, &to_me);
            vec3_normalize(&fwd, &fwd);
            dot = ulVec3Dot(&to_me, &fwd);
            z = EN_HANUMAN_SHIELD_REC->away_z;
            k = vec3_distance_squared(&cur_pos, &pl2);
            if (z * z < k * -dot)
                enemy_task_set_state(obj, 2);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b06bb8 stg02, 0x00b00f00 stg22 */
void en_hanuman_shield_exec_normal(void *obj, EnHanumanShieldWork *w)
{
    if (EN_HANUMAN_SHIELD_REC->frm_mirroring * FPS + EN_HANUMAN_SHIELD_REC->frm_attack_start * FPS < w->timer) {
        if (w->phase == 0)
            do {
                w->next_step = 0;
                w->next = 1;
            } while (0);
    }
    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        if (en_hanuman_shield_states[w->phase])
            en_hanuman_shield_states[w->phase](obj, w);
    }
}

/* 0x00b06c88 stg02, 0x00b00fd0 stg22 */
void en_hanuman_shield_exec_destroyed(void *obj, void *w)
{
    if (enemy_task_get_phase(obj) == 1)
        enemy_task_next_phase(obj);
}

/* 0x00b06cc8 stg02, 0x00b01010 stg22 */
void en_hanuman_shield_exec(void *obj, void *w)
{
    void (**t)(void *obj, void *w) = en_hanuman_shield_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (t[i])
        t[i](obj, w);
}

/* 0x00b06d28 stg02, 0x00b01070 stg22 */
void en_hanuman_shield_pre_exec(void *obj, EnHanumanShieldWork *w)
{
    void *owner = w->owner;
    float t;

    if (!owner || TASK_OF(owner)->flags & TASK_F_END) {
        w->owner = 0;
        if (!(w->flags & 0x1000000)) {
            en_hanuman_shield_shatter_all(obj, (int *)w);
            enemy_task_set_state(obj, 3);
            owner = w->owner;
        } else
            owner = 0;
    }
    if (owner) {
        t = ((ModelPart *)enemy_task_gfx_ptr(owner))->frame;
        ((ModelPart *)enemy_task_gfx_ptr(obj))->frame = t;
        if (EN_HANUMAN_SHIELD_REC->frm_mirror_start <= t)
            w->flags |= 0x400000;
    }
    en_hanuman_shield_hit_check(obj, w);
}

/* 0x00b06e00 stg02, 0x00b01148 stg22 */
void en_hanuman_shield_post_exec(void *obj, int *w)
{
    en_hanuman_shield_calc_parts_once(obj, w);
    w[12] &= ~0x200000;
    en_hanuman_shield_uv_scroll(obj, w);
}

/* 0x00b06e50 stg02, 0x00b01198 stg22 */
void en_hanuman_shield_toggle_parts(void *obj, int *w)
{
    int i;
    int *n = (int *)((int *)((ModelPart *)enemy_task_gfx_ptr(obj))->root)[11];

    for (i = 0; i < 7; i++) {
        if (w[12] & (0x4000 << i) || !((w[12] >> i) & 1))
            util_node_toggle(n);
        n = (int *)n[12];
    }
}

/* 0x00b06ee0 stg02, 0x00b01228 stg22 */
void en_hanuman_shield_toggle_cores(void *obj, int *w)
{
    int i;
    int *n = (int *)((int *)((ModelPart *)enemy_task_gfx_ptr(obj))->root)[11];

    for (i = 0; i < 7; i++) {
        if (!(w[12] & (0x4000 << i)) && (w[12] >> i) & 1)
            util_node_toggle((int *)n[11]);
        n = (int *)n[12];
    }
}

/* 0x00b06f70 stg02, 0x00b012b8 stg22 */
/* steer(cast) */
void en_hanuman_shield_disp_body(void *obj, EnHanumanShieldWork *w)
{
    Vec3 p;
    int a[3];
    Vec3 scl;
    ModelPart *gfx;
    int prev, dl;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, a);
    enemy_task_get_scl(obj, &scl);
    if (!view_sphere_visible(&p, EN_HANUMAN_SHIELD_REC->clip_r))
        return;
    gfx = enemy_task_gfx_ptr(obj);
    if (w->flags & 0x800000)
        prev = draw_set_override_flags(0x20820);
    else
        prev = draw_set_override_flags(0x20000);
    ulMatrixPush();
    do {
        Vec3 *p_ = &p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_hanuman_shield_rotate_yxz(a);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    en_hanuman_shield_toggle_parts(obj, (int *)w);
    if (w->flags & 0x800000) {
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(stage_textures);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        draw_set_override_color(en_hanuman_shield_fade_color);
        en_hanuman_shield_toggle_cores(obj, (int *)w);
        nj_set_screen_offset(w->u, w->v);
        dc_object_draw(gfx);
        nj_set_screen_offset(0.0f, 0.0f);
        en_hanuman_shield_toggle_cores(obj, (int *)w);
        en_hanuman_shield_toggle_parts(obj, (int *)w);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
        RenderSetCurrentDisplayList(dl);
        util_node_toggle((int *)gfx->root);
        en_hanuman_shield_toggle_cores(obj, (int *)w);
        dc_object_draw(gfx);
        en_hanuman_shield_toggle_cores(obj, (int *)w);
        util_node_toggle((int *)gfx->root);
        en_hanuman_shield_toggle_parts(obj, (int *)w);
    } else {
        ulGxSetTextureList(stage_textures);
        nj_set_screen_offset(w->u, w->v);
        dc_object_draw(gfx);
        nj_set_screen_offset(0.0f, 0.0f);
    }
    if (w->flags & 0x800000) {
        dl = RenderSetCurrentDisplayList(6);
        draw_set_color(RGBA(0x10, 0xc8, 0xc8, 0x80));
        model_object_draw(&w->body, gfx->frame);
        RenderSetCurrentDisplayList(dl);
    } else {
        draw_set_color(RGBA(0x10, 0xc8, 0xc8, 0x80));
        model_object_draw(&w->body, gfx->frame);
    }
    en_hanuman_shield_toggle_parts(obj, (int *)w);
    ulMatrixPop();
}

/* 0x00b07228 stg02, 0x00b01570 stg22 */
void en_hanuman_shield_disp(void *obj, void *w)
{
    void (**t)(void *obj, void *w) = en_hanuman_shield_disp_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (t[i])
        t[i](obj, w);
}

/* 0x00b07288 stg02, 0x00b015d0 stg22 */
void en_hanuman_shield_init(void *obj, EnHanumanShieldWork *w)
{
    int i;
    ModelPart *gfx;
    ModelPart *part;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    do {
        w->next = 0;
        w->next_step = 0;
    } while (0);
    enemy_task_clear_flags(obj, 64);

    part = &w->body;
    for (i = 7; --i >= 0;)
        lockon_obj_init(&w->lock[i], obj, &w->pts[i], 0, en_hanuman_shield_lock_name, enemy_task_get_status_slot(obj), 1, 1, 0);
    gfx = enemy_task_gfx_ptr(obj);
    dc_object_init(part, gfx->root, gfx->clip, 0);
    dc_object_set_wire(part, model_pnb_get_wire(stage_models, EN_HANUMAN_SHIELD_MODEL(32)));
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b073c0 stg02, 0x00b01708 stg22 */
void en_hanuman_shield_release(void *obj, EnHanumanShieldWork *w)
{
    LockObj *lock;
    int i;

    for (i = 6, lock = &w->lock[6]; i >= 0; i--, lock--) {
        do {
            lockon_remove(lock);
        } while (0);
    }
    dc_object_release(&w->body);
}

/* 0x00b07410 stg02, 0x00b01758 stg22 */
int en_hanuman_shield_create(void *parent, EnHanumanShieldArg *prm, void *owner, int dir_set)
{
    ObjDesc desc;
    ObjFuncs funcs;
    int task;
    EnHanumanShieldWork *w;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_hanuman_shield_init;
    funcs.fn[1] = en_hanuman_shield_release;
    funcs.fn[2] = en_hanuman_shield_exec;
    funcs.fn[3] = en_hanuman_shield_disp;
    funcs.fn[4] = en_hanuman_shield_pre_exec;
    funcs.fn[5] = en_hanuman_shield_post_exec;
    desc.name = en_hanuman_shield_name;
    desc.funcs = &funcs;
    desc.arg = prm;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = EN_HANUMAN_SHIELD_MODEL(32);
    desc.model_no2 = dir_set ? EN_HANUMAN_SHIELD_MODEL(34) : EN_HANUMAN_SHIELD_MODEL(33);
    desc.hp = 1;
    desc.score = 10;
    *(unsigned char *)prm = *(unsigned char *)&en_hanuman_shield_init_arg;
    desc.size = 780;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    task = enemy_task_task_create(parent, &desc);
    w = enemy_task_work_ptr((void *)task);
    w->owner = owner;
    w->k = dir_set;
    return task;
}

/* 0x00b07588 stg02, 0x00b018d0 stg22 */
/* steer(cast) */
void en_hanuman_shield_nodes_init(void)
{
    int mdl = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HANUMAN_SHIELD_MODEL(32));
    int *src = (int *)*((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_HANUMAN_SHIELD_MODEL(33));

    anim_start(src, (float)(unsigned int)src[1] - 1.0f);
    node_tree_draw_matching_at(mdl, 0x1000000, 7, (int)&en_hanuman_shield_node_mtx, 1);
}

/* 0x00b07628 stg02, 0x00b01970 stg22 */
void en_hanuman_shield_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
