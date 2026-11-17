/*
 * Venus boss particle effects: spark sprays, bursts of one spray or a fan, the flare at a pod when
 * it fires or revives, and the ending's dust cloud of spinning debris over a growing sphere.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "color.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_ptcl.h"
#include "../common/boss_common.h"

extern char venus_ptcl_vardef[];
extern GwVarDef venus_ptcl_vardefs[];
extern struct GwVarNode *venus_ptcl_vars_group;
extern struct TaskDesc venus_dust_desc;
extern char str_venus_ptcl_c[];
extern int cmd_dust_num;
extern int cmd_dust_t;
extern float cmd_dust_sy;
extern float cmd_dust_sax;
extern float cmd_pt_t;
extern float cmd_pt_frate;
extern float cmd_pt_r;
extern float cmd_pt_scl;
extern float cmd_pt_scl_r;
extern float cmd_pt_bscl;
extern int cmd_pt_num;
extern float cmd_pt_rd;
extern float cmd_pt_at_tl;
extern float cmd_pt_add_t;
extern float cmd_pt_len;
extern float cmd_pt_len_s;
extern struct TaskDesc venus_sp_draw_desc;
extern struct TaskDesc venus_ptcl_ex_desc;
extern float cmd_dust_vs;
extern float cmd_dust_vl;
extern float cmd_dust_vys;
extern float cmd_dust_vyl;
extern float cmd_dust_ts;
extern float cmd_dust_tl;
extern float cmd_dust_as;
extern float cmd_dust_al;
extern float cmd_dust_vr;
extern float cmd_dust_dr;
extern float cmd_dust_sr;
extern const float cmd_dust_rmax;
extern float cmd_dust_dax;
extern int cmd_dust_st;
extern int cmd_dust_ffrm;

/* four strip vertices, 64 bytes, copied whole */
typedef struct VenusDustQuad {
    TrailVtx v[4];
} VenusDustQuad;

extern VenusDustQuad venus_dust_quad;

/* spark burst, 136 bytes: up to 31 sprays */
typedef struct VenusPtclExWork {
    char pad_00[8];
    int n;                      /* 0x08 */
    CmPtclWork *spark[31];      /* 0x0c */
} VenusPtclExWork;

/* 0x00cad930 */
void venus_ptcl_vars_add(void)
{
    venus_ptcl_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_ptcl_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_ptcl_vars_group, venus_ptcl_vardefs);
}

/* 0x00cad980 */
void venus_ptcl_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_ptcl_vardef);
}

/* 0x00cad998 */
CmPtclWork *venus_ptcl_create(void *parent, Vec3 *follow, Vec3 *dirsrc, int tex)
{
    BossSparkDef def;

    def.tex = tex;
    def.speed = cmd_pt_len_s;
    def.dscale = cmd_pt_len;
    def.secs = cmd_pt_t;
    def.psize = cmd_pt_r;
    def.k1 = cmd_pt_scl;
    def.size = cmd_pt_bscl;
    def.k2 = cmd_pt_scl_r;
    def.max = cmd_pt_num;
    def.spread = cmd_pt_rd;
    def.life_max = cmd_pt_add_t;
    def.life_min = cmd_pt_at_tl;
    def.fade = cmd_pt_frate;
    return cm_ptcl_create(parent, follow, dirsrc, &def);
}

/* 0x00cada58 */
void venus_ptcl_ex_exec(VenusTaskList *w)
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

/* 0x00cadac0 */
int *venus_ptcl_ex_create(void *parent, Vec3 *pos, int n)
{
    int *w = task_create_inherit(parent, &venus_ptcl_ex_desc, 136);
    Vec3 dir;
    int ang[3];
    float sc[4];
    int i;

    w[2] = n % 32;
    dir.x = pos->x;
    dir.z = pos->z;
    dir.y = 0.0f;
    vec3_normalize(&dir, &dir);
    if (w[2] >= 2) {
        do {
            ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
            ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        } while (0);
        ang[0] = -8192;
        for (i = 0; i < w[2]; i++) {
            dir.x = dir.y = 0.0f;
            dir.z = -1.0f;
            ROT_X(ang[0], sc, dir.y, dir.z);
            ROT_Y(ang[1], (sc + 2), dir.x, dir.z);
            ((VenusPtclExWork *)w)->spark[i] = venus_ptcl_create(w, pos, &dir, 5);
            ang[0] += ANG_90;
        }
    } else {
        w[3] = (int)venus_ptcl_create(w, pos, &dir, 4);
    }
    return w;
}

/* 0x00cadd30 */
void venus_ptcl_ex_hide(VenusTaskList *w, int k)
{
    int i;

    for (i = 0; i < w->n; i++)
        ((void (*)(int *, int))cm_ptcl_hide)(w->t[i], 1);
}

/* 0x00cadda0 */
void venus_sp_draw_exec(int *w)
{
    float t;

    switch (w[3]) {
    case 0:
        if (--w[4] <= 0) {
            w[3] = 1;
            w[4] = w[5];
        } else {
            t = 1.0f - (float)w[4] / (float)w[5];
            t *= t;
            *(float *)&w[10] = *(float *)&w[11] * t;
            break;
        }
    case 1:
        t = (float)w[4] / (float)w[5];
        *(float *)&w[10] = *(float *)&w[11] * 0.5f * t;
        w[13] = (w[12] & 0xffffff) | (((int)(color_get_a(w[12]) * (t * t)) + 1) >> 1 << 24);
        if (--w[4] <= 0)
            FINISH_IF(w);
        break;
    }
}

/* 0x00cadee8 */
void venus_sp_draw_disp(char *w)
{
    ulMatrixPush();
    MTX_TRANS(*(Vec3 **)(w + 36));
    draw_set_blend(4, 1);
    flare_draw_here(*(int *)(w + 8), *(unsigned int *)(w + 52), *(float *)(w + 40));
    ulMatrixPop();
}

/* 0x00cadf48 */
int *venus_sp_draw_create(VenusFbManWork *parent, VenusFbManPod *pos, int tex, unsigned int color, int follow, float sec, float scl)
{
    int *w = task_create_sibling(parent, &venus_sp_draw_desc, 56);

    task_set_dlist(w, 6);
    if (!follow) {
        w[9] = (int)(w + 6);
        v3_copy((Vec3 *)(w + 6), (Vec3 *)pos);
    } else
        w[9] = (int)pos;
    *(float *)&w[11] = scl;
    w[2] = tex;
    w[12] = color;
    w[13] = color;
    w[4] = w[5] = sec * 0.5f * FPS;
    return w;
}

/* 0x00cae030 */
void venus_dust_respawn(char *dust, char *rec)
{
    float sc[4];
    Vec3 *v;

    {
        float zero = 0.0f;

        ((Vec3 *)(rec + 12))->y = -*(float *)(dust + 24);
        ((Vec3 *)(rec + 12))->z = zero;
        ((Vec3 *)(rec + 12))->x = zero;
    }
    ROT_X((int)DEG_TO_ANG(*(float *)(dust + 28)), sc, ((Vec3 *)(rec + 12))->y, ((Vec3 *)(rec + 12))->z);
    ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), (sc + 2), ((Vec3 *)(rec + 12))->x, ((Vec3 *)(rec + 12))->z);
    v3_sum((Vec3 *)rec, (Vec3 *)(dust + 12), (Vec3 *)(rec + 12));
    v = (Vec3 *)(rec + 12);
    vec_set_length(v, RAND_FLOAT_A() * (cmd_dust_vl - cmd_dust_vs) + cmd_dust_vs);
    *(float *)(rec + 60) = RAND_FLOAT_A() * (cmd_dust_vyl - cmd_dust_vys) + cmd_dust_vys;
    *(int *)(rec + 48) = *(int *)(rec + 52) = (RAND_FLOAT_A() * (cmd_dust_tl - cmd_dust_ts) + cmd_dust_ts) * FPS;
    if (!*(int *)(rec + 24)) {
        *(int *)(rec + 24) = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        *(int *)(rec + 28) = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        *(int *)(rec + 32) = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        do {
            *(int *)(rec + 36) = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_al - cmd_dust_as) + cmd_dust_as);
            *(int *)(rec + 40) = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_al - cmd_dust_as) + cmd_dust_as);
            *(int *)(rec + 44) = DEG_TO_ANG(RAND_FLOAT_A() * (cmd_dust_al - cmd_dust_as) + cmd_dust_as);
        } while (0);
    }
}

/* 0x00cae3b8 */
void venus_dust_exec(VenusDustWork *w)
{
    Vec3 dif;
    char *rec = w->buf;
    int n = w->n;
    int i;
    float rad_sq = w->rad * w->rad;
    float ring_r;

    for (; n; n--, rec += 64) {
        if (!*(int *)(rec + 56))
            continue;
        if (--*(int *)(rec + 48) <= 0) {
            if (cmd_dust_ffrm < w->life)
                venus_dust_respawn((char *)w, rec);
            else
                *(int *)(rec + 56) = 0;
        }
        ((Vec3 *)(rec + 12))->x *= cmd_dust_vr;
        ((Vec3 *)(rec + 12))->z *= cmd_dust_vr;
        ((Vec3 *)(rec + 12))->y += *(float *)(rec + 60);
        v3_sum((Vec3 *)rec, (Vec3 *)(rec + 12), (Vec3 *)rec);
        v3_sub(&dif, (Vec3 *)rec, &w->centre);
        if (dif.x * dif.x + dif.y * dif.y + dif.z * dif.z < rad_sq) {
            ring_r = ((Vec3 *)rec)->y - w->centre.y;
            ring_r = sqrtf(rad_sq - ring_r * ring_r);
            dif.y = 0.0f;
            vec_set_length(&dif, ring_r);
            ((Vec3 *)rec)->x = dif.x;
            ((Vec3 *)rec)->z = dif.z;
        }
        i3_sum((int *)(rec + 24), (int *)(rec + 24), (int *)(rec + 36));
    }
    w->rad += cmd_dust_dr;
    if (w->rad >= cmd_dust_rmax)
        w->rad = cmd_dust_rmax;
    else if (w->rad > cmd_dust_sr)
        w->spin += cmd_dust_dax;
    if (w->shown < w->n - cmd_dust_st) {
        for (i = 0; i < cmd_dust_st; i++) {
            ((struct { int pad[14]; int on; } *)(w->buf + w->shown * 64))->on = 1;
            ((struct { int pad[12]; int k; } *)w)->k++;
        }
    }
    if (--w->life <= 0)
        FINISH_IF(w);
}

/* 0x00cae6a8 */
void venus_dust_disp(VenusDustWork *w)
{
    VenusDustQuad quad = venus_dust_quad;
    TrailVtx out[4];
    ScreenCfg *scr = &screen_cfg;
    int n = w->n;
    char *rec = w->buf;
    TrailVtx *src, *dst;
    int i;
    float inv_q;

    ulGxSetTextureList(w->textures);
    out[0].c = out[1].c = out[2].c = out[3].c = 0x80000000;
    nj_color_strip_begin(0);
    for (; n; rec += 64, n--) {
        if (!*(int *)(rec + 56))
            continue;
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, ((Vec3 *)rec)->x, ((Vec3 *)rec)->y, ((Vec3 *)rec)->z);
        } while (0);
        venus_ptcl_rotate_yxz((int *)(rec + 24));
        src = quad.v;
        dst = out;
        for (i = 4; i; i--) {
            ulMatrixApplyPoint((float *)ulMatrixCur, &src->x, &dst->x);
            src++;
            inv_q = -1.0f / dst->q;
            dst->q = inv_q;
            dst->x = dst->x * inv_q * scr->sx + scr->cx;
            dst->y = dst->y * inv_q * scr->sy + scr->cy;
            dst++;
        }
        nj_color_strip_vertices(out, 4);
        ulMatrixPop();
    }
    nj_color_strip_end();
}

/* 0x00cae8c8 */
void venus_dust_release(VenusDustWork *w)
{
    heap_free(w->buf, 0);
}

/* 0x00cae8d8 */
VenusDustWork *venus_dust_create(void *parent, int arg)
{
    VenusDustWork *w = task_create_inherit(parent, &venus_dust_desc, 56);

    w->arg = arg;
    w->n = cmd_dust_num;
    w->buf = heap_block_tag(game_calloc(w->n << 6), str_venus_ptcl_c, 422);
    w->life = w->life_max = cmd_dust_t;
    w->centre.y = cmd_dust_sy;
    w->spin = cmd_dust_sax;
    return w;
}

/* 0x00cae970 */
void venus_ptcl_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
