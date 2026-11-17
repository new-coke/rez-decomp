/*
 * Venus boss rolling balls: lockable balls that bounce off the boss's cylinders, floor, ceiling and
 * outer wall and burst on the player or when shot. The manager keeps them apart, holds the cylinder
 * list and launches test balls from the debug keys.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "color.h"
#include "mathdef.h"
#include "camera.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_ball.h"
#include "venus.h"
#include "../common/boss_common.h"

extern char venus_tire_vardef[];
extern GwVarDef venus_tire_vardefs[];
extern struct GwVarNode *venus_tire_vars_group;
extern float cmd_ball_g;
extern struct TaskDesc venus_ball_m_desc;
extern char venus_ball_m_cyls[];
extern int venus_ball_m_deb_sw;
extern float cmd_ball_gr;
extern float cmd_ball_r;
extern float cmd_ball_test_cx;
extern float cmd_ball_test_cy;
extern float cmd_ball_test_cy1;
extern float cmd_ball_test_cy2;
extern float cmd_ball_test_cy3;
extern float cmd_ball_test_cz;
extern float cmd_ball_test_cr;
extern float cmd_ball_test_cr1;
extern float cmd_ball_test_cr2;
extern float cmd_ball_test_cr3;
extern float cmd_ball_test_ch;
extern float cmd_ball_test2_cx;
extern float cmd_ball_test2_cy;
extern float cmd_ball_test2_cz;
extern float cmd_ball_test2_ch;
extern float cmd_ball_test2_cr;
extern float cmd_ball_lv;
extern float cmd_break_l;
extern float cmd_break_t;
extern float cmd_break_ang;
extern float cmd_ball_test_len;
extern float cmd_ball_test_h;
extern struct TaskDesc venus_ball_desc;
extern float cmd_ball_v;
extern float cmd_ball_dv;
extern float cmd_ball_cdv;
extern int cmd_ball_hp;
extern float cmd_ball_e;
extern float cmd_ball_gh;
extern float cmd_ball_th;
extern float cmd_ball_vm;
extern float cmd_ball_rv;
extern float cmd_ball_ce;
extern float cmd_ball_cv;
extern float cmd_ball_sv;
extern float cmd_ball_t;
extern float cmd_ball_vlim;

/* vertical cylinder: axis (x, z), from bottom up to top */
typedef struct VenusCyl {
    float x, top, z;
    float bottom;               /* 0x0c */
    float r;                    /* 0x10 */
} VenusCyl;

/* 0x00cb4970 */
void venus_tire_vars_add(void)
{
    venus_tire_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_tire_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_tire_vars_group, venus_tire_vardefs);
}

/* 0x00cb49c0 */
void venus_tire_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_tire_vardef);
}

/* 0x00cb49d8 */
int venus_ball_cyl_hit(void *pos_in, void *vel_in, VenusParam *cyl, float rad, float bounce, float friction)
{
    Vec3 *pos = pos_in;
    Vec3 *vel = vel_in;
    float *cylf = (float *)cyl;
    Vec3 nrm, refl;
    float dis_sq, dis;

    if (cyl->y < pos->y - rad)
        return 0;
    if (pos->y < cylf[3])
        return 0;
    dis_sq = (cyl->x - pos->x) * (cyl->x - pos->x) + (cyl->z - pos->z) * (cyl->z - pos->z);
    if (pos->y < cyl->y && cylf[3] < pos->y && dis_sq < (cylf[4] + rad) * (cylf[4] + rad)) {
        vel->y = 0.0f;
        if (fabsf(dis_sq) < FLT_EPSILON) {
            nrm.y = vel->y;
            nrm.x = vel->y;
            nrm.z = -1.0f;
        } else {
            nrm.x = pos->x - cyl->x;
            nrm.y = 0.0f;
            nrm.z = pos->z - cyl->z;
            vec3_normalize(&nrm, &nrm);
        }
        if (ulVec3Dot(vel, &nrm) < 0.0f) {
            vec_reflect_unit(vel, &nrm, &refl);
            v3_mul(vel, &refl, bounce);
        }
        v3_scale(&nrm, rad + cylf[4] - sqrtf(dis_sq));
        v3_sub(&nrm, &nrm, vel);
        v3_add(pos, &nrm);
        v3_add(pos, vel);
        return 4;
    }
    if (fabsf(dis_sq) < FLT_EPSILON)
        goto top;
    dis = sqrtf(dis_sq);
    if (cylf[4] + rad < dis)
        return 0;
    if (dis <= cylf[4])
        goto top;
    if (cyl->y <= pos->y) {
        dis -= cylf[4];
        pos->y = cyl->y + sqrtf(rad * rad - dis * dis);
        vel->x *= friction;
        vel->z *= friction;
        vel->y = fabsf(vel->y) * bounce;
        return 2;
    }
    return 0;
top:
    pos->y = cyl->y + rad;
    vel->y = fabsf(vel->y) * bounce;
    return 1;
}

/* 0x00cb4d40 */
int venus_ball_cyls_hit(void *pos, void *vel, VenusParam *cyl, int n, float rad, float bounce, float friction)
{
    int hits = 0;
    int hit;

    for (; n; n--, cyl++) {
        hit = venus_ball_cyl_hit(pos, vel, cyl, rad, bounce, friction);
        if (hit)
            hits = hit;
    }
    return hits;
}

/* 0x00cb4df0 */
int venus_ball_floor_hit(Vec3 *pos, Vec3 *v, float rad, float floor_y, float bounce)
{
    if (pos->y - rad <= floor_y) {
        pos->y = floor_y + rad;
        v->y = fabsf(v->y) * bounce;
        return 16;
    }
    return 0;
}

/* 0x00cb4e30 */
int venus_ball_ceil_hit(Vec3 *pos, Vec3 *v, float rad, float top, float bounce)
{
    if (top < pos->y + rad) {
        pos->y = top - rad - cmd_ball_g;
        v->y = -fabsf(v->y) * bounce;
        return 32;
    }
    return 0;
}

/* 0x00cb4e80 */
int venus_ball_wall_hit(Vec3 *pos, Vec3 *v, float rad, float wall_r, float bounce)
{
    float lim = wall_r - rad;
    Vec3 in;
    float len;

    in.x = -pos->x;
    in.y = 0.0f;
    in.z = -pos->z;
    len = vec3_length(&in);
    if (lim < len) {
        v->y = 0.0f;
        vec3_normalize(&in, &in);
        vec_reflect_unit(v, &in, v);
        v3_scale(v, bounce);
        v3_scale(&in, len - lim);
        v3_add(pos, &in);
        return 64;
    }
    return 0;
}

/* 0x00cb4fa0 */
void venus_ball_rotate_axes(Vec3 *axis_a, Vec3 *axis_b, int ry, int rx)
{
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_x(ulMatrixCur, rx);
    vu0_matrix_rotate_y(ulMatrixCur, -ry);
    ulMatrixApplyVector((float *)ulMatrixCur, &axis_a->x, &axis_a->x);
    ulMatrixApplyVector((float *)ulMatrixCur, &axis_b->x, &axis_b->x);
    ulMatrixPop();
}

/* 192 bytes */
typedef struct VenusBallWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 vel;
    Vec3 push;
    Vec3 fwd;
    Vec3 side;
    LockObj lock;
    int frozen;
    int still_cnt;
    char pad_8c[4];
    int grounded;                       /* 0x90 */
    char *models;
    int ang[3];
    float spin;
    int head;
    void *mng;
    char pad_b0[4];
    VenusParam *cyls;                   /* 0xb4 */
    int cyl_num;
    int hit;
} VenusBallWork;

/* 0x00cb5038 */
void venus_ball_move(VenusBallWork *w)
{
    float sc[2];
    int old_ang = w->head;
    int hit, bounds_hit, roll;
    float spd, len;

    v3_sum(&w->vel, &w->push, &w->vel);
    w->vel.y -= cmd_ball_g;
    hit = venus_ball_cyls_hit(&w->pos, &w->vel, w->cyls, w->cyl_num, cmd_ball_r, cmd_ball_ce, cmd_ball_rv);
    bounds_hit = venus_ball_floor_hit(&w->pos, &w->vel, cmd_ball_r, cmd_ball_gh, cmd_ball_e);
    bounds_hit |= venus_ball_ceil_hit(&w->pos, &w->vel, cmd_ball_r, cmd_ball_th, 1.0f);
    bounds_hit += venus_ball_wall_hit(&w->pos, &w->vel, cmd_ball_r, cmd_ball_gr, cmd_ball_e);
    hit += bounds_hit;
    if (hit) {
        w->hit = hit;
        w->grounded = 1;
    } else {
        w->grounded = 0;
    }
    spd = w->vel.x * w->vel.x + w->vel.z * w->vel.z;
    if (spd < cmd_ball_sv) {
        if (cmd_ball_t * FPS <= (float)++w->still_cnt)
            FINISH_IF(w);
        return;
    }
    w->still_cnt = 0;
    spd = sqrtf(spd);
    if (spd < cmd_ball_cv)
        ROT_Y((int)DEG_TO_ANG(w->spin), sc, w->vel.x, w->vel.z);
    spd *= cmd_ball_vm;
    v3_scale(&w->vel, cmd_ball_vm);
    roll = -(int)(spd / cmd_ball_r * ANG_PER_RAD);
    len = vec3_length(&w->vel);
    if (cmd_ball_vlim < len)
        v3_scale(&w->vel, cmd_ball_vlim / len);
    w->head = RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
    venus_ball_rotate_axes(&w->fwd, &w->side, w->head - old_ang, roll);
    angles_from_dirs(&w->fwd, &w->side, w->ang);
    w->ang[1] += w->head;
    if (w->grounded) {
        w->pos.x += w->vel.x;
        w->pos.z += w->vel.z;
    } else {
        v3_add(&w->pos, &w->vel);
    }
}

/* 0x00cb5458 */
void venus_ball_exec(VenusBallWork *w)
{
    int n;

    if (!w->frozen) {
        for (n = enemy_status_get_ticks(); n; n--)
            venus_ball_move(w);
    }
    if (w->lock.flags & LOCK_HIT)
        v3_mul(&w->push, &w->lock.imp.v, cmd_ball_lv);
    else
        v3_set(&w->push, 0.0f, 0.0f, 0.0f);
    if (w->lock.life <= 0) {
        do {
            venus_zxyb_create(w->mng, &w->pos, &w->vel, w->ang,
                                   cmd_ball_r, cmd_break_t, cmd_break_l, cmd_break_ang);
        } while (0);
    } else {
        if (!(player_sphere_test(&w->pos, cmd_ball_r) & 1))
            return;
        venus_zxyb_create(w->mng, &w->pos, &w->vel, w->ang,
                               cmd_ball_r, cmd_break_t, cmd_break_l, cmd_break_ang);
        venus_player_damage();
    }
    if (w)
        FINISH(w);
}

/* 0x00cb55d0 */
void venus_ball_disp(VenusBallWork *w)
{
    int old_blend;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_ball_rotate_yxz(w->ang);
    dc_object_draw(*(ModelPart **)(w->models + 132));
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00cb5650 */
void venus_ball_release(VenusBallWork *w)
{
    lock_remove_entry(4, &w->lock);
}

/* 0x00cb5660 */
int *venus_ball_create(int *mng, Vec3 *pos, Vec3 *dir, int models, int cyls, int cyl_num)
{
    VenusBallWork *ball = task_create_inherit(mng, &venus_ball_desc, 192);
    float spin;

    ball->mng = mng;
    ball->models = (char *)models;
    v3_copy(&ball->pos, pos);
    v3_copy(&ball->vel, dir);
    vec_set_length(&ball->vel, cmd_ball_v + (RAND_FLOAT_A() * (cmd_ball_dv + cmd_ball_dv) - cmd_ball_dv));
    spin = RAND_FLOAT_A() * (cmd_ball_cdv + cmd_ball_cdv) - cmd_ball_cdv;
    ball->fwd.z = -1.0f;
    ball->side.y = 1.0f;
    ball->side.x = ball->side.z = ball->fwd.x = ball->fwd.y = 0.0f;
    ball->spin = spin;
    ball->head = RAD_TO_ANG(atan2f(-ball->vel.x, -ball->vel.z));
    ball->cyls = (VenusParam *)cyls;
    ball->cyl_num = cyl_num;
    lock_init(4, &ball->lock, ball, &ball->pos, 0, cmd_ball_hp, 0);
    lockon_entry(&ball->lock);
    return (int *)ball;
}

/* 0x00cb5860 */
void venus_ball_m_cyls_set(VenusCyl *cyl)
{
    cyl->x = cmd_ball_test_cx;
    cyl->top = cmd_ball_test_cy;
    cyl->z = cmd_ball_test_cz;
    cyl->bottom = cmd_ball_test_cy - cmd_ball_test_ch;
    cyl->r = cmd_ball_test_cr;
    cyl++;
    cyl->x = cmd_ball_test_cx;
    cyl->top = cmd_ball_test_cy1;
    cyl->z = cmd_ball_test_cz;
    cyl->bottom = cmd_ball_test_cy1 - cmd_ball_test_ch;
    cyl->r = cmd_ball_test_cr1;
    cyl++;
    cyl->x = cmd_ball_test_cx;
    cyl->top = cmd_ball_test_cy2;
    cyl->z = cmd_ball_test_cz;
    cyl->bottom = cmd_ball_test_cy2 - cmd_ball_test_ch;
    cyl->r = cmd_ball_test_cr2;
    cyl++;
    cyl->x = cmd_ball_test_cx;
    cyl->top = cmd_ball_test_cy3;
    cyl->bottom = cmd_ball_test_cy3 - cmd_ball_test_ch;
    cyl->z = cmd_ball_test_cz;
    cyl->r = cmd_ball_test_cr3;
    cyl++;
    cyl->x = cmd_ball_test2_cx;
    cyl->top = cmd_ball_test2_cy;
    cyl->z = cmd_ball_test2_cz;
    cyl->bottom = cmd_ball_test2_cy - cmd_ball_test2_ch;
    cyl->r = cmd_ball_test2_cr;
}

/* steer(cast) */
/* 0x00cb5970 */
void venus_ball_m_cyls_draw(float *cyl, int n)
{
    Vec3 v;

    for (; n; n--) {
        circle_draw_flat((Vec3 *)cyl, COL_WHITE, cyl[4]);
        v.x = cyl[0];
        v.z = cyl[2];
        v.y = cyl[3];
        circle_draw_flat(&v, 0x80000080, cyl[4]);
        cyl += 5;
    }
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    circle_draw_flat((Vec3 *)cyl, 0x80800000, cmd_ball_gr);
    v.y += 30.0f;
    circle_draw_flat((Vec3 *)cyl, 0x80640000, cmd_ball_gr);
}

/* steer(cast) */
/* 0x00cb5a48 */
void venus_ball_m_separate(void *w)
{
    char *ball, *other, *prev;

    for (ball = TASK_OF(w)->child ? (char *)((Task *)TASK_OF(w)->child + 1) : 0; ball; ball = TASK_OF(ball)->next ? (char *)((Task *)TASK_OF(ball)->next + 1) : 0) {
        for (prev = ball; (other = TASK_OF(prev)->next ? (char *)((Task *)TASK_OF(prev)->next + 1) : 0) != 0; prev = other) {
            if (((int (*)(Vec3 *, Vec3 *, Vec3 *, Vec3 *, float, float, float, float))spheres_separate)((Vec3 *)(ball + 8), (Vec3 *)(other + 8), (Vec3 *)(ball + 20), (Vec3 *)(other + 20), cmd_ball_r, cmd_ball_r, 1.0f, 1.0f)) {
                (*(unsigned short *)(ball + 74))--;
                (*(unsigned short *)(other + 74))--;
            }
        }
    }
}

/* 0x00cb5b30 */
void venus_ball_m_exec(int *w)
{
    Vec3 dir, pos, eye, view;
    float sc[2];

    venus_ball_m_separate(w);
    venus_ball_m_cyls_set((VenusCyl *)w[4]);
    if (debug_flags_test(4) && (kbd_state[7] & 0x44)) {
        switch (kbd_state[5]) {
        case 5:
            dir.x = 0.0f;
            dir.y = cmd_ball_test_h;
            dir.z = -cmd_ball_test_len;
            ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f), sc, dir.x, dir.z);
            v3_sum(&pos, (Vec3 *)w[2], &dir);
            dir.y = 0.0f;
            venus_ball_create(w, &pos, &dir, w[3], w[4], w[5]);
            break;
        case 6:
            camera_get_eye(&eye);
            camera_get_view_vec(&view);
            venus_ball_create(w, &eye, &view, w[3], w[4], w[5]);
            break;
        }
    }
}

/* 0x00cb5cf0 */
void venus_ball_m_disp(int *w)
{
    if (venus_ball_m_deb_sw)
        venus_ball_m_cyls_draw((void *)w[4], w[5]);
}

/* 0x00cb5d18 */
int *venus_ball_m_create(void *parent, void *pos, void *models)
{
    int *w = task_create_inherit(parent, &venus_ball_m_desc, 24);

    w[2] = (int)pos;
    w[3] = (int)models;
    w[4] = (int)venus_ball_m_cyls;
    w[5] = 5;
    return w;
}

/* 0x00cb5d78 */
void venus_ball_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
