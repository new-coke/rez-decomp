/*
 * Area 5 stage support: event listeners, screen filter, and the Gate: a path barrier with eight
 * lock-on points; each hit breaks a bar and plays the next change-symbol note, all eight shatter
 * it, and a gate still standing when the player reaches it hurts the player.
 */

#include "types_gw.h"
#include "color.h"
#include "stg05.h"
#include "mathdef.h"
#include "approach.h"
#include "callback.h"
#include "camera.h"
#include "change_symbol.h"
#include "debug.h"
#include "debugdraw.h"
#include "enemy_status.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "line_batch.h"
#include "lockon.h"
#include "map_manager.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "stage_base.h"
#include "stage05.h"
#include "../common/en_shatter.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/scr_filter.h"

extern ListenerDef listener_defs[5];
extern char filter_vardef[];
extern GwVarDef filter_vardefs[];
extern void *listener_handles[5];
extern struct TaskDesc gate_desc;
extern char filter_area_colors[15][20];

/* 48 bytes: node position, open end, drawn end */
typedef struct GateBar {
    Vec3 base;                  /* node position (gate_node_cb) */
    Vec3 open_pos;                     /* 0x0c: the gate position when opened */
    Vec3 end;                     /* 0x18: moves toward open_pos, or back once hit */
    char pad_24[12];
} GateBar;

/* 1492 bytes */
typedef struct GateWork {
    void *models;
    void *tex;                  /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    char pad_20[12];
    Vec3 pos0;                  /* 0x2c: the path position, moved with the scroll */
    Vec3 sp;                    /* 0x38: spring offset */
    Vec3 vel;                   /* 0x44 */
    Vec3 acc;                   /* 0x50: pushed by hits */
    int ang0[3];                /* 0x5c: the placement angles */
    ModelPart part3;               /* 0x68 */
    ModelPart body;                /* 0xc4 */
    ModelPart part2;               /* 0x120 */
    unsigned int state;         /* 0x17c */
    float t;                    /* 0x180: seconds left */
    float t0;                   /* 0x184 */
    float base;                 /* 0x188: game_pass_time_get at creation */
    LockObj lock[8];               /* 0x18c */
    Vec3 pts[8];                /* 0x38c: the lock-on points (node positions) */
    int hit[8];                 /* 0x3ec */
    int node[8];                /* 0x40c: the node index of each point */
    int open;                   /* 0x42c */
    GateBar rec[8];             /* 0x430 */
    char pad_5b0[4];
    int fx;                     /* 0x5b4 */
    int fx_op;                  /* 0x5b8 */
    int counting;               /* 0x5bc */
    int show3;                  /* 0x5c0: part3 drawn */
    int sym;                    /* 0x5c4: change_symbol_model_draw called in the draw */
    int node_i;                 /* 0x5c8: node counter of the node callback */
    int broken;                 /* 0x5cc: points hit */
    int note_done;              /* 0x5d0: 0 while the note of SoundRhythmSe_ChangeSymbol(0) sounds */
} GateWork;

extern void *gate_models;
extern void *gate_textures;
extern char gate_texture_data[];
extern char gate_model_data[];
extern GwVarDef gate_vardef[];
extern GwVarDef gate_vardefs[];
extern unsigned int system_flags;
extern float gate_hit_push;
extern float gate_spring_rate;
extern float gate_spring_damp;
extern float gate_turn_rate;
extern float gate_hit_turn_x;
extern float gate_hit_turn_y;
extern float gate_bar_len;
extern unsigned int gate_bar_col;
extern float gate_bar_width;
extern float gate_bar_close_rate;
extern float gate_bar_return_rate;
extern Vec3 gate_spring_origin;
extern char gate_lock_name[];

/* placement */
typedef struct GateArg {
    char pad_00[16];
    int ang[3];                 /* 0x10 */
    char pad_1c[12];
    float time;                 /* 0x28: path time of the gate */
} GateArg;

/* 0x00af6700 */
void listeners_add(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_handles[i] = listener_add(listener_defs[i].idx, listener_defs[i].fn);
}

/* 0x00af6768 */
void listeners_remove(void)
{
    unsigned int i;

    for (i = 0; i < 5; i++)
        listener_remove(listener_defs[i].idx, listener_handles[i]);
}

/* 0x00af67d0 */
void listener_stars_on(void)
{
}

/* 0x00af67d8 */
void listener_next_map(void)
{
    map_menu_next();
}

/* 0x00af67e0 */
void listener_mute(int *mute)
{
    midiseq_mute_request(*mute);
}

/* 0x00af67e8 */
void listener_lock_hit(void)
{
}

/* 0x00af67f0 */
void listener_area_change(int *ev)
{
    int area;

    stage_get_buffer();
    map_get_current();
    area = map_manager_get_scene_id(ev[0]);
    if (!map_manager_scene_flag(ev[0]))
        player_command_start(0);
    filter_set_area(area);
}

/* 0x00af6850 */
void filter_vars_add(void)
{
    int group = stage_vars_group_get();

    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, filter_vardef),
                         filter_vardefs);
}

/* 0x00af68a0 */
void filter_vars_remove(void)
{
    int group = stage_vars_group_get();

    gw_varedit_remove(&gw_varedit_root, group, (struct GwVarDef *)filter_vardef);
}

/* 0x00af68d0 */
void filter_reset(void)
{
    int *w = (int *)stage_get_buffer();
    void *filt = (void *)w[1];

    if (filt && !(TASK_OF(filt)->flags & TASK_F_END))
        scr_filter_fade(filt, filter_area_colors, 0);
}

/* 0x00af6918 */
void filter_set_area(unsigned int area)
{
    int *w = (int *)stage_get_buffer();

    if (w[1] == 0)
        w[1] = (int)scr_filter_mng_create((void *)w[0]);
    if (area > 14)
        area = 14;
    scr_filter_fade((void *)w[1], filter_area_colors[area], 60);
}

/* 0x00af6988 */
void filter_fade_last(void)
{
    int *buf = (int *)stage_get_buffer();

    if (buf && buf[1]) {
        FINISH(buf[1]);
        buf[1] = 0;
    }
}

/* 0x00af69c8 */
void gate_shatter(GateWork *w)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    desc.time = w->body.frame;
    desc.kind = 0x40000;
    desc.type = 6;
    desc.motion = 7;
    v3_copy(&desc.pos, &w->pos);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.models = w->models;
    i3_copy(desc.ang, w->ang);
    v3_set(&desc.v, 0.0f, 0.0f, 0.0f);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00af6a90 */
void gate_bar_shatter(GateWork *w, int i)
{
    SpawnDesc desc;
    LockObj *lock;

    nj_fill8(&desc, 0, sizeof desc);
    lock = &w->lock[i];
    v3_copy(&desc.pos, &w->pos);
    desc.kind = 0x20000;
    desc.idx = w->node[i];
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.type = 0;
    i3_copy(desc.ang, w->ang);
    desc.models = w->models;
    desc.lock = lock;
    v3_copy(&desc.v, &lock->imp.v);
    enemy_shatter_create(w, &desc, 0, 0);
}

/* 0x00af6b80 */
int gate_node_cb(unsigned int *n, int idx, GateWork *w)
{
    float pt[4];
    float base[4];
    int ret = 1;
    unsigned int flags = *n;
    int k = w->node_i;

    if (flags & 0x20000000) {
        float *cmtx = camera_get_inv_mtx();
        ulMatrixGetTranslation((float *)ulMatrixCur, pt);
        ulMatrixApplyPoint(cmtx, pt, &w->pts[k].x);
        w->node[k] = idx;
        if (w->hit[k])
            ret = 0;
    }
    if (flags & 0x80000000) {
        if (!w->hit[k]) {
            float *cmtx = camera_get_inv_mtx();
            ulMatrixGetTranslation((float *)ulMatrixCur, base);
            ulMatrixApplyPoint(cmtx, base, &w->rec[k].base.x);
        }
        if (w->node_i < 7)
            w->node_i++;
    }
    return ret;
}

/* 0x00af6ca8 */
void gate_bars_open(GateWork *w)
{
    Vec3 p;
    Vec3 dir;
    GateBar *bar;
    int i;

    w->body.frame = w->body.frames;
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    gate_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    w->node_i = 0;
    model_node_run_default(&w->body);
    ulMatrixPop();
    w->body.frame = 0;
    v3_copy(&p, &w->pos);
    for (i = 8, bar = w->rec; i != 0; i--, bar++) {
        v3_sub(&dir, &bar->base, &p);
        vec3_normalize(&dir, &dir);
        v3_scale(&dir, gate_bar_len);
        v3_sum(&bar->end, &dir, &p);
        v3_copy(&bar->open_pos, &p);
    }
}

/* 0x00af6e10 */
void gate_bars_move(GateWork *w)
{
    GateBar *bar;
    int *hit;
    int i;

    switch (w->open) {
    case 0:
        break;
    case 1:
        for (bar = w->rec, i = 8; i != 0; i--, bar++)
            vec3_approach(&bar->end, &bar->open_pos, gate_bar_close_rate);
        break;
    }
    for (i = 8, bar = w->rec, hit = w->hit; i != 0; bar++, hit++, i--) {
        if (*hit)
            vec3_approach(&bar->end, &bar->base, gate_bar_return_rate);
    }
}

/* 0x00af6ee8 */
void gate_locks_init(GateWork *w)
{
    LockObj *lock = w->lock;
    Vec3 *p = w->pts;
    int *hit = w->hit;
    int i;

    for (i = 7; i >= 0; lock++, p++, hit++, i--) {
        *hit = 0;
        lockon_obj_init(lock, w, p, 0, gate_lock_name, w->fx, 1, 1, 136);
        lockon_entry(lock);
        lock->flags &= ~LOCK_LOCKABLE;
    }
}

/* 0x00af6fb0 */
void gate_locks_remove(GateWork *w)
{
    LockObj *lock = w->lock;
    int i;

    for (i = 8; i != 0; i--) {
        lockon_remove(lock);
        lock++;
    }
}

/* 0x00af6ff0 */
void gate_locks_enable(GateWork *w)
{
    LockObj *lock = w->lock;
    int i;

    for (i = 8; i != 0; i--) {
        lock->flags |= LOCK_LOCKABLE;
        lock++;
    }
}

/* 0x00af7020 */
int gate_hit_check(GateWork *w)
{
    Vec3 dir;
    float sc[4][2];
    float x, y, z;
    LockObj *lock = w->lock;
    int *hit = w->hit;
    int i, n = 0, k;

    for (i = 0; i < 8; i++, hit++, lock++) {
        if (*hit) {
            n++;
            continue;
        }
        if (!(lock->flags & LOCK_HIT) || lock->life > 0)
            continue;
        v3_sub(&dir, &w->pts[i], &w->pos);
        vec3_normalize(&dir, &dir);
        gate_bar_shatter(w, i);
        *hit = 1;
        do {
            x = dir.x;
            y = dir.y;
            z = dir.z;
        } while (0);
        w->acc.x -= x * gate_hit_push;
        w->acc.y -= y * gate_hit_push;
        w->acc.z -= z * gate_hit_push;
        ROT_Y(ANG_180, sc[0], x, z);
        ROT_Z(-w->ang[2], sc[1], x, y);
        ROT_X(-w->ang[0], sc[2], y, z);
        ROT_Y(-w->ang[1], sc[3], x, z);
        w->ang[0] += (int)DEG_TO_ANG(y * gate_hit_turn_x);
        w->ang[1] += (int)DEG_TO_ANG(x * gate_hit_turn_y);
        k = 7 - w->broken++;
        SoundRhythmSe_ChangeSymbol(k);
        if (!k)
            w->note_done = 0;
        n++;
    }
    return n >= 8;
}

/* 0x00af72e8 */
void gate_move(GateWork *w)
{
    Vec3 scroll;

    spring3_step(gate_spring_rate, gate_spring_damp, &gate_spring_origin, &w->sp, &w->vel, &w->acc);
    player_get_vec_old(&scroll);
    v3_add(&w->pos0, &scroll);
    v3_sum(&w->pos, &w->pos0, &w->sp);
    ang3_approach(w->ang0, w->ang, gate_turn_rate);
}

/* steer(cast) */
/* 0x00af73c0 */
void gate_models_init(GateWork *w)
{
    float rad = 160.0f;
    int root, clip, wire_id;

    root = *((int *)pnb_get_entry(w->models, 2));
    clip = *((int *)pnb_get_entry(w->models, 5));
    dc_object_init(&w->body, root, clip, 0);
    w->body.radius = rad;
    root = *((int *)pnb_get_entry(w->models, 4));
    clip = *((int *)pnb_get_entry(w->models, 5));
    wire_id = model_pnb_get_wire(w->models, 4);
    dc_object_init(&w->part2, root, clip, 0);
    dc_object_set_wire(&w->part2, wire_id);
    w->part2.radius = rad;
    root = *((int *)pnb_get_entry(w->models, 6));
    clip = *((int *)pnb_get_entry(w->models, 7));
    dc_object_init(&w->part3, root, clip, 0);
    w->part3.radius = rad;
}

/* steer(cast) */
/* 0x00af74d0 */
void gate_models_open(GateWork *w)
{
    int root, wire_id;

    dc_object_release(&w->body);
    dc_object_release(&w->part2);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(w->models, 0)), 0, 0);
    root = *((int *)pnb_get_entry(w->models, 1));
    wire_id = model_pnb_get_wire(w->models, 1);
    dc_object_init(&w->part2, root, 0, 0);
    dc_object_set_wire(&w->part2, wire_id);
    dc_object_set_callback(&w->body, gate_node_cb, w);
}

/* 0x00af7598 */
void gate_init(GateWork *w, GateArg *prm)
{
    EnArmorDispArg ent;

    i3_copy(w->ang0, prm->ang);
    w->t0 = w->t = prm->time;
    i3_copy(w->ang, prm->ang);
    w->base = game_pass_time_get();
    player_pass_pos_a(&w->pos, w->t);
    v3_copy(&w->pos0, &w->pos);
    w->fx = enemy_status_ent_create(prm, w);
    enemy_status_event(w->fx, 2, &w->pos);
    w->note_done = 1;
    w->broken = 0;
    w->fx_op = 3;
    w->models = gate_models;
    w->tex = gate_textures;
    gate_models_init(w);
    gate_locks_init(w);
    w->show3 = 1;
    w->counting = 0;
    w->sym = 0;
    heap_bzero(&ent, 20);
    ent.owner = w;
    ent.work = w;
    ent.pos = &w->pos;
    ent.fn = (void (*)(void *))gate_disp;
    en_sort_add(&ent);
}

/* 0x00af76e8 */
void gate_release(GateWork *w)
{
    if (!w->note_done)
        SoundRhythmSe_ChangeSymbolStop();
    dc_object_release(&w->body);
    dc_object_release(&w->part2);
    dc_object_release(&w->part3);
    if (w->counting)
        GameFlowCountStop();
    gate_locks_remove(w);
    enemy_status_event(w->fx, w->fx_op, &w->pos);
}

/* 0x00af7760 */
void gate_pose(GateWork *w)
{
    gate_move(w);
    if (!(system_flags & SYS_DISPLAY_PASS)) {
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        gate_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        w->node_i = 0;
        model_node_run_default(&w->body);
        ulMatrixPop();
    }
}

/* 0x00af77f8 */
void gate_exec(GateWork *w)
{
    CsArg prm;
    float z = 0.0f;

    w->t = w->t0 - (game_pass_time_get() - w->base);
    if (w->t <= z) {
        if (w->state < 2) {
            player_status_add_damage(1.0f);
            w->state = 2;
        }
        v3_copy(&prm.pos, &w->pos);
        prm.ang[0] = 0;
        prm.ang[1] = 0;
        prm.ang[2] = 0;
        prm.scl[0] = z;
        prm.scl[1] = z;
        prm.scl[2] = z;
        prm.scale = 0.9f;
        prm.skip = 1;
        change_symbol_create(&prm);
        w->note_done = 1;
    }
    gate_move(w);
    switch (w->state) {
    case 0:
        dc_object_advance_time(&w->body);
        dc_object_advance_time(&w->part2);
        dc_object_advance_time(&w->part3);
        if (w->body.flags2 & 1) {
            gate_models_open(w);
            gate_bars_open(w);
            gate_locks_enable(w);
            w->open = 1;
            w->state = 1;
        }
        break;
    case 1:
        if (gate_hit_check(w)) {
            w->sym = 1;
            w->state = 2;
        }
        break;
    case 2:
        gate_shatter(w);
        w->show3 = 0;
        w->state++;
    case 3:
        if (w->t <= 0.0f) {
            w->state = 0;
            w->sym = 0;
            TASK_OF(w)->exec = (void (*)(void *))gate_pose;
        }
        break;
    }
    gate_bars_move(w);
    if (!(system_flags & SYS_DISPLAY_PASS)) {
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        gate_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        w->node_i = 0;
        model_node_run_default(&w->body);
        ulMatrixPop();
    }
}

/* steer(cast) */
/* 0x00af7a20 */
void gate_disp(GateWork *w)
{
    GateBar *bar;
    void *v;
    unsigned int col, col_rgb;
    int i;

    ulGxSetTextureList(w->tex);
    ulMatrixPush();
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    gate_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    w->node_i = 0;
    dc_object_draw(&w->body);
    draw_set_color(RGBA(0x32, 0x32, 0x32, 0x80));
    dc_object_draw(&w->part2);
    if (w->show3)
        dc_object_draw(&w->part3);
    if (w->sym)
        change_symbol_model_draw();
    ulMatrixPop();
    draw_set_blend(4, 5);
    col = gate_bar_col;
    col_rgb = col & 0xffffff;
    bar = w->rec;
    i = 8;
    v = line_set_translucent(1);
    do {
        v = ((void *(*)(void *, int, int, float *, float *, float))line_add_v_rgb)(v, col, col_rgb, &bar->base.x, &bar->open_pos.x, gate_bar_width);
        bar++;
    } while (--i);
    line_end(v);
}

/* 0x00af7b50 */
void *gate_create(void *parent, void *prm)
{
    return task_create_inherit_arg(parent, &gate_desc, 1492, prm);
}

/* 0x00af7b78 */
int gate_marker_draw(void *w, GateArg *prm)
{
    float t = prm->time;
    float size = 10.0f;
    Vec3 p;

    ulGxSetTextureList(gate_textures);
    player_pass_pos_a(&p, t);
    ulMatrixPush();
    if (view_sphere_visible(&p, size)) {
        do {
            Vec3 *p_ = &p;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        gate_rotate_yxz(prm->ang);
        debug_draw_box(COL_WHITE, size);
    }
    ulMatrixPop();
    return 0;
}

/* steer(cast) */
/* 0x00af7c28 */
int gate_vars_register(void)
{
    gate_textures = model_create_timed(gate_texture_data, 0x54001);
    gate_models = ((void *(*)(void *))model_pnb_register)(gate_model_data);
    gw_varedit_add_table(&gw_varedit_root,
                         gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), gate_vardef),
                         gate_vardefs);
    return 0;
}

/* 0x00af7cb0 */
int gate_vars_unregister(void)
{
    enemy_vars_group_release();
    model_list_remove(gate_textures);
    model_free_all_buffers_checked(gate_models);
    gate_textures = 0;
    gate_models = 0;
    return 0;
}

/* 0x00af7d00 */
void gate_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
