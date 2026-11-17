/*
 * Virtues base, Area 5: left on the path by a Virtues laser; rises, blinks with the beat, raises
 * its four Virtues walls in turn while travelling with the path, then fades.
 */

#include "stg05.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "en_virtues_base.h"
#include "en_vts_cmd_dmg_eff.h"
#include "virtues_wall.h"
#include "../common/enemy_vars.h"

extern DominionsBodyAndTailSet en_virtues_base_types[1];
extern VarDef en_virtues_base_vardef;
extern VarDef en_virtues_base_vardefs[11];
extern float en_virtues_base_secs[4];
extern float en_virtues_base_total_sec;

/* 8 bytes */
typedef struct EnVirtuesBaseCreateArg {
    int idx;
    EnVirtuesBaseArg *arg;                /* 0x04 */
} EnVirtuesBaseCreateArg;

extern int en_virtues_base_light_models[2];
extern struct TaskDesc en_virtues_base_desc;

/* clears objs entries whose task finished */
#define VBASE_CHECK_OBJS(w) { void **p_ = (w)->objs; int i_; for (i_ = 0; i_ < 4; i_++, p_++) { if (*p_ && (TASK_OF(*p_)->flags & 1)) *p_ = 0; } }

/* 0x00b1aeb0 */
void en_virtues_base_vars_register(void)
{
    int top, grp, i;
    unsigned int j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_virtues_base_vardef);
    for (i = 0; i < 1U; i++) {
        def = en_virtues_base_vardefs;
        val = en_virtues_base_types[i].v;
        en_virtues_base_vardef.name = en_virtues_base_types[i].name;
        en_virtues_base_vardef.fmt = en_virtues_base_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_virtues_base_vardef);
        for (j = 0; j < 11; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_virtues_base_vardefs);
    }
    en_virtues_base_total_sec = 0.0f;
    for (i = 0; i < 4; i++)
        en_virtues_base_total_sec += en_virtues_base_secs[i];
}

/* 0x00b1aff8 */
void en_virtues_base_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* steer(cast) */
/* 0x00b1b000 */
void en_virtues_base_init(EnVirtuesBaseWork *w, EnVirtuesBaseCreateArg *prm)
{
    unsigned int i;

    w->type = (EnVirtuesBaseRec *)&en_virtues_base_types[prm->idx];
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        w->models = m_;
        w->tex = t_;
    } while (0);
    dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(stage_models, 45)), 0, 0);
    dc_object_rebuild(&w->body);
    for (i = 0; i < 2; i++) {
        dc_object_init(&w->parts[i], *((struct DcNode **)pnb_get_entry(w->models, en_virtues_base_light_models[i])), 0, 0);
        dc_object_rebuild(&w->parts[i]);
    }
    en_virtues_laser_place_on_path(&w->pos, w->ang, prm->arg);
    w->arg = *prm->arg;
    w->sel = 0;
    w->count = 0;
    w->phase = rand_lcg_next_a() % 4;
    heap_bzero(w->objs, 16);
    w->fade = 0.0f;
    w->t = 0.0f;
    w->scale = 0.0f;
}

/* 0x00b1b1a8 */
void en_virtues_base_release(EnVirtuesBaseWork *w)
{
    unsigned int i;

    for (i = 0; i < 2; i++)
        dc_object_release(&w->parts[i]);
    dc_object_release(&w->body);
}

/* 0x00b1b200 */
void en_virtues_base_exec_end(EnVirtuesBaseWork *w)
{
    Vec3 mv;
    EnVirtuesBaseRec *typ;
    int cnt;

    VBASE_CHECK_OBJS(w);
    cnt = w->count;
    typ = w->type;
    w->count = cnt + 1;
    w->scale = 1.0f - cnt / (typ->fade_sec * FPS);
    if (typ->fade_sec * FPS < cnt + 1)
        FINISH_IF(w);
    player_get_vec(&mv);
    v3_add(&w->pos, &mv);
}

/* 0x00b1b310 */
void en_virtues_base_exec_rise(EnVirtuesBaseWork *w)
{
    EnVirtuesBaseArg *prm = &w->arg;
    Vec3 *pos = &w->pos;
    float spd;
    int cnt, n;

    VBASE_CHECK_OBJS(w);
    if (prm->h < 0.0f)
        en_virtues_base_stop_walls(w);
    n = w->type->rise_sec * FPS;
    cnt = w->count + 1;
    w->count = cnt < n ? cnt : n;
    w->fade = (float)w->count / (float)n;
    spd = game_pass_count_spd_get();
    prm->h -= spd * prm->fall * enemy_status_get_ticks();
    en_virtues_laser_place_on_path(pos, w->ang, prm);
}

/* 0x00b1b438 */
void en_virtues_base_exec_hold(EnVirtuesBaseWork *w)
{
    int *ang;
    Vec3 *pos = &w->pos;
    EnVirtuesBaseArg *prm = &w->arg;
    void **p;
    float *tbl;
    float rest, spd;
    int i, k, n;

    ang = w->ang;
    k = enemy_status_get_ticks();
    n = w->type->spawn_sec * FPS;
    {
        void **p_ = w->objs;
        for (i = 0; i < 4; i++, p_++) {
            if (*p_ && (TASK_OF(*p_)->flags & TASK_F_END))
                *p_ = 0;
        }
    }
    k--;
    while (k != -1) {
        w->count++;
        w->t += en_virtues_base_total_sec / n;
        rest = w->t;
        for (i = 0; i < 4; i++) {
            rest -= en_virtues_base_secs[i];
            if (w->objs[i])
                virtues_wall_set_height(w->objs[i], 0.0f < rest || fabsf(rest) < FLT_EPSILON, rest);
            else if (-en_virtues_base_secs[i] < rest)
                w->objs[i] = virtues_wall_create(w, i == w->phase, i, rest, en_virtues_base_secs[i]);
        }
        if (w->count >= n) {
            w->count = 0;
            TASK_OF(w)->exec = (void (*)(void *))en_virtues_base_exec_rise;
            break;
        }
        k--;
    }
    spd = game_pass_count_spd_get();
    prm->h -= spd * prm->fall * enemy_status_get_ticks();
    en_virtues_laser_place_on_path(pos, ang, prm);
}

/* 0x00b1b688 */
void en_virtues_base_exec(EnVirtuesBaseWork *w)
{
    EnVirtuesBaseArg *prm = &w->arg;
    float spd;

    w->count += enemy_status_get_ticks();
    w->scale = safe_powf(w->count / (w->type->grow_sec * FPS), 5.0f);
    if (w->type->grow_sec * FPS < w->count) {
        w->count = 0;
        TASK_OF(w)->exec = (void (*)(void *))en_virtues_base_exec_hold;
        w->scale = 1.0f;
    }
    spd = game_pass_count_spd_get();
    prm->h -= spd * prm->fall * enemy_status_get_ticks();
    en_virtues_laser_place_on_path(&w->pos, w->ang, prm);
}

/* steer(cast) */
/* 0x00b1b780 */
void en_virtues_base_disp(EnVirtuesBaseWork *w)
{
    float col_add[4];
    float sc[2];
    EnVirtuesBaseRec *typ;
    float phase;
    int col, prev, dl;

    ulMatrixPush();
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_virtues_base_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (!(fabsf(w->fade) < FLT_EPSILON) && view_sphere_visible(&w->pos, w->parts[w->sel].radius)) {
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        typ = w->type;
        col = typ->col;
        phase = yuki_beat_phase(typ->div);
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->tex);
        w->sel = !(0.25f < phase && phase < 0.75f);
        ulSinCos(phase * 131072.0f + ANG_QUARTER, &sc[0], &sc[1]);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        col_add[1] = ((int (*)(unsigned int))color_get_r)(col) / 255.0f - 1.0f;
        col_add[2] = color_get_g(col) / 255.0f - 1.0f;
        col_add[3] = color_get_b(col) / 255.0f - 1.0f;
        col_add[0] = w->fade * (sc[0] + 1.0f) * 0.5f * color_get_a(col) / 255.0f - 1.0f;
        draw_set_override_color(col_add);
        dc_object_draw(&w->parts[w->sel]);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
        RenderSetCurrentDisplayList(dl);
    }
    vu0_matrix_scale(ulMatrixCur, w->scale, w->scale, w->scale);
    if (view_sphere_visible(&w->pos, w->body.radius)) {
        ulGxSetTextureList(w->tex);
        dc_object_draw(&w->body);
    }
    ulMatrixPop();
}

/* 0x00b1ba48 */
void *en_virtues_base_create(void *w, EnVirtuesBaseArg *prm)
{
    EnVirtuesBaseCreateArg arg;

    arg.idx = 0;
    arg.arg = prm;
    return task_create_sibling_inherit_arg(w, &en_virtues_base_desc, 384, &arg);
}

/* 0x00b1ba78 */
void en_virtues_base_stop_walls(EnVirtuesBaseWork *w)
{
    void **p = w->objs;
    int i;

    for (i = 0; i < 4; i++, p++) {
        if (*p) {
            virtues_wall_destroy(*p);
            *p = 0;
        }
    }
    w->count = 0;
    w->fade = 0;
    TASK_OF(w)->exec = (void (*)(void *))en_virtues_base_exec_end;
}

/* 0x00b1bae8 */
void en_virtues_base_get_pose(PosAngRec *w, Vec3 *pos, int *ang)
{
    v3_copy(pos, &w->pos);
    i3_copy(ang, w->ang);
}

/* 0x00b1bb28 */
void en_virtues_base_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
