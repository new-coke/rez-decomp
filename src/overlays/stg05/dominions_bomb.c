/*
 * Dominions bomb, Area 5: dropped by a shooter head; falls with a smoke tail, hangs and shakes,
 * fires a shot from each of its eight bomblets, then drops away.
 */

#include "stg05.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "dominions_bomb.h"
#include "enb_sidewinder.h"
#include "../common/air_mng.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int dominions_bomb_init_arg;
extern void (*dominions_bomb_exec_table[])(void *o, struct DominionsBombWork *w);
extern DominionsBombSet dominions_bomb_types[1];
extern VarDef dominions_bomb_vardef;
extern VarDef dominions_bomb_vardefs[18];
extern void (*dominions_bomb_states[])(void *o, DominionsBombWork *w);
extern char dominions_bomb_lock_name[];
extern char dominions_bomb_name[];
extern struct TaskDesc dominions_bomb_tail_desc;
extern ObjFuncs dominions_bomb_funcs;
extern Vec3 dominions_bomb_offsets[8];
extern void (*dominions_bomb_disp_table[])(void *o, DominionsBombWork *w);

/* 0x00b22018 */
void dominions_bomb_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&dominions_bomb_vardef);
    for (i = 0; i < 1; i++) {
        def = dominions_bomb_vardefs;
        val = dominions_bomb_types[i].v;
        dominions_bomb_vardef.name = dominions_bomb_types[i].name;
        dominions_bomb_vardef.fmt = dominions_bomb_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&dominions_bomb_vardef);
        for (j = 0; j < 18; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)dominions_bomb_vardefs);
    }
}

/* 0x00b22130 */
void dominions_bomb_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b22138 */
int dominions_bomb_get_init_arg(void)
{
    return dominions_bomb_init_arg;
}

/* 0x00b22148 */
int dominions_bomb_vars_init(int *prm)
{
    dominions_bomb_init_arg = *prm;
    dominions_bomb_vars_register();
    return 0;
}

/* 0x00b22170 */
int dominions_bomb_vars_release(void)
{
    dominions_bomb_vars_unregister();
    return 0;
}

/* steer(cast) */
/* 0x00b22190 */
void dominions_bomb_models_load(void)
{
    ModelPart part;

    dc_object_init(&part, *((struct DcNode **)pnb_get_entry(stage_models, 18)), 0, 0);
    ulMatrixPush();
    matrix_unit_set(0);
    yuki_model_positions(dominions_bomb_offsets, &part, 0x1000000, 8);
    ulMatrixPop();
    dc_object_release(&part);
}

/* 0x00b22200 */
void dominions_bomb_models_free(void)
{
}

/* 0x00b22208 */
void dominions_bomb_place_bomblets(void *obj, DominionsBombWork *w)
{
    Vec3 p, ofs;
    int ang[3];
    float sc[4][2];
    float x, y, z;
    Vec3 *src, *pt;
    int *bang;
    int i;

    pt = w->pts;
    bang = w->angs[0];
    src = dominions_bomb_offsets;
    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    for (i = 0; i < 8; i++) {
        x = src->x;
        y = src->y;
        z = src->z;
        ROT_Y(ANG_180, sc[0], x, z);
        ROT_Z(ang[2], sc[1], x, y);
        ROT_X(ang[0], sc[2], y, z);
        ROT_Y(ang[1], sc[3], x, z);
        ofs.x = x;
        ofs.y = y;
        ofs.z = z;
        do {
            bang[1] = RAD_TO_ANG(atan2f(-ofs.x, -ofs.z));
            bang[0] = RAD_TO_ANG(atan2f(ofs.y, sqrtf(ofs.x * ofs.x + ofs.z * ofs.z)));
        } while (0);
        bang[2] = 0;
        src++;
        bang += 3;
        pt->x = p.x + ofs.x;
        pt->y = p.y + ofs.y;
        pt->z = p.z + ofs.z;
        pt++;
    }
}

/* steer(cast) */
/* 0x00b224b8 */
void dominions_bomb_init(void *obj, DominionsBombWork *w, int place, int *rec)
{
    DominionsBombRec *typ;
    TrailArg prm;

    typ = (DominionsBombRec *)&dominions_bomb_types[*rec];
    w->type = typ;
    enemy_task_clear_flags(obj, 64);
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((void **)obj)[0] = m_;
        ((void **)obj)[1] = t_;
    } while (0);
    lockon_obj_set_name(enemy_task_lock_ptr(obj), dominions_bomb_lock_name);
    enemy_task_get_rot(obj, w->ang);
    w->tail = task_create_inherit(obj, &dominions_bomb_tail_desc, 324);
    heap_bzero(&prm, 40);
    prm.body = 0;
    prm.pos = enemy_task_pos_ptr(obj);
    prm.ang = w->ang;
    prm.tail_seg = 1;
    prm.frames = typ->trail_sec * FPS;
    prm.tail_tex = 1;
    prm.tail_width = typ->trail_f;
    prm.one = 0;
    prm.tail_skip = typ->trail_n;
    air_mng_create(w->tail, &prm);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 19)), *((struct DcClip **)pnb_get_entry(stage_models, 20)), 0);
    dc_object_rebuild(&w->body);
    w->body.radius = 10.0f;
    dominions_bomb_place_bomblets(obj, w);
    w->state = 0;
    w->count = 0;
}

/* 0x00b22640 */
void dominions_bomb_release(void *obj, DominionsBombWork *w)
{
    dc_object_release(&w->body);
}

/* 0x00b22648 */
void dominions_bomb_state_fall(void *obj, DominionsBombWork *w)
{
    DominionsBombRec *typ = w->type;
    int ang[3];
    Vec3 p, v;
    float step;
    int cnt, n;

    cnt = w->count;
    n = typ->dur * FPS;
    w->count = cnt + 1;
    enemy_task_get_param_vec(obj, &v);
    enemy_task_get_pos(obj, &p);
    step = safe_powf((cnt + 1) / (float)n, typ->ease);
    step -= safe_powf(cnt / (float)n, typ->ease);
    v.y -= step * typ->drop;
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    ang[2] = ang[2] + typ->roll / FPS;
    enemy_task_set_rot(obj, ang);
    i3_copy(w->ang, ang);
    w->ang[2] = 0;
    dominions_bomb_place_bomblets(obj, w);
    step = cnt / (float)n;
    if (typ->anim < step)
        w->body.frame = w->body.frames * (step - typ->anim) / (1.0f - typ->anim);
    if (cnt >= n)
        w->state = 1;
}

/* 0x00b22848 */
void dominions_bomb_state_fire(void *obj, DominionsBombWork *w)
{
    DominionsBombRec *typ = w->type;
    Vec3 p;
    int ang[3];
    Vec3 dir;
    float sc[2][2];
    float x, y, z;
    int *bang;
    Vec3 *pt;
    int i, n, k;

    n = typ->wait * FPS;
    k = enemy_task_get_time(obj) - w->count;
    enemy_task_get_pos(obj, &p);
    p.x += RAND_FLOAT_A() * (typ->jitter + typ->jitter) - typ->jitter;
    p.y += RAND_FLOAT_A() * (typ->jitter + typ->jitter) - typ->jitter;
    p.z += RAND_FLOAT_A() * (typ->jitter + typ->jitter) - typ->jitter;
    enemy_task_set_pos(obj, &p);
    if (n < k) {
        enemy_task_get_rot(obj, ang);
        bang = w->angs[0];
        pt = w->pts;
        for (i = 0; i < 8; i++) {
            x = 0.0f;
            y = 0.0f;
            z = -1.0f;
            ROT_X(bang[0], sc[0], y, z);
            ROT_Y(bang[1], sc[1], x, z);
            v3_set(&dir, x, y, z);
            e_shot_fire(obj, pt, &dir, 6);
            pt++;
            bang += 3;
        }
        w->state = 2;
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        FINISH_IF(w->tail);
    }
}

/* 0x00b22ad0 */
void dominions_bomb_state_drop(void *obj, DominionsBombWork *w)
{
    DominionsBombRec *typ = w->type;
    Vec3 p, v;
    int ang[3];
    float step;
    int cnt, n;

    n = typ->dur * FPS;
    cnt = w->count++;
    enemy_task_get_param_vec(obj, &v);
    enemy_task_get_pos(obj, &p);
    step = safe_powf((cnt + 1) / (float)n, typ->ease);
    step -= safe_powf(cnt / (float)n, typ->ease);
    v.y -= step * typ->drop;
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    ang[2] = ang[2] + typ->roll / FPS;
    enemy_task_set_rot(obj, ang);
    i3_copy(w->ang, ang);
    w->ang[2] = 0;
    cnt -= n;
    n = typ->life * FPS;
    if (cnt >= n)
        FINISH_IF(obj);
}

/* 0x00b22ca0 */
void dominions_bomb_update(void *obj, DominionsBombWork *w)
{
    void (*fn)(void *obj, DominionsBombWork *w) = dominions_bomb_states[w->state];

    if (fn)
        fn(obj, w);
}

/* 0x00b22ce0 */
void dominions_bomb_exec_appear(void *obj, DominionsBombWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 0);
        break;
    case 2:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags |= LOCK_LOCKABLE;
        break;
    default:
        dominions_bomb_update(obj, w);
    }
}

/* 0x00b22d88 */
void dominions_bomb_exec_normal(void *obj, DominionsBombWork *w)
{
    if (enemy_task_get_phase(obj) == 1)
        dominions_bomb_update(obj, w);
}

/* 0x00b22dd8 */
void dominions_bomb_exec_leave(void *obj, DominionsBombWork *w)
{
    switch (enemy_task_get_phase(obj)) {
    case 0:
        ((LockObj *)enemy_task_lock_ptr(obj))->flags &= ~LOCK_LOCKABLE;
        break;
    case 2:
        break;
    default:
        dominions_bomb_update(obj, w);
    }
}

/* 0x00b22e50 */
void dominions_bomb_exec_destroyed(void *obj, DominionsBombWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 0) {
        heap_bzero(&desc, 84);
        desc.type = 211;
        desc.models = stage_models;
        desc.kind = 1;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        en_shatter_spawn(obj, &desc, 6);
        FINISH_IF(obj);
    }
}

/* 0x00b22ef0 */
void dominions_bomb_pre_exec(void *obj)
{
    Vec3 p, org;

    player_get_frame_vel(&org);
    enemy_task_get_pos(obj, &p);
    p.x -= org.x;
    p.y -= org.y;
    p.z -= org.z;
    enemy_task_set_pos(obj, &p);
}

/* 0x00b22f60 */
void dominions_bomb_exec(void *obj, DominionsBombWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (dominions_bomb_exec_table[i])
        dominions_bomb_exec_table[i](obj, w);
}

/* steer(cast) */
/* 0x00b22fb8 */
void dominions_bomb_post_exec(void *obj, DominionsBombWork *w)
{
    LockObj *lock = enemy_task_lock_ptr(obj);

    if (lock->flags & LOCK_HIT)
        v3_copy(&w->hitv, &lock->imp.v);
    if (lock->life <= 0 && ((unsigned int)enemy_task_get_pstate(obj)) < 2)
        enemy_task_set_state(obj, 3);
}

/* steer(cast) */
/* 0x00b23050 */
void dominions_bomb_disp(void *obj, DominionsBombWork *w)
{
    void (*fn)(void *obj, DominionsBombWork *w);
    float col_add[4];
    DominionsBombRec *typ;
    Vec3 *p;
    int *bang;
    int i, prev;

    i = enemy_task_get_pstate(obj);
    fn = dominions_bomb_disp_table[i];
    if (fn) {
        switch (w->state) {
        case 0:
        case 1:
            fn(obj, w);
            p = w->pts;
            bang = w->angs[0];
            for (i = 0; i < 8; i++, p++, bang += 3) {
                if (view_sphere_visible(p, w->body.radius)) {
                    do {
                        ulMatrixPush();
                    } while (0);
                    vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
                    dominions_bomb_rotate_yxz(bang);
                    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
                    dc_object_draw(&w->body);
                    ulMatrixPop();
                }
            }
            break;
        case 2:
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            typ = w->type;
            heap_bzero(col_add, 16);
            col_add[0] = -(enemy_task_get_time(obj) - typ->dur * FPS) / (typ->life * FPS);
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_set_override_color(col_add);
            fn(obj, w);
            draw_pop_strip_flags();
            draw_pop_override_color();
            draw_set_override_flags(prev);
            break;
        }
    }
}

/* 0x00b23200 */
int dominions_bomb_create(void *obj, Vec3 *pos, Vec3 *dir)
{
    int kind[2];
    ObjDesc desc;
    ObjFuncs funcs = dominions_bomb_funcs;
    ShotArg prm;
    DominionsBombRec *typ = (DominionsBombRec *)dominions_bomb_types;

    kind[0] = 0;
    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    prm.kind = dominions_bomb_init_arg;
    prm.fx_arg = -1;
    prm.pos = *pos;
    do {
        prm.ang[1] = RAD_TO_ANG(atan2f(-dir->x, -dir->z));
        prm.ang[0] = RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    } while (0);
    prm.ang[2] = 0;
    vec3_normalize(&prm.vel, dir);
    v3_scale(&prm.vel, typ->speed * FRAME_SEC);
    desc.name = dominions_bomb_name;
    desc.funcs = &funcs;
    desc.arg = &prm;
    desc.kind = kind;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->he;
    desc.same_lock = typ->hp;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 18;
    desc.model_no2 = -1;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = (typ->dur + typ->wait) * FPS;
    desc.size = 324;
    return enemy_task_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &desc);
}

/* 0x00b23498 */
void dominions_bomb_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
