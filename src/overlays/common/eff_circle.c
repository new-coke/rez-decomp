/*
 * Circle effect: two line rings on screen around a point that moves with the player; they ease
 * out to full radius at their own rates and fade over the last frames.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "types_gw.h"

#include "debug.h"
#include "color.h"
#include "fog.h"
#include "gw_varedit.h"
#include "line_batch.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "eff_circle.h"
#include "system.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

extern struct GwVarNode *gw_varedit_root;

extern char eff_circle_vardef[];
extern GwVarDef eff_circle_vardefs[];
extern struct GwVarNode *eff_circle_vars_group;
extern struct TaskDesc eff_circle_class;
extern int cmd_def_add_line1_n;
extern float cmd_line1_rate, cmd_line2_rate, cmd_line2_alpha_rate;
extern float project_point_size(Vec3 *p, Vec3 *out, float *sx, float *sy);

/* 0x00b11f40 stg01, 0x00b1d1d0 stg02, 0x00b16328 stg03, 0x00b258b0 stg22, 0x00b01b88 stg23, 0x00cc7210 b_earth, 0x00cd9750 b_eden, 0x00cca4e8 b_uranus */
void eff_circle_vars_add(void)
{
    eff_circle_vars_group = gw_varedit_add(&gw_varedit_root, 0, eff_circle_vardef);
    gw_varedit_add_table(&gw_varedit_root, eff_circle_vars_group, eff_circle_vardefs);
}

/* 0x00b11f90 stg01, 0x00b1d220 stg02, 0x00b16378 stg03, 0x00b25900 stg22, 0x00b01bd8 stg23, 0x00cc7260 b_earth, 0x00cd97a0 b_eden, 0x00cca538 b_uranus */
void eff_circle_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eff_circle_vardef);
}

/* 0x00b11fa8 stg01, 0x00b1d238 stg02, 0x00b16390 stg03, 0x00b25918 stg22, 0x00b01bf0 stg23, 0x00cc7278 b_earth, 0x00cd97b8 b_eden, 0x00cca550 b_uranus */
void eff_circle_exec(EffCircleWork *w)
{
    Vec3 pl_vel;

    player_get_frame_vel(&pl_vel);
    w->pos.x += pl_vel.x;
    w->pos.y += pl_vel.y;
    w->pos.z += pl_vel.z;
    w->k1 += (1.0f - w->k1) * cmd_line1_rate;
    w->k2 += (1.0f - w->k2) * cmd_line2_rate;
    if (--w->count <= 0)
        do { FINISH(w); } while (0);
}

/* 0x00b12060 stg01, 0x00b1d2f0 stg02, 0x00b16448 stg03, 0x00b259d0 stg22, 0x00b01ca8 stg23, 0x00cc7330 b_earth, 0x00cd9870 b_eden, 0x00cca608 b_uranus */
void eff_circle_draw_ring(unsigned int col, int n, float x, float y, float z, float width, float r)
{
    float sc[2];
    int i;
    int step = 0xffff / n;
    float px = x + r;
    float py = y;
    int ang = step;
    void *cur = line_set_color(col);

    for (i = n; i != 0; i--) {
        ulSinCos(ang, &sc[0], &sc[1]);
        ang += step;
        sc[1] = x + sc[1] * r;
        sc[0] = y + sc[0] * r;
        cur = line_add_seg(cur, px, py, z, sc[1], sc[0], z);
        px = sc[1];
        py = sc[0];
    }
    line_flush(cur);
}

/* 0x00b12180 stg01, 0x00b1d410 stg02, 0x00b16568 stg03, 0x00b25af0 stg22, 0x00b01dc8 stg23, 0x00cc7450 b_earth, 0x00cd9990 b_eden, 0x00cca728 b_uranus */
int eff_circle_on_screen(float r, float *p)
{
    if (1000.0f < p[0] + r)
        return 0;
    if (p[0] - r < -1000.0f)
        return 0;
    if (1000.0f < p[1] + r)
        return 0;
    if (p[1] - r < -1000.0f)
        return 0;
    return 1;
}

/* 0x00b121f0 stg01, 0x00b1d480 stg02, 0x00b165d8 stg03, 0x00b25b60 stg22, 0x00b01e38 stg23, 0x00cc74c0 b_earth, 0x00cd9a00 b_eden, 0x00cca798 b_uranus */
void eff_circle_disp(EffCircleWork *w)
{
    Vec3 v, s;
    float size[4];
    int fade_frm = w->fade - w->count;
    int a;
    unsigned int col1, col2, base;
    float z;
    float x, y;
    void *fog;

    if (fade_frm > 0) {
        a = color_get_a(w->color);
        base = w->color & 0xffffff;
        a = a * (1.0f - (float)fade_frm / w->fade);
        col1 = base | (a + 1) >> 1 << 24;
        col2 = base | ((int)(a * cmd_line2_alpha_rate) + 1) >> 1 << 24;
    } else {
        a = color_get_a(w->color);
        col1 = w->color;
        col2 = (col1 & 0xffffff) | ((int)(a * cmd_line2_alpha_rate) + 1) >> 1 << 24;
    }
    draw_set_blend(4, 1);
    fog = fog_set_enable(0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &w->pos.x, &v.x);
    if (v.z < -1.0f) {
        size[0] = w->radius * w->k1;
        z = project_point_size(&v, &s, size, 0);
        if (eff_circle_on_screen(size[0], &s.x)) {
            x = s.x;
            y = s.y;
            eff_circle_draw_ring(col1, w->seg_a, x, y, z, w->width, size[0]);
            size[0] = w->radius * w->k2;
            project_point_size(&v, &s, size, 0);
            eff_circle_draw_ring(col2, w->seg_b, x, y, z, w->width, size[0]);
        }
    }
    fog_set_enable(fog);
}

/* 0x00b12438 stg01, 0x00b1d6c8 stg02, 0x00b16820 stg03, 0x00b25da8 stg22, 0x00b02080 stg23, 0x00cc7708 b_earth, 0x00cd9c48 b_eden, 0x00cca9e0 b_uranus */
EffCircleWork *eff_circle_create(void *parent, EffCircleArg *prm)
{
    EffCircleWork *w = task_create(parent, &eff_circle_class, 52);
    Vec3 *p = prm->pos;

    w->color = prm->color;
    v3_copy(&w->pos, p);
    w->count = w->frames = prm->frames;
    w->fade = prm->fade;
    w->radius = prm->radius;
    w->seg_b = prm->n;
    w->width = prm->width;
    if (w->seg_b < 3)
        w->seg_b = 3;
    w->seg_a = w->seg_b + cmd_def_add_line1_n;
    task_set_dlist(w, 6);
    return w;
}
