/*
 * Garuda, Area 2 enemy launched by its nest: swoops at the player on a curved path, fires a
 * missile, banks away and leaves once far from the camera.
 */

#include "stg02.h"
#include "mathdef.h"
#include "camera.h"
#include "debug.h"
#include "enemy_task.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_garuda.h"
#include "e_shot.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/s_sin.h"
#include "../common/util.h"

extern int en_garuda_init_arg;
extern void (*en_garuda_exec_table[])(void *o, void *w);
extern ParamRec en_garuda_params;
extern VarDef en_garuda_vardefs[8];
extern int en_garuda_missile_node;

typedef struct EnGarudaWork {
    Vec3 *pos;                  /* 0x00: the object's vectors (enemy_task_pos_ptr ...) */
    Vec3 *param_vec;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_garuda_states */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    ModelPart glow;                /* 0x20 */
    Vec3 rel;                   /* 0x7c: the start relative to the player */
    Vec3 off;                   /* 0x88: placement parameters 2..4 */
    Vec3 to;                     /* 0x94: from the object to the player + off */
    Vec3 fwd;                   /* 0xa0: +z turned by the placement angles */
    int dang[3];                /* 0xac: the direction of to */
    Vec3 vel;                   /* 0xb8: damped by 0.97 each frame */
    Vec3 mv;                    /* 0xc4: the escape direction */
    int rate[3];                /* 0xd0: added to the angles while escaping */
    float spd;                  /* 0xdc */
    float len;                  /* 0xe0: the length of to */
    int path;                   /* 0xe4: index into en_garuda_paths / en_garuda_path_rolls */
    int flags;                  /* 0xe8 */
    float cnt;                  /* 0xec: frames */
    float cnt_end;              /* 0xf0 */
} EnGarudaWork;

extern void (*en_garuda_states[])(void *o, EnGarudaWork *w);
extern float en_garuda_glow_col[4];
extern Vec3 en_garuda_paths[][200];
extern int en_garuda_path_rolls[][200];
extern float en_garuda_missile_pos[];
extern char en_garuda_name[];

/* en_garuda_params: name, label, values */
typedef struct EnGarudaRec {
    char *name;
    char *label;                /* 0x04 */
    int he;                     /* 0x08 */
    int score;                  /* 0x0c */
    float spd_attack, spd_escape_up, spd_ang_escape, pow_shot, dst_clip;      /* 0x10 */
    int debug;                  /* 0x24: shows the debug lines (with flag 16) */
} EnGarudaRec;

#define EN_GARUDA_REC ((EnGarudaRec *)&en_garuda_params)
extern void (*en_garuda_disp_table[])(void *o, EnGarudaWork *w);
extern char en_garuda_lock_name[];

/* 0x00affde8 */
void en_garuda_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_garuda_params.name;
    def.fmt = en_garuda_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_garuda_vardefs;
    fld = en_garuda_params.v;
    for (i = 0; i < 8; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_garuda_vardefs);
}

/* 0x00affe98 */
void en_garuda_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00affea0 */
int en_garuda_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 11);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00affef8 */
int en_garuda_vars_init(int *id)
{
    en_garuda_vars_register();
    en_garuda_init_arg = *id;
    return 0;
}

/* 0x00afff30 */
int en_garuda_vars_release(void)
{
    en_garuda_vars_unregister();
    return 0;
}

/* 0x00afff50 */
void en_garuda_fire_missile(void *obj, int w, Vec3 *dir)
{
    Vec3 pos;
    int level = 10;

    if (game_type_get() == 3)
        level = 11;
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_garuda_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    ulMatrixApplyPoint((float *)ulMatrixCur, en_garuda_missile_pos, &pos.x);
    ulMatrixPop();
    e_shot_fire(obj, &pos, dir, level);
}

/* 0x00b00018 */
void en_garuda_sample_path(float t, void *obj, EnGarudaWork *w, Vec3 *out, int *ang)
{
    Vec3 seg;
    Vec3 *base;
    Vec3 *path;
    int *angs;
    int i, j, dang, rot;

    t *= 199.0f;
    i = (int)t;
    t -= (float)i;
    j = i + 1;
    if (j >= 200)
        j = 199;
    path = en_garuda_paths[w->path];
    angs = en_garuda_path_rolls[w->path];
    v3_sub(&seg, &path[j], &path[i]);
    v3_scale(&seg, t);
    base = &path[i];
    v3_sum(out, base, &seg);
    dang = angs[j] - angs[i];
    dang = dang & 0x8000 ? dang | 0xffff0000 : dang & 0xffff;
    rot = (int)(angs[i] + dang * t);
    *ang = rot & 0x8000 ? rot | ~0xffff : rot & 0xffff;
}

/* steer(cast) */
/* 0x00b00178 */
void en_garuda_state_attack(void *obj, EnGarudaWork *w)
{
    Vec3 pos;
    Vec3 prev;
    Vec3 base;
    Vec3 dir;
    int rot[4];
    int shot_ang[3];
    int roll;
    float sx, cx, sy, cy, sq, cq;
    float frm, s, arc, x, y, z;
    ModelPart *gfx;

    switch (w->step) {
    case 0:
        w->cnt = 1.0f;
        frm = w->len / (EN_GARUDA_REC->spd_attack * FRAME_SEC);
        frm = 60.0f <= frm ? (int)frm : 60.0f;
        w->cnt_end = frm;
        w->step++;
        w->next_step++;
    case 1:
        s = sin(w->cnt * 1.5707964f / w->cnt_end);
        enemy_task_get_pos(obj, &prev);
        v3_sub(&prev, &prev, &w->vel);
        player_get_pos(&base);
        v3_add(&base, &w->rel);
        en_garuda_sample_path((s + w->cnt / w->cnt_end) * 0.5f, obj, w, &dir, &roll);
        dir.z *= w->len;
        ulMatrixPush();
        matrix_unit_set(0);
        en_garuda_rotate_yxz(w->dang);
        ulMatrixApplyVector((float *)ulMatrixCur, (float *)&dir, (float *)&dir);
        ulMatrixPop();
        v3_sum(&pos, &base, &dir);
        arc = sin(s * PI);
        vec3_scale_to_length(arc * 50.0f, &w->fwd.x, &dir.x);
        v3_add(&pos, &dir);
        enemy_task_set_pos(obj, &pos);
        v3_sub(&dir, &pos, &prev);
        do {
            rot[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            rot[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        rot[2] = roll;
        enemy_task_set_rot(obj, rot);
        if (w->cnt_end - 60.0f < w->cnt) {
            gfx = enemy_task_gfx_ptr(obj);
            if (!(w->flags & 2)) {
                dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 12)));
                w->flags |= 2;
            }
            gfx->frame = (w->cnt - w->cnt_end + 60.0f) * 76.0f / FPS;
            if (!(w->flags & 1) && 76.0f <= gfx->frame
                && !(fabsf(enemy_task_get_param(obj, 1)) < FLT_EPSILON)) {
                en_garuda_fire_missile(obj, (int)w, &dir);
                x = 0.0f;
                y = 0.0f;
                z = EN_GARUDA_REC->pow_shot;
                do {
                    shot_ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
                    shot_ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
                } while (0);
                do {
                    float old_y = y, old_z = z;
                    ulSinCos(shot_ang[0], &sx, &cx);
                    y = old_y * cx - old_z * sx;
                    z = old_y * sx + old_z * cx;
                } while (0);
                do {
                    float old_x = x, old_z = z;
                    ulSinCos(shot_ang[1], &sy, &cy);
                    x = old_x * cy + old_z * sy;
                    z = -old_x * sy + old_z * cy;
                } while (0);
                do {
                    float old_y = y, old_z = z;
                    ulSinCos(ANG_90, &sq, &cq);
                    y = old_y * cq - old_z * sq;
                    z = old_y * sq + old_z * cq;
                } while (0);
                v3_set(&w->vel, x, y, z);
                w->flags |= 1;
            }
        }
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            vec3_scale_to_length(FRAME_SEC, &dir.x, &dir.x);
            enemy_task_set_param_vec(obj, &dir);
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* steer(cast) */
/* 0x00b007d8 */
void en_garuda_state_escape(void *obj, EnGarudaWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    Vec3 side;
    Vec3 pos;
    int ang[3];
    Vec3 vel;
    Vec3 eye;
    float len, spd, clip, y, xz;

    switch (w->step) {
    case 0:
        dc_object_set_clip(gfx, *((struct DcClip **)pnb_get_entry(stage_models, 13)));
        dc_object_set_clip(&w->glow, *((struct DcClip **)pnb_get_entry(stage_models, 20)));
        *(int *)&gfx->frame = 0;
        gfx->speed = 1.0f;
        util_dir_to_player_frame(&enemy_task_param_vec_ptr(obj)->x, &side.x);
        if (0.0f < side.x)
            w->rate[2] = (int)DEG_TO_ANG(EN_GARUDA_REC->spd_ang_escape * FRAME_SEC);
        else
            w->rate[2] = -(int)DEG_TO_ANG(EN_GARUDA_REC->spd_ang_escape * FRAME_SEC);
        y = EN_GARUDA_REC->spd_escape_up * FRAME_SEC;
        xz = 0.0f;
        util_rotate_xyz(enemy_task_rot_ptr(obj), xz, y, xz, &w->mv);
        w->step++;
        w->cnt = 1.0f;
        w->next_step++;
    case 1:
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_rot(obj, ang);
        enemy_task_get_param_vec(obj, &vel);
        dc_object_advance_time(gfx);
        len = vec3_length(&vel);
        spd = len + 0.027777778f < 1.6666667f ? len + 0.027777778f : 1.6666667f;
        v3_scale(&vel, spd / len);
        enemy_task_set_param_vec(obj, &vel);
        v3_add(&pos, &vel);
        if (30.0f <= gfx->frame) {
            len = w->spd;
            len = len + 0.0027777778f < 0.33333334f ? len + 0.0027777778f : 0.33333334f;
            w->spd = len;
            vec3_scale_to_length(len, &w->mv.x, &vel.x);
            v3_add(&pos, &vel);
            i3_add(ang, w->rate);
        }
        enemy_task_set_pos(obj, &pos);
        enemy_task_set_rot(obj, ang);
        clip = EN_GARUDA_REC->dst_clip;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&pos, &eye) > clip * clip)
            enemy_task_set_state(obj, 2);
        break;
    case 2:
    default:
        return;
    }
    if (w->phase != w->next || w->step != w->next_step)
        w->step++;
}

/* 0x00b00bc8 */
void en_garuda_exec_normal(void *obj, EnGarudaWork *w)
{
    Vec3 pos;

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) == 1) {
        enemy_task_get_pos(obj, &pos);
        v3_scale(&w->vel, 0.97f);
        v3_add(&pos, &w->vel);
        enemy_task_set_pos(obj, &pos);
        if (en_garuda_states[w->phase])
            en_garuda_states[w->phase](obj, w);
    }
}

/* 0x00b00cc0 */
void en_garuda_exec_destroyed(void *obj, EnGarudaWork *w)
{
    SpawnDesc desc;

    if (enemy_task_get_phase(obj) == 1) {
        nj_fill8(&desc, 0, sizeof desc);
        desc.type = 21;
        desc.models = stage_models;
        desc.kind = 0;
        enemy_task_get_pos(obj, &desc.pos);
        enemy_task_get_rot(obj, desc.ang);
        enemy_task_get_scl(obj, &desc.scale);
        enemy_task_get_hit_imp(obj, &desc.v);
        enemy_shatter_create(obj, &desc, 0, 0);
        enemy_task_next_phase(obj);
    }
}

/* 0x00b00d68 */
void en_garuda_exec(void *obj, void *w)
{
    void (**tbl)(void *obj, void *w) = en_garuda_exec_table;
    unsigned int state = enemy_task_get_pstate(obj);

    if (tbl[state])
        tbl[state](obj, w);
}

/* 0x00b00dc8 */
void en_garuda_pre_exec(void)
{
}

/* 0x00b00dd0 */
void en_garuda_post_exec(void *obj, float *w)
{
    w[23] = ((float *)enemy_task_gfx_ptr(obj))[15];
}

/* 0x00b00e00 */
void en_garuda_toggle_missile(void *obj, int *w)
{
    if (w[58] & 1)
        util_node_toggle((int *)en_garuda_missile_node);
}

/* steer(cast) */
/* 0x00b00e20 */
void en_garuda_disp_glow(void *obj, EnGarudaWork *w)
{
    int blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);

    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    en_garuda_glow_col[0] = ease_sine_in_out_2x_neg(beat_clock_get_bar());
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_garuda_glow_col);
    dc_object_draw(&w->glow);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* 0x00b00ec8 */
void en_garuda_disp_body(void *obj, EnGarudaWork *w)
{
    Vec3 *pos = enemy_task_pos_ptr(obj);

    if (!view_sphere_visible(pos, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius))
        return;
    ulGxSetTextureList(stage_textures);
    do {
        ulMatrixPush();
    } while (0);
    vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    en_garuda_rotate_yxz((int *)enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    en_garuda_toggle_missile(obj, (int *)w);
    dc_object_draw(enemy_task_gfx_ptr(obj));
    en_garuda_toggle_missile(obj, (int *)w);
    en_garuda_disp_glow(obj, w);
    ulMatrixPop();
}

/* 0x00b00fb8 */
void en_garuda_disp(void *obj, EnGarudaWork *w)
{
    void (**tbl)(void *obj, EnGarudaWork *w) = en_garuda_disp_table;
    unsigned int state;
    Vec3 pos;
    int ang[3];
    Vec3 pl;
    Vec3 rel;

    state = enemy_task_get_pstate(obj);
    if (tbl[state])
        tbl[state](obj, w);
    if (debug_flags_test(16) && EN_GARUDA_REC->debug) {
        debug_set_origin(10, 20);
        enemy_task_get_pos(obj, &pos);
        debug_printf("P:%d,%d,%d", (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_rot(obj, ang);
        debug_printf("A:%d,%d,%d", (int)(ang[0] * DEG_PER_ANG), (int)(ang[1] * DEG_PER_ANG),
                     (int)(ang[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        player_get_pos(&pl);
        v3_sub(&rel, enemy_task_pos_ptr(obj), &pl);
        debug_printf("SUB:%f,%f,%f", rel.x, rel.y, rel.z);
        debug_newline();
        debug_printf("CNT:%d/%d", (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        debug_printf("MOD:%d", w->phase);
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00b011d0 */
void en_garuda_init(void *obj, EnGarudaWork *w, void *prm)
{
    EnGarudaArg *place = prm;
    Vec3 pl;
    Vec3 tgt;
    Vec3 pos;
    float x, y;

    w->pos = enemy_task_pos_ptr(obj);
    w->param_vec = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    if (MAX_GE(enemy_task_get_param(obj, 0), 0.0f) < 4.0f)
        w->path = MAX_GE(enemy_task_get_param(obj, 0), 0.0f);
    else
        w->path = 4;
    do { w->next = 0; } while (0);
    do { w->next_step = 0; } while (0);
    enemy_task_clear_flags(obj, 64);
    lockon_obj_set_name(enemy_task_lock_ptr(obj), en_garuda_lock_name);
    x = enemy_task_get_param(obj, 2);
    y = enemy_task_get_param(obj, 3);
    v3_set(&w->off, x, y, enemy_task_get_param(obj, 4));
    player_get_pos(&pl);
    v3_sub(&w->rel, &place->pos, &pl);
    v3_set(&w->fwd, 0.0f, 0.0f, 1.0f);
    ulMatrixPush();
    matrix_unit_set(0);
    en_garuda_rotate_yxz(place->ang);
    ulMatrixApplyVector((float *)ulMatrixCur, &w->fwd.x, &w->fwd.x);
    ulMatrixPop();
    player_get_pos(&tgt);
    v3_add(&tgt, &w->off);
    enemy_task_get_pos(obj, &pos);
    v3_sub(&w->to, &tgt, &pos);
    do {
        w->dang[1] = (int)RAD_TO_ANG(atan2f(-w->to.x, -w->to.z));
        w->dang[0] = (int)RAD_TO_ANG(atan2f(w->to.y, sqrtf(w->to.x * w->to.x + w->to.z * w->to.z)));
    } while (0);
    w->dang[2] = 0;
    w->len = vec3_length(&w->to);
    dc_object_init(&w->glow, *((struct DcNode **)pnb_get_entry(stage_models, 18)), *((struct DcClip **)pnb_get_entry(stage_models, 19)), 0);
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00b01548 */
void en_garuda_release(void *obj, char *w)
{
    dc_object_release((struct MapPart *)(w + 0x20));
}

/* 0x00b01550 */
int en_garuda_create(void *parent, EnGarudaArg *place)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_garuda_init;
    funcs.fn[1] = en_garuda_release;
    funcs.fn[2] = en_garuda_exec;
    funcs.fn[3] = en_garuda_disp;
    funcs.fn[4] = en_garuda_pre_exec;
    funcs.fn[5] = en_garuda_post_exec;
    desc.name = en_garuda_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 16;
    desc.dc_flags = 0;
    desc.model_no = 11;
    desc.model_no2 = -1;
    desc.hp = en_garuda_params.v[0];
    desc.score = en_garuda_params.v[1];
    *(unsigned char *)place = *(unsigned char *)&en_garuda_init_arg;
    desc.size = 244;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00b01690 */
void en_garuda_nodes_init(void)
{
    void *mdl = (void *)*((int *)pnb_get_entry(stage_models, 11));
    void *clip = (void *)*((int *)pnb_get_entry(stage_models, 12));

    node_tree_collect_to(mdl, 0x80000000, (struct NodeTreeNode * *)&en_garuda_missile_node);
    ulMatrixPush();
    matrix_unit_set(0);
    util_node_collect_pos_at(mdl, clip, 76.0f, 0x80000000, 1, (int)en_garuda_missile_pos);
    ulMatrixPop();
}

/* 0x00b01720 */
void en_garuda_se_play(void)
{
    SoundRhythmSe_Enemy(en_garuda_init_arg, 2);
}

/* 0x00b01730 */
void en_garuda_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
