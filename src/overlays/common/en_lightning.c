/*
 * Lightning between the Perfect Wall's halves: runs of jagged segments between two points, each
 * swinging round the line as it spins, redrawn every frame as a wide polyline.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lens.h"
#include "libc.h"
#include "math_util.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_lightning.h"
#include "enemy_vars.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern struct TaskDesc en_lightning_desc;
extern ParamRec en_lightning_params;
extern VarDef en_lightning_vardefs[11];

#define EN_LIGHTNING_REC ((EnLightningRec *)&en_lightning_params)

extern LineStyle en_lightning_line_style;
extern LineStyle en_lightning_custom_style;
extern char str_en_lightning_c[];

/* 0x00b08cf8 stg04, 0x00b05318 stg22 */
void en_lightning_vars_register(void)
{
    VarDef hdr;
    VarDef *def;
    int *val;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = en_lightning_params.name;
    hdr.fmt = en_lightning_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&hdr);
    def = en_lightning_vardefs;
    val = en_lightning_params.v;
    for (i = 0; i < 11; i++) {
        def->ptr = val;
        val++;
        def++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_lightning_vardefs);
}

/* 0x00b08da8 stg04, 0x00b053c8 stg22 */
void en_lightning_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b08db0 stg04, 0x00b053d0 stg22 */
void en_lightning_init_segments(EnLightningWork *w)
{
    EnLightningSeg *seg = w->seg;
    int i = w->n * w->m;
    float rnd_max = 32769.0f;
    float full = 360.0f;
    float deg = ANG_PER_DEG;
    float sec = FRAME_SEC;
    float half = 0.5f;

    do {
        i--;
        seg->len = rand_lcg_next_a() / rnd_max * (EN_LIGHTNING_REC->len_max - EN_LIGHTNING_REC->len_min) + EN_LIGHTNING_REC->len_min;
        seg->ang = (int)(rand_lcg_next_a() / rnd_max * full * deg);
        seg->spin = (int)(rand_lcg_next_a() / rnd_max *
                        (float)((int)(EN_LIGHTNING_REC->ang_spd_max * sec * deg) - (int)(EN_LIGHTNING_REC->ang_spd_min * sec * deg)) +
                        (float)(int)(EN_LIGHTNING_REC->ang_spd_min * sec * deg));
        if (rand_lcg_next_a() / rnd_max < half)
            seg->spin = -seg->spin;
        seg++;
    } while (i);
}

/* 0x00b08f50 stg04, 0x00b05570 stg22 */
void en_lightning_lay_points(EnLightningWork *w)
{
    Vec3 pos;
    Vec3 step;
    EnLightningSeg *seg = w->seg;
    int n = w->n;
    int runs = w->m, j;
    float frac = 1.0f / (float)(n + 1);
    int i;

    v3_copy(&pos, w->a);
    v3_sub(&step, w->to, &pos);
    v3_scale(&step, frac);
    j = runs;
    do {
        i = n;
        do {
            i--;
            v3_add(&pos, &step);
            v3_copy(&seg->d, &pos);
            seg++;
        } while (i);
        v3_add(&pos, &step);
        step.x = -step.x;
        step.y = -step.y;
        step.z = -step.z;
    } while (--j);
}

/* 0x00b09088 stg04, 0x00b056a8 stg22 */
void en_lightning_build_polyline(EnLightningWork *w)
{
    Vec3 v;
    int ang[3];
    Vec3 dir;
    EnLightningSeg *seg;
    Vec3 *p;
    int n, i, j, prev, rot, runs;

    prev = 0;
    en_lightning_lay_points(w);
    seg = w->seg;
    p = w->pts;
    ulMatrixPush();
    n = w->n;
    runs = w->m;
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 0.0f;
    v3_sub(&dir, w->to, w->a);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = 0;
    matrix_unit_set(0);
    en_lightning_rotate_yxz(ang);
    i = runs;
    do {
        p++;
        j = n;
        do {
            j--;
            rot = seg->ang += seg->spin;
            v.x = seg->len;
            vu0_matrix_rotate_z(ulMatrixCur, rot - prev);
            prev = rot;
            ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &p->x);
            v3_add(p, &seg->d);
            seg++;
            p++;
        } while (j);
    } while (--i);
    ulMatrixPop();
}

/* 0x00b09290 stg04, 0x00b058b0 stg22 */
void en_lightning_exec(EnLightningWork *w)
{
    int n, j = 0, k;

    en_lightning_build_polyline(w);
    n = w->n + 1;
    j = w->m;
    if (j < 0)
        return;
    k = n;
    v3_copy(w->pts, w->a);
    for (;;) {
        if (--j < 0)
            return;
        v3_copy(&w->pts[k], w->to);
        k += n;
        if (--j < 0)
            return;
        v3_copy(&w->pts[k], w->a);
        k += n;
    }
}

/* 0x00b09358 stg04, 0x00b05978 stg22 */
void en_lightning_disp(EnLightningWork *w)
{
    EnLightningRec *rec = EN_LIGHTNING_REC;
    LineStyle *style = &en_lightning_line_style;
    float prev;

    if (rec->use_custom_laser) {
        en_lightning_custom_style.col[0] = rec->pcol_center;
        en_lightning_custom_style.col[1] = rec->pcol_out1;
        en_lightning_custom_style.col[2] = rec->pcol_out2;
        en_lightning_custom_style.width = rec->r_all;
        en_lightning_custom_style.rate_in = rec->rate_in;
        en_lightning_custom_style.unk_0c = 0;
        en_lightning_custom_style.unk_18[0] = 0.0f;
        en_lightning_custom_style.unk_18[1] = 0.0f;
        style = &en_lightning_custom_style;
    }
    prev = polyline_set_width_min(1.0f);
    draw_polyline_wide_nocap(style, w->pts, w->npts - 1);
    polyline_set_width_min(prev);
}

/* 0x00b09410 stg04, 0x00b05a30 stg22 */
void en_lightning_init(EnLightningWork *w, EnLightningArg *prm)
{
    int n;
    int runs;
    int nseg;
    EnLightningSeg *seg;

    w->a = prm->a;
    n = prm->n;
    if (n <= 0)
        n = 1;
    w->n = n;
    w->to = prm->to;
    runs = prm->m;
    if (runs <= 0)
        runs = 1;
    w->m = runs;
    nseg = n * runs;
    w->npts = nseg + runs + 1;
    seg = heap_block_tag(game_malloc(nseg * 24 + w->npts * 12), str_en_lightning_c, 466);
    w->pts = (Vec3 *)((char *)seg + nseg * 24);
    w->seg = seg;
    en_lightning_init_segments(w);
    task_set_dlist(w, 6);
}

/* 0x00b094c8 stg04, 0x00b05ae8 stg22 */
void en_lightning_release(int *w)
{
    heap_free((void *)w[2], 0);
}

/* 0x00b094d8 stg04, 0x00b05af8 stg22 */
void *en_lightning_create(void *parent, void *prm)
{
    return task_create_arg(parent, &en_lightning_desc, 28, prm);
}

/* 0x00b09500 stg04, 0x00b05b20 stg22 */
void en_lightning_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
