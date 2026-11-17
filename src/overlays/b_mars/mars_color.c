/*
 * Mars boss body colouring: a manager resets the body's vertex colours each frame and keeps its
 * inverse matrix; ripple tasks spread a ring of colour over those vertices from a point.
 */

#include "types_gw.h"
#include "b_mars.h"
#include "color.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars_color.h"
#include "mars.h"
#include "../common/boss_common.h"

extern int mars_vcc_keys[];
extern int mars_vcc_level;
extern char mars_d8_vardef[];
extern int mars_vcc_col[];
extern float mars_vcc_fade_spd;
extern float mars_vcc_fade_sec;
extern int mars_vcc_col_a;
extern int mars_vcc_col_b;
extern float mars_vcc_sec;
extern float mars_vcc_spd;

/* vertex-colour manager: the boss model's vertices and the boss's inverse pose, 112 bytes */
typedef struct MarsVccManWork {
    void *pack;
    void *textures;
    int pos;
    int ang;
    void *obj;
    int **verts;
    int vert_cnt;
    char pad_1c[4];
    float mtx[16];                      /* 0x20 */
    char *boss;
    char pad_64[12];
} MarsVccManWork;

/* colour ripple through the boss model's vertices, 36 bytes */
typedef struct MarsPtWork {
    Vec3 pos;
    float rad;
    float spd;
    int timer;
    int timer_len;
    unsigned int col;
    MarsVccManWork *man;
} MarsPtWork;

extern struct TaskDesc mars_vcc_desc;
extern struct TaskDesc mars_vcc_man_desc;
extern char str_mars_color_c[];
extern GwVarDef mars_d8_vardefs[];
extern int mars_d8_vars_group;
extern void vu0_matrix_inverse_rigid(float *d, float *s);
extern float mars_vcc_band;

/* 0x00ca8040 */
void mars_vcc_keys_build(void)
{
    int v = ((mars_vcc_level + 1) >> 1) & 0xff;

    mars_vcc_keys[0] = 0x80000000 | v << 16 | v << 8;
    mars_vcc_keys[1] = 0x80000000 | v;
    mars_vcc_keys[2] = 0x80000000 | v << 16 | v << 8;
    mars_vcc_keys[3] = 0x80000000 | v;
}

/* 0x00ca8088 */
short mars_vcc_collect(void ***w, char **out)
{
    char *hdr = (char *)*w[1];
    char *p = hdr + 8;
    int i;

    for (i = *(short *)(hdr + 6); i != 0; i--, out++) {
        *out = p;
        p += 28;
    }
    return *(unsigned short *)(hdr + 6);
}

/* 0x00ca80d0 */
short mars_vcc_count(void ***w)
{
    return ((short *)*w[1])[3];
}

/* 0x00ca80e0 */
void mars_vcc_man_release(MarsVccManWork *w)
{
    heap_free(w->verts, 0);
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_d8_vardef);
}

/* 0x00ca8118 */
void mars_vcc_man_exec(MarsVccManWork *w)
{
    Vec3 pos;
    int ang[3];
    int i;
    int **p;

    for (i = w->vert_cnt, p = w->verts; i; i--, p++)
        (*p)[6] = mars_vcc_col[0];
    ulMatrixPush();
    mars_pose_still_cb((struct MarsWork *)w->boss, &pos, ang);
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    mars_color_rotate_yxz(ang);
    matrix_get_or_set(0, w->mtx);
    vu0_matrix_inverse_rigid(w->mtx, w->mtx);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00ca81f8 */
char *mars_vcc_man_create(void *parent, void **obj, int pos, int ang)
{
    MarsVccManWork *w = task_create_inherit(parent, &mars_vcc_man_desc, 112);

    w->obj = obj[1];
    w->boss = parent;
    w->pos = pos;
    w->ang = ang;
    w->vert_cnt = mars_vcc_count((void ***)((WireObj *)w->obj)->child);
    w->verts = heap_block_tag(game_malloc(w->vert_cnt * 4), str_mars_color_c, 196);
    mars_vcc_collect((void ***)((WireObj *)w->obj)->child, (char **)w->verts);
    mars_d8_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, mars_d8_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_d8_vars_group, mars_d8_vardefs);
    return (char *)w;
}

/* 0x00ca82e8 */
void mars_vcc_set_alpha(float alpha)
{
    mars_vcc_col[0] &= 0xffffff;
    if (alpha >= 1.0f)
        mars_vcc_col[0] |= 0xff000000;
    else if (alpha > 0.0f)
        mars_vcc_col[0] |= (int)(alpha * 255.0f) << 24;
}

/* 0x00ca8358 */
void mars_vcc_man_set_alpha(MarsVccManWork *w, float alpha)
{
    int i;
    int **p;

    mars_vcc_set_alpha(alpha);
    for (i = w->vert_cnt, p = w->verts; i; i--, p++)
        (*p)[6] = mars_vcc_col[0];
}

/* 0x00ca83b0 */
void mars_vcc_exec(MarsPtWork *w)
{
    float sc[2];
    float in_sq, out_sq, rad, fade, t, alpha;
    int n;
    int **list;
    int *vtx;
    unsigned int col;
    int ca, cr, cg, cb;

    fade = (float)w->timer / (float)w->timer_len;
    in_sq = (w->rad - mars_vcc_band) * (w->rad - mars_vcc_band);
    out_sq = (w->rad + mars_vcc_band) * (w->rad + mars_vcc_band);
    list = w->man->verts;
    n = w->man->vert_cnt;
    rad = sqrtf(w->rad * w->rad);
    for (; n; n--, list++) {
        vtx = *list;
        t = vec3_distance_squared(&w->pos, (struct Vec3 *)vtx);
        if (in_sq < t && t < out_sq) {
            alpha = (sqrtf(t) - rad) / mars_vcc_band;
            ulSinCosf(alpha * 1.5707964f, &sc[0], &sc[1]);
            alpha = sc[1] * fade;
            do {
                col = w->col;
            } while (0);
            ca = (int)((float)color_get_a(col) * alpha);
            col = w->col;
            cr = (int)((float)(col & 0xff) * alpha);
            cg = (int)((float)(col >> 8 & 0xff) * alpha);
            cb = (int)((float)(col >> 16 & 0xff) * alpha);
            ca++;
            cg++;
            ca = ca >> 1 << 24 | ((cr + 1) >> 1 & 0xff);
            ca |= ((cg >> 1 & 0xff) << 8) | (((cb + 1) >> 1 & 0xff) << 16);
            do { /* steer(barrier) */
                vtx[6] |= ca;
            } while (0);
        }
    }
    w->rad += w->spd;
    if (--w->timer <= 0)
        FINISH_IF(w);
}

/* 0x00ca8690 */
void *mars_vcc_create(void *parent, void *pos, int col, float x, float y)
{
    MarsPtWork *pt = task_create(parent, &mars_vcc_desc, 36);

    pt->spd = x;
    pt->col = col;
    pt->man = parent;
    pt->timer = pt->timer_len = (int)(y * FPS);
    ulMatrixApplyPoint(pt->man->mtx, pos, (float *)&pt->pos);
    return pt;
}

/* 0x00ca8738 */
void *mars_vcc_start(void *parent, void *pos, int mode)
{
    switch (mode) {
    case 0:
        return mars_vcc_create(parent, pos, mars_vcc_col_a, mars_vcc_spd, mars_vcc_sec);
    case 1:
        return mars_vcc_create(parent, pos, mars_vcc_col_b, mars_vcc_spd, mars_vcc_sec / 5.0f);
    }
    return 0;
}

/* 0x00ca87c0 */
CmCchangeWork *mars_vcc_fade_create(void *parent)
{
    return cm_cchange_create(parent, mars_vcc_col, mars_vcc_keys, 4, mars_vcc_fade_spd, mars_vcc_fade_sec);
}

/* 0x00ca8800 */
void mars_color_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
