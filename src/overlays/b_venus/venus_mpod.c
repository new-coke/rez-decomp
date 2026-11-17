/*
 * Venus boss fog, star colour and lasers. A laser grows from its base, hurts on contact, flashes
 * and stops short at the arena poles; venus_l_ex_create fires one beam or a fan with a ring flash.
 * Also the capsule drawing of line effects.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "color.h"
#include "mathdef.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "libc.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_mpod.h"
#include "venus.h"
#include "venus_ptcl.h"
#include "../common/boss_common.h"

extern char venus_laser_vardef[];
extern GwVarDef venus_laser_vardefs[];
extern struct GwVarNode *venus_laser_vars_group;
extern struct TaskDesc venus_fog_desc;

/* laser burst; finishes once all its beams have */
typedef struct VenusLExWork {
    char pad_00[8];
    int *t[4];                  /* 0x08 */
    int n;                      /* 0x18 */
} VenusLExWork;

extern float venus_l_range_bottom;
extern float venus_l_range_top;
extern float cmd_star_ch_t;
extern struct TaskDesc venus_ch_desc;
extern int cmd_fog_edit;
extern int cmd_fog_t;
extern int venus_l_pole_sw;

/* segment_draw line style, 24 bytes: colour, width, height factor */
/* 124 bytes */
typedef struct VenusLaserWork {
    void *pack;
    void *textures;
    Vec3 *base;
    Vec3 start;
    Vec3 end;
    Vec3 dir;
    int ang[3];
    int *cols;
    int state;
    int life;
    int life_len;
    int grow;
    int grow_len;
    int fade;
    int fade_len;
    int hit;
    int flash;
    float len;
    float len_max;
    float width;
    float width_max;
    float ofs;
    float flash_t;
} VenusLaserWork;

extern int cm_laser_color2[3];
extern float cmd_laser_w_h;
extern int venus_l_pole_num;
extern float venus_l_pole_ang;
extern float venus_l_pole_dis;
extern float venus_l_pole_r;
extern float venus_l_pole_h;
extern float cmd_laser_len;
extern float cmd_laser_len_s;
extern float cmd_laser_colrate;
extern int venus_l_flash_num;
extern const float venus_l_flash_len;
extern float venus_l_flash_spd;
extern int venus_l_flash_test;
extern struct TaskDesc venus_l_desc;
extern float cmd_laser_rate;
extern float cmd_laser_w_rate;
extern struct TaskDesc venus_l_ex_desc;
extern float cmd_laser_w;
extern float cmd_laser_w2;
extern float cmd_laser_offset;
extern float cmd_laser_offset2;
extern float venus_l_ex_ring_sec;
extern float venus_l_ex_ring_r;
extern float venus_l_ex_ring_ofs;
extern int cm_laser_color[];
extern int cm_laser_color1[];

/* 0x00ca6e78 */
int *venus_fog_create(void *parent, int mode, float sec)
{
    VenusFogWork *w = task_create(parent, &venus_fog_desc, 12);

    w->mode = mode;
    w->frm = w->frm_max = sec * FPS;
    return (int *)w;
}

/* 0x00ca6ed8 */
int venus_fog_set(void *w)
{
    if (cmd_fog_edit) {
        venus_fog_cur.col = venus_fog_cur.back = cm_fog_col;
        venus_fog_cur.near = cmd_fog_n;
        venus_fog_cur.far = cmd_fog_f;
    }
    do {
        fog_set_enable((void *)1);
        BackColReq(venus_fog_cur.back);
    } while (0);
    fog_set(venus_fog_cur.back, cmd_fog_t, venus_fog_cur.near, venus_fog_cur.far);
    return (int)fog_set_enable((void *)1);
}

/* 0x00ca6f70 */
int venus_fog_set_far(void *w, float far_rate)
{
    float far;

    if (cmd_fog_edit) {
        venus_fog_cur.col = venus_fog_cur.back = cm_fog_col;
        venus_fog_cur.near = cmd_fog_n;
        venus_fog_cur.far = cmd_fog_f;
    }
    do {
        BackColReq(venus_fog_cur.back);
        far = venus_fog_cur.far * far_rate;
    } while (0);
    fog_set(venus_fog_cur.back, cmd_fog_t, venus_fog_cur.near, far);
    return (int)fog_set_enable((void *)1);
}

/* 0x00ca7010 */
void venus_ch_exec(int *w)
{
    boss_star_set_color((BossStarCtl *)w[4], boss_color_blend(w[2], w[3], (float)w[0] / w[1]));
    if (--w[0] <= 0)
        FINISH(w);
}

/* 0x00ca7080 */
void venus_ch_release(int *w)
{
    boss_star_set_color((BossStarCtl *)w[4], w[3]);
}

/* 0x00ca7090 */
int *venus_ch_create(void *parent, void *star)
{
    int *w = task_create(parent, &venus_ch_desc, 20);

    w[4] = (int)star;
    w[0] = w[1] = (int)(cmd_star_ch_t * FPS);
    w[2] = boss_star_get_color((BossStarCtl *)star);
    w[3] = star_color;
    star_ctl = (EffStarWork *)star;
    return w;
}

/* 0x00ca7118 */
void venus_laser_vars_add(void)
{
    venus_laser_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_laser_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_laser_vars_group, venus_laser_vardefs);
}

/* 0x00ca7168 */
void venus_laser_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_laser_vardef);
}

/* steer(cast) */
/* 0x00ca7180 */
void venus_l_pole_disp(void)
{
    Vec3 pos;
    float sc[2];
    int i, step, base;

    base = DEG_TO_ANG(venus_l_pole_ang);
    step = 0xffff / venus_l_pole_num;
    for (i = 0; i < venus_l_pole_num; i++) {
        pos.z = venus_l_pole_dis;
        pos.x = pos.y = 0.0f;
        ROT_Y(step * i + base, sc, pos.x, pos.z);
        ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&pos, venus_l_pole_r, COL_WHITE);
        pos.y += venus_l_pole_h;
        ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&pos, venus_l_pole_r, COL_WHITE);
    }
    pos.x = pos.z = 0.0f;
    pos.y = venus_l_range_bottom;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&pos, venus_l_pole_r, COL_WHITE);
    pos.y = venus_l_range_top;
    ((void (*)(Vec3 *, float, unsigned int))circle_draw_flat)(&pos, venus_l_pole_r, COL_WHITE);
}

/* 0x00ca7340 */
int venus_l_pole_hit(Vec3 *start, Vec3 *end)
{
    Vec3 pole;
    float s, c;
    int base = DEG_TO_ANG(venus_l_pole_ang);
    int step = 0xffff / venus_l_pole_num;
    int i;

    for (i = 0; i < venus_l_pole_num; i++) {
        pole.z = venus_l_pole_dis;
        pole.x = pole.y = 0.0f;
        do {
            float px = pole.x, pz = pole.z;

            ulSinCos(step * i + base, &s, &c);
            pole.x = px * c + pz * s;
            pole.z = -px * s + pz * c;
        } while (0);
        if (segment_circle_xz_hit(start, end, &pole, venus_l_pole_r))
            return 1;
    }
    return 0;
}

/* 0x00ca74a0 */
int venus_l_clip_y(Vec3 *start, Vec3 *end, float *rate)
{
    if (end->y < venus_l_range_bottom) {
        *rate = 1.0f - (venus_l_range_bottom - end->y) / (start->y - end->y);
        return 1;
    }
    if (venus_l_range_top < end->y) {
        *rate = 1.0f - (end->y - venus_l_range_top) / (end->y - start->y);
        return 1;
    }
    return 0;
}

/* 0x00ca7520 */
void venus_l_release(Earth1stDemoWork *w)
{
}

/* 0x00ca7528 */
void venus_l_exec(VenusLaserWork *w)
{
    Vec3 start, end, pl, hit_pt;
    int n;
    float t;

    if (venus_l_pole_sw) {
        v3_copy(&start, (&w->start));
        vec3_normalize(&end, &start);
        v3_scale(&end, cmd_laser_len);
        if (venus_l_pole_hit(&start, &end))
            w->len_max = cmd_laser_len_s;
        else
            w->len_max = cmd_laser_len;
    }
    for (n = enemy_status_get_ticks(); n; n--) {
        switch (w->state) {
        case 0: {
            float rate;

            if (--w->grow <= 0)
                w->state = 1;
            if (--w->fade <= 0)
                w->fade = 0;
            rate = 1.0f - (float)w->grow / (float)w->grow_len;
            w->len = w->len_max * rate;
            rate = (float)w->fade / (float)w->fade_len;
            w->width = w->width_max * rate;
            break;
        }
        case 1:
            w->width = w->width_max * 0.1f + w->width * 0.9f;
            if (w->life <= w->grow_len) {
                w->grow = w->grow_len;
                w->state = 2;
            }
            break;
        case 2:
            if (--w->grow <= 0)
                w->grow = 0;
            t = (float)w->grow / (float)w->grow_len;
            w->len = w->len_max * t;
            w->width = w->width_max * t * t;
            break;
        }
        w->dir.x = w->dir.y = 0.0f;
        w->dir.z = -1.0f;
        vec_rotate_zxy((&w->dir), w->ang);
        v3_mul((&w->start), (&w->dir), w->ofs);
        v3_sum((&w->start), w->base, (&w->start));
        v3_scale((&w->dir), w->len);
        if (--w->life <= 0) {
            FINISH_IF(w);
            return;
        }
    }
    v3_sum(&w->end, (&w->start), (&w->dir));
    if (venus_l_flash_test && !w->flash) {
        w->flash = venus_l_flash_num;
        w->flash_t = 0.0f;
    }
    if (!w->hit) {
        player_get_pos(&pl);
        if (segment_point_near((&w->start), &w->end, &pl, &hit_pt, w->width * cmd_laser_colrate)) {
            w->hit = 1;
            venus_player_damage();
            w->flash = venus_l_flash_num;
            w->flash_t = 0.0f;
        }
    }
    if (w->flash) {
        w->flash_t += venus_l_flash_spd / system_fps;
        if (w->flash_t >= venus_l_flash_len) {
            w->flash_t = 0.0f;
            (w->flash)--;
        }
    }
}

/* 0x00ca7958 */
void venus_l_disp(VenusLaserWork *w)
{
    WideLineStyle line;
    int flash;

    if (venus_l_pole_sw)
        venus_l_pole_disp();
    draw_set_blend(4, 1);
    flash = w->flash;
    if (flash % 2 == 1)
        i3_copy(line.col, cm_laser_color2);
    else
        i3_copy(line.col, w->cols);
    line.width = w->width;
    line.k = cmd_laser_w_h;
    segment_draw((int)&line, &w->start, 1);
}

/* 0x00ca7a18 */
int *venus_l_create(void *parent, Vec3 *base, int *ang, int life, int cols, float ofs, float width)
{
    VenusLaserWork *w = task_create_inherit(parent, &venus_l_desc, 124);
    Vec3 start, end;
    float rate;

    w->cols = (int *)cols;
    w->base = base;
    w->life_len = w->life = life;
    w->grow = w->grow_len = life * cmd_laser_rate;
    w->fade = w->fade_len = life * cmd_laser_w_rate;
    w->width = w->width_max = width;
    w->ofs = ofs;
    if (ang)
        i3_copy(w->ang, ang);
    v3_copy(&start, base);
    end.x = end.y = 0.0f;
    end.z = -1.0f;
    vec_rotate_zxy(&end, w->ang);
    v3_scale(&end, cmd_laser_len);
    v3_sum(&end, &start, &end);
    if (venus_l_pole_hit(&start, &end))
        w->len_max = cmd_laser_len_s;
    else
        w->len_max = cmd_laser_len;
    if (venus_l_clip_y(&start, &end, &rate))
        w->len_max = cmd_laser_len * rate;
    return (int *)w;
}

/* 0x00ca7be0 */
void venus_l_ex_exec(VenusLExWork *w)
{
    int i;
    int n = 0;

    for (i = 0; i < w->n; i++) {
        if (w->t[i] && !(TASK_OF(w->t[i])->flags & TASK_F_END))
            n++;
    }
    if (!n)
        FINISH_IF(w);
}

/* 0x00ca7c48 */
int *venus_l_ex_create(void *parent, Vec3 *pos, int life, int num)
{
    int *w = task_create_inherit(parent, &venus_l_ex_desc, 28);
    Vec3 dir;
    int ang[3];
    Vec3 ring_pos;
    int i;

    w[6] = num % 5;
    dir.x = pos->x;
    dir.z = pos->z;
    dir.y = 0.0f;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = 0;
    if (num >= 2) {
        ang[0] = -8192;
        for (i = 0; i < w[6]; i++) {
            ((struct { int *r[5]; } *)(w + 2))->r[i] = venus_l_create(w, pos, ang, life, (int)cm_laser_color1, cmd_laser_offset2, cmd_laser_w2);
            ang[0] += ANG_90;
        }
    } else {
        w[2] = (int)venus_l_create(w, pos, ang, life, (int)cm_laser_color, cmd_laser_offset, cmd_laser_w);
    }
    ring_pos.x = pos->x;
    ring_pos.z = pos->z;
    ring_pos.y = 0.0f;
    vec_set_length(&ring_pos, venus_l_ex_ring_ofs);
    v3_sum(&ring_pos, pos, &ring_pos);
    venus_sp_draw_create((VenusFbManWork *)w, (VenusFbManPod *)&ring_pos, 6, COL_WHITE, 0, venus_l_ex_ring_sec, venus_l_ex_ring_r);
    return w;
}

/* 0x00ca7ef0 */
void venus_l_ex_finish(void *w, int k)
{
    FINISH_IF(w);
}

/* 0x00ca7f10 */
float venus_line_project(Vec3 *v, Vec3 *out)
{
    float inv_z = -1.0f / v->z;
    float x = v->x, y = v->y;
    float sx = screen_cfg.sx, sy = screen_cfg.sy, cx = screen_cfg.cx, cy = screen_cfg.cy;

    x *= inv_z;
    out->z = inv_z;
    y *= inv_z;
    out->x = x * sx + cx;
    out->y = y * sy + cy;
    return fabsf(inv_z * screen_cfg.sx);
}

/* 0x00ca7f80 */
int venus_line_capsule(Vec3 *p0, Vec3 *p1, int *buf, float scale, float r_min, float r_max)
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
        vtx[0].x = scr0.x - rad_a;
        vtx[0].y = scr0.y + rad_a;
        vtx[1].x = scr0.x + rad_a;
        vtx[1].y = scr0.y + rad_a;
        nj_tex_strip_vertices(vtx, 4);
        return 1;
    } else {
        len_sq = 1.0f / sqrtf(len_sq);
        dy *= len_sq;
        dx *= len_sq;
        dyp = dy + dx * 0.5f;
        dym = dy - dx * 0.5f;
        dxm = dx - dy * 0.5f;
        dxp = dx + dy * 0.5f;
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
        buf = (int *)(vtx + 1);
        vtx[0].x = scr1.x + (dx * 0.5f - dy) * rb;
        vtx[0].y = scr1.y + dxp * rb;
        vtx[0].q = scr1.z;
        vtx[0].u = 0.0f;
        vtx[0].v = 1.0f;
        ((TaVtx *)buf)->x = scr1.x + dyp * rb;
        ((TaVtx *)buf)->y = scr1.y + (dy * 0.5f - dx) * rb;
        ((TaVtx *)buf)->q = scr1.z;
        ((TaVtx *)buf)->u = 1.0f;
        ((TaVtx *)buf)->v = 1.0f;
        nj_tex_strip_vertices(vtx, 4);
        return 1;
    }
fail:
    return 0;
}
