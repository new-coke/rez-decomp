/*
 * Tiamat's head (stg03): lockable lead of the serpent; creates its body and tail. Flies its path,
 * brakes, poses, fires a laser after the charge flash, returns from the pose and speeds away.
 */

#include "types_common.h"
#include "task.h"

#include "types_overlays.h"

#include "mathdef.h"
#include "debug.h"
#include "anim.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_matrix.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "pass.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "tiamat_body_and_tail.h"
#include "tiamat_laser.h"
#include "system.h"

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

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        int a_ = ang; \
        float rot_u = x, rot_v = y; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_sum(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x + rhs->x;
    dst->y = lhs->y + rhs->y;
    dst->z = lhs->z + rhs->z;
}

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
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
extern void *stage_sequence;

extern int tiamat_head_init_arg;
extern void (*tiamat_head_exec_table[])(void *o, TiamatHeadWork *w);
extern void (*tiamat_head_disp_table[])(void *o, TiamatHeadWork *w);
void tiamat_head_vars_register(void);
void tiamat_head_vars_unregister(void);
int tiamat_head_vars_init(int *fx_id);
int tiamat_head_vars_release(void);
void tiamat_head_exec(void *obj, TiamatHeadWork *w);
void tiamat_head_disp(void *obj, TiamatHeadWork *w);
void tiamat_head_rotate_yxz(int *ang);
extern TiamatHeadVars tiamat_head_types[1];
extern VarDef tiamat_head_vardef;
extern VarDef tiamat_head_vardefs[24];
extern void tiamat_head_walk_nodes(int *out_a, int *out_b, int naddr);
void tiamat_head_collect_node_positions(float **out, unsigned int *n);

void tiamat_head_update(void *obj, TiamatHeadWork *w);
void tiamat_head_place(void *obj, TiamatHeadWork *w);
extern void (*tiamat_head_states[])(void *o, TiamatHeadWork *w);
void tiamat_head_update_chain(void *obj, TiamatHeadWork *w);
int tiamat_head_next_state(void *obj, TiamatHeadWork *w, int n, int state);
extern char tiamat_head_score_name[];
extern ObjFuncs tiamat_head_funcs;
extern char tiamat_head_name[];
extern int tiamat_head_path_sym;
int tiamat_head_path_frames(TiamatHeadVars *types, int frm, int path, int *out, float spd);
void tiamat_head_swing_angles(void *obj, TiamatHeadWork *w, int *ang);
extern char tiamat_head_lock_name[];
extern Vec3 tiamat_head_start_scl;

static inline void i3_mulf(int *dst, int *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void i3_sum(int *dst, int *lhs, int *rhs)
{
    dst[0] = lhs[0] + rhs[0];
    dst[1] = lhs[1] + rhs[1];
    dst[2] = lhs[2] + rhs[2];
}

static inline void v3_diff_to(Vec3 *lhs, Vec3 *rhs, Vec3 *dst)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

/* model list entry n as stg22 numbers it */
#if defined(OVERLAY_STG22)
#define TIAMAT_HEAD_MODEL(n) ((n) + 10)
#else
#define TIAMAT_HEAD_MODEL(n) (n)
#endif

/* 0x00b08148 stg03, 0x00b15b10 stg22 */
void tiamat_head_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *prm;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&tiamat_head_vardef);
    for (i = 0; i < 1; i++) {
        def = tiamat_head_vardefs;
        prm = (int *)&tiamat_head_types[i].p;
        tiamat_head_vardef.name = tiamat_head_types[i].name;
        tiamat_head_vardef.fmt = tiamat_head_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&tiamat_head_vardef);
        for (j = 0; j < 24; j++) {
            def->ptr = prm;
            prm++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)tiamat_head_vardefs);
    }
}

/* 0x00b08260 stg03, 0x00b15c28 stg22 */
void tiamat_head_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b08268 stg03, 0x00b15c30 stg22 */
int tiamat_head_vars_init(int *fx_id)
{
    tiamat_head_init_arg = *fx_id;
    tiamat_head_vars_register();
    return 0;
}

/* 0x00b08290 stg03, 0x00b15c58 stg22 */
int tiamat_head_vars_release(void)
{
    tiamat_head_vars_unregister();
    return 0;
}

/* 0x00b082b0 stg03, 0x00b15c78 stg22 */
int tiamat_head_marker_draw(int obj, void *place)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, TIAMAT_HEAD_MODEL(25));
    return obj_draw_at(obj, place, (void *)*ent);
}

/* 0x00b08308 stg03, 0x00b15cd0 stg22 */
int tiamat_head_get_init_arg(void)
{
    return tiamat_head_init_arg;
}

/* 0x00b08318 stg03, 0x00b15ce0 stg22 */
void tiamat_head_path_eval(void *out, int idx, float t)
{
    pass_table_push(stage_sequence);
    sequence_eval(idx, (pass_table_end(idx) - 1.0f) * t, 0, out, 0);
    pass_table_pop();
}

/* 0x00b08388 stg03, 0x00b15d50 stg22 */
void tiamat_head_walk_nodes(int *out_a, int *out_b, int naddr)
{
    float **pa = (float **)out_a;
    float **pb = (float **)out_b;
    unsigned int *node = (unsigned int *)naddr;

    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, *pa);
            *pa += 3;
        }
        if (node[0] & 0x10000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, *pb);
            *pb += 3;
        }
        if (!(node[0] & 0x10))
            tiamat_head_walk_nodes(out_a, out_b, node[11]);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b08468 stg03, 0x00b15e30 stg22 */
void tiamat_head_pose_nodes(int out_a, int out_b, ModelPart *gfx)
{
    int cur[2];

    cur[0] = out_a;
    cur[1] = out_b;
    anim_start((void *)gfx->clip, gfx->frame);
    tiamat_head_walk_nodes(&cur[0], &cur[1], gfx->root);
}

/* 0x00b084b0 stg03, 0x00b15e78 stg22 */
void tiamat_head_collect_node_positions(float **out, unsigned int *node)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        ulMatrixGetTranslation((float *)ulMatrixCur, *out);
        *out += 3;
        if (!(node[0] & 0x10))
            tiamat_head_collect_node_positions(out, (unsigned int *)node[11]);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b08538 stg03, 0x00b15f00 stg22 */
void tiamat_head_node_positions(float *out, ModelPart *gfx)
{
    float *dst = out;

    anim_start((void *)gfx->clip, gfx->frame);
    tiamat_head_collect_node_positions(&dst, (unsigned int *)gfx->root);
}

/* 0x00b08578 stg03, 0x00b15f40 stg22 */
/* steer(cast) */
void tiamat_head_init(void *obj, TiamatHeadWork *w, TiamatHeadArg *blk, int *kind)
{
    TiamatHeadVars *typ = &tiamat_head_types[kind[0]];
    Vec3 org;
    Vec3 pl_vel;
    Vec3 p;
    Vec3 v;
    int ang[3];
    float sc[3][2];
    float x, y, z;
    float dis;
    int n;

    w->type = typ;
    lockon_obj_set_name(enemy_task_lock_ptr(obj), tiamat_head_lock_name);
    ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
    ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_SILENT;
    enemy_task_clear_flags(obj, 64);
    w->life = kind[2];
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(*(void **)obj, TIAMAT_HEAD_MODEL(36))), 0, 0);
    dc_object_rebuild(&w->body);
    dc_object_init(&w->ring, *((struct DcNode **)pnb_get_entry(*(void **)obj, TIAMAT_HEAD_MODEL(37))), 0, 0);
    ((ModelPart *)enemy_task_gfx_ptr(obj))->radius = 21.0f;
    enemy_task_set_scl(obj, &tiamat_head_start_scl);
    w->path = kind[1];
    w->prog = 0;
    enemy_task_get_rot(obj, ang);
    vec3_normalize(&v, (Vec3 *)&kind[3]);
    v3_copy(&w->dir, &v);
    do { Vec3 *p_ = &v; x = p_->x; y = p_->y; z = p_->z; } while (0);
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    dis = typ->p.appear_dis;
    v3_set(&v, x * dis, y * dis, z * dis);
    enemy_task_get_pos(obj, &p);
    v3_sum(&w->base, &p, &v);
    player_get_frame_pos(&org);
    player_get_frame_vel(&pl_vel);
    v3_add(&w->base, &pl_vel);
    v3_sub(&w->base, &w->base, &org);
    enemy_task_get_rot(obj, w->base_ang);
    w->speed = vec3_length((Vec3 *)&kind[3]);
    v3_mul(&w->cur, &w->dir, -typ->p.appear_dis);
    w->state = 0;
    w->frames = 0;
    w->fired = 0;
    w->swing_count = 0;
    w->swing_phase = 0;
    i3_set(w->swing_base, 0, 0, 0);
    i3_set(w->swing_ang, 0, 0, 0);
    w->chain = 0;
    v3_copy(&w->prev, &w->cur);
    v3_copy(&w->from, &w->cur);
    w->look = 0.0f;
    w->chain_wait = 0;
    w->arg = *blk;
    n = 13.0f - enemy_task_get_param(obj, 3);
    w->hits = 0;
    {
        int m_ = n > 1 ? n : 2;
        w->segments = m_ < 14 ? m_ : 13;
    }
}

/* 0x00b08a18 stg03, 0x00b163e0 stg22 */
void tiamat_head_release(void *obj, TiamatHeadWork *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->ring);
}

/* 0x00b08a48 stg03, 0x00b16410 stg22 */
void tiamat_head_swing_angles(void *obj, TiamatHeadWork *w, int *out)
{
    TiamatHeadVars *typ = w->type;
    int base[3];
    int swing[3];
    float sc[2];

    i3_mulf(base, w->swing_base, w->swing_count / (typ->p.damage_swing_sec * FPS + 1.0f));
    ulSinCos(w->swing_phase, &sc[0], &sc[1]);
    i3_mulf(swing, w->swing_ang, sc[0] * w->swing_count / (typ->p.damage_swing_sec * FPS + 1.0f));
    i3_sum(out, swing, base);
}

/* 0x00b08ba8 stg03, 0x00b16570 stg22 */
void tiamat_head_swing_start(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    Vec3 v;
    float sc[2][2];
    float x, z;

    tiamat_head_swing_angles(obj, w, w->swing_base);
    z = -1.0f;
    x = 0.0f;
    ulSinCos(typ->p.damage_swing_size, &sc[0][0], &sc[0][1]);
    do {
        float old_x = x, old_y = sc[0][0];

        ulSinCos(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), &sc[1][0], &sc[1][1]);
        x = old_x * sc[1][1] - old_y * sc[1][0];
        sc[0][0] = old_x * sc[1][0] + old_y * sc[1][1];
    } while (0);
    v3_set(&v, x, sc[0][0], z);
    do {
        w->swing_ang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        w->swing_ang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    w->swing_ang[2] = 0;
    w->swing_phase = 0;
    w->swing_count = typ->p.damage_swing_sec * FPS + 1.0f;
}

/* 0x00b08d58 stg03, 0x00b16720 stg22 */
void tiamat_head_damage_swing(void *obj, TiamatHeadWork *w)
{
    int v = w->swing_count - 1;

    w->swing_count = v < 0 ? 0 : v;
    w->swing_phase = w->swing_phase + w->type->p.damage_swing_ang / FPS;
}

/* 0x00b08da8 stg03, 0x00b16770 stg22 */
void tiamat_head_pose_model(void *obj, TiamatHeadWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    tiamat_head_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl.x, scl.y, scl.z);
    tiamat_head_pose_nodes((int)&w->mouth, (int)&w->neck, enemy_task_gfx_ptr(obj));
    ulMatrixPop();
}

/* 0x00b08e80 stg03, 0x00b16848 stg22 */
void tiamat_head_place(void *obj, TiamatHeadWork *w)
{
    Vec3 p;
    Vec3 prev;
    Vec3 org;
    Vec3 dorg;
    int mang[3];
    int ang[3];
    int oang[3];
    int prm[4];

    enemy_task_get_pos(obj, &prev);
    enemy_task_get_rot(obj, ang);
    ulMatrixPush();
    matrix_unit_set(0);
    player_get_frame_pos(&org);
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do {
        Vec3 *p_ = &w->base;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    tiamat_head_rotate_yxz(w->base_ang);
    do {
        Vec3 *p_ = &w->cur;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&p);
    ulMatrixPop();
    enemy_task_set_pos(obj, &p);
    if (!(fabsf(vec3_distance_squared(&p, &prev)) < FLT_EPSILON)) {
        i3_copy(mang, ang);
        yuki_draw_aimed(&p, &prev, mang);
    } else {
        i3_copy(mang, ang);
    }
    if (!(fabsf(w->look) < FLT_EPSILON)) {
        player_get_pos(&org);
        dorg.x = org.x - p.x;
        dorg.y = org.y - p.y;
        dorg.z = org.z - p.z;
        do {
            oang[1] = (int)RAD_TO_ANG(atan2f(-dorg.x, -dorg.z));
            oang[0] = (int)RAD_TO_ANG(atan2f(dorg.y, sqrtf(dorg.x * dorg.x + dorg.z * dorg.z)));
        } while (0);
        oang[2] = 0;
        yuki_lerp3(mang, w->look_from, oang, w->look);
    }
    enemy_task_set_rot(obj, mang);
    tiamat_head_pose_model(obj, w);
    if (w->chain == 0 && w->chain_wait++ > 0) {
        prm[0] = (int)&w->neck;
        prm[1] = (int)enemy_task_rot_ptr(obj);
        prm[2] = (int)&w->hits;
        prm[3] = w->segments >= 13;
        w->chain = (void *)tiamat_body_and_tail_create(obj, (int)prm, w->segments, w->life, (char *)&w->arg, 0.0f, w->type->p.spike_rate);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
    }
}

/* 0x00b09188 stg03, 0x00b16b50 stg22 */
int tiamat_head_next_state(void *obj, TiamatHeadWork *w, int n, int state)
{
    Vec3 p_end;
    Vec3 p_prev;
    Vec3 dv;
    float unit;
    float end;

    unit = 1.0f;
    if (fabsf(w->prog - unit) < FLT_EPSILON || w->prog > unit) {
        pass_table_push(stage_sequence);
        end = pass_table_end(w->path);
        w->state = 7;
        v3_sub(&dv, &w->cur, &w->prev);
        w->speed = vec3_length(&dv);
        sequence_eval(w->path, end - unit, 0, &p_end, 0);
        sequence_eval(w->path, end - 2.0f, 0, &p_prev, 0);
        v3_sub(&dv, &p_end, &p_prev);
        vec3_normalize(&w->dir, &dv);
        w->prog_from = w->prog;
        v3_copy(&w->from, &w->cur);
        w->frames = 1;
        pass_table_pop();
        return 1;
    }
    v3_copy(&w->prev, &w->cur);
    if (++w->frames > n) {
        w->prog_from = w->prog;
        w->frames = 1;
        v3_copy(&w->from, &w->cur);
        w->state = state;
        return 1;
    }
    return 0;
}

/* 0x00b09380 stg03, 0x00b16d48 stg22 */
int tiamat_head_state_appear(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    float v, sec, start_spd, tm, dis;
    int n;
    Vec3 dv;

    v = w->speed * FPS;
    sec = typ->p.appear_sec;
    n = sec * FPS;
    start_spd = (typ->p.appear_dis + typ->p.appear_dis - v * sec) / sec;
    tm = sec * (w->frames / (float)n);
    dis = start_spd * tm + (v - start_spd) / sec * tm * tm * 0.5f;
    v3_mul(&dv, &w->dir, dis);
    v3_sum(&w->cur, &w->from, &dv);
    tiamat_head_next_state(obj, w, n, 1);
    return 0;
}

/* 0x00b09488 stg03, 0x00b16e50 stg22 */
int tiamat_head_state_move(void *obj, TiamatHeadWork *w)
{
    float prog;
    int n;

    n = enemy_task_get_param(obj, 1) * 60.0f / 60.0f + 1.0f;
    w->prog += enemy_task_get_param(obj, 0) / FPS;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    tiamat_head_path_eval(&w->cur, w->path, prog);
    tiamat_head_next_state(obj, w, n, 2);
    return 0;
}

/* 0x00b09568 stg03, 0x00b16f30 stg22 */
int tiamat_head_state_break(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    float sec, v, prog;
    int n;

    sec = typ->p.break_sec;
    n = sec * FPS + 1.0f;
    sec = sec * (w->frames / (float)n);
    v = enemy_task_get_param(obj, 0);
    w->prog = w->prog_from + v * sec + -v / sec * sec * sec * 0.5f;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    tiamat_head_path_eval(&w->cur, w->path, prog);
    if (tiamat_head_next_state(obj, w, n, 3)) {
        heap_bzero(w->seg, sizeof w->seg);
        tiamat_body_and_tail_chain_set_swing_rate(w->chain, &typ->p.damage_swing_rate);
        enemy_task_get_rot(obj, w->look_from);
    }
    return 0;
}

/* 0x00b096a8 stg03, 0x00b17070 stg22 */
void tiamat_head_update_chain(void *obj, TiamatHeadWork *w)
{
    Vec3 p;
    Vec3 pts[14];
    int ang[3];
    int i;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    tiamat_head_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    tiamat_head_node_positions((float *)pts, &w->ring);
    ulMatrixPop();
    for (i = 1; i < w->segments + 1; i++)
        v3_diff_to(&pts[i], &pts[i - 1], &w->seg[i]);
    tiamat_body_and_tail_chain_set_points(w->chain, &w->seg[1],
                                                 tiamat_body_and_tail_chain_count(w->chain) + 1);
}

/* 0x00b097e0 stg03, 0x00b171a8 stg22 */
int tiamat_head_state_pose(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    ModelPart *gfx;
    float rate, frm;
    int n;

    n = typ->p.pose_sec * FPS + 1.0f;
    rate = w->frames / (float)n;
    tiamat_head_update_chain(obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    frm = gfx->frames - gfx->speed * (n - w->frames);
    gfx->frame = frm <= 0.0f ? 0.0f : frm;
    w->look = rate;
    tiamat_head_next_state(obj, w, n, 4);
    return 0;
}

/* 0x00b098c8 stg03, 0x00b17290 stg22 */
int tiamat_head_state_stop(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    float fps, unit, rate;
    int n;
    TiamatLaserAttackRayArg ray;

    fps = FPS;
    unit = 1.0f;
    n = typ->p.stop_sec * fps + unit;
    rate = w->frames / (float)n;
    tiamat_head_update_chain(obj, w);
    if (w->fired == 0 && typ->p.attack_ray_rate < rate) {
        w->fired = 1;
        heap_bzero(&ray, sizeof ray);
        ray.pos = &w->mouth;
        ray.ang = enemy_task_rot_ptr(obj);
        ray.frames = typ->p.stop_sec * fps * (unit - typ->p.attack_ray_rate);
        ray.owner = obj;
        tiamat_laser_attack_ray_create(obj, &ray);
        SoundRhythmSe_Enemy(tiamat_head_init_arg, 3);
    }
    w->look = 1.0f;
    return tiamat_head_next_state(obj, w, n, 5);
}

/* 0x00b09a08 stg03, 0x00b173d0 stg22 */
int tiamat_head_state_pose_return(void *obj, TiamatHeadWork *w)
{
    ModelPart *gfx;
    float frm;

    tiamat_head_update_chain(obj, w);
    gfx = enemy_task_gfx_ptr(obj);
    frm = gfx->frame - gfx->speed;
    if (frm < 0.0f)
        gfx->frame = 0.0f;
    else {
        gfx->frame = frm;
        w->frames--;
    }
    if (tiamat_head_next_state(obj, w, 1, 6)) {
        tiamat_body_and_tail_chain_set_points(w->chain, 0, 0);
        tiamat_body_and_tail_chain_set_swing_rate(w->chain, 0);
    }
    return 0;
}

/* 0x00b09ab8 stg03, 0x00b17480 stg22 */
int tiamat_head_state_accel(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    float sec, rate, prog;
    int n;

    n = typ->p.accel_sec * FPS + 1.0f;
    rate = w->frames / (float)n;
    sec = typ->p.break_sec * rate;
    w->prog = w->prog_from + enemy_task_get_param(obj, 0) / sec * sec * sec * 0.5f;
    prog = MIN(w->prog, 1.0f);
    w->prog = prog;
    tiamat_head_path_eval(&w->cur, w->path, prog);
    w->look = 1.0f - rate;
    if (tiamat_head_next_state(obj, w, n, 1))
        w->fired = 0;
    return 0;
}

/* 0x00b09bd8 stg03, 0x00b175a0 stg22 */
int tiamat_head_state_disappear(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ = w->type;
    float v, sec, acc, tm, dis;
    int n;
    Vec3 dv;

    v = w->speed * FPS;
    n = typ->p.disappear_sec * FPS;
    tm = (float)w->frames++;
    sec = typ->p.disappear_sec;
    acc = (typ->p.disappear_dis + typ->p.disappear_dis - v * sec) / sec;
    acc = (acc - v) / sec;
    sec *= tm / n;
    dis = v * sec + acc * sec * sec * 0.5f;
    v3_mul(&dv, &w->dir, dis);
    v3_sum(&w->cur, &w->from, &dv);
    return 0;
}

/* 0x00b09ce0 stg03, 0x00b176a8 stg22 */
void tiamat_head_update(void *obj, TiamatHeadWork *w)
{
    void (*fn)(void *obj, TiamatHeadWork *w) = tiamat_head_states[w->state];

    if (fn)
        fn(obj, w);
    tiamat_head_damage_swing(obj, w);
    tiamat_head_place(obj, w);
    tiamat_body_and_tail_chain_update(w->chain);
}

/* 0x00b09d50 stg03, 0x00b17718 stg22 */
void tiamat_head_exec_appear(void *obj, TiamatHeadWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        enemy_task_appear_scale(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        enemy_task_appear_scale(obj, w);
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        tiamat_head_update(obj, w);
        if (w->life > 0 && --w->life <= 0) {
            ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
            enemy_task_set_state(obj, 2);
        }
        enemy_task_appear_scale(obj, w);
        break;
    }
}

/* 0x00b09e48 stg03, 0x00b17810 stg22 */
void tiamat_head_exec_normal(void *obj, TiamatHeadWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (enemy_task_get_phase(obj) != 1)
        return;
    tiamat_head_update(obj, w);
    if (w->life > 0 && --w->life <= 0) {
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 2);
    }
}

/* 0x00b09ed8 stg03, 0x00b178a0 stg22 */
void tiamat_head_exec_leave(void *obj, TiamatHeadWork *w)
{
    if (enemy_task_get_phase(obj) != 1) {
        enemy_task_vanish_scale(obj, w);
        return;
    }
    tiamat_head_update(obj, w);
    enemy_task_vanish_scale(obj, w);
}

/* 0x00b09f40 stg03, 0x00b17908 stg22 */
#if defined(OVERLAY_STG22)
void tiamat_head_exec_destroyed(void *obj, TiamatHeadWork *w)
{
    TiamatHeadVars *typ;
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        typ = w->type;
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 36;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        tiamat_body_and_tail_chain_destroy(w->chain, 3, 3);
        if (w->hits >= 13)
            player_status_add_score_event(tiamat_head_score_name, typ->p.bonus_score);
        FINISH_IF(obj);
    }
}
#else
void tiamat_head_exec_destroyed(void *obj, TiamatHeadWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        TiamatHeadVars *typ = w->type;

        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 26;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        tiamat_body_and_tail_chain_destroy(w->chain, 3, 3);
        if (w->hits >= 13)
            player_status_add_score_event(tiamat_head_score_name, typ->p.bonus_score);
        FINISH_IF(obj);
    }
}
#endif

/* 0x00b0a058 stg03, 0x00b17a20 stg22 */
void tiamat_head_exec(void *obj, TiamatHeadWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (tiamat_head_exec_table[i])
        tiamat_head_exec_table[i](obj, w);
}

/* 0x00b0a0b0 stg03, 0x00b17a78 stg22 */
#if defined(OVERLAY_STG22)
/* steer(cast) */
void tiamat_head_post_exec(void *obj, TiamatHeadWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);
    Vec3 p;
    SpawnDesc desc;
    int hits;

    if (!enemy_status_get_ticks())
        tiamat_head_pose_model(obj, w);
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        tiamat_head_swing_start(obj, w);
        tiamat_body_and_tail_chain_swing_start(w->chain);
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 48;
        desc.models = stage_models;
        desc.kind = 0;
        desc.unk_50 = 0;
        desc.lock = enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_get_pos(obj, &p);
        ulMatrixApplyPoint((float *)ulMatrixCur, &p.x, &p.x);
        project_point(&p, &p);
        hits = lock->life;
        if (!hits)
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 1, &p.x);
        else if (!(hits & 1))
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 2, &p.x);
        else
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 0, &p.x);
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}
#else
/* steer(cast) */
void tiamat_head_post_exec(void *obj, TiamatHeadWork *w)
{
    LockObj *lock = (LockObj *)enemy_task_lock_ptr(obj);
    Vec3 p;
    SpawnDesc desc;
    int hits;

    if (!enemy_status_get_ticks())
        tiamat_head_pose_model(obj, w);
    if (lock->flags & LOCK_HIT) {
        v3_copy(&w->hitv, &lock->imp.v);
        tiamat_head_swing_start(obj, w);
        tiamat_body_and_tail_chain_swing_start(w->chain);
        nj_fill8(&desc, 0, sizeof desc);
        desc.kind = 0;
        desc.models = stage_models;
        desc.type = 38;
        desc.unk_50 = 0;
        desc.lock = (LockObj *)enemy_task_lock_ptr(obj);
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        v3_copy(&desc.v, &w->hitv);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_get_pos(obj, &p);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&p, (float *)&p);
        project_point(&p, &p);
        hits = lock->life;
        if (!hits)
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 1, (float *)&p);
        else if (!(hits & 1))
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 2, (float *)&p);
        else
            SoundRhythmSe_EnemyPos(tiamat_head_init_arg, 0, (float *)&p);
    }
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}
#endif

/* 0x00b0a280 stg03, 0x00b17c48 stg22 */
int tiamat_head_collect_node_matrices(float **out, unsigned int *node, int idx)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node[0] & 0x4000000) {
            njGetMatrix(*out);
            idx++;
            *out += 16;
        }
        if (!(node[0] & 0x10))
            idx = tiamat_head_collect_node_matrices(out, (unsigned int *)node[11], idx);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return idx;
}

/* 0x00b0a330 stg03, 0x00b17cf8 stg22 */
void tiamat_head_draw_at_nodes(ModelPart *mdl, ModelPart *src, float zscl)
{
    float buf[4][16];
    float (*p)[16];
    int n;

    p = buf;
    anim_start((void *)src->clip, src->frame);
    n = tiamat_head_collect_node_matrices((float **)&p, (unsigned int *)src->root, 0);
    p = buf;
    while (n--) {
        ulMatrixPush();
        ulMatrixLoad(*p++);
        vu0_matrix_scale(ulMatrixCur, 1.0f, 1.0f, zscl);
        dc_object_draw(mdl);
        ulMatrixPop();
    }
}

/* 0x00b0a3f8 stg03, 0x00b17dc0 stg22 */
/* steer(cast) */
void tiamat_head_disp_body(void *obj, TiamatHeadWork *w)
{
    Vec3 p;
    int ang[4];
    Vec3 scl;
    float rgba[4];
    ModelPart *gfx;
    float unit, eps, beat;
    int blend;

    gfx = enemy_task_gfx_ptr(obj);
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    enemy_task_get_scl(obj, &scl);
    if (view_sphere_visible(&p, gfx->radius)) {
        unit = 1.0f;
        eps = FLT_EPSILON;
        blend = 0;
        if (!(fabsf(scl.x - unit) < eps)) {
            blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
            heap_bzero(rgba, 16);
            rgba[0] = scl.x - unit;
            draw_set_override_color(rgba);
        }
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        beat = beat_clock_get_phase();
        do {
            Vec3 *p_ = &p;

            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        tiamat_head_rotate_yxz(ang);
        tiamat_head_swing_angles(obj, w, ang);
        tiamat_head_rotate_yxz(ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        tiamat_head_draw_at_nodes(&w->body, gfx, beat);
        ulMatrixPop();
        if (!(fabsf(scl.x - unit) < eps)) {
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(blend);
        }
    }
}

/* 0x00b0a5c8 stg03, 0x00b17f90 stg22 */
void tiamat_head_disp(void *obj, TiamatHeadWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (tiamat_head_disp_table[i])
        tiamat_head_disp_table[i](obj, w);
}

/* 0x00b0a620 stg03, 0x00b17fe8 stg22 */
/* steer(cast) */
int tiamat_head_path_frames(TiamatHeadVars *types, int frm, int path, int *out, float spd)
{
    Vec3 *seg = (Vec3 *)out;
    float sec = frm / FPS;
    float total = 0.0f;
    float pos = 0.0f;
    float dur;
    float next;
    float stop_pos;
    float resume_pos;
    float dis;
    Vec3 p0;
    Vec3 pt;
    ModelPart prt;

    dc_object_init(&prt, *((struct DcNode **)pnb_get_entry(stage_models, TIAMAT_HEAD_MODEL(25))), *((struct DcClip **)pnb_get_entry(stage_models, TIAMAT_HEAD_MODEL(27))), 0);
    dur = prt.frames / prt.speed / FPS;
    dc_object_release(&prt);
    pass_table_push(stage_sequence);
    sequence_eval(path, 0.0f, 0, &p0, 0);
    pass_table_pop();
    tiamat_head_path_eval(&pt, path, spd / FPS);
    v3_sub(seg, &pt, &p0);
    vec3_length(seg);
    pass_table_push(stage_sequence);
    sequence_eval(path, 1.0f, 0, &pt, 0);
    pass_table_pop();
    v3_sub(seg, &pt, &p0);
    total += types->p.appear_sec;
    for (;;) {
        next = spd * sec + pos;
        if (fabsf(next - 1.0f) < FLT_EPSILON || next > 1.0f) {
            total += (1.0f - pos) / spd;
            break;
        }
        {
            float adis = types->p.break_sec;

            adis = spd * adis * 0.5f;
            dis = adis;
            stop_pos = dis + next;
        }
        total += sec;
        if (fabsf(stop_pos - 1.0f) < FLT_EPSILON || stop_pos > 1.0f) {
            total += types->p.break_sec - sqrtf(2.0f * (next - 1.0f) * types->p.break_sec / spd);
            break;
        }
        pos = stop_pos;
        total += types->p.break_sec;
        total += types->p.pose_sec + dur + types->p.stop_sec;
        dis = spd * types->p.accel_sec * 0.5f;
        resume_pos = dis + pos;
        if (fabsf(resume_pos - 1.0f) < FLT_EPSILON || resume_pos > 1.0f) {
            total += sqrtf(2.0f * (1.0f - pos) * types->p.accel_sec / spd);
            break;
        }
        total += types->p.accel_sec;
        pos = resume_pos;
    }
    total += types->p.disappear_sec;
    return total * 60.0f - 60.0f;
}

/* 0x00b0a9a8 stg03, 0x00b18370 stg22 */
int tiamat_head_create(void *parent, Placement *place)
{
    int prm[8];
    ObjDesc desc;
    ObjFuncs funcs = tiamat_head_funcs;
    TiamatHeadVars *typ = tiamat_head_types;

    prm[0] = 0;
    if (fabsf(place->param0) < FLT_EPSILON || place->param0 < 0.0f)
        place->param0 = 0.1f;
    pass_table_push(stage_sequence);
    prm[1] = MIN(place->param2 >= 0.0f ? place->param2 : 0.0f, 2.0f) + pass_find_by_symbol(tiamat_head_path_sym);
    pass_table_pop();
    prm[2] = tiamat_head_path_frames(typ, (int)place->param1, prm[1], &prm[3], place->param0);
    nj_fill8(&desc, 0, sizeof desc);
    desc.name = tiamat_head_name;
    desc.funcs = &funcs;
    desc.kind = prm;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = 0.5f < (int)place->param3 ? typ->p.hp_short : typ->p.hp;
    desc.same_lock = typ->p.same_lock;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = TIAMAT_HEAD_MODEL(25);
    desc.model_no2 = TIAMAT_HEAD_MODEL(27);
    desc.score = 0.5f < (int)place->param3 ? typ->p.score_short : typ->p.score;
    desc.hp_reduce = typ->p.hp_reduce;
    desc.crash_r = typ->p.crash_r;
    desc.crash_damage = typ->p.crash_damage;
    desc.time = prm[2];
    desc.size = 636;
    return enemy_task_task_create(parent, &desc);
}

/* 0x00b0ac10 stg03, 0x00b185d8 stg22 */
void tiamat_head_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
