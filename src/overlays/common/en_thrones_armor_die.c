/*
 * Thrones armour coming off: on death each plate flies off, slowing, spinning and fading, then
 * splits into two halves that drift apart until they shatter. The second task throws a single
 * plate that flies off and fades.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "debug.h"
#include "color.h"
#include "gw_varedit.h"
#include "heap.h"
#include "main.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_thrones_armor_die.h"
#include "en_shatter.h"
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

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

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

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void i3_add(int *dst, int *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

static inline void i3_scale(int *v, float k)
{
    v[0] *= k;
    v[1] *= k;
    v[2] *= k;
}

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

extern void *stage_models;

extern struct TaskDesc en_thrones_armor_die_desc;
extern struct TaskDesc en_thrones_armor_die2_desc;
extern DominionsBodyAndTailSet en_thrones_armor_die_sets[1];
extern VarDef en_thrones_armor_die_vardef;
extern VarDef en_thrones_armor_die_vardefs[11];

/* "enThronesArmorDie2", 260 bytes */
typedef struct EnThronesArmorDie2Work {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnThronesArmorDieRec *type;       /* 0x08: en_thrones_armor_die_sets */
    int kind;                    /* 0x0c: row of en_thrones_armor_die_recs */
    int t;                  /* 0x10: frames */
    ModelPart body;                /* 0x14 */
    Vec3 pos;                   /* 0x70 */
    int ang[3];                 /* 0x7c */
    Vec3 vel;                   /* 0x88 */
    int spin[3];                /* 0x94 */
    ModelPart body2;               /* 0xa0 */
    unsigned int col;         /* 0xfc: faded from color0 */
    unsigned int color0;        /* 0x100 */
} EnThronesArmorDie2Work;

/* "enThronesArmorDie", 724 bytes */
typedef struct EnThronesArmorDieWork {
    void *models;
    void *tex;                  /* 0x04 */
    struct EnThronesArmorDieRec *type;       /* 0x08: en_thrones_armor_die_sets */
    int kind;                    /* 0x0c: row of en_thrones_armor_die_recs */
    int t;                  /* 0x10: frames */
    EnThronesArmorPiece body[3];              /* 0x14 */
    unsigned int color;         /* 0x2cc: of the glow parts */
    unsigned int base;        /* 0x2d0 */
} EnThronesArmorDieWork;

/* en_thrones_armor_die_sets record, fields named as in en_thrones_armor_die_vardefs */
typedef struct EnThronesArmorDieRec {
    char *name;
    char *label;                /* 0x04 */
    float spdY;                 /* 0x08 */
    float spdZ;                 /* 0x0c */
    float spd_den;              /* 0x10 */
    int ang_add;                /* 0x14 */
    float ang_add_den;          /* 0x18 */
    float div_spd;              /* 0x1c */
    float div_spd_den;          /* 0x20 */
    int div_ang_add;            /* 0x24 */
    float div_ang_add_den;      /* 0x28 */
    float div_sec;              /* 0x2c */
    float explode_sec;          /* 0x30 */
} EnThronesArmorDieRec;

extern int en_thrones_armor_die_recs[][6];

/* 0x00b161b8 stg05, 0x00b10e48 stg22 */
void en_thrones_armor_die_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_thrones_armor_die_vardef);
    for (i = 0; i < 1; i++) {
        def = en_thrones_armor_die_vardefs;
        val = en_thrones_armor_die_sets[i].v;
        en_thrones_armor_die_vardef.name = en_thrones_armor_die_sets[i].name;
        en_thrones_armor_die_vardef.fmt = en_thrones_armor_die_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_thrones_armor_die_vardef);
        for (j = 0; j < 11; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_thrones_armor_die_vardefs);
    }
}

/* 0x00b162d0 stg05, 0x00b10f60 stg22 */
void en_thrones_armor_die_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b162d8 stg05, 0x00b10f68 stg22 */
/* steer(cast) */
void en_thrones_armor_die_init(EnThronesArmorDieWork *w, EnThronesArmorDieArg *prm)
{
    EnThronesArmorDieRec *typ = (EnThronesArmorDieRec *)en_thrones_armor_die_sets;
    int *rec;
    EnThronesArmorPiece *piece = w->body;
    Vec3 org;
    float sc[3][2];
    float x, y, z;
    int i;

    rec = en_thrones_armor_die_recs[prm->i];
    w->t = 0;
    w->kind = prm->i;
    w->type = typ;
    y = typ->spdZ * (1.0f / FPS);
    z = -(typ->spdY * (1.0f / FPS));
    w->color = w->base = prm->color;
    player_get_frame_vel(&org);
    x = 0.0f;
    v3_sub(&piece->pos, prm->pos, &org);
    {
        IVec3 *d_ = (IVec3 *)piece->ang;
        IVec3 *s_ = (IVec3 *)prm->ang;
        d_->x = s_->x;
        d_->y = s_->y;
        d_->z = s_->z;
    }
    dc_object_init(&piece->model, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, rec[0]), 0, 0);
    dc_object_rebuild(&piece->model);
    dc_object_init(&piece->body2, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, rec[5]), 0, 0);
    dc_object_set_wire(&piece->body2, model_pnb_get_wire(w->models, rec[5]));
    ROT_Z(prm->ang[2], sc[0], x, y);
    ROT_X(prm->ang[0], sc[1], y, z);
    ROT_Y(prm->ang[1], sc[2], x, z);
    v3_set(&piece->vel, x, y, z);
    i3_set(piece->spin, typ->ang_add / FPS, 0, 0);
    piece++;
    for (i = 0; i < 2; i++, piece++) {
        dc_object_init(&piece->model, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, rec[i + 1]), 0, 0);
        dc_object_rebuild(&piece->model);
    }
}

/* 0x00b165a8 stg05, 0x00b11238 stg22 */
void en_thrones_armor_die_release(EnThronesArmorDieWork *w)
{
    EnThronesArmorPiece *p;
    int i;

    dc_object_release(&w->body[0].model);
    dc_object_release(&w->body[0].body2);
    p = &w->body[1];
    for (i = 0; i < 2; i++, p++)
        dc_object_release(&p->model);
}

/* 0x00b16608 stg05, 0x00b11298 stg22 */
void en_thrones_armor_die_move_piece(void *w, EnThronesArmorPiece *p, float drag, float spin_drag)
{
    Vec3 org;

    player_get_frame_vel(&org);
    v3_add(&p->pos, &org);
    i3_add(p->ang, p->spin);
    v3_add(&p->pos, &p->vel);
    v3_scale(&p->vel, drag);
    i3_scale(p->spin, spin_drag);
}

/* 0x00b16748 stg05, 0x00b113d8 stg22 */
void en_thrones_armor_die_draw_piece(EnThronesArmorDieWork *work, EnThronesArmorPiece *piece)
{
    Vec3 *pos;

    if (view_sphere_visible(&piece->pos, piece->model.radius)) {
        ulMatrixPush();
        do {
            pos = &piece->pos;
            vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        } while (0);
        en_thrones_armor_die_rotate_yxz(piece->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(&piece->model);
        draw_set_blend(4, 5);
        draw_set_color(work->color);
        dc_object_draw(&piece->body2);
        ulMatrixPop();
    }
}

/* 0x00b16808 stg05, 0x00b11498 stg22 */
void en_thrones_armor_die_fall(EnThronesArmorDieWork *w)
{
    EnThronesArmorDieRec *rec = w->type;
    SpawnDesc desc;
    EnThronesArmorPiece *p;
    int i;

    p = &w->body[1];
    for (i = 0; i < 2; i++, p++)
        en_thrones_armor_die_move_piece(w, p, rec->div_spd_den, rec->div_ang_add_den);
    if (rec->explode_sec * FPS < ++w->t) {
        p = &w->body[1];
        for (i = 0; i < 2; i++, p++) {
            nj_fill8(&desc, 0, sizeof desc);
            v3_copy(&desc.pos, &p->pos);
            desc.models = stage_models;
            desc.type = *(int *)((long)(en_thrones_armor_die_recs[w->kind] + 3) + i * 4);
            i3_copy(desc.ang, p->ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            v3_copy(&desc.v, &p->vel);
            desc.kind = 0;
            en_shatter_spawn(w, &desc, 2);
        }
        FINISH_IF(w);
    }
}

/* 0x00b169b0 stg05, 0x00b11640 stg22 */
void en_thrones_armor_die_draw_pieces(EnThronesArmorDieWork *w)
{
    EnThronesArmorPiece *p = &w->body[1];
    int i;

    ulGxSetTextureList(w->tex);
    for (i = 0; i < 2; i++, p++)
        en_thrones_armor_die_draw_piece(w, p);
}

/* 0x00b16a08 stg05, 0x00b11698 stg22 */
/* steer(cast) */
void en_thrones_armor_die_exec(EnThronesArmorDieWork *w)
{
    EnThronesArmorDieRec *typ = w->type;
    EnThronesArmorPiece *main = &w->body[0];
    EnThronesArmorPiece *piece;
    float sc[3][2];
    float rate, alpha, x, y, z;
    int a = 0, r, g, b, ia;
    int spin, i;

    en_thrones_armor_die_move_piece(w, main, typ->spd_den, typ->ang_add_den);
    rate = w->t / (typ->div_sec * FPS);
    alpha = color_get_a(w->base) * (1.0f - rate);
    ia = 0.0f <= alpha ? (alpha < 255.0f ? (0.0f <= alpha ? (int)alpha : 0) : 255) : 0;
    a = ia;
    r = ((int (*)(unsigned int))color_get_r)(w->base);
    g = color_get_g(w->base);
    b = color_get_b(w->base);
    w->color = (a + 1) >> 1 << 24 | ((r + 1) >> 1 & 0xff | ((g + 1) << 7 & 0xff00 | ((b + 1) >> 1 & 0xff) << 16));
    if (++w->t > typ->div_sec * FPS) {
        EnThronesArmorPiece *body = &w->body[0];

        spin = typ->div_ang_add / FPS;
        w->t = 0;
        x = typ->div_spd * (1.0f / FPS);
        y = 0.0f;
        z = 0.0f;
        ROT_Z(body->ang[2], sc[0], x, y);
        ROT_X(body->ang[0], sc[1], y, z);
        ROT_Y(body->ang[1], sc[2], x, z);
        piece = &w->body[1];
        for (i = 0; i < 2; i++, piece++) {
            v3_copy(&piece->pos, &body->pos);
            piece->vel.x = x;
            piece->vel.y = y;
            piece->vel.z = z;
            v3_add(&piece->vel, &body->vel);
            i3_copy(piece->ang, body->ang);
            x = -x;
            y = -y;
            z = -z;
            i3_set(piece->spin, 0, spin * RAND_FLOAT_B(), 0);
            spin = -spin;
            i3_add(piece->spin, body->spin);
        }
        TASK_OF(w)->exec = (void (*)(void *))en_thrones_armor_die_fall;
        TASK_OF(w)->disp = (void (*)(void *))en_thrones_armor_die_draw_pieces;
    }
}

/* 0x00b16d98 stg05, 0x00b11a28 stg22 */
void en_thrones_armor_die_disp(EnThronesArmorDieWork *w)
{
    ulGxSetTextureList(w->tex);
    en_thrones_armor_die_draw_piece(w, &w->body[0]);
}

/* 0x00b16dc8 stg05, 0x00b11a58 stg22 */
void *en_thrones_armor_die_create(void *parent, void *prm)
{
    return task_create_sibling_inherit_arg(parent, &en_thrones_armor_die_desc, 724, prm);
}

/* 0x00b16df0 stg05, 0x00b11a80 stg22 */
/* steer(cast) */
void en_thrones_armor_die2_init(EnThronesArmorDie2Work *w, EnThronesArmorDieArg *prm)
{
    EnThronesArmorDieRec *typ = (EnThronesArmorDieRec *)en_thrones_armor_die_sets;
    int *rec;
    EnThronesArmorPiece *piece = (EnThronesArmorPiece *)&w->body;
    Vec3 org;
    float sc[3][2];
    float x, y, z;
    unsigned int col;

    rec = en_thrones_armor_die_recs[prm->i];
    w->t = 0;
    w->kind = prm->i;
    w->type = typ;
    y = typ->spdZ * (1.0f / FPS);
    z = -(typ->spdY * (1.0f / FPS));
    w->col = w->color0 = prm->color;
    player_get_frame_vel(&org);
    x = 0.0f;
    v3_sub(&w->pos, prm->pos, &org);
    {
        IVec3 *d_ = (IVec3 *)w->ang;
        IVec3 *s_ = (IVec3 *)prm->ang;
        d_->x = s_->x;
        d_->y = s_->y;
        d_->z = s_->z;
    }
    dc_object_init(&w->body, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, rec[0]), 0, 0);
    dc_object_rebuild(&w->body);
    dc_object_init(&w->body2, *((int *(*)(void *, unsigned int))pnb_get_entry)(stage_models, rec[5]), 0, 0);
    dc_object_set_wire(&w->body2, model_pnb_get_wire(w->models, rec[5]));
    ROT_Z(prm->ang[2], sc[0], x, y);
    ROT_X(prm->ang[0], sc[1], y, z);
    ROT_Y(prm->ang[1], sc[2], x, z);
    v3_set(&piece->vel, x, y, z);
    i3_set(piece->spin, typ->ang_add / FPS, 0, 0);
}

/* 0x00b17078 stg05, 0x00b11d08 stg22 */
void en_thrones_armor_die2_release(EnThronesArmorDie2Work *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->body2);
}

/* 0x00b170a8 stg05, 0x00b11d38 stg22 */
/* steer(cast) */
void en_thrones_armor_die2_exec(EnThronesArmorDie2Work *w)
{
    EnThronesArmorDieRec *typ = w->type;
    Vec3 org;
    float rate, alpha;
    int a = 0, r, g, b, ia;

    player_get_frame_vel(&org);
    v3_add(&w->pos, &org);
    v3_add(&w->pos, &w->vel);
    v3_scale(&w->vel, typ->spd_den);
    i3_add(w->ang, w->spin);
    i3_scale(w->spin, typ->ang_add_den);
    rate = w->t / (typ->div_sec * FPS);
    alpha = color_get_a(w->color0) * (1.0f - rate);
    ia = 0.0f <= alpha ? (alpha < 255.0f ? (0.0f <= alpha ? (int)alpha : 0) : 255) : 0;
    a = ia;
    r = ((int (*)(unsigned int))color_get_r)(w->color0);
    g = color_get_g(w->color0);
    b = color_get_b(w->color0);
    w->col = (a + 1) >> 1 << 24 | ((r + 1) >> 1 & 0xff | ((g + 1) << 7 & 0xff00 | ((b + 1) >> 1 & 0xff) << 16));
    if (typ->div_sec * FPS < ++w->t)
        FINISH_IF(w);
}

/* 0x00b17338 stg05, 0x00b11fc8 stg22 */
/* steer(cast) */
void en_thrones_armor_die2_disp(EnThronesArmorDie2Work *w)
{
    EnThronesArmorDieRec *rec = w->type;
    ModelPart *body = &w->body;
    float col_add[4];
    int prev;

    ulGxSetTextureList(w->tex);
    if (view_sphere_visible(&w->pos, body->radius)) {
        prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        heap_bzero(col_add, 16);
        col_add[0] = -(float)w->t / (rec->div_sec * FPS);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 0x800);
        draw_set_override_color(col_add);
        ulMatrixPush();
        do {
            Vec3 *pos = &w->pos;
            vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
        } while (0);
        en_thrones_armor_die_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(body);
        ulMatrixPop();
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(prev);
    }
}

/* 0x00b17468 stg05, 0x00b120f8 stg22 */
void *en_thrones_armor_die2_create(void *parent, void *prm)
{
    return task_create_sibling_inherit_arg(parent, &en_thrones_armor_die2_desc, 260, prm);
}

/* 0x00b17490 stg05, 0x00b12120 stg22 */
void en_thrones_armor_die_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
