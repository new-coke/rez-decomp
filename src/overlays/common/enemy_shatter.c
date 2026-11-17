/*
 * Break effect: a shot-down enemy's model flies apart, each drawn node a piece (with wireframe)
 * that scatters from the centre, carried by the player's scroll, slows, falls, spins and fades.
 * Parameters from the enemy type's table entry or the caller.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "color.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "nj_cnk_vu1.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "model.h"
#include "node_tree.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_copy(Vec3 *dst, Vec3 *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
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

static inline void v3_mul(Vec3 *dst, Vec3 *src, float scale)
{
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

/* allocation tag line numbers of the original source */
#ifdef VERSION_US
#define SRC_LINE(early, us) (us)
#else
#define SRC_LINE(early, us) (early)
#endif

/* enemy_shatter_types entry, 56 bytes */
typedef struct ShatterType {
    char *name;
    void *fmt;
    union {
        int param[12];          /* as debug variables */
        ShatterParams p;
    } p;
} ShatterType;

/* random unit vector */
#define V3_RAND(v) \
    do { \
        do { \
            v3_set(v, RAND_FLOAT_A() - 0.5f, RAND_FLOAT_A() - 0.5f, \
                   RAND_FLOAT_A() - 0.5f); \
        } while (v3_is_zero(v)); \
        vec3_normalize(v, v); \
    } while (0)

extern void matrix_get_or_set(void *m, float *dst);
extern int modeltest_compile_wire;
extern char str_enemy_shatter[];
extern char str_enemy_shatter_label[];
extern VarDef enemy_shatter_vardefs[12];
extern ShatterType enemy_shatter_types[3];
extern struct TaskDesc enemy_shatter_desc;
extern char str_enemy_shatter_c[];

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void i3_mul(int *dst, int *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void i3_scale(int *v, float k)
{
    v[0] = v[0] * k;
    v[1] = v[1] * k;
    v[2] = v[2] * k;
}

/* the three enemy types */
/* 0x00b103d8 stg01, 0x00b1b668 stg02, 0x00b147c0 stg03, 0x00b1b038 stg04, 0x00b2e650 stg05, 0x00af9990 stg20, 0x00b23d48 stg22, 0x00b03758 stg23 */
void enemy_shatter_vars_register(void)
{
    VarDef def;
    struct GwVarNode *group;
    struct GwVarNode *sub;
    unsigned int i, j;
    VarDef *ent;
    int *prm;

    memset(&def, 0, sizeof def);
    def.name = str_enemy_shatter;
    def.fmt = str_enemy_shatter_label;
    group = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    for (i = 0; i < 3; i++) {
        ent = enemy_shatter_vardefs;
        prm = enemy_shatter_types[i].p.param;
        def.name = enemy_shatter_types[i].name;
        def.fmt = enemy_shatter_types[i].fmt;
        sub = gw_varedit_add(&gw_varedit_root, group, (struct GwVarDef *)&def);
        for (j = 0; j < 12; j++) {
            ent->ptr = prm;
            prm++;
            ent++;
        }
        gw_varedit_add_table(&gw_varedit_root, sub, (struct GwVarDef *)enemy_shatter_vardefs);
    }
}

/* 0x00b10510 stg01, 0x00b1b7a0 stg02, 0x00b148f8 stg03, 0x00b1b170 stg04, 0x00b2e788 stg05, 0x00af9ac8 stg20, 0x00b23e80 stg22, 0x00b03890 stg23 */
void enemy_shatter_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* Depth-first, counting *nth down; 0 if not found. */
/* 0x00b10518 stg01, 0x00b1b7a8 stg02, 0x00b14900 stg03, 0x00b1b178 stg04, 0x00b2e790 stg05, 0x00af9ad0 stg20, 0x00b23e88 stg22, 0x00b03898 stg23 */
void *enemy_shatter_nth_node(unsigned int *node, int *nth)
{
    void *found;
    int rest;
    unsigned int *child;

    do {
        rest = *nth;
        if (!rest)
            return node;
        child = (unsigned int *)node[4];
        *nth = rest - 1;
        if (child && (found = enemy_shatter_nth_node(child, nth)) != 0)
            return found;
        node = (unsigned int *)node[3];
    } while (node);
    return 0;
}

/* 0x00b10588 stg01, 0x00b1b818 stg02, 0x00b14970 stg03, 0x00b1b1e8 stg04, 0x00b2e800 stg05, 0x00af9b40 stg20, 0x00b23ef8 stg22, 0x00b03908 stg23 */
void *enemy_shatter_nth_node_at(unsigned int *node, int nth)
{
    return enemy_shatter_nth_node(node, &nth);
}

/* Flag 8 hides a node, flag 0x10 its children. */
/* 0x00b105a8 stg01, 0x00b1b838 stg02, 0x00b14990 stg03, 0x00b1b208 stg04, 0x00b2e820 stg05, 0x00af9b60 stg20, 0x00b23f18 stg22, 0x00b03928 stg23 */
int enemy_shatter_count_drawn(unsigned int *node)
{
    int cnt = 0;

    do {
        if (!(node[0] & 8))
            cnt++;
        if (!(node[0] & 0x10))
            cnt += enemy_shatter_count_drawn((unsigned int *)node[11]);
        node = (unsigned int *)node[12];
    } while (node);
    return cnt;
}

/* Applies node transforms up to the *nth and stores its matrix in mtx. */
/* 0x00b10610 stg01, 0x00b1b8a0 stg02, 0x00b149f8 stg03, 0x00b1b270 stg04, 0x00b2e888 stg05, 0x00af9bc8 stg20, 0x00b23f80 stg22, 0x00b03990 stg23 */
void enemy_shatter_node_matrix_walk(unsigned int *node, int *nth, float *mtx)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (--*nth == -1)
            matrix_get_or_set(0, mtx);
        else if (!(node[0] & 0x10))
            enemy_shatter_node_matrix_walk((unsigned int *)node[11], nth, mtx);
        ulMatrixPop();
    } while (*nth >= 0 && (node = (unsigned int *)node[12]) != 0);
}

/* 0x00b106c0 stg01, 0x00b1b950 stg02, 0x00b14aa8 stg03, 0x00b1b320 stg04, 0x00b2e938 stg05, 0x00af9c78 stg20, 0x00b24030 stg22, 0x00b03a40 stg23 */
void enemy_shatter_mul_node_matrix(unsigned int *node, int nth)
{
    float mtx[16];

    if (nth >= 0) {
        ulMatrixPush();
        matrix_unit_set(0);
        enemy_shatter_node_matrix_walk(node, &nth, mtx);
        ulMatrixPop();
        vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx);
    }
}

/* Zero dir: any direction. Adds scatter and part of vel; piece idx draws node idx. */
/* 0x00b10728 stg01, 0x00b1b9b8 stg02, 0x00b14b10 stg03, 0x00b1b388 stg04, 0x00b2e9a0 stg05, 0x00af9ce0 stg20, 0x00b24098 stg22, 0x00b03aa8 stg23 */
void enemy_shatter_emit_piece(Shatter *w, unsigned int *src, Vec3 *pos, Vec3 *dir, Vec3 *vel, int idx)
{
    Piece *pc = &w->pieces[w->count];
    Vec3 fly, rnd;

    v3_copy(&pc->pos, pos);
    if (!v3_is_zero(dir)) {
        vec3_normalize(&fly, dir);
        V3_RAND(&rnd);
        v3_add(&fly, &rnd);
    } else {
        V3_RAND(&fly);
    }
    v3_add(&fly, vel);
    if (!v3_is_zero(&fly))
        vec3_normalize(&fly, &fly);
    else
        V3_RAND(&fly);
    v3_mul(&pc->vel, &fly, w->PowVec);
    v3_mul(&fly, vel, w->inherit);
    v3_add(&pc->vel, &fly);
    yuki_matrix_get_scale(&pc->scale);
    model_camera_angles(pc->ang);
    pc->src = src;
    i3_set(pc->spin, (int)(w->PowAng * RAND_FLOAT_A()),
           (int)(w->PowAng * RAND_FLOAT_A()), (int)(w->PowAng * RAND_FLOAT_A()));
    if (!(w->flags & 0x10000)) {
        *(int *)&pc->model = (int)model_list_nth(w->models, idx);
        pc->node = enemy_shatter_nth_node_at(w->tree, idx);
    }
    pc->alive = 1;
    w->count++;
}

/* 0x00b10d00 stg01, 0x00b1bf90 stg02, 0x00b150e8 stg03, 0x00b1b960 stg04, 0x00b2ef78 stg05, 0x00afa2b8 stg20, 0x00b24670 stg22, 0x00b04080 stg23 */
void enemy_shatter_emit_tree_from(Shatter *w, unsigned int *node, Vec3 *center, Vec3 *vel, int *idx)
{
    unsigned int fl;
    Vec3 npos, ofs;

    do {
        fl = node[0];
        ulMatrixPush();
        anim_apply_node(node);
        if (!(fl & 8)) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &npos.x);
            v3_sub(&ofs, &npos, center);
            enemy_shatter_emit_piece(w, node, &npos, &ofs, vel, *idx);
        }
        (*idx)++;
        if (!(fl & 0x10))
            enemy_shatter_emit_tree_from(w, (unsigned int *)node[11], center, vel, idx);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b10e20 stg01, 0x00b1c0b0 stg02, 0x00b15208 stg03, 0x00b1ba80 stg04, 0x00b2f098 stg05, 0x00afa3d8 stg20, 0x00b24790 stg22, 0x00b041a0 stg23 */
void enemy_shatter_emit_tree(Shatter *w, unsigned int *node, Vec3 *center, Vec3 *vel)
{
    int idx = 0;

    enemy_shatter_emit_tree_from(w, node, center, vel, &idx);
}

/* Velocity eased at first, then damped; pieces fall, spin and drop when out of view. */
/* 0x00b10e40 stg01, 0x00b1c0d0 stg02, 0x00b15228 stg03, 0x00b1baa0 stg04, 0x00b2f0b8 stg05, 0x00afa3f8 stg20, 0x00b247b0 stg22, 0x00b041c0 stg23 */
void enemy_shatter_move(Shatter *w)
{
    int ease = 0;
    Vec3 scr, v, step;
    int dspin[3];
    float rate, t, fall, vdec, adec;
    Piece *p;
    int n;

    player_get_frame_vel(&scr);
    player_get_frame_accel(&v);
    rate = w->RotSpdPlayer - 1.0f;
    v3_scale(&v, rate);
    v3_add(&scr, &v);
    t = w->time;
    fall = w->Gravity * (t - 0.5f);
    if (w->b_acc && t < w->frm_acc)
        ease = 1;
    if (ease) {
        vdec = ease_sine_in_out(t / w->frm_acc);
        adec = vdec;
    } else {
        vdec = w->RtoVecDec;
        adec = w->RtoAngDec;
    }
    p = w->pieces;
    n = w->count;
    do {
        Vec3 *pos = &p->pos;

        if (p->alive) {
            p->pos.x += scr.x;
            p->pos.y += scr.y;
            p->pos.z += scr.z;
            if (ease) {
                v3_mul(&step, &p->vel, vdec);
                p->pos.x += step.x;
                p->pos.y += step.y;
                p->pos.z += step.z;
            } else {
                v3_scale(&p->vel, vdec);
                p->pos.x += p->vel.x;
                p->pos.y += p->vel.y;
                p->pos.z += p->vel.z;
            }
            p->pos.y -= fall;
            if (view_sphere_visible(pos, ((float *)p->src[1])[5])) {
                if (ease) {
                    i3_mul(dspin, p->spin, adec);
                    i3_add(p->ang, dspin);
                } else {
                    i3_scale(p->spin, adec);
                    i3_add(p->ang, p->spin);
                }
            } else {
                p->alive = 0;
            }
        }
        p++;
    } while (--n);
}

/* 0x00b11170 stg01, 0x00b1c400 stg02, 0x00b15558 stg03, 0x00b1bdd0 stg04, 0x00b2f3e8 stg05, 0x00afa728 stg20, 0x00b24ae0 stg22, 0x00b044f0 stg23 */
void enemy_shatter_exec(Shatter *w)
{
    float t;

    enemy_shatter_move(w);
    t = w->time;
    if (w->TimeAlphaStart <= t)
        w->color[0] = -(t - w->TimeAlphaStart) / (w->TimeAlphaEnd - w->TimeAlphaStart);
    w->time = t + 1.0f;
    if (w->TimeAlphaEnd < w->time)
        FINISH_IF(w);
}

/* 0x00b111f8 stg01, 0x00b1c488 stg02, 0x00b155e0 stg03, 0x00b1be58 stg04, 0x00b2f470 stg05, 0x00afa7b0 stg20, 0x00b24b68 stg22, 0x00b04578 stg23 */
/* steer(cast) */
void enemy_shatter_disp(Shatter *w)
{
    void *fog = 0;
    float mtx[16];
    Piece *p;
    int n;
    int a;
    int blend;
    int *mdl;

    if (w->fog)
        fog = fog_set_enable(w->fog_v);
    vu0_matrix_copy(mtx, ulMatrixCur);
    if (!(w->flags & 0x10000)) {
        draw_set_blend(w->blend_a, w->blend_b);
        a = color_get_a(w->pcol_wire);
        draw_set_color((w->pcol_wire & 0xffffff) | ((int)(w->color[0] * a + a) + 1) >> 1 << 24);
        p = w->pieces;
        n = w->count;
        do {
            if (p->alive) {
                do {
                    Vec3 *p_ = &p->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                enemy_shatter_rotate_yxz(p->ang);
                vu0_matrix_scale(ulMatrixCur, p->scale.x, p->scale.y, p->scale.z);
                if (modeltest_compile_wire) {
                    mdl = (int *)p->src[1];
                    if (!(draw_cull_box(*(float *)&mdl[5], (float *)&mdl[2]) & 0x2f))
                        model_packet_draw_normal(p->node);
                } else {
                    model_obj_draw((void *)p->src[1], p->model);
                }
                ulMatrixLoad(mtx);
            }
            p++;
        } while (--n);
    } else {
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        ulGxSetTextureList(w->textures);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_set_override_color(w->color);
        p = w->pieces;
        n = w->count;
        do {
            if (p->alive) {
                do {
                    Vec3 *p_ = &p->pos;
                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                enemy_shatter_rotate_yxz(p->ang);
                vu0_matrix_scale(ulMatrixCur, p->scale.x, p->scale.y, p->scale.z);
                nj_cnk_vu1_draw((void *)p->src[1]);
                ulMatrixLoad(mtx);
            }
            p++;
        } while (--n);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
    }
    if (w->fog)
        fog_set_enable(fog);
}

/* Counts every node but does not descend below hidden children (flag 0x10). */
/* 0x00b11478 stg01, 0x00b1c708 stg02, 0x00b15860 stg03, 0x00b1c0d8 stg04, 0x00b2f6f0 stg05, 0x00afaa30 stg20, 0x00b24de8 stg22, 0x00b047f8 stg23 */
unsigned int *enemy_shatter_nth_any_node(unsigned int *node, int *nth)
{
    unsigned int *found;
    int rest;

    for (;;) {
        rest = (*nth)--;
        if (rest < 1)
            return node;
        if (!(node[0] & 0x10)) {
            found = enemy_shatter_nth_any_node((unsigned int *)node[11], nth);
            if (found)
                return found;
        }
        node = (unsigned int *)node[12];
        if (!node)
            return 0;
    }
}

/* 0x00b114f8 stg01, 0x00b1c788 stg02, 0x00b158e0 stg03, 0x00b1c158 stg04, 0x00b2f770 stg05, 0x00afaab0 stg20, 0x00b24e68 stg22, 0x00b04878 stg23 */
unsigned int *enemy_shatter_nth_any_node_at(unsigned int *node, int nth)
{
    unsigned int *found = enemy_shatter_nth_any_node(node, &nth);

    if (!found)
        found = node;
    return found;
}

/* Picks the model tree (or one piece) and wire data; one piece per drawn node. */
/* 0x00b11528 stg01, 0x00b1c7b8 stg02, 0x00b15910 stg03, 0x00b1c188 stg04, 0x00b2f7a0 stg05, 0x00afaae0 stg20, 0x00b24e98 stg22, 0x00b048a8 stg23 */
/* steer(cast) */
void enemy_shatter_init(Shatter *w, SpawnDesc *desc)
{
    unsigned int *root = (unsigned int *)*((int *(*)(void *, unsigned int))pnb_get_entry)(desc->models, desc->type);
#ifdef VERSION_US
    Vec3 nscr;
#endif
    Vec3 scr, vel;
    unsigned int fl;
    unsigned int sib;
    unsigned int kind;

#ifdef VERSION_US
    if (desc->kind & 0x80000) {
        player_get_frame_vel(&nscr);
        nscr.x = -nscr.x;
        nscr.y = -nscr.y;
        nscr.z = -nscr.z;
        ulMatrixPush();
        do {
            Vec3 *p_ = &nscr;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
    } else {
        player_get_frame_vel(&scr);
        v3_sub(&desc->pos, &desc->pos, &scr);
    }
#else
    player_get_frame_vel(&scr);
    v3_sub(&desc->pos, &desc->pos, &scr);
#endif
    if (desc->kind & 0x20000)
        w->root = enemy_shatter_nth_any_node_at(root, desc->idx);
    else
        w->root = root;
    w->inherit = desc->inherit;
    kind = desc->kind;
    w->flags = kind;
    if (w->force_wire)
        w->flags = kind & ~0x10000;
    if (!(w->flags & 0x10000)) {
        w->models = (void *)model_pnb_get_wire(desc->models, desc->type);
        if (desc->kind & 0x20000)
            w->models = model_list_nth(w->models, desc->idx);
        w->tree = wire_compile(w->root, w->models);
    }
    w->count = 0;
    w->pieces = heap_block_tag(game_malloc(enemy_shatter_count_drawn(w->root) * sizeof(Piece)),
                               str_enemy_shatter_c, SRC_LINE(882, 891));
    fl = desc->kind;
    if (v3_is_zero(&desc->v)) {
        vel.x = 0;
        vel.y = 0;
        vel.z = 0;
    } else {
        vec3_normalize(&vel, &desc->v);
    }
    ulMatrixPush();
    if (!(fl & 0x80000)) {
        matrix_unit_set(0);
        do {
            Vec3 *p_ = &desc->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        enemy_shatter_rotate_yxz(desc->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        vu0_matrix_scale(ulMatrixCur, desc->scale.x, desc->scale.y, desc->scale.z);
    }
    sib = 0;
    if (fl & 0x20000) {
        sib = w->root[12];
        w->root[12] = 0;
    }
    if (fl & 0x40000) {
        anim_start((void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(desc->models, desc->motion), desc->time);
        if (fl & 0x20000)
            enemy_shatter_mul_node_matrix(root, desc->idx - 1);
    } else {
        anim_start(0, 0.0f);
    }
    enemy_shatter_emit_tree(w, w->root, &desc->pos, &vel);
    if (fl & 0x20000)
        w->root[12] = sib;
    ulMatrixPop();
#ifdef VERSION_US
    if (desc->kind & 0x80000)
        ulMatrixPop();
#endif
}

/* 0x00b118a0 stg01, 0x00b1cb30 stg02, 0x00b15c88 stg03, 0x00b1c500 stg04, 0x00b2fb18 stg05, 0x00afae58 stg20, 0x00b25210 stg22, 0x00b04c20 stg23 */
void enemy_shatter_release(Shatter *w)
{
    if (!(w->flags & 0x10000))
        wire_free(w->tree);
    heap_free(w->pieces, 0);
}

/* params override the type's; fog: fog setting, else the default blend */
/* 0x00b118e8 stg01, 0x00b1cb78 stg02, 0x00b15cd0 stg03, 0x00b1c548 stg04, 0x00b2fb60 stg05, 0x00afaea0 stg20, 0x00b25258 stg22, 0x00b04c68 stg23 */
Shatter *enemy_shatter_create(void *parent, SpawnDesc *desc, float *params, int *fog)
{
    Shatter *work = task_create_sibling_inherit(parent, &enemy_shatter_desc, 120);
    ShatterType *typ;

    if (params) {
        work->force_wire = ((ShatterParams *)params)->force_wire;
        work->Gravity = ((ShatterParams *)params)->Gravity * 0.00027777778f;
        work->pcol_wire = ((ShatterParams *)params)->pcol_wire;
        work->PowVec = ((ShatterParams *)params)->PowVec;
        work->b_acc = ((ShatterParams *)params)->b_acc;
        work->PowAng = (int)DEG_TO_ANG(((ShatterParams *)params)->PowAng);
        work->RtoVecDec = ((ShatterParams *)params)->RtoVecDec;
        work->RtoAngDec = ((ShatterParams *)params)->RtoAngDec;
        work->RotSpdPlayer = ((ShatterParams *)params)->RotSpdPlayer;
        work->TimeAlphaStart = ((ShatterParams *)params)->TimeAlphaStart;
        work->TimeAlphaEnd = ((ShatterParams *)params)->TimeAlphaEnd;
        work->frm_acc = ((ShatterParams *)params)->frm_acc;
    } else {
        typ = &enemy_shatter_types[desc->kind & 3];
        work->force_wire = ((ShatterParams *)typ->p.param)->force_wire;
        work->Gravity = ((ShatterParams *)typ->p.param)->Gravity * 0.00027777778f;
        work->pcol_wire = ((ShatterParams *)typ->p.param)->pcol_wire;
        work->PowVec = ((ShatterParams *)typ->p.param)->PowVec;
        work->b_acc = ((ShatterParams *)typ->p.param)->b_acc;
        work->PowAng = (int)DEG_TO_ANG(((ShatterParams *)typ->p.param)->PowAng);
        work->RtoVecDec = ((ShatterParams *)typ->p.param)->RtoVecDec;
        work->RtoAngDec = ((ShatterParams *)typ->p.param)->RtoAngDec;
        work->RotSpdPlayer = ((ShatterParams *)typ->p.param)->RotSpdPlayer;
        work->TimeAlphaStart = ((ShatterParams *)typ->p.param)->TimeAlphaStart;
        work->TimeAlphaEnd = ((ShatterParams *)typ->p.param)->TimeAlphaEnd;
        work->frm_acc = ((ShatterParams *)typ->p.param)->frm_acc;
    }
    if (fog) {
        work->blend_a = fog[0];
        work->fog = 1;
        work->fog_v = ((void **)fog)[2];
        work->blend_b = fog[1];
    } else {
        work->blend_a = 4;
        work->blend_b = 1;
        work->fog = 0;
    }
    enemy_shatter_init(work, desc);
    return work;
}

/* 0x00b11ac8 stg01, 0x00b1cd58 stg02, 0x00b15eb0 stg03, 0x00b1c728 stg04, 0x00b2fd40 stg05, 0x00afb080 stg20, 0x00b25438 stg22, 0x00b04e48 stg23 */
void enemy_shatter_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
