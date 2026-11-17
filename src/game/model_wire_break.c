/*
 * Wire-break effect: a model's edges fly apart as spinning, fading line segments under gravity
 * and drag. Also the plain model node-tree draw walks.
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "model_wire_break.h"
#include "anim.h"
#include "color.h"
#include "fog.h"
#include "heap.h"
#include "light.h"
#include "line_batch.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "player.h"
#include "yuki_tool.h"
#include "task.h"
#include "system.h"

typedef struct WireBreakWork {
    char pad_00[8];
    float width;
    int timer;
    float grav[3];
    float drag;
    int col;
    int col_to;
    WbMode blend_src;
    WbMode blend_dst;
    float spd_min;
} WireBreakWork;

typedef struct WireBreakTaskCreate2WireBreakWork { char pad_00[48]; float spd_min; } WireBreakTaskCreate2WireBreakWork;
typedef struct WireBreakData { void *seg; int seg_cnt; float width; } WireBreakData;

#define ROT_X(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; \
        z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; \
        z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

typedef struct WireBreakLink { struct WireBreakLink *next; int val; } WireBreakLink;

/* steer(alias): the code that matched reads DcNode through these member types; one view
 * shared by those sites (src/game/model_wire_break.c). */
typedef struct DcNodeRotView {
    unsigned int flags;
    int *obj;
    float pos[3];
    int rot[3];
    float scale[3];
    struct DcNodeRotView *child;
    struct DcNodeRotView *next;
    float quat_w;
} DcNodeRotView;

/* steer(alias): the code that matched reads DcObject through these member types; one view
 * shared by those sites (src/game/model_wire_break.c). */
typedef struct DcObjectNodesView {
    char pad_00[4];
    DcSceneNode *nodes;
    int *anim;
    char pad_0c[8];
    void *chain;
    char pad_18[36];
    float time;
} DcObjectNodesView;

typedef struct WireBreakWorkView {
    DcObjectNodesView *obj;
    char pad_04[20];
    float spd;
    float rot_range;
    char pad_20[16];
    float spd_min;
} WireBreakWorkView;

typedef struct WireBreakDataView { int seg; int seg_cnt; float width; } WireBreakDataView;

extern struct TaskDesc wire_break_desc;
extern struct TaskDesc wire_break_desc2;
extern struct TaskDesc wire_break_desc3;
extern void nj_copy8(void *, void *, int);
extern int model_disp_cb;
extern int model_disp_cb_arg;
extern void ulSinCos(int, float *, float *);
extern WireBreakChain *wire_break_chain;
extern WireBreakSeg *wire_break_seg_out;
extern float wire_break_spd;
extern float wire_break_rot_range;
extern int wire_break_rot_ofs;
extern float wire_break_spd_min;
extern char gx_vertex_buffer[];
extern void ulGxSetMatrix(int, int);
extern void nj_cnk_wire_vlist(int, char *);
extern int model_disp_count;
extern int njPushMatrixEx(void);
extern void njPopMatrixEx(void);
extern void vu0_matrix_translate(int, float, float, float);
extern void nj_matrix_rotate_cur_ang3(int *, int);
extern void vu0_matrix_scale(int, float, float, float);
extern void ulMatrixPush(void);
extern void ulMatrixPop(void);
extern void ulMatrixApplyPoint(int, float *, float *);
extern void ulMatrixUnitRotation(int);
extern void nj_cnk_vu1_draw(int *);

static void wire_break_step(WireBreakData *w, float *force, float damp);
static void wire_break_draw_lines(WireBreakData *w, int col);
static void wire_break_setup(WireBreakDataView *w, WireBreakWorkView *src);
static void model_free_indirect(void **slot);

static inline void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int wire_break_default_force;

/* 0x0013d2e8 */
static void wire_break_init(WireBreakWork *w, WireBreakArg *src)
{
    wire_break_setup(w, src);
    w->timer = src->w;
    vcopy(w->grav, src->v);
    w->col = src->flags;
    w->blend_src = src->mode0;
    w->drag = src->drag;
    w->col_to = src->h;
    w->blend_dst = src->mode1;
    w->spd_min = src->spd_min;
}

/* 0x0013d370 */
static void wire_break_release(void *w)
{
    model_free_indirect(w);
}

/* Stage 2: steps segments, blends col toward col_to, arriving as timer runs out. */
/* 0x0013d378 */
static void wire_break_fade_exec(WireBreakWork *w)
{
    int left = w->timer - 1;

    w->timer = left;
    if (left <= 0) {
        if (w)
            C09_TASK(w)->flags |= 1;
    } else {
        wire_break_step(w, w->grav, w->drag);
        w->col = color_blend(w->col, w->col_to, 1.0f / (float)w->timer);
    }
}

/* 0x0013d400 */
static void wire_break_wait_exec(WireBreakWork *w)
{
    int left = w->timer - 1;

    w->timer = left;
    if (left <= 0) {
        if (w)
            C09_TASK(w)->flags |= 1;
    } else {
        C09_TASK(w)->exec = wire_break_fade_exec;
    }
}

/* 0x0013d440 */
static void wire_break_disp(WireBreakWork *w)
{
    int old = fog_set_enable(0);

    draw_set_blend(w->blend_src, w->blend_dst);
    wire_break_draw_lines(w, w->col);
    fog_set_enable(old);
}

/* Default blend modes, fixed segment speed. */
/* 0x0013d490 */
WireBreakWork *wire_break_task_create(int parent, int src)
{
    WireBreakWork *w = task_create_arg(parent, &wire_break_desc, 52, src);

    w->blend_src = 4;
    w->blend_dst = 5;
    w->spd_min = 1.0f;
    return w;
}

/* Fixed segment speed. */
/* 0x0013d4e0 */
static WireBreakTaskCreate2WireBreakWork *wire_break_task_create_no_blend(int parent, int src)
{
    WireBreakTaskCreate2WireBreakWork *w = task_create_arg(parent, &wire_break_desc2, 52, src);

    w->spd_min = 1.0f;
    return w;
}

/* 0x0013d518 */
static int wire_break_create(int parent, int src)
{
    return task_create_arg(parent, &wire_break_desc3, 52, src);
}

/* Sets default blend modes and a fixed segment speed in the args first. */
/* 0x0013d540 */
static void *wire_break_data_create_blend(WireBreakWork *w)
{
    void *data = heap_block_tag(game_malloc(12), "model_wire_break.c", 261);

    w->blend_src = 4;
    w->blend_dst = 5;
    w->spd_min = 1.0f;
    wire_break_setup(data, w);
    return data;
}

/* Sets a fixed segment speed in the args first. */
/* 0x0013d5b8 */
static void *wire_break_data_create_no_blend(WireBreakTaskCreate2WireBreakWork *w)
{
    void *data = heap_block_tag(game_malloc(12), "model_wire_break.c", 285);

    w->spd_min = 1.0f;
    wire_break_setup(data, w);
    return data;
}

/* 0x0013d620 */
static void *wire_break_data_create(int src)
{
    void *data = heap_block_tag(game_malloc(12), "model_wire_break.c", 307);

    wire_break_setup(data, src);
    return data;
}

/* Null allowed. */
/* 0x0013d678 */
static void wire_break_data_free(void *data)
{
    if (data) {
        model_free_indirect(data);
        heap_free(data, 0);
    }
}

/* Null force -> wire_break_default_force. */
/* 0x0013d6c0 */
void wire_break_step_default(WireBreakData *w, float *force, float damp)
{
    if (force == 0)
        force = &wire_break_default_force;
    wire_break_step(w, force, damp);
}

/* 0x0013d6d8 */
void wire_break_draw(void * obj, int col)
{
    wire_break_draw_lines(obj, col);
}

/* Returns 0 when src has no segments. */
/* 0x0013d6e0 */
static WireBreakData *wire_break_data_dup(WireBreakData *src)
{
    int n = src->seg_cnt;
    WireBreakData *dst;
    void *buf;
    int size;

    if (n <= 0)
        return 0;
    dst = heap_block_tag(game_malloc(12), "model_wire_break.c", 392);
    size = n * 44;
    buf = heap_block_tag(game_malloc(size), "model_wire_break.c", 393);
    nj_copy8(buf, src->seg, size);
    dst->seg = buf;
    dst->seg_cnt = n;
    dst->width = src->width;
    return dst;
}

/* 0x0013d798 */
static void wire_break_set_rate(WireBreakWork *w, float v)
{
    w->width = v;
}

/* Per-node callback and argument for the segment-building walk. */
/* 0x0013d7a0 */
void wire_break_set_globals(int cb, int cb_arg)
{
    model_disp_cb = cb;
    model_disp_cb_arg = cb_arg;
}

/*
 * Damps velocity, adds force, moves ends by velocity plus player travel (player_get_vec), spins
 * about the centre (X, then Y).
 */
/* 0x0013d7b8 */
static void wire_break_step(WireBreakData *w, float *force, float damp)
{
    float ofs[3];
    float *p;
    int n;
    float half;
    float mx, my, mz;
    float dx, dy, dz;

    if (w) {
        player_get_vec(ofs);
        n = w->seg_cnt;
        if (n > 0) {
            half = 0.5f;
            p = (float *)((char *)w->seg + 12);
            do {
                p[3] = p[3] * damp;
                p[4] = p[4] * damp;
                p[5] = p[5] * damp;
                p[3] = p[3] + force[0];
                p[4] = p[4] + force[1];
                p[5] = p[5] + force[2];
                *(p - 3) = *(p - 3) + p[3];
                *(p - 2) = *(p - 2) + p[4];
                *(p - 1) = *(p - 1) + p[5];
                p[0] = p[0] + p[3];
                p[1] = p[1] + p[4];
                p[2] = p[2] + p[5];
                *(p - 3) = *(p - 3) + ofs[0];
                *(p - 2) = *(p - 2) + ofs[1];
                *(p - 1) = *(p - 1) + ofs[2];
                p[0] = p[0] + ofs[0];
                p[1] = p[1] + ofs[1];
                p[2] = p[2] + ofs[2];
                mx = (*(p - 3) + p[0]) * half;
                my = (*(p - 2) + p[1]) * half;
                mz = (*(p - 1) + p[2]) * half;
                dx = *(p - 3) - mx;
                dy = *(p - 2) - my;
                dz = *(p - 1) - mz;
                ROT_X(*(int *)&p[6], dy, dz);
                ROT_Y(*(int *)&p[7], dx, dz);
                *(p - 3) = mx + dx;
                p[0] = mx - dx;
                *(p - 2) = my + dy;
                p[1] = my - dy;
                *(p - 1) = mz + dz;
                p[2] = mz - dz;
                p += 11;
                n--;
            } while (n);
        }
    }
}

/* steer(cast) */
/* 0x0013da28 */
static void wire_break_draw_lines(WireBreakData *w, int col)
{
    int line;
    int n;
    WireBreakSeg *seg;
    float width;

    if (w) {
        n = w->seg_cnt;
        width = w->width;
        if (n > 0) {
            seg = w->seg;
            line = line_set_color(col);
            do {
                line = ((int (*)(float, int, WireBreakSeg *, char *))line_add_v_w)(width, line, seg, (char *)seg + 12);
                n--;
                seg++;
            } while (n);
            do {
                line_flush(line);
            } while (0);
        }
    }
}

/* Sums link->val over the chain, excluding the terminating link. */
/* 0x0013da98 */
static int wire_break_chain_length(WireBreakLink *link)
{
    int sum = 0;

    if (link != 0) {
        if (link->next != 0) {
            do {
                sum += link->val;
                link = link->next;
            } while (link->next != 0);
        }
    }
    return sum;
}

/*
 * One segment per edge of the current link; velocity scaled by wire_break_spd (zero for an edge
 * centred on ref), random spins in +-rot_range degrees.
 */
/* 0x0013dad8 */
static void wire_break_fill_lines(char *verts, float *ref)
{
    unsigned short *ip = wire_break_chain->idx;
    int n = wire_break_chain->edge_cnt;
    char *out = (char *)wire_break_seg_out;
    float *p;
    float *vtx;
    float x0;
    float y0;
    float z0;
    float x1;
    float y1;
    float z1;
    float rx;
    float ry;
    float rz;
    float dx;
    float dy;
    float dz;

    if (n > 0) {
        do {
            rx = ref[0];
            ry = ref[1];
            rz = ref[2];
        } while (0);
        p = (float *)((char *)wire_break_seg_out + 24);
        do {
            vtx = (float *)(verts + ip[0] * 64);
            x0 = vtx[0];
            p[-6] = x0;
            dx = x0 - rx;
            y0 = vtx[1];
            p[-5] = y0;
            dy = y0 - ry;
            z0 = vtx[2];
            vtx = (float *)(verts + ip[1] * 64);
            ip += 2;
            p[-4] = z0;
            dz = z0 - rz;
            x1 = vtx[0];
            p[-3] = x1;
            dx += x1 - rx;
            y1 = vtx[1];
            p[-2] = y1;
            dy += y1 - ry;
            z1 = vtx[2];
            p[-1] = z1;
            dz += z1 - rz;
            if (fabsf(dx) < FLT_EPSILON && fabsf(dy) < FLT_EPSILON
                && fabsf(dz) < FLT_EPSILON) {
                p[0] = 0.0f;
                p[1] = 0.0f;
                p[2] = 0.0f;
            } else {
                float speed = wire_break_spd;

                p[0] = p[0] * speed;
                p[1] = p[1] * speed;
                p[2] = p[2] * speed;
            }
            *(int *)&p[3] = (int)DEG_TO_ANG(RAND_FLOAT_B() * wire_break_rot_range) - wire_break_rot_ofs;
            *(int *)&p[4] = (int)DEG_TO_ANG(RAND_FLOAT_B() * wire_break_rot_range) - wire_break_rot_ofs;
            out += 44;
            p += 11;
            n--;
        } while (n);
    }
    wire_break_seg_out = (WireBreakSeg *)out;
}

static inline void vscale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

/*
 * Like wire_break_fill_lines, but straight away from ref at wire_break_spd_min..1 times
 * wire_break_spd.
 */
/* 0x0013dd18 */
static void wire_break_fill_lines_rand(char *verts, float *ref)
{
    unsigned short *ip = wire_break_chain->idx;
    int n = wire_break_chain->edge_cnt;
    char *out = (char *)wire_break_seg_out;
    float *p;
    float *vtx;
    float x0, y0, z0, x1, y1, z1;
    float rx, ry, rz;
    float dx, dy, dz;
    float speed;

    if (n > 0) {
        rx = ref[0];
        ry = ref[1];
        rz = ref[2];
        do { p = (float *)((char *)wire_break_seg_out + 24); } while (0);
        do {
            vtx = (float *)(verts + ip[0] * 64);
            x0 = vtx[0];
            p[-6] = x0;
            dx = x0 - rx;
            y0 = vtx[1];
            p[-5] = y0;
            dy = y0 - ry;
            z0 = vtx[2];
            vtx = (float *)(verts + ip[1] * 64);
            ip += 2;
            p[-4] = z0;
            dz = z0 - rz;
            x1 = vtx[0];
            p[-3] = x1;
            dx += x1 - rx;
            y1 = vtx[1];
            p[-2] = y1;
            dy += y1 - ry;
            z1 = vtx[2];
            p[-1] = z1;
            dz += z1 - rz;
            if (fabsf(dx) < FLT_EPSILON && fabsf(dy) < FLT_EPSILON
                && fabsf(dz) < FLT_EPSILON) {
                p[0] = 0.0f;
                p[1] = 0.0f;
                p[2] = 0.0f;
            } else {
                vec3_normalize_into(p, dx, dy, dz);
                speed = (RAND_FLOAT_B() * (1.0f - wire_break_spd_min) + wire_break_spd_min) * wire_break_spd;
                vscale(p, p, speed);
            }
            *(int *)&p[3] = (int)DEG_TO_ANG(RAND_FLOAT_B() * wire_break_rot_range) - wire_break_rot_ofs;
            *(int *)&p[4] = (int)DEG_TO_ANG(RAND_FLOAT_B() * wire_break_rot_range) - wire_break_rot_ofs;
            out += 44;
            p += 11;
            n--;
        } while (n);
        wire_break_seg_out = (WireBreakSeg *)out;
    } else {
        wire_break_seg_out = (WireBreakSeg *)out;
    }
}

/* Per-object step of the build walk; random speeds unless wire_break_spd_min is 1. */
/* 0x0013df98 */
static void model_disp(int *obj, int ref)
{
    int vlist;
    char *col;

    if (!wire_break_chain)
        return;
    col = gx_vertex_buffer;
    vlist = *obj;
    ulGxSetMatrix(0, ulMatrixCur);
    if (vlist)
        nj_cnk_wire_vlist(vlist, col);
    if (wire_break_spd_min == 1.0f)
        wire_break_fill_lines(col, ref);
    else
        wire_break_fill_lines_rand(col, ref);
}

/*
 * Build walk, unanimated tree. Flag 0x400: quaternion rotation. All three no-transform bits: no
 * matrix push.
 */
/* 0x0013e038 */
static void model_node_disp_tree_xform(DcNodeRotView *node, int ref)
{
    unsigned int flags;
    int *obj;
    float *pos;

    do {
        flags = node->flags;
        obj = node->obj;
        if ((flags & 7) == 7) {
            if (model_disp_cb)
                (*(int (*(*))(DcNodeRotView *, int, int))&model_disp_cb)(node, model_disp_count, model_disp_cb_arg); /* steer(cast) */
            model_disp_count = model_disp_count + 1;
            if ((flags & 8) == 0)
                model_disp(obj, ref);
            (*(ModelKindEnt * *)&wire_break_chain) = (*(ModelKindEnt * *)&wire_break_chain)->next; /* steer(cast) */
            if ((flags & 0x10) == 0)
                model_node_disp_tree_xform(node->child, ref);
        } else {
            njPushMatrixEx();
            if ((flags & 1) == 0) {
                pos = node->pos;
                do {
                    vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
                } while (0);
            }
            if ((flags & 2) == 0) {
                if ((flags & 0x1000) == 0) {
                    if (flags & 0x400)
                        quat_to_matrix_split(node->quat_w, (float *)node->rot[0]);
                    else
                        nj_matrix_rotate_cur_ang3(node->rot, flags & 0x20);
                }
            }
            if ((flags & 4) == 0)
                vu0_matrix_scale(ulMatrixCur, node->scale[0], node->scale[1], node->scale[2]);
            if (model_disp_cb)
                (*(int (*(*))(DcNodeRotView *, int, int))&model_disp_cb)(node, model_disp_count, model_disp_cb_arg); /* steer(cast) */
            model_disp_count = model_disp_count + 1;
            if ((flags & 8) == 0)
                model_disp(obj, ref);
            (*(ModelKindEnt * *)&wire_break_chain) = (*(ModelKindEnt * *)&wire_break_chain)->next; /* steer(cast) */
            if ((flags & 0x10) == 0)
                model_node_disp_tree_xform(node->child, ref);
            njPopMatrixEx();
        }
        node = node->next;
    } while (node);
}

/*
 * Build walk, animated tree. Flag 8 skips the node's segments, 0x10 its children. Each node
 * advances wire_break_chain.
 */
/* 0x0013e230 */
static void model_node_disp_tree(DcSceneNode *node, int ref)
{
    unsigned int flags;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_disp_cb)
            (*(int (*(*))(DcSceneNode *, int, int))&model_disp_cb)(node, model_disp_count, model_disp_cb_arg); /* steer(cast) */
        model_disp_count = model_disp_count + 1;
        if ((flags & 8) == 0)
            model_disp(node->obj, ref);
        (*(ModelKindEnt * *)&wire_break_chain) = (*(ModelKindEnt * *)&wire_break_chain)->next; /* steer(cast) */
        if ((flags & 0x10) == 0)
            model_node_disp_tree(node->child, ref);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* One segment per edge, filled by one walk in the current pose. No edge chain -> nothing. */
/* 0x0013e320 */
static void wire_break_setup(WireBreakDataView *w, WireBreakWorkView *src)
{
    DcObjectNodesView *obj = src->obj;
    DcSceneNode *nodes = obj->nodes;
    int *anim = obj->anim;
    float f3c = obj->time;
    void *chain = obj->chain;
    int n;
    void *rec;
    float origin[3];

    if (chain) {
        n = wire_break_chain_length(chain);
        if (n <= 0) {
            w->seg = 0;
            w->seg_cnt = 0;
        } else {
            rec = heap_block_tag(game_malloc(n * 44), "model_wire_break.c", 1007);
            w->width = 2.0f;
            w->seg = (int)rec;
            w->seg_cnt = n;
            wire_break_chain = chain;
            wire_break_seg_out = rec;
            wire_break_spd = src->spd;
            wire_break_spd_min = src->spd_min;
            wire_break_rot_ofs = (int)DEG_TO_ANG(src->rot_range);
            wire_break_rot_range = src->rot_range + src->rot_range;
            model_disp_count = 0;
            origin[0] = 0;
            origin[1] = 0;
            origin[2] = 0;
            ulMatrixApplyPoint(ulMatrixCur, origin, origin);
            if (anim == 0) {
                model_node_disp_tree_xform(nodes, origin);
            } else {
                anim_start(anim, f3c);
                model_node_disp_tree(nodes, origin);
            }
        }
    }
}

/* 0x0013e488 */
static void model_free_indirect(void **slot)
{
    heap_free(*slot, 0);
}

/* Flag bits 0x3000000: billboard, faces the camera; light colour and ambient saved around it. */
/* 0x0013e498 */
static void model_node_draw_tree(DcSceneNode *node)
{
    float scale[3];
    int color;
    int ambient;
    int *obj;

    do {
        obj = node->obj;
        ulMatrixPush();
        anim_apply_node(node);
        if (obj) {
            if (node->flags & 0x3000000) {
                light_get_color_packed(&color);
                light_get_ambient_packed(&ambient);
                ulMatrixPush();
                yuki_matrix_get_scale(scale);
                ulMatrixUnitRotation(ulMatrixCur);
                vu0_matrix_scale(ulMatrixCur, scale[0], scale[1], scale[2]);
                nj_cnk_vu1_draw(obj);
                ulMatrixPop();
                light_set_color_packed(color);
                light_set_ambient_packed(ambient);
            } else {
                nj_cnk_vu1_draw(obj);
            }
        }
        if (node->child)
            model_node_draw_tree(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* 0x0013e5b0 */
static void model_node_draw_tree_anim(void *tree, int clip, float time)
{
    anim_start((void *)clip, time);
    model_node_draw_tree(tree);
}

/* Identical copy of model_node_draw_tree. */
/* 0x0013e5e0 */
static void wire_break_node_draw_tree(DcSceneNode *node)
{
    float scale[3];
    int color;
    int ambient;
    int *obj;

    do {
        obj = node->obj;
        ulMatrixPush();
        anim_apply_node(node);
        if (obj) {
            if (node->flags & 0x3000000) {
                light_get_color_packed(&color);
                light_get_ambient_packed(&ambient);
                ulMatrixPush();
                yuki_matrix_get_scale(scale);
                ulMatrixUnitRotation(ulMatrixCur);
                vu0_matrix_scale(ulMatrixCur, scale[0], scale[1], scale[2]);
                nj_cnk_vu1_draw(obj);
                ulMatrixPop();
                light_set_color_packed(color);
                light_set_ambient_packed(ambient);
            } else {
                nj_cnk_vu1_draw(obj);
            }
        }
        if (node->child)
            wire_break_node_draw_tree(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* 0x0013e6f8 */
void model_node_draw_tree2_anim(void *tree, int clip, float time)
{
    anim_start((void *)clip, time);
    wire_break_node_draw_tree(tree);
}
