/*
 * Eden boss claw form: a ring of posed wire trees whose leaf links carry the lockable pieces. The
 * pieces at the link tips launch a claw wall: a lockable model that grows as it flies at the
 * player and hurts them unless shot down.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_claw.h"
#include "eden.h"
#include "eden_core.h"
#include "eden_piece.h"
#include "eden_snake.h"
#include "eden_tree.h"
#include "../common/boss_common.h"

extern char eden_claw_vardef[];
extern GwVarDef eden_claw_vardefs[];
extern struct GwVarNode *eden_claw_vars_group;
struct EdenClawLink { Vec3 pos; Vec3 spring_pos; Vec3 target; int ang[3]; Vec3 blend_ang; int ang2[3]; int vel[3]; };
extern float cmd_claw_fv;
extern float cmd_cl_wall_mv;
extern float cmd_claw_at_frm;
extern float cmd_cl_move_nl, cmd_cl_move_kf, cmd_cl_move_mf;
extern float cmd_cl_kf, cmd_cl_mf, cmd_cl_rr;
extern float cmd_cl_wall_t;
extern float cmd_cl_wall_v;
extern int cmd_cl_wall_hp;
extern struct TaskDesc eden_claw_wall_desc;
extern unsigned int eden_claw_col;
extern struct TaskDesc eden_claw_desc;
extern char str_eden_claw_c[];
extern const int eden_claw_arm_num;
extern float cmd_cl_seq_t1;
extern float cmd_cl_ax;
extern float cmd_cl_ays;
extern float cmd_cl_ayl;
extern float cmd_cl_ays2;
extern float cmd_cl_ayl2;
extern float cmd_cl_tree_ang;
extern int cmd_cl_tree_num;
extern float cmd_cl_wall_vm, cmd_cl_wall_vr;
extern float cmd_cl_seq_t3, cmd_cl_seq_t4, cmd_cl_seq_t5, cmd_cl_seq_t6;
extern const float cmd_cl_seq_t2;
extern int eden_claw_seq_kinds[];
extern float cmd_claw_len, eden_claw_swing_r, cmd_claw_rz, cmd_claw_scl_x, cmd_claw_scl_y, cmd_claw_scl_z;
extern float cmd_rz_rate;
extern int cmd_claw_sh_sw, cmd_claw_mode_sw, cmd_cl_core_node;
extern EdenReq cmd_claw_at_sw;

/* one arm of the claw, 68 bytes */
struct EdenClawRow {
    void *tree;
    WireObj *wire;
    float rate;
    float len;
    float step;
    int spd;                    /* 1 opening, 2 open, 3 closing */
    char pad_18[8];
    int first;                  /* 0x20: the link the arm fires from next */
    int fired;
    EdenClawLink *sub;
    Vec3 fire_pos;
    int fire_ang[3];
};

/* 168 bytes */
typedef struct EdenClawWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    void *parent;
    Vec3 pos;
    Vec3 tip;                   /* where the trees and walls launch from */
    int ang[3];
    int spin[3];
    char pad_40[8];
    int roll;                   /* 0x48 */
    EdenClawRow *rows;
    int nrow;
    int nlink;
    float phase;
    int seq;
    int seq_cnt;
    int time;
    int len;
    int step;
    int ramp;                   /* the arms' open state: 0 closed, 2 open */
    Vec3 dir;
    Vec3 dir_to;
    Vec3 dir_vel;
    float twist;
    float twist_to;
    float twist_vel;
    float yaw;
} EdenClawWork;

/* 164 bytes */
typedef struct EdenClawWallWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    Vec3 start;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    int spin[3];
    int time;
    int len;
    int state;                  /* 0 opening, 1 grown */
    float spd;
    LockObj lock;
    float frm;
    float frm_to;
} EdenClawWallWork;

/* 0x00cab5f8 */
void eden_claw_vars_add(void)
{
    eden_claw_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_claw_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_claw_vars_group, eden_claw_vardefs);
}

/* 0x00cab648 */
void eden_claw_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_claw_vardef);
}

/* 0x00cab660 */
void eden_claw_count_leaves(WireObj *obj, int *n)
{
    if (!obj->child && !obj->sibling)
        (*n)++;
    if (obj->child)
        eden_claw_count_leaves(obj->child, n);
    if (obj->sibling)
        eden_claw_count_leaves(obj->sibling, n);
}

/* 0x00cab6c8 */
void eden_claw_tree_place_leaves(WireObj *obj, EdenClawLink **pp)
{
    ulMatrixPush();
    anim_apply_node((struct AnimNode *)obj);
    if (!obj->child && !obj->sibling) {
        matrix_current_pose(&(*pp)->pos, (*pp)->ang);
        (*pp)--;
    }
    if (obj->child)
        eden_claw_tree_place_leaves(obj->child, pp);
    ulMatrixPop();
    if (obj->sibling)
        eden_claw_tree_place_leaves(obj->sibling, pp);
}

/* 0x00cab758 */
void eden_claw_tree_pose(void *w, void *src, int val, float t)
{
    EdenClawLink *link = (EdenClawLink *)val;

    anim_start(src, t);
    eden_claw_tree_place_leaves(w, &link);
}

/* 0x00cab790 */
void eden_claw_row_reset(EdenClawWork *w, EdenClawRow *row)
{
    EdenClawLink *p = row->sub;
    int i;

    for (i = 0; i < w->nlink; i++, p++) {
        i3_copy((int *)&p->blend_ang, p->ang);
        v3_copy(&p->target, &p->pos);
        v3_copy(&p->spring_pos, &p->pos);
        i3_copy(p->ang2, p->ang);
    }
}

/* 0x00cab810 */
void eden_claw_row_follow(EdenClawWork *w, EdenClawRow *row)
{
    EdenClawLink *base = row->sub;
    EdenClawLink *p = base;
    int i;

    for (i = 0; i < w->nlink; i++, p++) {
        float damp = cmd_cl_mf;
        float k = cmd_cl_kf;
        int j = i - row->first;

        if (j < 0)
            j = 0;
        v3_copy(&p->target, &base[j].pos);
        i3_copy(p->ang2, base[j].ang);
        spring_to_v((Vec3 *)p->vel, &p->spring_pos, &p->target, damp, k);
        angles_blend((int *)&p->blend_ang, p->ang2, (int *)&p->blend_ang, cmd_cl_rr);
    }
}

/* 0x00cab938 */
void eden_claw_ramps_step(EdenClawWork *w)
{
    EdenClawRow *row = w->rows;
    int i;

    for (i = 0; i < w->nrow; i++, row++) {
        switch (row->spd) {
        case 1:
            w->ramp = row->spd;
            row->rate += row->step;
            if (row->rate >= cmd_claw_at_frm) {
                row->rate = cmd_claw_at_frm;
                row->spd++;
                w->ramp++;
            }
            break;
        case 2:
            break;
        case 3:
            row->rate += row->step;
            if (row->rate >= row->len) {
                row->spd = 0;
                row->rate = 0;
                w->ramp = 0;
            }
            break;
        }
    }
}

/* 0x00caba18 */
void eden_claw_rows_set_speed(EdenClawWork *w, int spd)
{
    EdenClawRow *row = w->rows;
    int i;

    for (i = 0; i < w->nrow; i++, row++) {
        row->spd = spd;
        row->step = cmd_claw_fv;
    }
}

/* 0x00caba58 */
void eden_claw_swing(EdenClawWork *w, int flag)
{
    float val;
    float deg;
    float sc[4];

#ifdef VERSION_US
    do {
        w->dir_to.x = w->dir_to.y = 0.0f;
        w->dir_to.z = -1.0f;
    } while (0);
    if (!flag) {
        {
            float t, u;

            do {
                t = RAND_FLOAT_A() * ((cmd_cl_ays - cmd_cl_ayl) - (cmd_cl_ayl - cmd_cl_ays)) + (cmd_cl_ayl - cmd_cl_ays);
            } while (0);
            if (0.0f < t)
                u = t + cmd_cl_ays;
            else
                u = t - cmd_cl_ays;
            val = u;
        }
    } else {
        {
            float t, u;

            do {
                t = RAND_FLOAT_A() * ((cmd_cl_ays2 - cmd_cl_ayl2) - (cmd_cl_ayl2 - cmd_cl_ays2)) + (cmd_cl_ayl2 - cmd_cl_ays2);
            } while (0);
            if (0.0f < t)
                u = t + cmd_cl_ays2;
            else
                u = t - cmd_cl_ays2;
            val = u;
        }
        w->twist_to = val;
    }
#else
    {
        w->dir_to.x = w->dir_to.y = 0.0f;
    }
    w->dir_to.z = -1.0f;
    if (flag) {
        val = RAND_FLOAT_A() * ((cmd_cl_ays2 - cmd_cl_ayl2) - (cmd_cl_ayl2 - cmd_cl_ays2)) + (cmd_cl_ayl2 - cmd_cl_ays2);
        if (0.0f < val)
            val += cmd_cl_ays2;
        else
            val -= cmd_cl_ays2;
        w->twist_to = val;
    } else {
        val = RAND_FLOAT_A() * ((cmd_cl_ays - cmd_cl_ayl) - (cmd_cl_ayl - cmd_cl_ays)) + (cmd_cl_ayl - cmd_cl_ays);
        if (0.0f < val)
            val += cmd_cl_ays;
        else
            val -= cmd_cl_ays;
    }
#endif
#ifdef VERSION_US
    w->yaw += val;
    do {
        float rot_u = w->dir_to.y, rot_v = w->dir_to.z;

        deg = ANG_PER_DEG;
        ulSinCos((int)(RAND_FLOAT_A() * cmd_cl_ax * deg), &sc[0], &sc[1]);
        w->dir_to.y = rot_u * sc[1] - rot_v * sc[0];
        w->dir_to.z = rot_u * sc[0] + rot_v * sc[1];
    } while (0);
#else
    w->yaw += val;
    deg = ANG_PER_DEG;
    ROT_X((int)(RAND_FLOAT_A() * cmd_cl_ax * deg), sc, w->dir_to.y, w->dir_to.z);
#endif
    ROT_Y((int)(w->yaw * deg), (sc + 2), w->dir_to.x, w->dir_to.z);
}

/* 0x00cabc68 */
int eden_claw_move_step(EdenClawWork *w)
{
    Vec3 dif;
    float vel_len, err_len;

    spring_to_v(&w->dir_vel, &w->dir, &w->dir_to, cmd_cl_move_mf, cmd_cl_move_kf);
    spring_to(&w->twist_vel, &w->twist, w->twist_to, cmd_cl_move_mf, cmd_cl_move_kf);
    v3_sub(&dif, &w->dir_to, &w->dir);
    vel_len = vec3_length(&w->dir_vel);
    err_len = vec3_length(&dif);
    return vel_len < cmd_cl_move_nl && err_len < cmd_cl_move_nl;
}

/* 0x00cabd68 */
void eden_claw_tree_shoot(EdenClawWork *w)
{
    Vec3 pl;
    int ang[3];
    Vec3 v;
    float sc[2];
    int i;

    player_get_pos(&pl);
    v3_sub(&pl, &pl, &w->tip);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-pl.x, -pl.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(pl.y, sqrtf(pl.x * pl.x + pl.z * pl.z)));
    } while (0);
    pl.z = -1.0f;
    pl.x = pl.y = 0.0f;
    ROT_X((int)DEG_TO_ANG(cmd_cl_tree_ang), sc, pl.y, pl.z);
    ang[2] = 0xffff / cmd_cl_tree_num;
    for (i = 0; i < cmd_cl_tree_num; i++) {
        ang[2] = i * 0xffff / cmd_cl_tree_num;
        v3_copy(&v, &pl);
        vec_rotate_zxy(&v, ang);
        eden_tree_create(w, &w->tip, &v, (int)w->parts, i);
    }
}

/* 0x00cabfa8 */
void eden_claw_attack_step(EdenClawWork *w)
{
    int k;

    switch (w->step) {
    case 0:
        k = eden_claw_move_step(w);
        if (w->time && --w->time)
            break;
        if (k)
            w->step++;
        break;
    case 1:
        w->seq_cnt = eden_claw_seq_kinds[w->seq];
        eden_claw_rows_set_speed(w, 1);
        w->step++;
        break;
    case 2:
        if (w->ramp == 2) {
            eden_claw_swing(w, 0);
            w->step++;
            w->time = w->len = (int)(cmd_cl_seq_t2 * FPS);
        }
        break;
    case 3:
        if (!eden_claw_move_step(w))
            break;
        if (--w->time > 0)
            break;
        eden_claw_piece_check(w);
        eden_claw_swing(w, 0);
        if (--w->seq_cnt <= 0) {
            if (w->seq >= 2) {
                float sec = cmd_cl_seq_t6;

                w->step = 6;
                w->time = w->len = (int)(sec * FPS);
            } else {
                w->step++;
                eden_claw_rows_set_speed(w, 3);
            }
        } else
            w->time = w->len = (int)(cmd_cl_seq_t3 * FPS);
        break;
    case 4:
        if (w->ramp)
            break;
        w->step++;
        eden_claw_tree_shoot(w);
        w->time = w->len = (int)(cmd_cl_seq_t4 * FPS);
        break;
    case 5:
        if (--w->time > 0)
            break;
        w->time = w->len = (int)(cmd_cl_seq_t5 * FPS);
        w->seq++;
        if (!(w->seq < 3)) {
            w->step = 6;
        } else {
            w->step = 0;
            eden_claw_swing(w, 1);
        }
        break;
    case 6:
        if (--w->time > 0)
            break;
        w->step++;
        break;
    case 7:
        break;
    }
    if (eden_get_lock_off() && w->step < 6) {
        w->time = w->len = (int)(cmd_cl_seq_t6 * FPS);
        w->step = 6;
    }
}

/* steer(cast) */
/* 0x00cac250 */
void eden_claw_exec(EdenClawWork *claw)
{
    Vec3 wob;
    int i;
    EdenClawRow *row;

    if (cmd_claw_mode_sw)
        eden_claw_attack_step(claw);
    eden_claw_ramps_step(claw);
    player_get_pos(&claw->pos);
    wobble_step_slow(&wob, eden_claw_swing_r, &claw->phase);
    {
        float rate;

        v3_add(&claw->pos, &wob);
        rate = cmd_rz_rate;
        claw->spin[2] = (int)DEG_TO_ANG(cmd_claw_rz);
        claw->roll += claw->spin[2];
        claw->ang[2] = claw->roll + (int)DEG_TO_ANG(claw->twist * rate);
    }
    do {
        claw->ang[1] = (int)RAD_TO_ANG(atan2f(-claw->dir.x, -claw->dir.z));
        claw->ang[0] = (int)RAD_TO_ANG(atan2f(claw->dir.y, sqrtf(claw->dir.x * claw->dir.x + claw->dir.z * claw->dir.z)));
    } while (0);
    do {
        claw->ang[0] = ANG_WRAP(claw->ang[0]);
        claw->ang[1] = ANG_WRAP(claw->ang[1]);
        claw->ang[2] = ANG_WRAP(claw->ang[2]);
    } while (0);
    row = claw->rows;
    tree_node_pose(row->wire, row->tree, &wob, 0, 0, 1, row->rate);
    wob.z = 0.0f;
    vec_rotate_zxy(&wob, claw->ang);
    v3_add(&claw->pos, &wob);
    tree_node_pose(row->wire, row->tree, &wob, 0, 0, cmd_cl_core_node, row->rate);
    wob.z += cmd_claw_len;
    wob.x = wob.y = 0.0f;
    vec_rotate_zxy(&wob, claw->ang);
    v3_sum(&claw->tip, &claw->pos, &wob);
    ulMatrixPush();
    matrix_unit_set(0);
    MTX_TRANS(&claw->pos);
    eden_claw_rotate_yxz(claw->ang);
    for (i = 0, row = claw->rows; i < claw->nrow; i++, row++) {
        ulMatrixPush();
        vu0_matrix_rotate_z(ulMatrixCur, i * 0xffff / claw->nrow);
        vu0_matrix_scale(ulMatrixCur, cmd_claw_scl_x, cmd_claw_scl_y, cmd_claw_scl_z);
        eden_claw_tree_pose(row->wire, row->tree, (int)&row->sub[claw->nlink - 1], row->rate);
        eden_claw_row_follow(claw, row);
        ulMatrixPop();
    }
    ulMatrixPop();
    if (cmd_claw_at_sw) {
        cmd_claw_at_sw = 0;
        if (!claw->ramp)
            eden_claw_rows_set_speed(claw, 1);
        else if (claw->ramp == 2)
            eden_claw_rows_set_speed(claw, 3);
    }
    if (cmd_claw_sh_sw) {
        cmd_claw_sh_sw = 0;
        eden_claw_piece_check(claw);
    }
}

/* 0x00cac6b8 */
void eden_claw_release(EdenClawWork *w)
{
    EdenClawRow *row = w->rows;
    int i;

    for (i = 0; i < w->nrow; i++, row++)
        heap_free(row->sub, 0);
    heap_free(w->rows, 0);
    eden_core_shine_reset(0);
}

/* 0x00cac730 */
void eden_claw_disp(EdenClawWork *w)
{
    EdenClawRow *row;
    EdenClawLink *link;
    int i, j;

    ulGxSetTextureList(w->tex_list);
    row = w->rows;
    if (w->step >= 6)
        draw_set_color((eden_claw_col & 0xffffff) | (color_get_a(eden_claw_col) * w->time / w->len + 1) >> 1 << 24);
    else
        draw_set_color(eden_claw_col);
    for (i = 0; i < w->nrow; i++, row++) {
        link = &row->sub[w->nlink - 1];
        for (j = 0; j < row->first; j++, link--) {
            ulMatrixPush();
            do {
                vu0_matrix_translate(ulMatrixCur, link->pos.x, link->pos.y, link->pos.z);
            } while (0);
            eden_claw_rotate_yxz(link->ang);
            dc_object_draw(w->parts[1]);
            ulMatrixPop();
        }
    }
}

/* 0x00cac8b8 */
int *eden_claw_create(void *parent, int *arg)
{
    EdenClawWork *w = task_create_inherit(parent, &eden_claw_desc, 168);
    char *src = (char *)arg;
    EdenClawRow *p;
    ModelPart *mdl;
    int n;

    mdl = *(ModelPart **)(src + 92);
    w->parent = parent;
    w->nrow = eden_claw_arm_num;
    {
        WireObj *obj = *(WireObj **)&mdl->root; /* steer(alias) */

        w->parts = (ModelPart **)src;
        eden_claw_count_leaves(obj, &w->nlink);
    }
    w->time = w->len = (int)(cmd_cl_seq_t1 * FPS);
    do {
        ((Vec3 *)&w->dir)->x = ((Vec3 *)&w->dir)->y = 0.0f; /* steer(cast): shares v3_copy's base */
        w->dir.z = -1.0f;
    } while (0);
    v3_copy(&w->dir_to, &w->dir);
    w->rows = heap_block_tag(game_calloc(w->nrow * 68), str_eden_claw_c, 511);
    for (n = w->nrow, p = w->rows; n; n--, p++) {
        p->sub = heap_block_tag(game_calloc(w->nlink * 84), str_eden_claw_c, 514);
        {
            ModelPart *node = w->parts[23];
            int cnt_a = node->root;
            int cnt_b = node->clip;
            float len = node->frames;

            p->wire = (WireObj *)cnt_a;
            p->tree = (void *)cnt_b;
            p->len = len;
        }
    }
    eden_claw_exec((char *)w);
    for (n = w->nrow, p = w->rows; n; n--, p++)
        eden_claw_row_reset(w, p);
    eden_core_shine_set_req(0, (int)&w->tip);
    return (int *)w;
}

/* 0x00caca60 */
void eden_claw_get_counts(EdenClawWork *w, int *nrow, int *nlink)
{
    *nrow = w->nrow;
    *nlink = w->nlink;
}

/* 0x00caca78 */
void eden_claw_get_link(EdenClawWork *w, int i, int j, Vec3 **pos, Vec3 **ang)
{
    EdenClawRow *row = &(w->rows)[i];
    EdenClawLink *link = &row->sub[j];

    *ang = &link->blend_ang;
    *pos = &link->spring_pos;
}

/* 0x00cacab0 */
int eden_claw_is_active(EdenClawWork *w)
{
    return w->step < 7;
}

/* 0x00cacac0 */
void eden_claw_piece_check(EdenClawWork *w)
{
    void *piece = eden_get_piece(w->parent);
    EdenClawRow *row = w->rows;
    int n = 0;
    int i;

    for (i = 0; i < w->nrow; i++, row++) {
        if (eden_piece_cell_get_pose(piece, i, row->first, &row->fire_pos, 0, row->fire_ang)) {
            eden_piece_cell_command(piece, i, row->first, -1);
            n++;
            row->fired = 1;
        }
        if (++row->first >= w->nlink)
            row->first = w->nlink - 1;
    }
    eden_claw_wall_create(w, (int)w->parts, &w->tip, w->ang, w->spin, n);
}

/* 0x00cacbe8 */
void eden_claw_wall_grow(EdenClawWallWork *w)
{
    switch (w->state) {
    case 0:
        w->frm += cmd_cl_wall_mv;
        if (w->frm >= w->parts[18]->frames) {
            w->state = 1;
            w->lock.flags |= LOCK_LOCKABLE;
            w->frm = 0;
        }
        break;
    case 1:
        if (w->frm < w->frm_to) {
            w->frm += cmd_cl_wall_mv;
            if (w->frm_to <= w->frm)
                w->frm = w->frm_to;
        }
        break;
    }
}

/* 0x00cacc88 */
void eden_claw_wall_exec(EdenClawWallWork *w)
{
    Vec3 pl;

    eden_claw_wall_grow(w);
    player_get_pos(&pl);
    v3_sub(&w->vel, &pl, &w->pos);
    if (cmd_cl_wall_vm < w->spd)
        w->spd = w->spd * cmd_cl_wall_vr + cmd_cl_wall_vm * (1.0f - cmd_cl_wall_vr);
    vec_set_length(&w->vel, w->spd);
    v3_add(&w->pos, &w->vel);
    i3_add(w->ang, w->spin);
    v3_sub(&pl, &w->pos, &pl);
    if (vec3_length(&pl) < w->spd)
        eden_player_damage();
    else if (--w->time > 0 && !eden_get_lock_off()) {
        w->frm_to = w->parts[19]->frames * (1.0f - (float)w->lock.life / (float)cmd_cl_wall_hp);
        if (!(w->lock.flags & LOCK_HIT))
            return;
        if (w->lock.life > 0)
            goto damaged;
        lock_score(9, &w->lock);
    }
    boss_se_play(29, (int)&w->lock, 0);
    cm_wire_break_simple(w, 2, w->parts[20], &w->pos, &w->vel, w->ang, eden_claw_col);
    FINISH_IF(w);
    return;
damaged:
    boss_se_play(28, (int)&w->lock, 0);
}

/* 0x00cacf08 */
void eden_claw_wall_disp(EdenClawWallWork *w)
{
    int old_blend = draw_set_override_flags(0);

    ulGxSetTextureList(w->tex_list);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    eden_claw_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (!w->state)
        model_object_draw(w->parts[18], w->frm);
    else
        model_object_draw(w->parts[19], w->frm);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00cacfb8 */
void eden_claw_wall_release(EdenClawWallWork *w)
{
    lock_remove_entry(9, &w->lock);
}

/* 0x00cacfc8 */
EdenClawWallWork *eden_claw_wall_create(void *parent, int arg, void *pos, void *ang, void *spin, int n)
{
    EdenClawWallWork *w = task_create_inherit(parent, &eden_claw_wall_desc, 164);
    int frm;
    void *hp;

    v3_copy(&w->pos, pos);
    i3_copy(w->spin, spin);
    v3_copy(&w->start, pos);
    {
        int *dst = w->ang;
        int *src = ang;

        w->ang[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }
    frm = (int)(cmd_cl_wall_t * FPS);
    w->spd = cmd_cl_wall_v;
    hp = *(void **)&cmd_cl_wall_hp;
    *(void **)&w->parts = (void *)arg; /* steer(alias) */
    w->time = w->len = frm;
    lock_init(9, &w->lock, w, &w->pos, 0, (int)hp, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    boss_se_play(27, (int)&w->lock, 0);
    return w;
}

/* 0x00cad120 */
void eden_claw_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
