/*
 * Venus boss missile pods and floor attacks: 48 pods in rows on four model nodes fire homing
 * bullets as the player passes their side; the plate manager throws lockable plates from the floor
 * and sends shock rings lit through vertex colours. Also capsule lines and the score lock table.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "color.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_plate.h"
#include "venus.h"
#include "venus_bullet.h"
#include "venus_mpod.h"
#include "venus_shell.h"
#include "venus_thunder.h"
#include "../common/boss_common.h"

extern BossLockDef score_lock_defs[];
extern char score_vardef[];
extern GwVarDef score_vardefs[];
extern char venus_m_vardef[];
extern GwVarDef venus_m_vardefs[];
extern int score_vars_group;
extern struct GwVarNode *venus_m_vars_group;
extern char venus_plate_vardef[];

/* missile pod, 160 bytes: lock-on target on springs, recoils on firing, turns when hit */
typedef struct VenusMpodPod {
    Vec3 base;                  /* rest position */
    Vec3 pos;                   /* 0x0c: base plus the spring offsets (venus_mpod_exec) */
    int ang[3];                 /* 0x18: [0] from turn, [1] the yaw */
    char pad_24[12];
    LockObj lock;                  /* 0x30 */
    float vel_y;               /* 0x70 */
    float ofs_y;                /* 0x74: spring offset added to base.y (vel_y is its velocity) */
    float dist;                 /* 0x78: distance from base along the pod's yaw */
    float recoil_vel;           /* 0x7c: kicked by venus_mpod_fire_kick */
    float recoil;               /* 0x80: spring offset added to dist */
    float turn_vel;             /* 0x84 */
    float turn;                 /* 0x88: degrees; springs to turn_to, gives ang[0] */
    float turn_to;              /* 0x8c: +120 on each shot or hit */
    int fire_timer;             /* 0x90: frames to the next shot */
    int alive;                 /* 0x94: the lock-on record is registered */
    char pad_98[8];
} VenusMpodPod;

extern GwVarDef venus_plate_vardefs[];
extern struct GwVarNode *venus_plate_vars_group;
extern VenusParam plate_set_data[];
extern int iset_data;
extern float set_data_len;
extern float set_data_fx;
extern float set_data_sec;
extern int set_data_x;
extern int set_data_z;
extern unsigned int venus_pod_wire_col;
extern struct TaskDesc plate_man_desc;
extern struct TaskDesc cm_vcc_man_desc;
extern char str_venus_plate_c[];
extern struct TaskDesc cm_vcc_man2_desc;
extern float cmd_plate_wait;
extern const float cmd_plate_y;
extern int plate_man_shot_num;
extern int cm_vcc_man_col_init;
extern float venus_mpod_fire_sec;
extern float plate_fade[4];
extern float plate_fade_sec;
extern unsigned int cm_vcc_man_col_full;
extern struct TaskDesc motion_t_desc;
extern float venus_mpod_ang_damp;
extern float venus_mpod_ang_k;
extern float venus_mpod_spring_damp;
extern float venus_mpod_spring_k;
extern float venus_mpod_side_ang;
extern float venus_mpod_ofs_max;
extern float venus_mpod_r_min;
extern float venus_mpod_ofs_spd;
extern float venus_mpod_ofs_y;
extern float venus_mpod_space;

/* 48 bytes, copied whole */
typedef struct VenusMpodPoints {
    Vec3 p[4];
} VenusMpodPoints;

extern int venus_mpod_nodes[];
extern float venus_mpod_row_gap;
extern float venus_mpod_push_rate;
extern int cmd_pod_bonus;
extern int venus_mpod_test_fire;
extern int venus_mpod_test_idx;
extern struct TaskDesc venus_mpod_desc;
extern char str_venus_mpod_c[];
extern int cmd_pod_hp;
extern float cmd_fish_seq_t;
extern float venus_mpod_fire_space;
extern float venus_mpod_fire_kick;
extern float venus_mpod_pass_r;
extern float venus_mpod_pass_sec;
extern float cmd_plate_sr;
extern float plate_man_shot_sec;
extern float plate_man_circle_r;
extern float plate_man_circle_sec;
extern float plate_man_oscilate_x;
extern float plate_man_oscilate_y;
extern float cmd_plate_e;
extern float cmd_plate_g;
extern float cmd_plate_vl;
extern float cmd_plate_vlr;
extern const float cmd_plate_r;
extern float cmd_plate_rr;
extern float cmd_plate_rvr;
extern float cmd_item_rate;
extern unsigned int plate_wire_col;
extern struct TaskDesc plate_desc;
extern float cmd_plate_rz;
extern float cmd_plate_drz;
extern float cmd_plate_vy;
extern float cmd_plate_dvy;
extern float cmd_plate_vxz;
extern float cmd_plate_dvxz;
extern const float cmd_plate_t;
extern struct TaskDesc plate_circle_desc;
extern float plate_circle_spawn_sec;
extern int plate_circle_spawn_num;
extern float plate_circle_obj_scl;
extern float plate_circle_wave_w;
extern float plate_circle_spawn_r;
extern float plate_circle_pl_dis;
extern float plate_circle_ang_rnd;
extern float plate_circle_ang;
extern float plate_circle_blur_sec;
extern float plate_circle_blur_margin;
extern float plate_circle_blur_alpha;
extern float plate_circle_blur_dis;

/* venus_mpod_desc, 84 bytes: 48 missile pods in four rows on Venus's body */
typedef struct VenusMpodWork {
    void *models;
    void *textures;             /* 0x04 */
    void *parent;               /* 0x08 */
    int *gb_man;                /* 0x0c: the bullet manager */
    VenusMpodPod *pods;         /* 0x10 */
    Vec3 *rows;                 /* 0x14: row anchors on the body, this frame */
    Vec3 *rows_prev;            /* 0x18 */
    Vec3 *rows_d;               /* 0x1c: their movement */
    ModelPart **parts;          /* 0x20 */
    ModelPart *body;            /* 0x24 */
    int pass;                   /* 0x28: 0 out, 1 inside venus_mpod_pass_r, 2 cooling down */
    int pass_timer;             /* 0x2c */
    int live;                   /* 0x30: pods still alive */
    char pad_34[4];
    int broken;                 /* 0x38: venus_mpod_break_all ran */
    int seq_timer;              /* 0x3c */
    float frame;                /* 0x40: body frame the rows were posed at */
    int row_out[4];             /* 0x44: row slides out */
} VenusMpodWork;

/* 0x00ca8430 */
void venus_line_set_color(int *vtx, int col)
{
    int i;

    for (i = 4; i; i--, vtx += 6)
        vtx[5] = col;
}

/* 0x00ca8460 */
void venus_line_draw_capsules(int pts, int num, int tex, unsigned int color, float scale, float r_min, float r_max)
{
    int buf[24];
    Vec3 *verts = (Vec3 *)pts;
    int i;

    venus_line_set_color(buf, color);
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    nj_tex_strip_begin(1);
    for (i = 0; i < num - 1; i++)
        venus_line_capsule(&verts[i], &verts[i + 1], buf, scale, r_min, r_max);
    nj_tex_strip_end();
}

/* 0x00ca8538 */
int venus_line_seg(Vec3 *p0, Vec3 *p1, int *buf, float scale, float r_min, float r_max)
{
    Vec3 scr0, scr1;
    TaVtx *vtx = (TaVtx *)buf;
    float rad_a, rb, dx, dy, len_sq;
    float dyp, dym, dxm, dxp;

    ulMatrixApplyPoint((float *)ulMatrixCur, &p0->x, &scr0.x);
    ulMatrixApplyPoint((float *)ulMatrixCur, &p1->x, &scr1.x);
    if (!segment_on_screen(&scr0, &scr1))
        goto fail;
    rad_a = scale * venus_line_project(&scr0, &scr0);
    rb = scale * venus_line_project(&scr1, &scr1);
    if (rad_a < r_min)
        rad_a = r_min;
    else if (rad_a > r_max)
        rad_a = r_max;
    if (rb < r_min)
        rb = r_min;
    else if (rb > r_max)
        rb = r_max;
    dx = scr1.x - scr0.x;
    dy = scr1.y - scr0.y;
    len_sq = dx * dx + dy * dy;
    if (fabsf(len_sq) < FLT_EPSILON) {
        rad_a *= 0.5f;
        ((TaVtx *)buf)->x = scr0.x - rad_a;
        ((TaVtx *)buf)->y = scr0.y - rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 0.0f;
        ((TaVtx *)buf)->v = 0.0f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x + rad_a;
        ((TaVtx *)buf)->y = scr0.y - rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 0.0f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x - rad_a;
        ((TaVtx *)buf)->y = scr0.y;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 0.0f;
        ((TaVtx *)buf)->v = 0.25f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x + rad_a;
        ((TaVtx *)buf)->y = scr0.y;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 0.25f;
        nj_tex_strip_vertices(vtx, 4);
        return 1;
    } else {
        len_sq = 1.0f / sqrtf(len_sq);
        dx *= len_sq;
        dy *= len_sq;
        dym = dy - dx * 0.5f;
        dyp = dy + dx * 0.5f;
        dxp = dx + dy * 0.5f;
        dxm = dx - dy * 0.5f;
        ((TaVtx *)buf)->x = scr0.x - dy * rad_a;
        ((TaVtx *)buf)->y = scr0.y + dx * rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 0.0f;
        ((TaVtx *)buf)->v = 0.25f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x + dy * rad_a;
        ((TaVtx *)buf)->y = scr0.y - dx * rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 0.25f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x - dyp * rad_a;
        ((TaVtx *)buf)->y = scr0.y + dxm * rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 0.0f;
        ((TaVtx *)buf)->v = 0.0f;
        buf += 6;
        ((TaVtx *)buf)->x = scr0.x + dym * rad_a;
        ((TaVtx *)buf)->y = scr0.y - dxp * rad_a;
        ((TaVtx *)buf)->q = scr0.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 0.0f;
        nj_tex_strip_vertices(vtx, 4);
        buf -= 6;
        ((TaVtx *)buf)->x = scr1.x - dy * rb;
        ((TaVtx *)buf)->y = scr1.y + dx * rb;
        ((TaVtx *)buf)->q = scr1.z;
        ((TaVtx *)buf)->u = 0.0f;
        ((TaVtx *)buf)->v = 0.75f;
        buf += 6;
        ((TaVtx *)buf)->x = scr1.x + dy * rb;
        ((TaVtx *)buf)->y = scr1.y - dx * rb;
        ((TaVtx *)buf)->q = scr1.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 0.75f;
        nj_tex_strip_vertices(vtx, 4);
        return 1;
    }
fail:
    return 0;
}

/* 0x00ca88b8 */
void venus_line_draw(int pts, int num, int tex, unsigned int color, float scale, float r_min, float r_max)
{
    int buf[24];
    Vec3 *verts = (Vec3 *)pts;
    int i;

    venus_line_set_color(buf, color);
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    nj_tex_strip_begin(1);
    for (i = 0; i < num - 1; i++) {
        if (i == 0)
            venus_line_seg(&verts[0], &verts[1], buf, scale, r_min, r_max);
        else if (i == num - 2)
            venus_line_capsule(&verts[i + 1], &verts[i], buf, scale, r_min, r_max);
        else
            venus_line_seg(&verts[i], &verts[i + 1], buf, scale, r_min, r_max);
    }
    nj_tex_strip_end();
}

/* steer(cast) */
/* 0x00ca89f0 */
void score_vars_add(void)
{
    lock_table_set(score_lock_defs, 11);
    score_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, score_vardef);
    gw_varedit_add_table(&gw_varedit_root, score_vars_group, score_vardefs);
}

/* 0x00ca8a50 */
void score_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)score_vardef);
}

/* 0x00ca8a68 */
void venus_mpod_vars_add(void)
{
    venus_m_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_m_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_m_vars_group, venus_m_vardefs);
}

/* 0x00ca8ab8 */
void venus_mpod_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_m_vardef);
}

/* 0x00ca8ad0 */
void venus_mpod_place(VenusMpodWork *w)
{
    Vec3 pos, step;
    int ang[3];
    Vec3 back;
    float sc[4];
    int i, j, k;
    VenusMpodPod *pod;
    int *row_flags = w->row_out;

    for (i = 0; i < 4; i++) {
        int *row_flag = row_flags + i;

        for (j = 0; j < 3; j++) {
            pos.z = venus_mpod_r_min;
            pos.x = 0.0f;
            pos.y = w->rows[i].y + venus_mpod_ofs_y;
            ang[1] = DEG_TO_ANG(venus_mpod_side_ang + j * 360.0f / 3.0f);
            ROT_Y(ang[1], sc, pos.x, pos.z);
            step.x = venus_mpod_space;
            step.y = step.z = 0.0f;
            ROT_Y(ang[1], (sc + 2), step.x, step.z);
            v3_mul(&back, &step, 1.5f);
            v3_sub(&pos, &pos, &back);
            for (k = 0; k < 4; k++) {
                pod = &w->pods[i * 12 + j * 4 + k];
                v3_copy(&pod->base, &pos);
                pod->ang[1] = ang[1];
                v3_add(&pos, &step);
                if (*row_flag && pod->dist < venus_mpod_ofs_max - venus_mpod_r_min)
                    pod->dist += venus_mpod_ofs_spd;
            }
        }
    }
}

/* 0x00ca8dc0 */
void venus_mpod_row_push(VenusMpodWork *w, int row, float dvy)
{
    VenusMpodPod *pod = w->pods + row * 12;
    int k;

    for (k = 12; k; k--, pod++)
        pod->vel_y += dvy;
}

/* 0x00ca8e00 */
void venus_mpod_fire(VenusMpodWork *w, VenusMpodPod *pod)
{
    Vec3 pos, dir, off;
    float sc[4];
    float live_rate;
    int i;

    pod->recoil_vel += venus_mpod_fire_kick;
    pod->turn_to += 120.0f;
    dir.z = 1.0f;
    dir.x = dir.y = 0.0f;
    ROT_Y(pod->ang[1], sc, dir.x, dir.z);
    off.x = venus_mpod_fire_space;
    off.z = 0.0f;
    off.y = 0.0f;
    ROT_Y(pod->ang[1], (sc + 2), off.x, off.z);
    v3_sub(&pos, &pod->pos, &off);
    live_rate = (float)w->live / 48.0f;
    for (i = 0; i < 2; i++) {
        venus_gb_create(w->gb_man, &pos, &dir, (char *)w->parts, 0.0f, live_rate);
        v3_sum(&pos, &pos, &off);
    }
    boss_se_play(17, 0, (int)&pos);
}

/* 0x00ca8fc0 */
void venus_mpod_set_fire(VenusMpodWork *w, int side)
{
    VenusMpodPod *pod;
    int i, k;

    for (k = 0; k < 4; k++) {
        pod = &w->pods[k * 12 + side * 4];
        for (i = 0; i < 4; i++, pod++) {
            if (pod->alive)
                pod->fire_timer = RAND_FLOAT_A() * venus_mpod_fire_sec * FPS;
        }
    }
}

/* 0x00ca90c0 */
void venus_mpod_pass_check(VenusMpodWork *w)
{
    Vec3 pl;
    int ang[3];
    Vec3 dir;
    int i;
    float sc[2];

    switch (w->pass) {
    case 0:
        player_get_pos(&pl);
        if (vec3_length(&pl) < venus_mpod_pass_r)
            w->pass++;
        break;
    case 1:
        player_get_pos(&pl);
        if (vec3_length(&pl) > venus_mpod_pass_r) {
            for (i = 0; i < 3; i++) {
                ang[1] = DEG_TO_ANG(venus_mpod_side_ang + i * 360.0f / 3.0f);
                dir.x = dir.y = 0.0f;
                dir.z = 1.0f;
                ROT_Y(ang[1], sc, dir.x, dir.z);
                vec3_normalize(&pl, &pl);
                if (ulVec3Dot(&pl, &dir) > 0.5f)
                    break;
            }
            venus_mpod_set_fire(w, i);
            w->pass++;
            w->pass_timer = venus_mpod_pass_sec * FPS;
        }
        break;
    case 2:
        if (--w->pass_timer <= 0)
            w->pass = 0;
        break;
    }
}

/* 0x00ca9318 */
void venus_mpod_move(VenusMpodWork *w)
{
    VenusMpodPod *pod = w->pods;
    float sc[2];
    float x, z;
    int i;

    for (i = 48; i != 0; i--, pod++) {
        x = 0.0f;
        if (pod->alive) {
            spring_step(&pod->vel_y, &pod->ofs_y, venus_mpod_spring_damp, venus_mpod_spring_k);
            spring_step(&pod->recoil_vel, &pod->recoil, venus_mpod_spring_damp, venus_mpod_spring_k);
            spring_to(&pod->turn_vel, &pod->turn, pod->turn_to, venus_mpod_ang_damp, venus_mpod_ang_k);
            pod->ang[0] = DEG_TO_ANG(pod->turn);
            pod->pos.y = pod->base.y + pod->ofs_y;
            z = pod->dist + pod->recoil;
            ROT_Y(pod->ang[1], sc, x, z);
            pod->pos.x = pod->base.x + x;
            pod->pos.z = pod->base.z + z;
            if (pod->fire_timer) {
                if (--pod->fire_timer <= 0)
                    venus_mpod_fire(w, pod);
            }
        }
    }
}

/* 0x00ca94b0 */
void venus_mpod_exec(VenusMpodWork *w)
{
    VenusMpodPod *pod;
    Vec3 prev_d, delta;
    int i;
    int n;

    if (w->frame != w->body->frame) {
        *(VenusMpodPoints *)w->rows_prev = *(VenusMpodPoints *)w->rows;
        w->frame = w->body->frame;
        for (i = 0; i < 4; i++)
            tree_node_pose((WireObj *)w->body->root, (void *)w->body->clip, w->rows + i, 0, 0,
                                   venus_mpod_nodes[i], w->body->frame);
        w->row_out[0] = 1;
        venus_mpod_place(w);
    }
    for (i = 0; i < 4; i += 1 + ((i & 1) >> 1)) { /* steer(zero) */
        v3_copy(&prev_d, &w->rows_d[i]);
        v3_sub(&w->rows_d[i], &w->rows[i], &w->rows_prev[i]);
        v3_sub(&delta, &w->rows_d[i], &prev_d);
        if (!(fabsf(delta.y) < FLT_EPSILON))
            venus_mpod_row_push(w, i, delta.y * venus_mpod_push_rate);
        if (i != 0 && fabsf(w->rows[i - 1].y - w->rows[i].y) > venus_mpod_row_gap)
            w->row_out[i] = 1;
    }
    venus_mpod_move(w);
    if (venus_mpod_test_fire) {
        venus_mpod_test_fire = 0;
        venus_mpod_fire(w, w->pods + venus_mpod_test_idx);
    }
    venus_mpod_pass_check(w);
    if (w->seq_timer)
        w->seq_timer--;
    w->live = 0;
    pod = w->pods;
    for (n = 0; n < 48; n++, pod++) {
        if (pod->alive) {
            if (pod->lock.flags & LOCK_HIT) {
                if (pod->lock.life <= 0) {
                    boss_se_play(19, (int)&pod->lock, 0);
                    lock_score(2, &pod->lock);
                    cm_wire_break_simple(w, 2, w->parts[10], &pod->pos, 0, &pod->ang[0], venus_pod_wire_col);
                    lock_remove_entry(2, &pod->lock);
                    pod->alive = 0;
                } else {
                    boss_se_play(18, (int)&pod->lock, 0);
                    pod->turn_to += 120.0f;
                }
            }
            w->live++;
        }
    }
    if (w->seq_timer) {
        if (w->live)
            return;
        if (!w->broken)
            player_status_add_score_event(lock_get_type(2), cmd_pod_bonus);
    }
    TASK_OF(w)->exec = 0;
    venus_seq_regist(w->parent, 2, 3);
}

/* 0x00ca9878 */
void venus_mpod_disp(VenusMpodWork *w)
{
    VenusMpodPod *pod;
    int old_blend;
    int i;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    pod = w->pods;
    for (i = 48; i != 0; i--, pod++) {
        if (pod->alive) {
            ulMatrixPush();
            MTX_TRANS(&pod->pos);
            venus_mpod_rotate_yxz(&pod->ang[0]);
            dc_object_draw(w->parts[9]);
            ulMatrixPop();
        }
    }
    draw_set_override_flags(old_blend);
}

/* 0x00ca9940 */
void venus_mpod_release(VenusMpodWork *w)
{
    VenusMpodPod *pod = w->pods;
    int i;

    for (i = 0; i < 48; i++, pod++) {
        if (pod->alive)
            lock_remove_entry(2, &pod->lock);
    }
    heap_free(w->pods, 0);
    heap_free(w->rows, 0);
}

/* 0x00ca99b0 */
void *venus_mpod_create(void *parent, int gb_man, Vec3 *pos, int *parts)
{
    VenusMpodWork *w = task_create_inherit(parent, &venus_mpod_desc, 84);
    VenusMpodPod *pod;
    Vec3 *pts;
    int i;

    w->body = (ModelPart *)parts[6];
    w->parts = (ModelPart **)parts;
    w->parent = parent;
    w->gb_man = (int *)gb_man;
    w->frame = 0;
    w->seq_timer = cmd_fish_seq_t * FPS;
    w->pods = heap_block_tag(game_calloc(7680), str_venus_mpod_c, 456);
    pts = heap_block_tag(game_calloc(192), str_venus_mpod_c, 457);
    w->rows_prev = pts + 4;
    w->rows_d = pts + 8;
    w->rows = pts;
    for (i = 0; i < 4; i++) {
        tree_node_pose((WireObj *)w->body->root, (void *)w->body->clip, w->rows + i, 0, 0,
                               venus_mpod_nodes[i], 0.0f);
        v3_copy(w->rows_prev + i, w->rows + i);
    }
    pod = w->pods;
    for (i = 0; i < 48; i++, pod++) {
        lock_init(2, &pod->lock, w, &pod->pos, 0, cmd_pod_hp, 0);
        lockon_entry(&pod->lock);
        pod->alive = 1;
    }
    return w;
}

/* 0x00ca9b60 */
void venus_mpod_break_all(VenusMpodWork *w)
{
    VenusMpodPod *pod;
    int i;

    w->broken = 1;
    pod = w->pods;
    for (i = 0; i < 48; i++, pod++) {
        if (pod->alive) {
            cm_wire_break_simple(w, 2, w->parts[10], &pod->pos, 0, &pod->ang[0], venus_pod_wire_col);
            lock_remove_entry(2, &pod->lock);
            pod->alive = 0;
        }
    }
}

/* 0x00ca9c00 */
void venus_mpod_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00ca9c50 */
void venus_plate_set_save(void)
{
    VenusParam *set = &plate_set_data[iset_data];

    set->x = set_data_sec;
    set->y = set_data_len;
    set->z = set_data_fx;
    set->data_x = set_data_x;
    set->data_z = set_data_z;
}

/* 0x00ca9cb0 */
void venus_plate_set_load(void)
{
    VenusParam *set = &plate_set_data[iset_data];

    set_data_sec = set->x;
    set_data_len = set->y;
    set_data_fx = set->z;
    set_data_x = set->data_x;
    set_data_z = set->data_z;
}

/* 0x00ca9d10 */
void venus_plate_vars_add(void)
{
    venus_plate_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_plate_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_plate_vars_group, venus_plate_vardefs);
    venus_plate_set_load();
}

/* 0x00ca9d68 */
void venus_plate_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_plate_vardef);
}

/* 148 bytes */
typedef struct VenusPlateWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 vel;
    int unk_20[3];                      /* 0x20: copied from the creation angles, never read */
    int dang[3];
    int ang[3];
    char *models;
    LockObj lock;
    int timer;
    int fading;
    int parent;
} VenusPlateWork;

/* 0x00ca9d80 */
void plate_exec(VenusPlateWork *w)
{
    Spawn spawn;
    int n;
    float vy, lim;

    for (n = enemy_status_get_ticks(); n; n--) {
        vy = w->vel.y + cmd_plate_g;
        lim = cmd_plate_vl;
        w->vel.y = vy;
        if (vy < -lim)
            w->vel.y = vy * cmd_plate_vlr - lim * (1.0f - cmd_plate_vlr);
        else if (vy > lim)
            w->vel.y = vy * cmd_plate_vlr + lim * (1.0f - cmd_plate_vlr);
        v3_add(&w->pos, &w->vel);
        if (w->pos.y < cmd_plate_y && w->vel.y < 0.0f) {
            w->vel.y = fabsf(w->vel.y) * cmd_plate_e;
            if (w->vel.y > cmd_plate_r) {
                if (!(int)(RAND_FLOAT_A() * 2.0f))
                    w->dang[2] = w->dang[2] * cmd_plate_rvr;
                else
                    w->dang[2] = w->dang[2] * -cmd_plate_rvr;
                i3_sum(w->ang, w->ang, w->dang);
            } else {
                w->pos.y = cmd_plate_y;
                w->ang[2] = 0;
            }
        } else {
            w->dang[2] = w->dang[2] * cmd_plate_rr;
            i3_sum(w->ang, w->ang, w->dang);
        }
        if (--w->timer <= 0) {
            FINISH_IF(w);
            return;
        }
    }
    if (w->fading)
        return;
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
        Vec3 *pos = &w->pos;

        boss_se_play(2, (int)&w->lock, 0);
        lock_score(1, &w->lock);
        cm_wire_break_simple(w, 2, *(ModelPart **)(w->models + 48), pos, &w->vel, w->ang, plate_wire_col);
        if (!(int)(RAND_FLOAT_A() * cmd_item_rate)) {
            spawn.x = pos->x;
            spawn.y = pos->y;
            spawn.z = pos->z;
            spawn.vel_x = 0;
            spawn.vel_y = 0;
            spawn.vel_z = 0;
            spawn.scale = 0.9f;
            spawn.kind = 1;
            item_create(&spawn);
        }
        FINISH_IF(w);
    }
    if ((float)w->timer < plate_fade_sec * FPS) {
        (w->fading)++;
        w->lock.flags &= ~LOCK_LOCKABLE;
    }
}

/* 0x00caa190 */
void plate_release(VenusPlateWork *w)
{
    lock_remove_entry(1, &w->lock);
}

/* steer(cast) */
/* 0x00caa1a0 */
void plate_disp(VenusPlateWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    vu0_matrix_rotate_y(ulMatrixCur, w->ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, w->ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, w->ang[2]);
    vu0_matrix_rotate_x(ulMatrixCur, -w->ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -w->ang[1]);
    if (!w->fading) {
        dc_object_draw_us(*(ModelPart **)(w->models + 44));
    } else {
        old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        draw_set_override_color(plate_fade);
        plate_fade[0] = (float)w->timer / (plate_fade_sec * FPS) - 1.0f;
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        dc_object_draw_us(*(ModelPart **)(w->models + 44));
        draw_set_override_flags(old_blend);
        draw_pop_strip_flags();
        draw_pop_override_color();
    }
    ulMatrixPop();
}

/* 0x00caa2f0 */
int *plate_create(int parent, Vec3 *pos, int *ang, int model)
{
    VenusPlateWork *w = task_create_inherit((void *)parent, &plate_desc, 148);
    Vec3 up, axis;
    float inv_len, x, z, unit;

    v3_copy(&w->pos, pos);
    i3_copy(w->unk_20, ang);
    do {
        w->parent = parent;
    } while (0);
    w->models = (char *)model;
    up.x = up.z = 0.0f;
    unit = 1.0f;
    do {
        up.y = unit;
    } while (0);
    ulVec3Cross(&axis, pos, &up);
    do {
        w->ang[1] = RAD_TO_ANG(atan2f(-axis.x, -axis.z));
        w->ang[0] = RAD_TO_ANG(atan2f(axis.y, sqrtf(axis.x * axis.x + axis.z * axis.z)));
    } while (0);
    w->dang[2] = DEG_TO_ANG(cmd_plate_rz + RAND_FLOAT_A() * cmd_plate_drz);
    w->vel.y = cmd_plate_vy + RAND_FLOAT_A() * cmd_plate_dvy;
    x = pos->x;
    z = pos->z;
    inv_len = x * x + z * z;
    inv_len = unit / sqrtf(inv_len);
    inv_len *= cmd_plate_vxz;
    w->vel.x = x * inv_len + cmd_plate_dvxz * RAND_FLOAT_A();
    w->vel.z = z * inv_len + cmd_plate_dvxz * RAND_FLOAT_A();
    w->timer = cmd_plate_t * FPS;
    lock_init(1, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    return w;
}

/* motion_t_desc, 124 bytes: plays a model's motion once at a fixed place */
typedef struct MotionTWork {
    void *models;
    void *textures;             /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    int ang[3];                 /* 0x14 */
    ModelPart obj;              /* 0x20 */
} MotionTWork;

/* 0x00caa600 */
void motion_t_exec(MotionTWork *w)
{
    w->obj.frame += w->obj.speed;
    if (w->obj.frames < w->obj.frame)
        FINISH_IF(w);
}

/* 0x00caa640 */
void motion_t_disp(MotionTWork *w)
{
    void *old_fog;

    ulGxSetTextureList(w->textures);
    old_fog = fog_set_enable(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_plate_rotate_yxz(w->ang);
    dc_object_draw((struct MapPart *)&w->obj);
    fog_set_enable(old_fog);
    ulMatrixPop();
}

/* 0x00caa6b8 */
int *motion_t_create(void *parent, ModelPart *src, Vec3 *pos, int *ang)
{
    MotionTWork *w = task_create_inherit(parent, &motion_t_desc, 124);

    if (pos)
        v3_copy(&w->pos, pos);
    if (ang)
        i3_copy(w->ang, ang);
    w->obj = *src;
    return (int *)w;
}

/* 0x00caa828 */
void plate_man_spawn(PlateManWork *w, VenusParam *set)
{
    Vec3 pl, fw, side;
    int ang[3];
    Vec3 pos;
    int i, j;

    player_get_pos(&pl);
    player_get_vec(&fw);
    vec_set_length(&fw, set->y);
    do {
        side.x = side.z = 0.0f;
        side.y = 1.0f;
    } while (0);
    ulVec3Cross(&side, &side, &fw);
    vec_set_length(&side, set->z);
    v3_sum(&fw, &fw, &side);
    v3_sum(&pl, &fw, &pl);
    ang[0] = 0;
    ang[1] = 0;
    ang[2] = 0;
    for (i = 0; i < set->data_z; i++) {
        for (j = 0; j < set->data_x; j++) {
            pos.y = cmd_plate_y;
            pos.x = pl.x + cmd_plate_sr * j;
            pos.z = pl.z + cmd_plate_sr * i;
            plate_create((int)w->parent, &pos, ang, w->obj);
        }
    }
}

/* 0x00caa9f8 */
void plate_man_exec(PlateManWork *w)
{
    int org[3];
    void *boss_pos, *boss_ang;

    if (--w->frames <= 0 && beat_grid_hit(0)) {
        w->frames = plate_man_shot_sec * FPS;
        peri_act_play_number(1);
        oscilate_create(plate_man_oscilate_x, plate_man_oscilate_y);
        org[0] = 0;
        org[1] = 0;
        org[2] = 0;
        boss_se_play(3, 0, (int)org);
        th_ball2_create(w);
        venus_get_pos_ang(w->parent, &boss_pos, &boss_ang);
        motion_t_create(w->parent, ((ModelPart **)w->obj)[35], boss_pos, boss_ang);
        if (--w->shot_num <= 0) {
            plate_circle_create((int)w->pts, w->obj, (int)w->pts, 1, plate_man_circle_r, 0.9f, plate_man_circle_sec);
            TASK_OF(w)->exec = 0;
        } else {
            plate_circle_create((int)w->pts, w->obj, (int)w->pts, 0, plate_man_circle_r, 0.9f, plate_man_circle_sec);
        }
    }
    if (--w->frames2 <= 0) {
        plate_man_spawn(w, &w->tab[w->set_idx]);
        w->set_idx++;
        w->frames2 = w->tab[w->set_idx].x * FPS;
        if (w->frames2 < 0)
            FINISH_IF(w);
    }
}

/* 0x00caabb8 */
PlateManWork *plate_man_create(void *parent, int *parts)
{
    PlateManWork *w = task_create_inherit(parent, &plate_man_desc, 52);
    VenusParam *set = plate_set_data;

    w->plate_y = cmd_plate_y;
    w->parent = parent;
    w->obj = (int)parts;
    w->frames = cmd_plate_wait * FPS;
    w->set_idx = 0;
    w->frames2 = set->x * FPS;
    w->shot_num = plate_man_shot_num;
    w->tab = set;
    w->pts = cm_vcc_man_create(w, ((int **)parts)[26]);
    return w;
}

/* plate_circle_desc, 164 bytes: a ring that spreads across the floor and drops plates */
typedef struct PlateCircleWork {
    void *models;
    void *textures;             /* 0x04 */
    char pad_08[12];
    int frm_max;                /* 0x14 */
    int frm;                    /* 0x18 */
    char pad_1c[4];
    float r_max;                /* 0x20 */
    float y;                    /* 0x24 */
    float rate;                 /* 0x28: 0 to 1 over its life */
    int spawn_timer;            /* 0x2c */
    int spawn_left;             /* 0x30 */
    int fill;                   /* 0x34: colour inside the ring too */
    int blurred;                /* 0x38 */
    int model;                  /* 0x3c */
    int parent;                 /* 0x40 */
    CmVccManWork *floor;        /* 0x44: floor vertex colours */
    ModelPart obj;              /* 0x48 */
} PlateCircleWork;

/* 0x00caac70 */
void plate_circle_exec(PlateCircleWork *w)
{
    Vec3 pl, pos;
    int ang[3];
    float sc[2];
    float pl_dis, t, ang_deg;

    player_get_pos(&pl);
    pl_dis = vec3_length(&pl);
    t = 1.0f - (float)w->frm / (float)w->frm_max;
    w->rate = t;
    if (--w->frm <= 0) {
        FINISH_IF(w);
        if (w->fill)
            cm_vcc_man_set_col(w->floor, COL_WHITE);
        return;
    }
    if (--w->spawn_timer <= 0 && w->spawn_left) {
        if (w->r_max * t > plate_circle_spawn_r && fabsf(pl_dis - w->r_max * t) > plate_circle_pl_dis) {
            pos.y = cmd_plate_y;
            pos.z = -w->r_max * t;
            ang[0] = 0;
            ang[1] = 0;
            ang[2] = 0;
            pos.x = 0.0f;
            ang_deg = plate_circle_ang + (RAND_FLOAT_A() * (plate_circle_ang_rnd + plate_circle_ang_rnd) - plate_circle_ang_rnd);
            ROT_Y(DEG_TO_ANG(ang_deg), sc, pos.x, pos.z);
            plate_create(w->parent, &pos, ang, w->model);
            w->spawn_timer = plate_circle_spawn_sec * RAND_FLOAT_A() * FPS;
            w->spawn_left--;
        } else {
            w->spawn_timer = 0;
        }
    }
    if (w->fill)
        cm_vcc_man_wave_fill(w->floor, w->r_max * w->rate, plate_circle_wave_w);
    else
        cm_vcc_man_wave(w->floor, w->r_max * w->rate, plate_circle_wave_w);
    if (!w->blurred && pl_dis < w->r_max * w->rate + plate_circle_blur_dis) {
        w->blurred = 1;
        cm_sc_blur_create(w, plate_circle_blur_margin, plate_circle_blur_alpha, 0.0f, plate_circle_blur_sec);
    }
    dc_object_advance_time((struct MapPart *)&w->obj);
}

/* 0x00caaf60 */
void plate_circle_disp(PlateCircleWork *w)
{
    ulGxSetTextureList(w->textures);
    dc_object_draw((struct MapPart *)&w->obj);
}

/* 0x00caaf90 */
int *plate_circle_create(int parent, int model, int pts, int fill, float r_max, float y, float sec)
{
    PlateCircleWork *w = task_create_inherit((void *)(parent + ((parent & 1) >> 1)), &plate_circle_desc, 164); /* steer(zero) */
    ModelPart *src = ((ModelPart **)model)[30];

    w->model = model + ((model & 1) >> 1); /* steer(zero) */
    w->obj = *src;
    w->obj.speed = plate_circle_obj_scl;
    w->parent = parent;
    w->r_max = r_max;
    w->y = y;
    w->floor = (CmVccManWork *)pts;
    w->fill = fill;
    w->frm_max = w->frm = sec * FPS;
    {
        int rnd = rand_lcg_next_a();
        float spawn_sec = plate_circle_spawn_sec;
        w->spawn_left = plate_circle_spawn_num;
        w->spawn_timer = spawn_sec * (rnd / 32769.0f) * FPS;
    }
    return (int *)w;
}

/* 0x00cab1a0 */
short cm_vcc_man_pts_get(int *w, char **pts, float *lens)
{
    char *mdl = *(char **)w[1];
    int n = *(short *)(mdl + 6);
    char *vtx = mdl + 8;

    for (; n; n--) {
        *pts = vtx;
        *lens = sqrtf(((float *)vtx)[0] * ((float *)vtx)[0] + ((float *)vtx)[2] * ((float *)vtx)[2]);
        vtx += 28;
        pts++;
        lens++;
    }
    return *(unsigned short *)(mdl + 6);
}

/* 0x00cab260 */
short cm_vcc_man_pt_num(void ***w)
{
    return ((short *)*w[1])[3];
}

/* 0x00cab270 */
void cm_vcc_man_exec(CmVccManWork *w)
{
    int n = w->n;
    int **pt = (int **)w->pts;

    for (; n; n--, pt++)
        (*pt)[6] = w->col;
}

/* 0x00cab2a8 */
void cm_vcc_man_release(void **man)
{
    heap_free(man[3], 0);
    heap_free(man[4], 0);
}

/* 0x00cab2e0 */
CmVccManWork *cm_vcc_man_create(void *parent, int *model)
{
    CmVccManWork *w = task_create_inherit(parent, &cm_vcc_man_desc, 44);
    int *mdl = (int *)model[1];

    w->parent = parent;
    w->m = mdl;
    w->n = cm_vcc_man_pt_num((void ***)mdl[11]);
    w->pts = heap_block_tag(game_malloc(w->n * 4), str_venus_plate_c, SRC_LINE(659, 669));
    w->len = heap_block_tag(game_malloc(w->n * 4), str_venus_plate_c, SRC_LINE(660, 670));
    cm_vcc_man_pts_get((int *)w->m[11], w->pts, w->len);
    w->col = cm_vcc_man_col_init;
    cm_vcc_man_exec(w);
    return w;
}

/* 0x00cab3b0 */
void cm_vcc_man_set_col(CmVccManWork *w, int col)
{
    w->col = col;
}

/* 0x00cab3b8 */
void cm_vcc_man_wave(CmVccManWork *w, float rad, float width)
{
    float *len = w->len;
    char **pts = w->pts;
    int n = w->n;
    float sc[2];
    float dis;
    char *vtx;
    int lum;

    for (; n; len++, n--, pts++) {
        dis = fabsf(*len - rad);
        if (dis < width) {
            vtx = *pts;
            ulSinCosf(dis / width * PI, &sc[0], &sc[1]);
            lum = (int)((1.0f - (sc[1] * -0.5f + 0.5f)) * 255.0f) + 1 >> 1 & 0xff;
            *(unsigned int *)(vtx + 24) = lum << 24 | lum << 16 | lum << 8 | lum;
        }
    }
}

/* 0x00cab4f8 */
void cm_vcc_man_wave_fill(CmVccManWork *w, float rad, float width)
{
    float *len = w->len;
    char **pts = w->pts;
    int n = w->n;
    float sc[2];
    float dis;
    char *vtx;
    int lum;

    for (; n; len++, n--, pts++) {
        if (*len < rad) {
            vtx = *pts;
            *(unsigned int *)(vtx + 24) = cm_vcc_man_col_full;
            continue;
        }
        dis = fabsf(*len - rad);
        if (dis < width) {
            vtx = *pts;
            ulSinCosf(dis / width * PI, &sc[0], &sc[1]);
            lum = (int)((1.0f - (sc[1] * -0.5f + 0.5f)) * 255.0f) + 1 >> 1 & 0xff;
            *(unsigned int *)(vtx + 24) = lum << 24 | lum << 16 | lum << 8 | lum;
        }
    }
}

/* 0x00cab660 */
void cm_vcc_man2_exec(CmVccManWork *w)
{
    float rate = (float)w->frm / (float)w->frm_max;
    int n = w->n;
    char **p = w->pts;
    unsigned int *col;

    for (; n; n--) {
        col = (unsigned int *)(*p++ + 24);
        *col = (*col & 0xffffff) | (((int)(color_get_a(*col) * rate) + 1) >> 1 << 24);
    }
    if (--w->frm <= 0)
        FINISH_IF(w);
}

/* 0x00cab750 */
void cm_vcc_man2_restore(CmVccManWork *w)
{
    int n = w->n;
    int **pt = (int **)w->pts;
    int *saved = w->val;

    for (; n; n--, pt++, saved++)
        (*pt)[6] = *saved;
}

/* 0x00cab788 */
void cm_vcc_man2_release(CmVccManWork *w)
{
    cm_vcc_man2_restore(w);
    heap_free(w->pts, 0);
    heap_free(w->val, 0);
}

/* 0x00cab7c0 */
short cm_vcc_man2_pts_get(int *w, char **pts, int *cols)
{
    char *mdl = *(char **)w[1];
    int n = *(short *)(mdl + 6);
    char *vtx = mdl + 8;

    for (; n; n--) {
        *pts = vtx;
        *cols = *(int *)(vtx + 24);
        vtx += 28;
        pts++;
        cols++;
    }
    return *(unsigned short *)(mdl + 6);
}

/* 0x00cab808 */
CmVccManWork *cm_vcc_man2_create(void *parent, int *model, int frm)
{
    CmVccManWork *w = task_create_inherit(parent, &cm_vcc_man2_desc, 44);
    int *mdl = (int *)model[1];

    w->parent = parent;
    w->m = mdl;
    w->n = cm_vcc_man_pt_num((void ***)mdl[11]);
    w->pts = heap_block_tag(game_malloc(w->n * 4), str_venus_plate_c, SRC_LINE(793, 803));
    w->val = heap_block_tag(game_malloc(w->n * 4), str_venus_plate_c, SRC_LINE(794, 804));
    cm_vcc_man2_pts_get((int *)w->m[11], w->pts, w->val);
    w->frm = w->frm_max = frm;
    return w;
}

/* 0x00cab8d8 */
void venus_plate_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
