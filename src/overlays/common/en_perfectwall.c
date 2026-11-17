/*
 * Perfect Wall half (stg04): a wall section with lock points, placed by en_perfectwall_manager.
 * Flashes when the halves join, regains its points when they unite, shatters once every point is
 * shot or the manager says so, otherwise floats away. Glow colours come from motion curves.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "approach.h"
#include "color.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_perfectwall.h"
#include "en_break.h"
#include "en_perfectwall_manager.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "util.h"
#include "mathdef.h"

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern void *stage_textures;
extern void *stage_models;

extern ObjStateFn en_perfectwall_exec_table[];
extern ObjStateFn en_perfectwall_disp_table[];
extern float en_perfectwall_flash_table[];
extern int en_perfectwall_part_models[2];

extern ParamRec en_perfectwall_params;
extern VarDef en_perfectwall_vardefs[13];
extern int en_perfectwall_motions[];
extern char en_perfectwall_name[];
#define EN_PERFECTWALL_REC ((EnPerfectWallRec *)&en_perfectwall_params)
extern Vec3 en_perfectwall_pull_points[];
extern Vec3 en_perfectwall_points[];
extern Vec3 en_perfectwall_pull_points2[];
extern Vec3 en_perfectwall_points2[];
extern Vec3 en_perfectwall_end_points[][2];
extern Vec3 en_perfectwall_tip_points[];
extern int en_perfectwall_break_models[];
extern void (*en_perfectwall_states[])(void *o, EnPerfectWallWork *w);
extern float en_perfectwall_fade_color[4];
extern char en_perfectwall_lock_name[];
extern int en_perfectwall_kind_models[2];
extern int en_perfectwall_lock_counts[2];

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define EN_PERFECTWALL_MODEL(n) ((n) + 14)
#else
#define EN_PERFECTWALL_MODEL(n) (n)
#endif

/* 0x00b0cd98 stg04, 0x00b05b70 stg22 */
float depth_fade_rate(float z_near, float z_far, float a_near, float a_far)
{
    Vec3 pos;
    float z;

    if (z_far <= z_near)
        return 1.0f;
    ulMatrixGetTranslation((float *)ulMatrixCur, &pos.x);
    z = fabsf(pos.z);
    if (z_far < z)
        return a_far;
    if (z < z_near)
        return a_near;
    return (a_far - a_near) * (z - z_near) / (z_far - z_near) + a_near;
}

/* 0x00b0ce50 stg04, 0x00b05c28 stg22 */
void motion_sample_color(void *mot, float t, float z_near, float z_far, float a_near, float a_far, float *out)
{
    Vec3 pos;
    Vec3 scl;

    ulMatrixPush();
    matrix_unit_set(0);
    anim_start(mot, t);
    anim_apply_node((unsigned int *)system_pnb_entry_get(0));
    ulMatrixGetTranslation((float *)ulMatrixCur, &pos.x);
    yuki_matrix_get_scale(&scl);
    ulMatrixPop();
    if (0.99f < scl.x)
        scl.x = 1.0f;
    out[1] = pos.x;
    out[2] = pos.y;
    out[3] = pos.z;
    do {
        out[0] = scl.x * depth_fade_rate(z_near, z_far, a_near, a_far);
    } while (0);
}

/* 0x00b0cf68 stg04, 0x00b05d40 stg22 */
unsigned int motion_sample_rgba(void *mot, float t, float z_near, float z_far, float a_near, float a_far)
{
    float rgba[4];

    motion_sample_color(mot, t, z_near, z_far, a_near, a_far, rgba);
    return (((int)(rgba[0] * 255.0f) + 1) >> 1 << 24) | (((int)(rgba[3] * 255.0f) & 0xff) << 16) | (((int)(rgba[2] * 255.0f) & 0xff) << 8) | ((int)(rgba[1] * 255.0f) & 0xff);
}

/* 0x00b0cff8 stg04, 0x00b05dd0 stg22 */
float motion_sample_alpha(void *mot, float t, float z_near, float z_far, float a_near, float a_far)
{
    Vec3 scl;

    ulMatrixPush();
    matrix_unit_set(0);
    anim_start(mot, t);
    anim_apply_node((unsigned int *)system_pnb_entry_get(0));
    yuki_matrix_get_scale(&scl);
    ulMatrixPop();
    if (0.99f < scl.x)
        scl.x = 1.0f;
    return scl.x * depth_fade_rate(z_near, z_far, a_near, a_far);
}

/* 0x00b0d0d0 stg04, 0x00b05ea8 stg22 */
void en_perfectwall_vars_register(void)
{
    VarDef grp_def;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&grp_def, 0, sizeof grp_def);
    grp_def.name = en_perfectwall_params.name;
    grp_def.fmt = en_perfectwall_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&grp_def);
    def = en_perfectwall_vardefs;
    val = en_perfectwall_params.v;
    for (i = 0; i < 13; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_perfectwall_vardefs);
}

/* 0x00b0d180 stg04, 0x00b05f58 stg22 */
void en_perfectwall_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0d188 stg04, 0x00b05f60 stg22 */
void en_perfectwall_calc_points(void *obj, EnPerfectWallWork *w)
{
    Vec3 *pull = 0, *rest = 0;
    Vec3 pos;
    int ang[3];
    Vec3 pulled[4];
    int n, i;
    float t;
    ModelPart *part;

    switch (w->kind) {
    case 0:
        pull = en_perfectwall_pull_points;
        rest = en_perfectwall_points;
        break;
    case 1:
        pull = en_perfectwall_pull_points2;
        rest = en_perfectwall_points2;
        break;
    }
    ulMatrixPush();
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    n = w->n;
    matrix_unit_set(0);
    do {
        vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    } while (0);
    en_perfectwall_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    matrix_apply_points(rest, w->pts, n);
    if (w->skip & 0x1c00) {
        matrix_apply_points(pull, pulled, n);
        part = &w->part;
        t = 1.0f - part->frame / part->frames;
        for (i = n - 1; i >= 0; i--)
            if (w->skip & (1024 << i))
                vec3_approach(&pulled[i], &w->pts[i], t);
    }
    ulMatrixPop();
    w->skip |= 1;
}

/* 0x00b0d338 stg04, 0x00b06110 stg22 */
void en_perfectwall_calc_ends(void *obj, EnPerfectWallWork *w)
{
    Task *parent = TASK_OF(obj)->parent;
    Vec3 pos;
    int ang[3];

    if (en_perfectwall_manager_has_lightning((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0))) {
        ulMatrixPush();
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        matrix_unit_set(0);
        do {
        vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    } while (0);
        vu0_matrix_rotate_z(ulMatrixCur, -ang[2]);
        matrix_apply_points(en_perfectwall_end_points[w->kind], w->ends, 2);
        ulMatrixPop();
    }
    w->skip |= 2;
}

/* 0x00b0d408 stg04, 0x00b061e0 stg22 */
void en_perfectwall_calc_tip(void *obj, EnPerfectWallWork *w)
{
    Vec3 pos;
    int ang[3];

    ulMatrixPush();
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    matrix_unit_set(0);
    do {
        vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    } while (0);
    vu0_matrix_rotate_z(ulMatrixCur, -ang[2]);
    ulMatrixApplyPoint((float *)ulMatrixCur, &en_perfectwall_tip_points[w->kind].x, &w->tip.x);
    ulMatrixPop();
    w->skip |= 4;
}

/* 0x00b0d4b8 stg04, 0x00b06290 stg22 */
void en_perfectwall_calc_shape(void *obj, int *work)
{
    EnPerfectWallWork *w = (EnPerfectWallWork *)work;

    if (!(w->skip & 1))
        en_perfectwall_calc_points(obj, w);
    if (!(w->skip & 2))
        en_perfectwall_calc_ends(obj, w);
    if (!(w->skip & 4))
        en_perfectwall_calc_tip(obj, w);
}

/* 0x00b0d538 stg04, 0x00b06310 stg22 */
void en_perfectwall_shatter(void *obj, void *work)
{
    EnPerfectWallWork *w = work;
    Vec3 pos;
    int ang[3];
    SpawnDesc desc;
    SpawnDesc *pdesc = &desc;
    Task *parent;
    int *oang;
    int *tang;
    Vec3 *tip;

    nj_fill8(pdesc, 0, sizeof desc);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.kind = 0x80000;
    desc.models = stage_models;
    enemy_task_get_pos(obj, &pos);
    oang = ang;
    enemy_task_get_rot(obj, oang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_perfectwall_rotate_yxz(oang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    desc.type = en_perfectwall_break_models[w->kind];
    enemy_task_get_pos(obj, &desc.pos);
    enemy_task_get_rot(obj, desc.ang);
    parent = TASK_OF(obj)->parent;
    enemy_shatter_create(parent ? parent + 1 : 0, pdesc, 0, 0);
    matrix_unit_set(0);
    do {
        Vec3 *p_ = tip = &w->tip;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_perfectwall_rotate_yxz(oang);
    tang = w->ang;
    en_perfectwall_rotate_yxz(tang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    desc.type = EN_PERFECTWALL_MODEL(144);
    v3_set(&desc.pos, tip->x, tip->y, tip->z);
    i3_set(desc.ang, tang[0], tang[1], tang[2]);
    parent = TASK_OF(obj)->parent;
    enemy_shatter_create(parent ? parent + 1 : 0, pdesc, 0, 0);
    ulMatrixPop();
}

/* 0x00b0d710 stg04, 0x00b064e8 stg22 */
void en_perfectwall_hit_check(void *obj, EnPerfectWallWork *w)
{
    LockObj *last = 0;
    LockObj *lock;
    Task *parent;
    int alive, i, score;

    if (w->skip & 0x10000)
        return;
    alive = i = w->n;
    for (i--; i >= 0; i--) {
        LockObj *cur = &w->lock[i];

        if (!cur->owner || (TASK_OF(cur->owner)->flags & 1) || !(cur->flags & LOCK_LOCKABLE))
            alive--;
    }
    for (i = w->n - 1; i >= 0; i--) {
        lock = &w->lock[i];
        if (!lock->owner || (TASK_OF(lock->owner)->flags & TASK_F_END))
            continue;
        if (!(lock->flags & LOCK_LOCKABLE))
            continue;
        score = lock->flags;
        if (!(score & 0x2000))
            continue;
        if (lock->life <= 0) {
            w->skip |= 0x2000 << i;
            lock->flags &= ~LOCK_LOCKABLE;
            last = lock;
            alive--;
        } else
            en_break_spawn(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj), 0);
        if (alive <= 0) {
            parent = TASK_OF(obj)->parent;
            en_perfectwall_manager_se_break((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), &w->pts[i]);
            break;
        }
        parent = TASK_OF(obj)->parent;
        en_perfectwall_manager_se_hit((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), &w->pts[i]);
    }
    if (alive > 0)
        return;
    w->skip |= 0x10000;
    {
        Task *parent = TASK_OF(obj)->parent;
        score = en_perfectwall_manager_side_score((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w->kind);
    }
    if (score)
        enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), score, lockon_hits_max_shot_cnt(last));
    parent = TASK_OF(obj)->parent;
    en_perfectwall_manager_wall_gone((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w->kind);
    do { parent = TASK_OF(obj)->parent; } while (0);
    if (!en_perfectwall_manager_is_united((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0))) {
        en_perfectwall_shatter(obj, w);
        ((int *)obj)[-11] = 0;
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00b0d990 stg04, 0x00b06768 stg22 */
float en_perfectwall_flash_curve(float t)
{
    return util_curve_eval(en_perfectwall_flash_table, MIN(MAX_GE(t, 0.0f), 1.0f));
}

/* 0x00b0d9f0 stg04, 0x00b067c8 stg22 */
void en_perfectwall_reset_locks(void *obj, EnPerfectWallWork *w)
{
    int i;
    LockObj *lock;
    Task *parent;

    i = w->n;
    for (i--; i >= 0; i--) {
        lock = &w->lock[i];
        if (lock->owner && !(*(int *)(lock->owner - 36) & 1) && (lock->flags & LOCK_LOCKABLE))
            continue;
        w->skip = (w->skip & ~(0x2000 << i)) | 0x400 << i;
        parent = TASK_OF(obj)->parent;
        lock->life = EN_PERFECTWALL_REC->hp;
        lock->flags |= LOCK_LOCKABLE;
        ((void (*)(void *))en_perfectwall_manager_se_reset)(parent ? parent + 1 : 0); /* steer(cast) */
    }
}

/* 0x00b0dae8 stg04, 0x00b068c0 stg22 */
void en_perfectwall_follow_manager(void *obj, EnPerfectWallWork *w)
{
    Task *parent = TASK_OF(obj)->parent;

    switch (en_perfectwall_manager_get_state((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0))) {
    case 0:
        break;
    case 1:
        w->next = 1;
        w->next_step = 0;
        break;
    case 2:
        if (enemy_task_get_pstate(obj) != 2)
            enemy_task_set_state(obj, 2);
        break;
    case 3:
        w->skip |= 8;
        break;
    case 4:
        if (w->phase != 2) {
            w->next = 2;
            w->next_step = 0;
        }
        break;
    case 5:
        en_perfectwall_reset_locks(obj, w);
        break;
    case 6:
        if (enemy_task_get_pstate(obj) != 3) {
            en_perfectwall_shatter(obj, w);
            ((int *)obj)[-11] = 0;
            enemy_task_set_state(obj, 3);
        }
        break;
    }
}

/* 0x00b0dc10 stg04, 0x00b069e8 stg22 */
void en_perfectwall_state_break(void *obj, void *w)
{
    en_perfectwall_shatter(obj, w);
    enemy_task_set_end_event(obj, 4);
    FINISH_IF(obj);
}

/* 0x00b0dc58 stg04, 0x00b06a30 stg22 */
void en_perfectwall_state_float_away(void *obj, EnPerfectWallWork *w)
{
#ifdef VERSION_US
    Vec3 pl_vel;
    Vec3 *opos;
#endif
    Vec3 pos;
    int ang[3];

    switch ((unsigned int)w->step) {
    case 2:
        break;
    case 0:
#ifdef VERSION_US
        player_get_frame_vel(&pl_vel);
        opos = enemy_task_pos_ptr(obj);
        opos->x += pl_vel.x;
        opos->y += pl_vel.y;
        opos->z += pl_vel.z;
#endif
        w->timer = 1.0f;
        w->timer_max = 240.0f;
        w->step++;
        w->next_step++;
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        pos.y += (0.5f - w->timer) * 0.011111111f;
        ang[0] += 60;
        ang[1] += 15;
        ang[2] += 15;
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        w->timer += 1.0f;
        if (w->timer_max < w->timer)
            enemy_task_set_state(obj, 3);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    }
}

/* 0x00b0de00 stg04, 0x00b06bd8 stg22 */
void en_perfectwall_exec_normal(void *obj, EnPerfectWallWork *w)
{
    void (*fn)(void *, EnPerfectWallWork *);

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) != 1)
        return;
    if (w->skip & 0x1c00) {
        ModelPart *part = &w->part;
        dc_object_advance_time(part);
        if (part->flags2 & 1)
            w->skip &= ~0x1c00;
    }
    if (w->skip & 8) {
        w->flash = en_perfectwall_flash_curve(w->flash_t / (EN_PERFECTWALL_REC->frm_flash * FPS));
        w->flash_t += 1.0f;
        if (EN_PERFECTWALL_REC->frm_flash * FPS < w->flash_t)
            w->skip &= ~8;
    }
    fn = en_perfectwall_states[w->phase];
    if (fn)
        fn(obj, w);
}

/* 0x00b0df40 stg04, 0x00b06d18 stg22 */
void en_perfectwall_exec_destroyed(void *obj)
{
    if (enemy_task_get_phase(obj) == 1)
        enemy_task_next_phase(obj);
}

/* 0x00b0df80 stg04, 0x00b06d58 stg22 */
void en_perfectwall_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_perfectwall_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b0dfe0 stg04, 0x00b06db8 stg22 */
void en_perfectwall_pre_exec(void *obj, EnPerfectWallWork *w)
{
    Task *parent;
    Vec3 pos;
    int ang[4];
    Vec3 pl_vel;

    w->skip &= ~7;
    if (w->phase != 2) {
        parent = TASK_OF(obj)->parent;
        en_perfectwall_manager_get_point((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w->kind, &pos, ang);
        player_get_frame_vel(&pl_vel);
        v3_sub(&pos, &pos, &pl_vel);
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        parent = TASK_OF(obj)->parent;
        en_perfectwall_manager_get_angles((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w->kind, w->ang);
    }
    en_perfectwall_follow_manager(obj, w);
    en_perfectwall_hit_check(obj, w);
}

/* 0x00b0e0d0 stg04, 0x00b06ea8 stg22 */
void en_perfectwall_post_exec(void *obj, int *w)
{
    Task *parent;

    en_perfectwall_calc_shape(obj, w);
    parent = TASK_OF(obj)->parent;
    en_perfectwall_manager_set_ends((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w[4], (Vec3 *)(w + 143));
}

/* 0x00b0e118 stg04, 0x00b06ef0 stg22 */
unsigned int color_add_saturate(unsigned int col_a, unsigned int col_b)
{
    int al, r, g, bl;
    unsigned int sa = color_get_a(col_a) + color_get_a(col_b);
    unsigned int sr = (col_a & 0xff) + (col_b & 0xff);
    unsigned int sg = (col_a >> 8 & 0xff) + (col_b >> 8 & 0xff);
    unsigned int sb = (col_a >> 16 & 0xff) + (col_b >> 16 & 0xff);

    al = sa < 256 ? sa : 255;
    r = sr < 256 ? sr : 255;
    g = sg < 256 ? sg : 255;
    bl = sb < 256 ? sb : 255;
    return (al + 1) >> 1 << 24 | (bl & 0xff) << 16 | (g & 0xff) << 8 | (r & 0xff);
}

/* 0x00b0e1f0 stg04, 0x00b06fc8 stg22 */
void en_perfectwall_draw_parts(void *obj, EnPerfectWallWork *w)
{
    ModelPart *part = &w->part;
    int dl;
    int *ang;
    float frm, frm_end;
    int i;
    Vec3 *p;

    dl = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(stage_textures);
    ang = (int *)enemy_task_rot_ptr(obj);
    frm = part->frame;
    frm_end = part->frames;
    for (i = w->n; --i >= 0;) {
        p = &w->pts[i];
        if (w->skip & (0x2000 << i))
            continue;
        ulMatrixPush();
        do {
            Vec3 *p_ = p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        en_perfectwall_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (w->skip & (0x400 << i)) {
            dc_object_draw(part);
        } else {
            part->frame = frm_end;
            dc_object_draw(part);
            part->frame = frm;
        }
        ulMatrixPop();
    }
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b0e348 stg04, 0x00b07120 stg22 */
void en_perfectwall_draw_glow(void *obj, int *w)
{
    float ph;

    dc_object_draw((struct MapPart *)(w + 57));
    ph = beat_clock_get_phase();
    draw_set_color(motion_sample_rgba((void *)w[56], (float)(unsigned int)(((int *)w[56])[1] - 1) * ph,
                                       100.0f, 400.0f, 1.0f, 0.2f));
    dc_object_draw((struct MapPart *)(w + 80));
}

/* 0x00b0e3f0 stg04, 0x00b071c8 stg22 */
void en_perfectwall_draw(void *obj, EnPerfectWallWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    int dl, blend;
    int *ang;

    if (!view_sphere_visible(pos, gfx->radius))
        return;
    dl = RenderSetCurrentDisplayList(6);
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    en_perfectwall_fade_color[0] = -w->timer / w->timer_max;
    draw_set_override_color(en_perfectwall_fade_color);
    ang = enemy_task_rot_ptr(obj);
    ulGxSetTextureList(stage_textures);
    en_perfectwall_draw_parts(obj, w);
    ulGxSetTextureList(stage_textures);
    ulMatrixPush();
    do {
        Vec3 *tip = &w->tip;

        do {
            Vec3 *p_ = tip;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
    } while (0);
    en_perfectwall_rotate_yxz(ang);
    en_perfectwall_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_perfectwall_draw_glow(obj, (int *)w);
    ulMatrixPop();
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_perfectwall_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (!EN_PERFECTWALL_REC->test)
        dc_object_draw(gfx);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
    RenderSetCurrentDisplayList(dl);
}

/* 0x00b0e5d8 stg04, 0x00b073b0 stg22 */
/* steer(cast) */
void en_perfectwall_disp_body(void *obj, EnPerfectWallWork *w)
{
    Vec3 *pos;
    ModelPart *gfx;
    int *ang;
    int blend;
    float rgba[4];
    unsigned int col, flash_col;
    float flash, r, grn, b;

    if (w->phase == 2) {
        en_perfectwall_draw(obj, w);
        return;
    }
    pos = enemy_task_pos_ptr(obj);
    gfx = enemy_task_gfx_ptr(obj);
    if (!view_sphere_visible(pos, gfx->radius))
        return;
    ang = enemy_task_rot_ptr(obj);
    ulGxSetTextureList(stage_textures);
    ulMatrixPush();
    en_perfectwall_draw_parts(obj, w);
    ulMatrixPush();
    do {
        Vec3 *tip = &w->tip;

        do {
            Vec3 *p_ = tip;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
    } while (0);
    en_perfectwall_rotate_yxz(ang);
    en_perfectwall_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_perfectwall_draw_glow(obj, (int *)w);
    ulMatrixPop();
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_perfectwall_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (!EN_PERFECTWALL_REC->test)
        dc_object_draw(gfx);
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 3072);
    motion_sample_color((void *)w->src, (float)(unsigned int)(((int *)w->src)[1] - 1) * beat_clock_get_phase(), 0.0f,
                         0.0f, 0.0f, 0.0f, rgba);
    col = (((int)(rgba[0] * 255.0f) + 1) >> 1 << 24) | (((int)(rgba[3] * 255.0f) & 0xff) << 16) |
          (((int)(rgba[2] * 255.0f) & 0xff) << 8) | ((int)(rgba[1] * 255.0f) & 0xff);
    if (w->skip & 8) {
        flash_col = EN_PERFECTWALL_REC->col_flash;
        flash = w->flash;
        r = (float)(flash_col & 0xff) * flash;
        grn = (float)(flash_col >> 8 & 0xff) * flash;
        b = (float)(flash_col >> 16 & 0xff) * flash;
        rgba[1] = MIN(rgba[1] + r / 255.0f, 1.0f);
        rgba[2] = MIN(rgba[2] + grn / 255.0f, 1.0f);
        rgba[3] = MIN(rgba[3] + b / 255.0f, 1.0f);
        col = color_add_saturate(col, (((int)(flash * 255.0f) + 1) >> 1 << 24) | (((int)b & 0xff) << 16) |
                                            (((int)grn & 0xff) << 8) | ((int)r & 0xff));
    }
    draw_set_override_color(rgba);
    dc_object_draw(&w->kind_part);
    draw_set_color(col);
    dc_object_draw(&w->wire_part);
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
    ulMatrixPop();
}

/* 0x00b0ea00 stg04, 0x00b077d8 stg22 */
void en_perfectwall_disp(void *obj, void *w)
{
    ObjStateFn *tab = en_perfectwall_disp_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tab[state])
        tab[state](obj, w);
}

/* 0x00b0ea60 stg04, 0x00b07838 stg22 */
/* steer(cast) */
void en_perfectwall_init(void *obj, EnPerfectWallWork *w)
{
    int kind;
    int i;

    ((Vec3 **)w)[0] = enemy_task_pos_ptr(obj);
    ((Vec3 **)w)[1] = enemy_task_param_vec_ptr(obj);
    ((int **)w)[2] = enemy_task_rot_ptr(obj);
    ((Vec3 **)w)[3] = enemy_task_scl_ptr(obj);
    kind = fabsf(enemy_task_get_param(obj, 0)) < FLT_EPSILON ? 0 : 1;
    w->kind = kind;
    w->n = en_perfectwall_lock_counts[kind];
    enemy_task_clear_flags(obj, 64);
    dc_object_init(&w->part, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_PERFECTWALL_MODEL(139)), *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_PERFECTWALL_MODEL(140)), 0);
    dc_object_init(&w->kind_part, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, en_perfectwall_kind_models[kind]), 0, 0);
    dc_object_init(&w->wire_part, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, en_perfectwall_part_models[kind]), 0, 0);
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_PERFECTWALL_MODEL(142)), 0, 0);
    dc_object_init(&w->glow, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_PERFECTWALL_MODEL(143)), 0, 0);
    dc_object_rebuild(&w->part);
    dc_object_rebuild(&w->kind_part);
    dc_object_rebuild(&w->body);
    dc_object_set_wire(&w->wire_part, model_pnb_get_wire(stage_models, en_perfectwall_part_models[kind]));
    dc_object_set_wire(&w->glow, model_pnb_get_wire(stage_models, EN_PERFECTWALL_MODEL(143)));
    w->src = *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, EN_PERFECTWALL_MODEL(141));
    for (i = w->n; --i >= 0;) {
        lockon_obj_init(&w->lock[i], obj, &w->pts[i], 0, en_perfectwall_lock_name, enemy_task_get_status_slot(obj), EN_PERFECTWALL_REC->hp,
                      EN_PERFECTWALL_REC->hp, 128);
        lockon_entry(&w->lock[i]);
    }
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b0ed18 stg04, 0x00b07af0 stg22 */
void en_perfectwall_release(void *obj, EnPerfectWallWork *w)
{
    Task *parent = TASK_OF(obj)->parent;
    int i;

    en_perfectwall_manager_wall_gone((EnPerfectWallManagerWork *)(parent ? parent + 1 : 0), w->kind);
    for (i = w->n; --i >= 0;)
        lockon_remove(&w->lock[i]);
    dc_object_release(&w->part);
    dc_object_release(&w->kind_part);
    dc_object_release(&w->wire_part);
    dc_object_release(&w->body);
    dc_object_release(&w->glow);
}

/* 0x00b0edf8 stg04, 0x00b07bd0 stg22 */
int en_perfectwall_create(void *parent, EnPerfectWallPlace *place)
{
    ObjDesc desc;
    ObjFuncs funcs;
    int kind = (int)place->param0;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_perfectwall_init;
    funcs.fn[1] = en_perfectwall_release;
    funcs.fn[2] = en_perfectwall_exec;
    funcs.fn[3] = en_perfectwall_disp;
    funcs.fn[4] = en_perfectwall_pre_exec;
    funcs.fn[5] = en_perfectwall_post_exec;
    desc.name = en_perfectwall_name;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.model_no = en_perfectwall_motions[kind];
    desc.hp = en_perfectwall_params.v[0];
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.score = 0;
    desc.model_no2 = -1;
    desc.time = 0;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    desc.size = 800;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b0ef40 stg04, 0x00b07d18 stg22 */
void en_perfectwall_models_init(void)
{
    int i;
    for (i = 2; --i >= 0;)
        model_pnb_get_wire(stage_models, en_perfectwall_part_models[i]);
    model_pnb_get_wire(stage_models, EN_PERFECTWALL_MODEL(143));
}

/* 0x00b0efa8 stg04, 0x00b07d80 stg22 */
void en_perfectwall_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
