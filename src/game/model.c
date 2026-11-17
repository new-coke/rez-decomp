/*
 * Model objects: node-tree model instances with clip and cursor animation, bounding radius, lock-on
 * points, double-buffered compiled packets and wires, and their draw walks. Also model file
 * loading, wire data (edge lists) from shapes, and timed texture uploads.
 */

#include "types_game.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "model.h"
#include "anim.h"
#include "file.h"
#include "heap.h"
#include "lockon.h"
#include "math_util.h"
#include "perform.h"
#include "render.h"
#include "yuki_tool.h"
#include "camera.h"
#include "system.h"

#define MAX(x, y) ((x) <= (y) ? (y) : (x))

typedef struct DcObjectColl {
    char pad_00[88];
    int life, scl_y, unk_60, key, unk_68;  /* unnamed ones: cleared, never read */
} DcObjectColl;

typedef struct DcObjectSlots { char pad_00[36]; int wire_buf[9]; float cache_time[9]; } DcObjectSlots;
#define HND(w, i) (((DcObjectSlots *)(w))->wire_buf[i])
#define TIM(w, i) (((DcObjectSlots *)(w))->cache_time[i])
typedef struct DcNodeView DcNodeView;
struct DcNodeView { unsigned int flags; DcNodeObj *obj; float v[3]; int rot[3]; float scl_x; char pad_24[8]; DcNodeView *child; DcNodeView *next; char pad_34[8]; float time; };
typedef struct DcNodeObjView DcNodeObjView;

struct DcNodeObjView {
    unsigned int flags;
    DcNodeObj *obj;
    float pos[3];
    int rot[3];
    float scale[3];
    DcNodeObjView *child;
    DcNodeObjView *next;
};

typedef struct DcObjectView {
    unsigned int flags;     /* 0x00 */
    void *obj;              /* 0x04 */
    int clip;                 /* 0x08 */
    int clip_b;                /* 0x0c */
    int clip_c;                /* 0x10 */
    char pad_14[24];
    void *tbl[2];           /* 0x2c */
    int dirty[2];           /* 0x34 */
    char pad_3c[12];
    float time[2];          /* 0x48 */
    char pad_50[4];
    DcObjectColl *arr;
    short count;
    char pad_5a[2];
} DcObjectView;

typedef struct DcObjectTreeView {
    unsigned int flags;     /* 0x00 */
    DcSceneNode *tree;          /* 0x04 */
    int clip;                 /* 0x08 */
    int clip_b;                /* 0x0c */
    int clip_c;                /* 0x10 */
    int wire_list;                /* 0x14 */
    char pad_18[4];
    int (*cb)(void *, int, int); /* 0x1c */
    int user;               /* 0x20 */
    int wire0;                /* 0x24 */
    char pad_28[4];
    int comp0;                /* 0x2c */
    char pad_30[24];
    float cache_time;             /* 0x48 */
    char pad_4c[4];
    float radius;           /* 0x50 */
    void *dbg;              /* 0x54 */
    char pad_58[2];
    short mode;             /* 0x5a */
} DcObjectTreeView;

typedef int (*NodeFn)(void *);

typedef struct DcNodeCbView {
    unsigned int flags;
    void *obj;
    void *obj8;
    char pad[16];
    NodeFn cb;
    int user;
    char pad_24[48];
    void *dbg;
} DcNodeCbView;

/* steer(alias): the code that matched reads DcNode through these member types; one view
 * shared by those sites (src/game/model.c). */
typedef struct DcNodeScaleView {
    unsigned int flags;
    void *obj;
    float pos[3];
    int rot[3];
    float scale[3];
    struct DcNodeScaleView *child;
    struct DcNodeScaleView *next;
} DcNodeScaleView;

typedef struct ModelExportRec {
    float time;
    float pos[3];
    float alpha;
    float col[3];
} ModelExportRec;

typedef struct ModelSnapshot { int cnt; float period; ModelExportRec *rec; } ModelSnapshot;
typedef struct ModelPartBuf { char pad_00[8]; void *rec; } ModelPartBuf;
#define CR(c) (((int)(c).v[3] & 0xff) << 16)
#define CG(c) (((int)(c).v[2] & 0xff) << 8)
#define CB(c) ((int)(c).v[1] & 0xff)
typedef struct ModelPath { int cnt; float period; float *rec; float end[3]; } ModelPath;

typedef struct ModelAllocator {
    void *(*alloc)(int);
    void *(*calloc)(int);
    void (*free)(void *, int);
} ModelAllocator;

typedef struct ModelTimedLoad { char pad_00[28]; void *res; } ModelTimedLoad;
typedef struct ModelEdgeVtx { char pad_00[16]; float x, y, z; } ModelEdgeVtx;
typedef struct ModelEdge { unsigned short vtx0, vtx1; } ModelEdge;
typedef struct ModelEdgeVtxView { int id; float v[6]; } ModelEdgeVtxView;

/* steer(alias): the code that matched reads DcNode through these member types; one view
 * shared by those sites (src/game/model.c). */
typedef struct DcNodePosView {
    unsigned int flags;
    int *obj;
    float pos[3];
    int rot[3];
    float scale[3];
    struct DcNodePosView *child;
    struct DcNodePosView *next;
    float quat_w;
} DcNodePosView;

extern void nj_compile_set_alloc(void *, void *);
extern void wire_alloc_set(void *, void *);
extern int model_clip_mask;
extern float vec3_length(float *);
extern int model_node_index;
extern void *model_coll_cur;
extern void wire_free(int);
extern int wire_tree_packet_size(int);
extern int wire_compile(int, int);
extern void nj_fill8(void *, int, int);
extern void nj_compile_call_hook(int);
extern int nj_compile_object(int);
extern float model_clip_probe_base[];
extern float model_clip_probe_dir[];
extern void ulSinCos(int, float *, float *);
extern float ulVec3Dot(float *, float *);
extern int model_node_cb_arg;
extern char model_axis_z[];
extern void vu0_matrix_translate(int, float, float, float);
extern void vu0_matrix_scale(int, float, float, float);
extern void ulMatrixPop(void);
extern void model_packet_draw_normal(DcWalkNode *);
extern void wire_write(int, DcNode *, int);
extern int (*model_draw_param_cb)(int);
extern int model_draw_param;
extern int ulMatrixPush(void);
extern void nj_cnk_vu1pkt_draw_clip(DcNodeObj *);
extern void nj_cnk_vu1pkt_draw_noclip(DcNodeObj *);
extern int nj_compile_object_refresh(void *, void *);
extern int ***anim_cur_cursor;
extern char model_origin[];
extern int modeltest_disp_obj;
extern int modeltest_disp_wire;
extern int modeltest_shape_once;
extern int modeltest_compile_wire;
extern int modeltest_compile_obj;
extern char model_white[];
extern void (*model_obj_draw_cb)();
extern void (*model_obj_draw_clip_cb)();
extern void dc_object_calc_radius(DcObjectTreeView *, float *, float *);
extern unsigned int draw_cull_box(float, char *);
extern void nj_cnk_vu1_draw_clip(void *);
extern void nj_cnk_vu1_draw_raw(void *);
extern NodeFn model_node_cb;
extern float ulVec3Distance(float *, float *);
extern void ulMatrixApplyPoint(int, float *, float *);

extern void ulGxBeginStrip(int);
extern void ulGxAddStrip(ModelLineVtx *, int);
extern void ulGxEndStrip(void);
extern void pnb_install_hooks(ModelAllocator *);
extern void pnb_free_handlers(void);
extern int tex_object_create(void *, int);
extern int tex_object_reload(int *);
extern ModelListNode *model_timed_list;
extern ModelListNode *tex_object_create_upload(int, int);
extern void gw_console_printf(const char *, ...);
extern void tex_object_release(ModelListNode *);
extern int pnb_register(void *);
extern int pnb_get_version(int);
extern PnbEntry *pnb_get_entry(int, int);
extern int modeltest_tex_reload;
extern void tex_object_update(ModelListNode *, int);
extern char str_model_version[];
extern int gw_varedit_root;
extern int modeltest_vars_group;
extern char modeltest_vardef[];
extern char modeltest_vardefs[];
extern int gw_varedit_add(int *, int, char *);
extern void gw_varedit_add_table(int *, int, char *);
extern ModelEdge *model_edges;
extern unsigned short *model_edge_out;
extern ModelEdgeVtxView *model_edge_vtxs;
extern int model_cmd_depth;
extern short *model_cmd_sub_streams[];
extern char gx_vertex_buffer[];
extern char model_wire_buf[];
extern void nj_copy8(void *, char *, int);
extern int njPushMatrixEx(void);
extern int njPopMatrixEx(void);
extern void model_obj_draw(int *, ModelKindEnt *);
extern int model_kind_list;
extern void draw_clear_poly_count(void);
extern void listener_add(int, void (*)(void));
extern void model_obj_draw_direct();
extern void model_obj_draw_normal();
extern void nj_matrix_rotate_cur_ang3();
extern void ulMatrixApplyVector();
extern void ulMatrixGetTranslation();

static void model_node_draw(DcNodeCbView *node, float time);
static void model_node_walk_cb(DcSceneNode *node);
static void model_node_walk_xform(DcNodeScaleView *node);
static int model_list_second(int list);
static void *model_malloc_tagged(int size);
static void *model_calloc_tagged(int size);

/* nj_compile_object hook; tagged "ModelCompiled". */
/* 0x00137db8 */
static void *compiled_model_alloc(int size)
{
    void *blk = game_malloc(size);

    heap_block_tag(blk, "ModelCompiled", -1);
    return blk;
}

/* nj_compile_object hook: freed three frames later; warns at 128+ waiting blocks. */
/* 0x00137df8 */
static void compiled_model_delay_free(void *blk)
{
    int n = heap_pool_add(blk, 3);

    if (n >= 128) {
        DEBUG_PRINTF("DelayFree Set : %d\n", n);
    }
}

/* wire_compile hook; tagged "WireCompiled". */
/* 0x00137e30 */
static void *compiled_wire_alloc(int size)
{
    void *blk = game_malloc(size);

    heap_block_tag(blk, "WireCompiled", -1);
    return blk;
}

/* wire_compile hook, as compiled_model_delay_free. */
/* 0x00137e70 */
static void compiled_wire_delay_free(void *blk)
{
    int n = heap_pool_add(blk, 3);

    if (n >= 128) {
        DEBUG_PRINTF("DelayFree Set : %d\n", n);
    }
}

/* 0x00137ea8 */
void compiled_alloc_register(void)
{
    nj_compile_set_alloc(compiled_model_alloc, compiled_model_delay_free);
    wire_alloc_set(compiled_wire_alloc, compiled_wire_delay_free);
}

/* Cull mask without the far bit. */
/* 0x00137ee0 */
void model_clip_far_off(void)
{
    model_clip_mask = 47;
}

/* 0x00137ef0 */
void model_clip_far_on(void)
{
    model_clip_mask = 63;
}

/* Rest pose: farthest shape sphere with node offsets and scales. */
/* 0x00137f00 */
static float model_node_bound_radius(DcNode *node, float ox, float oy, float oz, float sc)
{
    float radius, scl_max, t, child_radius;
    unsigned flags;
    DcNodeData *data;

    if (node == 0) {
        return 0;
    }
    radius = 0;
    do {
        ox += node->pos[0] * sc;
        oy += node->pos[1] * sc;
        oz += node->pos[2] * sc;
        flags = node->flags;
        scl_max = MAX(node->scl_z, MAX(node->scl_y, node->scl_x));
        if (!(flags & 8)) {
            data = node->data;
            t = sqrtf(ox * ox + oy * oy + oz * oz) + (vec3_length(data->center) + data->radius) * scl_max * sc;
            if (t <= radius) {
                t = radius;
            }
            radius = t;
        }
        if (!(flags & 0x10)) {
            child_radius = model_node_bound_radius(node->child, ox, oy, oz, sc * scl_max);
            radius = radius <= child_radius ? child_radius : radius;
        }
        node = node->next;
    } while (node);
    return radius;
}

/* Flag 0x2000 on any node (not under flag 0x10); such objects are not compiled. */
/* 0x001380d8 */
static int model_node_tree_flagged(DcNode *node)
{
    unsigned flags;

    if (node == 0) {
        return 0;
    }
    do {
        flags = node->flags;
        if (flags & 0x2000) {
            return 1;
        }
        if (!(flags & 0x10) && model_node_tree_flagged(node->child)) {
            return 1;
        }
        node = node->next;
    } while (node);
    return 0;
}

/* 0x00138140 */
static int dc_object_coll_count(DcNode *node)
{
    int cnt;
    unsigned fl;

    if (node == 0) {
        return 0;
    }
    cnt = 0;
    do {
        fl = node->flags;
        if (fl & 0x8000000) {
            cnt++;
        }
        if (!(fl & 0x10)) {
            cnt += dc_object_coll_count(node->child);
        }
        node = node->next;
    } while (node);
    return cnt;
}

/* Life and second value from the node's x and y scale; node index. */
/* 0x001381b8 */
static void dc_object_coll_fill(DcNode *node)
{
    unsigned flags;
    DcObjectColl *coll;
    int i;

    if (node == 0) {
        return;
    }
    do {
        flags = node->flags;
        if (flags & 0x8000000) {
            i = model_node_index;
            coll = model_coll_cur;
            model_coll_cur = coll + 1;
            coll->life = (int)node->scl_x;
            coll->scl_y = (int)node->scl_y;
            coll->key = i;
            coll->unk_68 = 0;
            coll->unk_60 = 0;
        }
        model_node_index++;
        if (!(flags & 0x10)) {
            dc_object_coll_fill(node->child);
        }
        node = node->next;
    } while (node);
}

/* Only with flag 0x2000 and a tree; flag 0x10: freed with the object. */
/* 0x00138258 */
static void dc_object_coll_build(MapPart *w)
{
    DcNode *root;
    short n;
    int cnt;

    root = w->root;
    if ((w->flags & 0x2000) && root) {
        cnt = dc_object_coll_count(root);
        n = cnt;
        w->ncoll = cnt;
        if (n > 0) {
            w->coll = heap_block_tag(game_malloc(n * 108), "DcObjectColl", -1);
            w->flags |= 0x10;
            model_node_index = 0;
            model_coll_cur = w->coll;
            dc_object_coll_fill(root);
        }
    } else {
        w->ncoll = 0;
        w->coll = 0;
    }
}

/* Wire drawing (0x500); compiles both wire buffers unless locked. Returns obj. */
/* 0x00138310 */
MapPart *dc_object_set_wire(MapPart *obj, int wire)
{
    if (obj->wire0) {
        wire_free(obj->wire0);
        obj->wire0 = 0;
    }
    if (obj->wire1) {
        wire_free(obj->wire1);
        obj->wire1 = 0;
    }
    obj->wire_list = wire;
    if (wire) {
        obj->flags = (obj->flags & -3841) | 0x500;
        if (obj->coll_lock == 0) {
            wire_tree_packet_size(wire);
            obj->wire0 = wire_compile((int)obj->root, wire);
            obj->wire1 = wire_compile((int)obj->root, wire);
        }
    }
    return obj;
}

/* w 0: allocates (flag 0x20). Radius, compile lock, animation mode/length, lock-on points. Or 0. */
/* 0x001383b8 */
MapPart *dc_object_init(MapPart *w, DcNode *root, DcClip *clip, unsigned flags)
{
    unsigned fl = flags & 0x3f00;
    float z;

    if (w == 0) {
        w = heap_block_tag(game_malloc(92), "DcObject", -1);
        fl |= 0x20;
    }
    if (w != 0) {
        nj_fill8(w, 0, 92);
        w->time = 0;
        z = w->time;
        w->flags = fl;
        w->root = root;
        w->clip = clip;
        w->flags2 = 0;
        w->unk_48 = 1.0e15f;
        w->unk_4c = 1.0e15f;
        w->speed = 1.0f;
        w->radius = model_node_bound_radius(root, z, z, z, 1.0f);
        w->coll_lock = model_node_tree_flagged(root);
        if (clip) {
            w->flags = (w->flags & -16) | 1;
            w->length = (float)clip->frames - 1.0f;
        } else {
            w->length = z;
            w->flags = w->flags & -16;
        }
        dc_object_coll_build(w);
    }
    return w;
}

/* All three clips: mode 2; main clip only: mode 1. */
/* 0x00138520 */
MapPart *dc_object_init_cursor(MapPart *w, DcNode *root, DcClip *clip, int clip_b, int clip_c, unsigned flags)
{
    unsigned fl = flags & 0x3f00;
    float z;

    if (w == 0) {
        w = heap_block_tag(game_malloc(92), "DcObject", -1);
        fl |= 0x20;
    }
    if (w != 0) {
        nj_fill8(w, 0, 92);
        w->time = 0;
        z = w->time;
        w->flags = fl;
        w->clip_b = clip_b;
        w->clip_c = clip_c;
        w->root = root;
        w->clip = clip;
        w->flags2 = 0;
        w->unk_48 = 1.0e15f;
        w->unk_4c = 1.0e15f;
        w->speed = 1.0f;
        w->radius = model_node_bound_radius(root, z, z, z, 1.0f);
        w->coll_lock = model_node_tree_flagged(root);
        if (w->clip == 0) {
            w->flags = w->flags & -16;
        } else {
            if (w->clip_b == 0 || w->clip_c == 0) {
                w->flags = (w->flags & -16) | 1;
            } else {
                w->flags = (w->flags & -16) | 2;
            }
            w->length = (float)clip->frames - 1.0f;
        }
        dc_object_coll_build(w);
    }
    return w;
}

/* Unless compilation is locked. */
/* 0x001386c0 */
void dc_object_rebuild(MapPart *obj)
{
    if (obj->coll_lock == 0) {
        if (obj->comp0) {
            nj_compile_call_hook(obj->comp0);
            obj->comp0 = 0;
        }
        if (obj->comp1) {
            nj_compile_call_hook(obj->comp1);
            obj->comp1 = 0;
        }
        obj->comp0 = nj_compile_object((int)obj->root);
        obj->comp1 = nj_compile_object((int)obj->root);
    }
}

/* 0x00138738 */
void dc_object_free_handles(MapPart *obj)
{
    if (obj->comp0) {
        nj_compile_call_hook(obj->comp0);
        obj->comp0 = 0;
    }
    if (obj->comp1) {
        nj_compile_call_hook(obj->comp1);
        obj->comp1 = 0;
    }
}

/* Compiles them when there are none. */
/* 0x00138788 */
void dc_object_refresh(MapPart *obj)
{
    if (obj->comp0 == 0) {
        dc_object_rebuild(obj);
        return;
    }
    nj_compile_object_refresh(obj->comp0, obj->root);
    nj_compile_object_refresh(obj->comp1, obj->root);
}

/* This frame's buffer. */
/* steer(cast) */
/* 0x001387d8 */
int dc_object_get_compiled(DcObjectCompiled *obj)
{
    return obj->comp[((int (*)(MapPart *))render_get_draw_slot)(obj)];
}

/* This frame's buffer. */
/* steer(cast) */
/* 0x00138808 */
int dc_object_get_wire(DcObjectWires *obj)
{
    return obj->wire[((int (*)(MapPart *))render_get_draw_slot)(obj)];
}

/* The object itself if dc_object_init allocated it, else only its tree. */
/* 0x00138838 */
void dc_object_release(MapPart *w)
{
    if (w) {
        if (w->flags & 0x10) {
            heap_free(w->coll, 0);
        }
        if (w->comp0) {
            nj_compile_call_hook(w->comp0);
            w->comp0 = 0;
        }
        if (w->comp1) {
            nj_compile_call_hook(w->comp1);
            w->comp1 = 0;
        }
        if (w->wire0) {
            wire_free(w->wire0);
            w->wire0 = 0;
        }
        if (w->wire1) {
            wire_free(w->wire1);
            w->wire1 = 0;
        }
        if (w->flags & 0x20) {
            do {
                heap_free(w, 0);
            } while (0);
        } else {
            w->root = 0;
        }
    }
}

/* Loops with flag 0x1000, else stops at the ends; flags2 bit 0 marks a wrap or stop frame. */
/* 0x00138900 */
void dc_object_advance_time(MapPart *obj)
{
    if (obj->clip) {
        obj->time += obj->speed;
        obj->flags2 &= ~1;
        while (obj->length < obj->time) {
            obj->flags2 |= 1;
            if (obj->flags & 0x1000) {
                obj->time -= obj->length;
            } else {
                obj->time = obj->length;
            }
        }
        while (obj->time < 0) {
            obj->flags2 |= 1;
            if (obj->flags & 0x1000) {
                obj->time += obj->length;
            } else {
                obj->time = 0;
            }
        }
    }
}

/* For a separate time x; returns it advanced. */
/* 0x001389e0 */
float dc_object_next_time(MapPart *obj, float x)
{
    if (obj->clip) {
        x += obj->speed;
        obj->flags2 &= ~1;
        while (obj->length < x) {
            obj->flags2 |= 1;
            if (obj->flags & 0x1000) {
                x -= obj->length;
            } else {
                x = obj->length;
            }
        }
        while (x < 0) {
            obj->flags2 |= 1;
            if (obj->flags & 0x1000) {
                x += obj->length;
            } else {
                x = 0;
            }
        }
    }
    return x;
}

/* Except the draw mode bits (0xf00). */
/* 0x00138aa0 */
void dc_object_set_flags(int *flags, int mask)
{
    *flags |= mask & ~0xf00;
}

/* 0x00138ab8 */
void dc_object_clear_flags(int *flags, int mask)
{
    *flags &= ~mask;
}

/* Wrapped below the clip length. */
/* 0x00138ad0 */
void dc_object_set_time(MapPart *obj, float x)
{
    while (obj->length <= x) {
        x -= obj->length;
    }
    obj->time = x;
}

/* Mode 1; clip 0 stops the animation. */
/* 0x00138b08 */
void dc_object_set_clip(MapPart *obj, DcClip *clip)
{
    if (clip) {
        obj->clip = clip;
        obj->length = (float)clip->frames - 1.0f;
        obj->flags = (obj->flags & ~0xf) | 1;
    } else {
        obj->clip = 0;
        obj->length = 0;
        obj->flags = obj->flags & ~0xf;
    }
}

/* Mode 2; without clip_b, dc_object_set_clip. */
/* 0x00138b80 */
void dc_object_set_clip_cursor(MapPart *obj, DcClip *clip, int clip_b, int clip_c)
{
    unsigned n;

    if (clip_b == 0) {
        dc_object_set_clip(obj, clip);
        return;
    }
    n = clip->frames;
    obj->clip_b = clip_b;
    obj->clip_c = clip_c;
    obj->clip = clip;
    obj->length = (float)n - 1.0f;
    obj->flags = (obj->flags & ~0xf) | 2;
}

/* Users: player_part_setup (player_part_node_cb), result_number_init (result_hit_callback). */
/* 0x00138bf0 */
void dc_object_set_callback(MapPart *obj, void *cb, void *cb_arg)
{
    obj->cb = cb;
    obj->cb_arg = cb_arg;
}

/* 0x00138c00 */
void dc_object_draw(MapPart *obj)
{
    model_object_draw(obj, obj->time);
}

/*
 * Cone half angle view_cone_ang: 1 inside, 2 partly or past the near plane, 0 outside. Code after
 * the first final return is dead.
 */
/* 0x00138c08 */
static int model_sphere_clip_test(float *p, float r)
{
    float nrm[4];
    float depth;
    float lim;
    float x, y, z, rate;
    float *base = model_clip_probe_base;
    float *dir = model_clip_probe_dir;

    ulSinCos(view_cone_ang, &depth, &lim);
    if (fabsf(depth) < FLT_EPSILON)
        return 0;
    if (-1.0f <= p[2] + r)
        return 2;
    rate = r / depth;
    x = base[0] + dir[0] * rate;
    y = base[1] + dir[1] * rate;
    z = base[2] + dir[2] * rate;
    vec3_normalize_into(nrm, p[0] - x, p[1] - y, p[2] - z);
    if (lim < ulVec3Dot(dir, nrm))
        return 1;
    x = base[0] - dir[0] * rate;
    y = base[1] - dir[1] * rate;
    z = base[2] - dir[2] * rate;
    vec3_normalize_into(nrm, p[0] - x, p[1] - y, p[2] - z);
    if (lim < ulVec3Dot(dir, nrm))
        return 2;
    return 0;
    if (-1.0f <= p[2] + r)
        return 2;
    rate = r / depth;
    vec3_normalize_into(nrm, p[0] - (base[0] + dir[0] * r), p[1] - (base[1] + dir[1] * r),
                        p[2] - (base[2] + dir[2] * r));
    if (lim < ulVec3Dot(dir, nrm))
        return 1;
    vec3_normalize_into(nrm, p[0] - (base[0] - dir[0] * r), p[1] - (base[1] - dir[1] * r),
                        p[2] - (base[2] - dir[2] * r));
    if (lim < ulVec3Dot(dir, nrm))
        return 2;
    return 0;
}

/* See map_node_anim_walk. */
/* 0x00138da8 */
static void model_node_anim_walk(DcNode *node)
{
    unsigned flags;
    DcNodeData *data;

    do {
        flags = node->flags;
        data = node->data;
        if ((flags & 0x80) == 0) {
            if (data && (flags & 8) == 0) {
                if (**anim_cur_cursor != 0) {
                    anim_cursor_play(data);
                }
            }
            anim_cursor_next();
        }
        if ((flags & 0x10) == 0) {
            model_node_anim_walk(node->child);
        }
        node = node->next;
    } while (node != 0);
}

/* Rest pose, node and wire trees in step: lock-on points, callback, wire packet unless culled. */
/* 0x00138e40 */
static void model_node_draw_walk_xform(DcNode *node, DcWalkNode *w)
{
    unsigned fl;
    int ok;
    float *pos;

    do {
        fl = node->flags;
        ulMatrixPush();
        pos = node->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
        } while (0);
        nj_matrix_rotate_cur_ang3(node->rot, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, node->scl_x, node->scl_y, node->scl_z);
        if (model_coll_cur != 0 && (fl & 0x8000000)) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur += 108;
        }
        if (model_node_cb) {
            ok = (*(int (*(*))(DcNode *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        } else {
            ok = 1;
        }
        model_node_index++;
        if (!(fl & 8)) {
            if (ok) {
                if (!(draw_cull_box(node->data->radius, node->data->center) & model_clip_mask)) {
                    model_packet_draw_normal(w);
                }
            }
        }
        if (!(fl & 0x10)) {
            model_node_draw_walk_xform(node->child, w->child);
        }
        ulMatrixPop();
        node = node->next;
        w = w->next;
    } while (node);
}

/* Animated transforms. */
/* 0x00138fe8 */
static void model_node_draw_walk(DcNode *node, DcWalkNode *w)
{
    unsigned fl;
    int ok;

    do {
        fl = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_coll_cur != 0 && (fl & 0x8000000)) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur += 108;
        }
        if (model_node_cb) {
            ok = (*(int (*(*))(DcNode *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        } else {
            ok = 1;
        }
        model_node_index++;
        if (!(fl & 8)) {
            if (ok) {
                if (!(draw_cull_box(node->data->radius, node->data->center) & model_clip_mask)) {
                    model_packet_draw_normal(w);
                }
            }
        }
        if (!(fl & 0x10)) {
            model_node_draw_walk(node->child, w->child);
        }
        ulMatrixPop();
        node = node->next;
        w = w->next;
    } while (node);
}

/* Mode 0 rest, 1 clip, 2 cursor animation; wire buffer rewritten when the time changed. */
/* steer(cast) */
/* 0x00139158 */
static void model_object_draw_kind(MapPart *w, float t)
{
    int mode = w->flags & 0xf;
    int idx = ((int (*)(MapPart *))render_get_draw_slot)(w);

    switch (mode) {
    case 0:
        model_node_draw_walk_xform(w->root, HND(w, idx));
        break;
    case 1:
        anim_start(w->clip, t);
        model_node_draw_walk(w->root, HND(w, idx));
        break;
    case 2:
        if (TIM(w, idx) != t) {
            anim_start(w->clip, t);
            anim_set_slot_table(w->clip_c);
            anim_select_cursor(0);
            anim_cursor_start(w->clip_b, t);
            model_node_anim_walk(w->root);
            wire_write(HND(w, idx), w->root, w->wire_list);
            TIM(w, idx) = t;
        }
        anim_start(w->clip, t);
        model_node_draw_walk(w->root, HND(w, idx));
        break;
    }
}

/*
 * No compiled buffers: model_obj_draw_normal for shapes crossing the screen edge, else
 * model_obj_draw_direct; post callback steps model_draw_param. mode != 0: all clipped, no culling.
 */
/* 0x001392e0 */
static void model_node_draw_walk_mode(DcNode *node, short mode)
{
    unsigned fl;
    int ok, mask;
    float *pos;

    do {
        fl = node->flags;
        ulMatrixPush();
        pos = node->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
        } while (0);
        nj_matrix_rotate_cur_ang3(node->rot, fl & 0x20);
        vu0_matrix_scale(ulMatrixCur, node->scl_x, node->scl_y, node->scl_z);
        if (model_coll_cur != 0 && (fl & 0x8000000)) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur += 108;
        }
        if (model_node_cb) {
            ok = (*(int (*(*))(DcNode *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        } else {
            ok = 1;
        }
        model_node_index++;
        if (!(fl & 8)) {
            if (ok) {
                if (mode != 0) {
                    model_obj_draw_normal(node->data, model_draw_param);
                } else {
                    mask = draw_cull_box(node->data->radius, node->data->center);
                    if (!(mask & model_clip_mask)) {
                        mask &= 0xff00;
                        if (mask) {
                            model_obj_draw_normal(node->data, model_draw_param);
                        } else {
                            model_obj_draw_direct(node->data, model_draw_param);
                        }
                    }
                }
            }
        }
        if (model_draw_param_cb) {
            model_draw_param = model_draw_param_cb(model_draw_param);
        }
        if (!(fl & 0x10)) {
            model_node_draw_walk_mode(node->child, mode);
        }
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* Animated transforms. */
/* 0x001394c8 */
static void model_node_draw_tree_rec(DcNodeView *node, short mode)
{
    unsigned int flags;
    int mask;
    int ok;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(void *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 8) == 0 && ok != 0) {
            DcNodeObj *obj = node->obj;
            if (mode != 0) {
                model_obj_draw_normal((int)obj, model_draw_param);
            } else {
                mask = draw_cull_box(obj->radius, obj->center);
                if (!(mask & model_clip_mask)) {
                    mask &= 0xff00;
                    if (mask)
                        model_obj_draw_normal((int)node->obj, model_draw_param);
                    else
                        model_obj_draw_direct((int)node->obj, model_draw_param);
                }
            }
        }
        if (model_draw_param_cb != 0)
            model_draw_param = model_draw_param_cb(model_draw_param);
        if ((flags & 0x10) == 0)
            model_node_draw_tree_rec(node->child, mode);
        ulMatrixPop();
        node = node->next;
    } while (node != 0);
}

/* Rest pose, always culling. */
/* 0x00139678 */
static void model_node_draw_tree_xform_pair(DcNodeObjView *node, DcNodeObjView *other)
{
    unsigned int flags;
    unsigned int mask;
    int ok;
    DcNodeObj *obj;
    float *pos;

    do {
        flags = node->flags;
        ulMatrixPush();
        pos = node->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
        } while (0);
        nj_matrix_rotate_cur_ang3(node->rot, flags & 0x20);
        vu0_matrix_scale(ulMatrixCur, node->scale[0], node->scale[1], node->scale[2]);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(void *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 8) == 0 && ok != 0) {
            obj = node->obj;
            mask = draw_cull_box(obj->radius, obj->center);
            if (!(mask & model_clip_mask)) {
                mask &= 0xff00;
                if (mask)
                    nj_cnk_vu1pkt_draw_clip(other->obj);
                else
                    nj_cnk_vu1pkt_draw_noclip(other->obj);
            }
        }
        if ((flags & 0x10) == 0)
            model_node_draw_tree_xform_pair(node->child, other->child);
        ulMatrixPop();
        node = node->next;
        other = other->next;
    } while (node);
}

/* Animated; culls per shape, clips across the screen edge. mode != 0: all clipped, no culling. */
/* 0x00139838 */
static void model_node_draw_tree_pair(DcNodeView *node, DcNodeView *other, int mode)
{
    unsigned int flags;
    unsigned int mask;
    int ok;
    DcNodeObj *obj;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(void *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 8) == 0 && ok != 0) {
            if (mode != 0) {
                nj_cnk_vu1pkt_draw_clip(other->obj);
            } else {
                obj = node->obj;
                mask = draw_cull_box(obj->radius, obj->center);
                if ((mask & model_clip_mask) == 0) {
                    mask &= 0xff00;
                    if (mask)
                        nj_cnk_vu1pkt_draw_clip(other->obj);
                    else
                        nj_cnk_vu1pkt_draw_noclip(other->obj);
                }
            }
        }
        if ((flags & 0x10) == 0)
            model_node_draw_tree_pair(node->child, other->child, mode);
        ulMatrixPop();
        node = node->next;
        other = other->next;
    } while (node);
}

/* Mode 0 rest, 1 animated (dirty buffer refreshed first), 2 cursor rebuild when time changes. */
/* 0x001399d0 */
static void model_part_draw(DcObjectView *part, float time)
{
    int kind = part->flags & 0xf;
    int idx = render_get_draw_slot();

    switch (kind) {
    case 0:
        if (part->dirty[idx]) {
            part->dirty[idx] = 0;
            nj_compile_object_refresh(part->tbl[idx], part->obj);
        }
        model_node_draw_tree_xform_pair(part->obj, part->tbl[idx]);
        break;
    case 1:
        if (part->dirty[idx]) {
            part->dirty[idx] = 0;
            nj_compile_object_refresh(part->tbl[idx], part->obj);
        }
        anim_start(part->clip, time);
        model_node_draw_tree_pair(part->obj, part->tbl[idx], 0);
        break;
    case 2:
        if (part->time[idx] != time || part->dirty[idx]) {
            anim_start(part->clip, time);
            anim_set_slot_table(part->clip_c);
            anim_select_cursor(0);
            anim_cursor_start(part->clip_b, time);
            model_node_anim_walk(part->obj);
            nj_compile_object_refresh(part->tbl[idx], part->obj);
            part->time[idx] = time;
            part->dirty[idx] = 0;
        }
        anim_start(part->clip, time);
        model_node_draw_tree_pair(part->obj, part->tbl[idx], 1);
        break;
    }
}

/* Cursor on each shape unless node flag 0x80; callback-accepted shapes via model_obj_draw_cb. */
/* 0x00139bf8 */
static void model_node_draw_tree_cursor(DcSceneNode *node, short mode)
{
    unsigned int flags;
    int *obj;
    int ok;

    do {
        flags = node->flags;
        ulMatrixPush();
        obj = node->obj;
        anim_apply_node(node);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(DcSceneNode *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 0x80) == 0) {
            if (obj && (flags & 8) == 0) {
                if (**anim_cur_cursor)
                    anim_cursor_play(obj);
                if (ok)
                    model_obj_draw_cb(node->obj);
            }
            anim_cursor_next();
        } else {
            if (ok) {
                if (obj) {
                    if ((flags & 8) == 0)
                        model_obj_draw_cb(node->obj);
                }
            }
        }
        if ((flags & 0x10) == 0)
            model_node_draw_tree_cursor(node->child, mode);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* model_obj_draw_cb across the screen edge (all with mode != 0), else model_obj_draw_clip_cb. */
/* 0x00139da8 */
static void model_node_draw_tree_cb(DcNodeView *node, int mode)
{
    unsigned int flags;
    unsigned int mask;
    int ok;
    DcNodeObj *obj;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(void *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 8) == 0 && ok != 0) {
            if (mode != 0) {
                model_obj_draw_cb(node->obj);
            } else {
                obj = node->obj;
                mask = draw_cull_box(obj->radius, obj->center);
                if ((mask & model_clip_mask) == 0) {
                    mask &= 0xff00;
                    if (mask)
                        model_obj_draw_cb(node->obj);
                    else
                        model_obj_draw_clip_cb(node->obj);
                }
            }
        }
        if ((flags & 0x10) == 0)
            model_node_draw_tree_cb(node->child, mode);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* Rest pose. */
/* 0x00139f40 */
static void model_node_draw_tree_cb_xform(DcNodeObjView *node, int mode)
{
    unsigned int flags;
    unsigned int mask;
    int ok;
    DcNodeObj *obj;
    float *pos;

    do {
        flags = node->flags;
        ulMatrixPush();
        pos = node->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
        } while (0);
        nj_matrix_rotate_cur_ang3(node->rot, flags & 0x20);
        vu0_matrix_scale(ulMatrixCur, node->scale[0], node->scale[1], node->scale[2]);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb != 0)
            ok = (*(int (*(*))(void *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        else
            ok = 1;
        model_node_index = model_node_index + 1;
        if ((flags & 8) == 0 && ok != 0) {
            if (mode != 0) {
                model_obj_draw_cb(node->obj);
            } else {
                obj = node->obj;
                mask = draw_cull_box(obj->radius, obj->center);
                if (!(mask & model_clip_mask)) {
                    mask &= 0xff00;
                if (mask)
                        model_obj_draw_cb(node->obj);
                    else
                        model_obj_draw_clip_cb(node->obj);
                }
            }
        }
        if ((flags & 0x10) == 0)
            model_node_draw_tree_cb_xform(node->child, mode);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/*
 * Radius check in debug mode. Wholly out of view: node walk only (callbacks, lock-on points).
 * modeltest switches draw compiled wire or packets; wire objects (0x500) use the wire walks.
 */
/* 0x0013a110 */
void model_object_draw(DcObjectTreeView *obj, float time)
{
    int kind = obj->flags & 0xf;
    int hi = obj->flags & 0xf00;
    float bound[2];

    if (obj->tree == 0)
        return;
    if (debug_flags_test(8)) {
        dc_object_calc_radius(obj, &bound[0], &bound[1]);
        if (obj->radius < bound[1])
            ERROR_PRINTF("ObjectR Error !!(%f, %f)\n", obj->radius, bound[1]);
    }
    model_draw_param = (int)model_white;
    model_draw_param_cb = 0;
    model_node_index = 0;
    model_coll_cur = obj->dbg;
    model_node_cb = obj->cb;
    model_node_cb_arg = obj->user;
    if (obj->mode == 0 && (draw_cull_box(obj->radius, model_origin) & model_clip_mask)) {
        model_node_draw(obj, time);
        return;
    }
    if (modeltest_compile_wire != 0 && obj->wire0 != 0 && hi == 0x500) {
        if (modeltest_disp_wire == 0)
            return;
        model_object_draw_kind(obj, time);
        return;
    }
    if (modeltest_compile_obj != 0 && obj->comp0 != 0 && hi != 0x500) {
        if (modeltest_disp_obj == 0)
            return;
        model_part_draw(obj, time);
        return;
    }
    if (hi != 0x500)
        goto normal;
    model_draw_param_cb = model_list_second;
    model_draw_param = obj->wire_list;
    switch (kind) {
    case 0:
        model_node_draw_walk_mode(obj->tree, obj->mode);
        return;
    case 1:
        break;
    case 2:
        if (obj->cache_time != time) {
            anim_start(obj->clip, time);
            anim_set_slot_table(obj->clip_c);
            anim_select_cursor(0);
            anim_cursor_start(obj->clip_b, time);
            model_node_anim_walk(obj->tree);
            obj->cache_time = time;
        }
        break;
    default:
        return;
    }
    anim_start(obj->clip, time);
    model_node_draw_tree_rec(obj->tree, obj->mode);
    return;
normal:
    if (obj->mode != 0) {
        model_obj_draw_cb = nj_cnk_vu1_draw_clip;
        model_obj_draw_clip_cb = nj_cnk_vu1_draw_clip;
    } else {
        model_obj_draw_cb = nj_cnk_vu1_draw_clip;
        model_obj_draw_clip_cb = nj_cnk_vu1_draw_raw;
    }
    switch (kind) {
    case 0:
        model_node_draw_tree_cb_xform(obj->tree, obj->mode);
        return;
    case 1:
        anim_start(obj->clip, time);
        model_node_draw_tree_cb(obj->tree, obj->mode);
        return;
    case 2:
        anim_start(obj->clip, time);
        if (modeltest_shape_once != 0) {
            if (obj->cache_time == time) {
                model_node_draw_tree_cb(obj->tree, 1);
                return;
            }
            anim_set_slot_table(obj->clip_c);
            anim_select_cursor(0);
            anim_cursor_start(obj->clip_b, time);
            model_node_draw_tree_cursor(obj->tree, obj->mode);
            obj->cache_time = time;
        } else {
            anim_set_slot_table(obj->clip_c);
            anim_select_cursor(0);
            anim_cursor_start(obj->clip_b, time);
            model_node_draw_tree_cursor(obj->tree, obj->mode);
        }
        return;
    }
}

/* See model_node_draw. */
/* 0x0013a4e8 */
void model_node_run_default(struct DcNodeView *node)
{
    model_node_draw(node, node->time);
}

/* Mode 0 rest or animated: lock-on points and per-node callback, as for an object out of view. */
/* 0x0013a4f0 */
static void model_node_draw(DcNodeCbView *node, float time)
{
    int kind = node->flags & 0xf;

    if (node->obj) {
        model_node_index = 0;
        model_coll_cur = node->dbg;
        model_node_cb = node->cb;
        model_node_cb_arg = node->user;
        switch (kind) {
        case 0:
            model_node_walk_xform(node->obj);
            break;
        case 1:
        case 2:
            anim_start(node->obj8, time);
            model_node_walk_cb(node->obj);
            break;
        }
        kind = 0;
    }
}

/* Lock-on points and per-node callback. */
/* 0x0013a580 */
static void model_node_walk_cb(DcSceneNode *node)
{
    unsigned int flags;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb)
            (*(int (*(*))(DcSceneNode *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        model_node_index = model_node_index + 1;
        if ((flags & 0x10) == 0)
            model_node_walk_cb(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* Rest pose. */
/* 0x0013a698 */
static void model_node_walk_xform(DcNodeScaleView *node)
{
    unsigned int flags;
    float *pos;

    do {
        flags = node->flags;
        ulMatrixPush();
        pos = node->pos;
        do {
            vu0_matrix_translate(ulMatrixCur, node->pos[0], pos[1], pos[2]);
        } while (0);
        nj_matrix_rotate_cur_ang3(node->rot, flags & 0x20);
        vu0_matrix_scale(ulMatrixCur, node->scale[0], node->scale[1], node->scale[2]);
        if (model_coll_cur != 0 && (flags & 0x8000000) != 0) {
            ulMatrixGetTranslation(ulMatrixCur, model_coll_cur + 64);
            ulMatrixApplyVector(ulMatrixCur, model_axis_z, model_coll_cur + 76);
            model_coll_cur = model_coll_cur + 108;
        }
        if (model_node_cb)
            (*(int (*(*))(DcNodeScaleView *, int, int))&model_node_cb)(node, model_node_index, model_node_cb_arg); /* steer(cast) */
        model_node_index = model_node_index + 1;
        if ((flags & 0x10) == 0)
            model_node_walk_xform(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* Wire walk post callback: next wire data block. */
/* 0x0013a7e8 */
static int model_list_second(int list)
{
    return model_list_nth(list, 1);
}

/* Each tracks the position and direction its node records while drawing. */
/* 0x0013a808 */
void model_parts_init(int owner, DcObjectView *obj)
{
    int n = obj->count;
    DcObjectColl *p;
    int *pos;

    if (n > 0) {
        p = obj->arr;
        pos = (int *)((char *)p + 64);
        do {
            pos[0] = 0;
            pos[1] = 0;
            *(float *)&pos[2] = -1.0f;
            lockon_obj_init(p, owner, pos, (char *)p + 76, 0, -1, pos[6], 1, 64);
            lockon_entry(p);
            p++;
            pos = (int *)((char *)pos + 108);
        } while (--n);
    }
}

/* 0x0013a8b8 */
void dc_object_coll_unlock(DcObjectView *obj)
{
    int n = obj->count;
    DcObjectColl *part;

    if (n > 0) {
        part = obj->arr;
        do {
            lockon_remove(part);
            n--;
            part++;
        } while (n);
    }
}

/* 0 if none. */
/* 0x0013a900 */
DcObjectColl *model_get_part(DcObjectView *obj, int idx)
{
    int n = obj->count;
    DcObjectColl *part;

    if (n > 0) {
        part = obj->arr;
        do {
            if (idx-- == 0)
                return part;
            part++;
        } while (--n);
    }
    return 0;
}

/* By node index key; 0 if none. */
/* 0x0013a938 */
static DcObjectColl *model_find_part_by_id(DcObjectView *obj, int key)
{
    int n = obj->count;
    DcObjectColl *part;

    if (n > 0) {
        part = obj->arr;
        do {
            if (part->key == key)
                return part;
            part++;
        } while (--n);
    }
    return 0;
}

/* 0x0013a970 */
static int model_node_count_flag(DcSceneNode *node, unsigned int mask)
{
    int cnt = 0;

    do {
        if (node->flags & mask)
            cnt++;
        if (node->child)
            cnt += model_node_count_flag(node->child, mask);
        node = node->next;
    } while (node);
    return cnt;
}

/* Original bug: no mask is passed on, so the count tests an undefined mask. */
/* 0x0013a9e0 */
int dc_object_count_flag(DcObjectView *obj, unsigned int mask)
{
    if (obj->obj == 0)
        return 0;
    return model_node_count_flag(obj->obj, mask);
}

/* 0x0013aa08 */
static int model_node_child_count(DcSceneNode *node)
{
    int cnt = 0;

    if (node) {
        do {
            cnt++;
            if (node->child)
                cnt += model_node_child_count(node->child);
            node = node->next;
        } while (node);
    }
    return cnt;
}

/* 0x0013aa60 */
int dc_object_child_count(DcObjectView *obj)
{
    if (obj->obj == 0)
        return 0;
    return model_node_child_count(obj->obj);
}

/* 0x0013aa88 */
static int model_node_count(DcSceneNode *node, int count)
{
    do {
        count++;
        if (node->child)
            count = model_node_count(node->child, count);
        node = node->next;
    } while (node);
    return count;
}

/* Per node: position, path length so far, x scale as alpha, angles as 0..255 RGB. Returns next. */
/* 0x0013aad0 */
static ModelExportRec *model_node_export_chain(DcNodeView *node, ModelExportRec *out, float *acc)
{
    float *sv;
    float *dv;

    do {
        sv = node->v;
        dv = out->pos;
        dv[0] = sv[0];
        dv[1] = sv[1];
        dv[2] = sv[2];
        *acc = *acc + ulVec3Distance(dv, out[-1].pos);
        out->time = *acc;
        out->alpha = node->scl_x;
        out->col[0] = (float)node->rot[0] * DEG_PER_ANG * 255.0f / 360.0f;
        out->col[1] = (float)node->rot[1] * DEG_PER_ANG * 255.0f / 360.0f;
        out->col[2] = (float)node->rot[2] * DEG_PER_ANG * 255.0f / 360.0f;
        out++;
        if (node->child)
            out = model_node_export_chain(node->child, out, acc);
        node = node->next;
    } while (node);
    return out;
}

/* First at length 0. Returns the path length. */
/* 0x0013ac08 */
static float model_node_export(DcNodeView *node, ModelExportRec *out)
{
    float total = 0.0f;
    float *sv;
    float *dv;

    sv = node->v;
    dv = out->pos;
    dv[0] = sv[0];
    dv[1] = sv[1];
    dv[2] = sv[2];
    out->time = 0.0f;
    out->alpha = node->scl_x;
    out->col[0] = (float)node->rot[0] * DEG_PER_ANG * 255.0f / 360.0f;
    out->col[1] = (float)node->rot[1] * DEG_PER_ANG * 255.0f / 360.0f;
    out->col[2] = (float)node->rot[2] * DEG_PER_ANG * 255.0f / 360.0f;
    out++;
    if (node->child)
        out = model_node_export_chain(node->child, out, &total);
    if (node->next)
        model_node_export_chain(node->next, out, &total);
    return total;
}

/* One record per node; time = path length from the second node. */
/* 0x0013ad08 */
static ModelSnapshot *model_node_snapshot(DcSceneNode *node)
{
    ModelSnapshot *snap = yuki_alloc(24);
    float base;
    int i;

    snap->cnt = model_node_count(node, 0);
    snap->rec = yuki_alloc(snap->cnt << 5);
    snap->period = model_node_export(node, snap->rec);
    base = snap->rec[1].time;
    snap->period = snap->period - base;
    for (i = 1; i < snap->cnt; i++)
        snap->rec[i].time -= base;
    snap->cnt = snap->cnt - 1;
    return snap;
}

/* steer(cast) */
/* 0x0013adc0 */
static int model_part_free(ModelPartBuf *part)
{
    yuki_free(part->rec);
    part->rec = 0;
    return ((int (*)(void *))yuki_free)(part);
}

/* Colour moves toward col_to by the same amount. */
/* 0x0013adf8 */
static void model_clip_near(float *p, float *to, float *col, float *col_to)
{
    float zn = -cam_near;
    float ratio = (zn - p[2]) / (to[2] - p[2]);

    p[2] = zn;
    p[0] += (to[0] - p[0]) * ratio;
    p[1] += (to[1] - p[1]) * ratio;
    col[0] += (col_to[0] - col[0]) * ratio;
    col[1] += (col_to[1] - col[1]) * ratio;
    col[2] += (col_to[2] - col[2]) * ratio;
    col[3] += (col_to[3] - col[3]) * ratio;
}

/* Screen width; near-plane clipped; start colour solid. */
/* 0x0013aeb8 */
static void model_draw_line3d(float *from, float *to, ModelColor4 *col_from, ModelColor4 *col_to, float width)
{
    float va[4];
    float vb[4];
    ModelColor4 ca;
    ModelColor4 cb;
    float nrm[4];
    ModelLineVtx quad[4];
    float nz;
    float iz0;
    float iz1;
    unsigned int col0;
    unsigned int col1;
    float ax;
    float ay;
    float bx;
    float by;
    float ex;
    float ey;

    ulMatrixApplyPoint(ulMatrixCur, from, va);
    ulMatrixApplyPoint(ulMatrixCur, to, vb);
    nz = -cam_near;
    if (nz < va[2] && nz < vb[2])
        return;
    ca = *col_from;
    cb = *col_to;
    if (nz < va[2])
        model_clip_near(va, vb, ca.v, cb.v);
    else if (nz < vb[2])
        model_clip_near(vb, va, cb.v, ca.v);
    project_point(va, va);
    project_point(vb, vb);
    col0 = (((int)ca.v[3] & 0xff) << 16) | (((int)ca.v[1] & 0xff) | ((((int)ca.v[2] & 0xff) << 8) | (int)0x80000000));
    col1 = (((int)cb.v[3] & 0xff) << 16) | (((int)cb.v[2] & 0xff) << 8) | ((int)cb.v[1] & 0xff);
    ax = va[0];
    ay = va[1];
    bx = vb[0];
    by = vb[1];
    nrm[0] = ay - by;
    nrm[2] = bx - ax;
    iz0 = 1.0f / va[2];
    iz1 = 1.0f / vb[2];
    vec2_normalize_xz(nrm, nrm);
    ex = nrm[0] * width;
    ey = nrm[2] * width;
    quad[0].x = ax - ex;
    quad[0].y = ay - ey;
    quad[2].x = bx - ex;
    quad[2].y = by - ey;
    quad[1].x = ax + ex;
    quad[1].y = ay + ey;
    quad[3].x = bx + ex;
    quad[3].y = by + ey;
    quad[0].z = iz0;
    quad[0].col = col0;
    quad[1].z = iz0;
    quad[1].col = col0;
    quad[2].z = iz1;
    quad[2].col = col1;
    quad[3].z = iz1;
    quad[3].col = col1;
    ulGxBeginStrip(0);
    ulGxAddStrip(quad, 4);
    ulGxEndStrip();
}

/* Soft edges: two quads, solid centre, transparent sides; the first uses col_from at both ends. */
/* 0x0013b1b0 */
static void model_draw_line3d_soft(float *from, float *to, ModelColor4 *col_from, ModelColor4 *col_to, float width)
{
    float va[4];
    float vb[4];
    ModelColor4 ca;
    ModelColor4 cb;
    float nrm[3];
    ModelLineVtx quad[4];
    float nz;
    float one;
    float ax, ay, bx, by;
    float ex, ey;
    unsigned int hi;

    ulMatrixApplyPoint(ulMatrixCur, from, va);
    ulMatrixApplyPoint(ulMatrixCur, to, vb);
    nz = -cam_near;
    if (nz < va[2] && nz < vb[2])
        return;
    ca = *col_from;
    cb = *col_to;
    if (nz < va[2])
        model_clip_near(va, vb, ca.v, cb.v);
    else if (nz < vb[2])
        model_clip_near(vb, va, cb.v, ca.v);
    project_point(va, va);
    project_point(vb, vb);
    one = 1.0f;
    hi = 0x80000000;
    ax = va[0];
    ay = va[1];
    bx = vb[0];
    by = vb[1];
    nrm[2] = bx - ax;
    nrm[0] = ay - by;
    vec2_normalize_xz(nrm, nrm);
    {
        float iz0, iz1;
        unsigned int col0, col0_hi;

        ex = nrm[0] * width;
        ey = nrm[2] * width;
        iz0 = one / va[2];
        col0 = CR(ca) | CG(ca) | CB(ca);
        col0_hi = CR(ca) | CB(ca) | (CG(ca) | hi);
        iz1 = one / vb[2];
        quad[0].x = ax - ex;
        quad[0].y = ay - ey;
        quad[0].z = iz0;
        quad[0].col = col0;
        quad[1].x = ax;
        quad[1].y = ay;
        quad[1].z = iz0;
        quad[1].col = col0_hi;
        quad[2].x = bx - ex;
        quad[2].y = by - ey;
        quad[2].z = iz1;
        quad[2].col = col0;
        quad[3].x = bx;
        quad[3].y = by;
        quad[3].z = iz1;
        quad[3].col = col0_hi;
    }
    ulGxBeginStrip(0);
    ulGxAddStrip(quad, 4);
    ulGxEndStrip();
    {
        float iz0, iz1;
        unsigned int col0, col0_hi, col1, col1_hi;

        iz0 = one / va[2];
        col0_hi = CR(ca) | CB(ca) | (CG(ca) | hi);
        col0 = CR(ca) | CG(ca) | CB(ca);
        iz1 = one / vb[2];
        col1_hi = CR(cb) | CB(cb) | (CG(cb) | hi);
        col1 = CR(cb) | CG(cb) | CB(cb);
        quad[0].x = ax;
        quad[0].y = ay;
        quad[0].z = iz0;
        quad[0].col = col0_hi;
        quad[1].x = ax + ex;
        quad[1].y = ay + ey;
        quad[1].z = iz0;
        quad[1].col = col0;
        quad[2].x = bx;
        quad[2].y = by;
        quad[2].z = iz1;
        quad[2].col = col1_hi;
        quad[3].x = bx + ex;
        quad[3].y = by + ey;
        quad[3].z = iz1;
        quad[3].col = col1;
    }
    ulGxBeginStrip(0);
    ulGxAddStrip(quad, 4);
    ulGxEndStrip();
}

static inline void mp_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* time wrapped into the period; interpolated end kept in path->end. soft: soft-edged lines. */
/* 0x0013b590 */
static void model_draw_path(float time, float width, ModelPath *path, int soft)
{
    float *next_rec = path->rec + 16;
    PathRec seg;
    float rate;

    while (time < 0.0f)
        time = time + path->period;
    while (path->period < time)
        time = time - path->period;
    if (soft) {
        while (next_rec[0] < time) {
            model_draw_line3d_soft(next_rec - 7, next_rec + 1, next_rec - 4, next_rec + 4, width);
            next_rec += 8;
        }
        rate = (time - *(next_rec - 8)) / (next_rec[0] - *(next_rec - 8));
        seg.pos[0] = *(next_rec - 7) * (1.0f - rate) + next_rec[1] * rate;
        seg.pos[1] = *(next_rec - 6) * (1.0f - rate) + next_rec[2] * rate;
        seg.pos[2] = *(next_rec - 5) * (1.0f - rate) + next_rec[3] * rate;
        seg.col[0] = *(next_rec - 4) * (1.0f - rate) + next_rec[4] * rate;
        seg.col[1] = *(next_rec - 3) * (1.0f - rate) + next_rec[5] * rate;
        seg.col[2] = *(next_rec - 2) * (1.0f - rate) + next_rec[6] * rate;
        seg.col[3] = *(next_rec - 1) * (1.0f - rate) + next_rec[7] * rate;
        model_draw_line3d_soft(next_rec - 7, seg.pos, next_rec - 4, seg.col, width);
        mp_vcopy(path->end, seg.pos);
    } else {
        while (next_rec[0] < time) {
            model_draw_line3d(next_rec - 7, next_rec + 1, next_rec - 4, next_rec + 4, width);
            next_rec += 8;
        }
        rate = (time - *(next_rec - 8)) / (next_rec[0] - *(next_rec - 8));
        seg.pos[0] = *(next_rec - 7) * (1.0f - rate) + next_rec[1] * rate;
        seg.pos[1] = *(next_rec - 6) * (1.0f - rate) + next_rec[2] * rate;
        seg.pos[2] = *(next_rec - 5) * (1.0f - rate) + next_rec[3] * rate;
        seg.col[0] = *(next_rec - 4) * (1.0f - rate) + next_rec[4] * rate;
        seg.col[1] = *(next_rec - 3) * (1.0f - rate) + next_rec[5] * rate;
        seg.col[2] = *(next_rec - 2) * (1.0f - rate) + next_rec[6] * rate;
        seg.col[3] = *(next_rec - 1) * (1.0f - rate) + next_rec[7] * rate;
        model_draw_line3d(next_rec - 7, seg.pos, next_rec - 4, seg.col, width);
        mp_vcopy(path->end, seg.pos);
    }
}

static inline void mpr_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* From the end backwards. */
/* 0x0013b8f8 */
static void model_draw_path_rev(float time, float width, ModelPath *path, int soft)
{
    float *prev_rec = path->rec + path->cnt * 8 - 16;
    PathRec seg;
    float rate;

    while (time < 0.0f)
        time = time + path->period;
    while (path->period < time)
        time = time - path->period;
    if (soft) {
        while (time < prev_rec[0]) {
            model_draw_line3d_soft(prev_rec + 9, prev_rec + 1, prev_rec + 12, prev_rec + 4, width);
            prev_rec -= 8;
        }
        rate = (time - prev_rec[8]) / (prev_rec[0] - prev_rec[8]);
        seg.pos[0] = prev_rec[9] * (1.0f - rate) + prev_rec[1] * rate;
        seg.pos[1] = prev_rec[10] * (1.0f - rate) + prev_rec[2] * rate;
        seg.pos[2] = prev_rec[11] * (1.0f - rate) + prev_rec[3] * rate;
        seg.col[0] = prev_rec[12] * (1.0f - rate) + prev_rec[4] * rate;
        seg.col[1] = prev_rec[13] * (1.0f - rate) + prev_rec[5] * rate;
        seg.col[2] = prev_rec[14] * (1.0f - rate) + prev_rec[6] * rate;
        seg.col[3] = prev_rec[15] * (1.0f - rate) + prev_rec[7] * rate;
        model_draw_line3d_soft(prev_rec - 7, seg.pos, prev_rec - 4, seg.col, width);
        mpr_vcopy(path->end, seg.pos);
    } else {
        while (time < prev_rec[0]) {
            model_draw_line3d(prev_rec + 9, prev_rec + 1, prev_rec + 12, prev_rec + 4, width);
            prev_rec -= 8;
        }
        rate = (time - prev_rec[8]) / (prev_rec[0] - prev_rec[8]);
        seg.pos[0] = prev_rec[9] * (1.0f - rate) + prev_rec[1] * rate;
        seg.pos[1] = prev_rec[10] * (1.0f - rate) + prev_rec[2] * rate;
        seg.pos[2] = prev_rec[11] * (1.0f - rate) + prev_rec[3] * rate;
        seg.col[0] = prev_rec[12] * (1.0f - rate) + prev_rec[4] * rate;
        seg.col[1] = prev_rec[13] * (1.0f - rate) + prev_rec[5] * rate;
        seg.col[2] = prev_rec[14] * (1.0f - rate) + prev_rec[6] * rate;
        seg.col[3] = prev_rec[15] * (1.0f - rate) + prev_rec[7] * rate;
        model_draw_line3d(prev_rec - 7, seg.pos, prev_rec - 4, seg.col, width);
        mpr_vcopy(path->end, seg.pos);
    }
}

/* Model file library hooks; clears the timed texture list. */
/* 0x0013bc40 */
void model_install_allocator(void)
{
    ModelAllocator hooks;

    hooks.alloc = model_malloc_tagged;
    model_timed_list = 0;
    hooks.calloc = model_calloc_tagged;
    hooks.free = heap_free;
    pnb_install_hooks(&hooks);
}

/* 0x0013bc88 */
void model_pnb_free_handlers(void)
{
    pnb_free_handlers();
}

/* 0x0013bc90 */
static int model_tex_create(void * src, int user)
{
    return tex_object_create(src, user);
}

/* 0x0013bcb0 */
static int model_tex_reload(int * tex)
{
    return tex_object_reload(tex);
}

/* Prints the transfer time; adds it to the timed list. */
/* 0x0013bcd0 */
ModelListNode *model_create_timed(int src, int user)
{
    int start = perform_timer_read();
    ModelListNode *tex = tex_object_create_upload(src, user);
    ModelListNode *p;

    gw_console_printf("Transfar Time = %fms\n", perform_elapsed(start) * 0.001f);
    if (model_timed_list == 0) {
        model_timed_list = tex;
        tex->next = 0;
    } else {
        p = model_timed_list;
        while (p->next)
            p = p->next;
        p->next = tex;
    }
    return tex;
}

/* 0x0013bda0 */
void model_list_remove(ModelListNode *tex)
{
    ModelListNode **link;

    if (tex) {
        for (link = &model_timed_list; *link; link = &(*link)->next) {
            if (*link == tex) {
                *link = tex->next;
                break;
            }
        }
    }
    tex_object_release(tex);
}

/* 0x0013bdf0 */
int model_pnb_register(void * pnb)
{
    return pnb_register(pnb);
}

/* Version 6+ files only. */
/* 0x0013be10 */
void model_free_all_buffers_checked(int pnb)
{
    int i;
    PnbEntry *ent;

    if (pnb) {
        if (pnb_get_version(pnb) >= 6) {
            i = 0;
            while ((ent = pnb_get_entry(pnb, i)) != 0) {
                i++;
                if (ent->buf) {
                    model_free_block(ent->buf);
                    ent->buf = 0;
                }
            }
        }
    }
}

/* Keeps the file data. 0 if it does not load. */
/* 0x0013be90 */
ModelTimedLoad *model_load_timed(int file, int user)
{
    ModelTimedLoad *w = 0;
    void *res = file_load_sync(file, 0, 0);

    if (res) {
        w = model_create_timed(res, user);
        w->res = res;
    }
    return w;
}

/* 0x0013bef0 */
void model_unload_timed(ModelTimedLoad *tex)
{
    if (tex) {
        if (tex->res) {
            file_heap_free(tex->res);
            tex->res = 0;
        }
    }
    model_list_remove(tex);
}

/* Via pnb_register. 0 if it does not load. */
/* 0x0013bf30 */
void *model_pnb_load(int file)
{
    void *pnb = file_load_sync(file, 0, 0);

    if (pnb)
        pnb_register(pnb);
    return pnb;
}

/* With its cached wire data. */
/* 0x0013bf70 */
void model_pnb_unload(void *pnb)
{
    model_free_all_buffers_checked(pnb);
    heap_free(pnb, 0);
}

/* In place with modeltest_tex_reload, else reloaded. */
/* 0x0013bfa0 */
void model_tex_reload_all(void)
{
    ModelListNode *tex;

    if (model_timed_list) {
        tex = model_timed_list;
        do {
            if (modeltest_tex_reload)
                tex_object_update(tex, 0xc0000);
            else
                tex_object_reload(tex);
            tex = tex->next;
        } while (tex);
    }
}

/* Tagged "mmm". */
/* 0x0013c008 */
static void *model_malloc_tagged(int size)
{
    void *blk = game_malloc(size);

    heap_block_tag(blk, "mmm", -1);
    return blk;
}

/* Tagged "mmm". */
/* 0x0013c048 */
static void *model_calloc_tagged(int size)
{
    void *blk = game_calloc(size);

    heap_block_tag(blk, "mmm", -1);
    return blk;
}

/* 1 for version 6+; else reports "mmm.exe ver2" needed and returns 0. */
/* 0x0013c088 */
static int model_check_model_version(int pnb)
{
    if (pnb_get_version(pnb) < 6) {
        ERROR_PRINTF(str_model_version);
        return 0;
    }
    return 1;
}

/* See model_get_part_cache. */
/* 0x0013c0c8 */
int model_pnb_get_wire(int pnb, int idx)
{
    return model_get_part_cache(pnb, idx);
}

/* Built from the entry's node tree on first use. 0 for a missing entry or old file. */
/* 0x0013c0e8 */
int model_get_part_cache(int pnb, int idx)
{
    PnbEntry *ent;

    if (pnb == 0)
        return 0;
    if (model_check_model_version(pnb) == 0)
        return 0;
    ent = pnb_get_entry(pnb, idx);
    if (ent != 0) {
        if (ent->buf)
            return ent->buf;
        return ent->buf = model_load_wire_data(ent->src);
    }
    return 0;
}

/* 0x0013c160 */
static void model_pnb_free_entry_buffer(int pnb, int idx)
{
    PnbEntry *ent = pnb_get_entry(pnb, idx);

    if (ent) {
        if (ent->buf) {
            model_free_block(ent->buf);
            ent->buf = 0;
        }
    }
}

/* 0x0013c1a8 */
static void model_free_all_buffers(int pnb)
{
    int i = 0;
    PnbEntry *ent;

    do {
        ent = pnb_get_entry(pnb, i);
        if (ent) {
            i++;
            if (ent->buf) {
                model_free_block(ent->buf);
                ent->buf = 0;
            }
        }
    } while (ent);
}

/* 0x0013c210 */
void modeltest_menu_create(void)
{
    modeltest_vars_group = gw_varedit_add(&gw_varedit_root, 0, modeltest_vardef);
    gw_varedit_add_table(&gw_varedit_root, modeltest_vars_group, modeltest_vardefs);
}

/* Same position. */
/* 0x0013c260 */
static int model_entry_pos_equal(int i, int j)
{
    ModelEdgeVtx *va = &model_edge_vtxs[i];
    ModelEdgeVtx *vb = &model_edge_vtxs[j];
    int r = 0;

    if (va->x == vb->x && va->y == vb->y && va->z == vb->z)
        r = 1;
    return r;
}

/* Unless an edge between the same positions is among the n collected. 1 if added. */
/* 0x0013c2c8 */
static int model_edge_add_unique(int a, int b, int n)
{
    int swap;
    int ia;
    int ib;
    ModelEdge *edge;

    if (b < a) {
        swap = a;
        a = b;
        b = swap;
    }
    ia = model_edge_vtxs[a].id;
    ib = model_edge_vtxs[b].id;
    edge = model_edges;
    if (n > 0) {
        do {
            int ka = edge->vtx0;
            int kb = edge->vtx1;
            int va = model_edge_vtxs[ka].id;
            int vb = model_edge_vtxs[kb].id;
            if (va == ia && vb == ib)
                return 0;
            edge++;
        } while (--n);
    }
    *model_edge_out++ = a;
    *model_edge_out++ = b;
    return 1;
}

/* Each takes the id of the first earlier vertex at its position. Flag 1: + normal; 2: + a word. */
/* 0x0013c388 */
static int model_load_vertices(short *p, int flags)
{
    int base = *p++;
    int count = *p++;
    ModelEdgeVtxView *out = &model_edge_vtxs[base];
    float *src = (float *)p;
    unsigned int i;
    unsigned int j;
    int id;
    float x, y, z;
    ModelEdgeVtxView *vtx;

    for (i = 0; i < count; i++) {
        x = *src++;
        id = base;
        y = *src++;
        z = *src++;
        vtx = model_edge_vtxs;
        for (j = 0; j < base; j++, vtx++) {
            if (vtx->v[0] == x && vtx->v[1] == y && vtx->v[2] == z) {
                id = vtx->id;
                break;
            }
        }
        out->id = id;
        out->v[0] = x;
        out->v[1] = y;
        out->v[2] = z;
        if (flags & 1) {
            out->v[3] = *src++;
            out->v[4] = *src++;
            out->v[5] = *src++;
        } else {
            out->v[3] = out->v[4] = out->v[5] = 0.0f;
        }
        if (flags & 2)
            src++;
        base++;
        out++;
    }
    return count;
}

/* With normals, each at its own index after base. Returns the count. */
/* 0x0013c508 */
static int model_load_entries(short *p)
{
    int base;
    int n;
    float *src;
    unsigned int i;
    int id;
    ModelEdgeVtxView *vtx;
    float x, y, z, attr_x, attr_y, attr_z;

    base = *p++;
    n = *p;
    src = (float *)(p + 1);
    for (i = 0; i < n; i++) {
        x = *src++;
        y = *src++;
        z = *src++;
        attr_x = *src++;
        attr_y = *src++;
        attr_z = *src++;
        id = base + *(unsigned short *)src;
        src++;
        vtx = &model_edge_vtxs[id];
        vtx->id = id;
        vtx->v[0] = x;
        vtx->v[1] = y;
        vtx->v[2] = z;
        vtx->v[3] = attr_x;
        vtx->v[4] = attr_y;
        vtx->v[5] = attr_z;
    }
    return n;
}

/* 0x0013c5a8 */
static void model_run_commands(short *p)
{
    int cmd;
    unsigned int len;

    for (cmd = *p++; cmd != 255; cmd = *p++) {
        len = *(unsigned short *)p;
        p++;
        switch (cmd & 0xff) {
        case 41:
            model_load_vertices(p, 1);
            break;
        case 44:
            model_load_entries(p);
            break;
        case 42:
        case 43:
            model_load_vertices(p, 3);
            break;
        case 34:
            model_load_vertices(p, 0);
            break;
        case 35:
        case 36:
            model_load_vertices(p, 2);
            break;
        }
        p = (short *)((char *)p + (len << 2));
    }
}

/* Skips edges to a vertex repeating its neighbour's position. Returns edges added. */
/* 0x0013c688 */
static int model_add_strip_edges(short *p)
{
    int n = 0;
    int strips;
    int len;
    int a, b, c;
    int added;
    unsigned short *idx;

    strips = *p++;
top:
    do {
        len = *p++;
        {
            int k = -1;

            len = len > k ? len : -len;
        }
        a = ((unsigned short *)p)[0];
        b = ((unsigned short *)p)[1];
        c = ((unsigned short *)p)[1 * 2];
        added = model_edge_add_unique(a, b, n);
    } while (0);
    len -= 2;
    n += added;
    added = model_edge_add_unique(a, c, n);
    a = b;
    b = c;
    idx = (unsigned short *)p + 1 * 3;
    n += added;
    len -= 1;
    c = idx[0];
    if (len > 0) {
        strips--;
        do {
            if (!model_entry_pos_equal(b, c))
                n += model_edge_add_unique(a, b, n);
            n += model_edge_add_unique(a, c, n);
            a = b;
            b = c;
            idx += 1;
            len--;
            c = idx[0];
        } while (len);
    } else {
        strips--;
    }
    n += model_edge_add_unique(a, b, n);
    p = (short *)idx;
    if (strips)
        goto top;
    return n;
}

/* Three words per vertex. */
/* 0x0013c7d8 */
static int model_add_strip_edges3(short *p)
{
    int n = 0;
    int strips;
    int len;
    int a, b, c;
    int added;
    unsigned short *idx;

    strips = *p++;
top:
    do {
        len = *p++;
        {
            int k = -1;

            len = len > k ? len : -len;
        }
        a = ((unsigned short *)p)[0];
        b = ((unsigned short *)p)[3];
        c = ((unsigned short *)p)[3 * 2];
        added = model_edge_add_unique(a, b, n);
    } while (0);
    len -= 2;
    n += added;
    added = model_edge_add_unique(a, c, n);
    a = b;
    b = c;
    idx = (unsigned short *)p + 3 * 3;
    n += added;
    len -= 1;
    c = idx[0];
    if (len > 0) {
        strips--;
        do {
            if (!model_entry_pos_equal(b, c))
                n += model_edge_add_unique(a, b, n);
            n += model_edge_add_unique(a, c, n);
            a = b;
            b = c;
            idx += 3;
            len--;
            c = idx[0];
        } while (len);
    } else {
        strips--;
    }
    n += model_edge_add_unique(a, b, n);
    p = (short *)idx;
    if (strips)
        goto top;
    return n;
}

/* Top two bits of the count: extra words per triangle. Returns edges added. */
/* 0x0013c928 */
static int model_add_triangle_edges(short *p)
{
    int n = 0;
    int cnt;
    int sel;
    int extra;
    short *idx;
    int a, b, c;

    cnt = *p++;
    idx = p + 1;
    sel = ((unsigned int)cnt >> 14) & 3;
    cnt &= 0x3fff;
    extra = sel << 1;
    do {
        a = p[0];
        b = idx[0];
        c = p[2];
        cnt--;
        n += model_edge_add_unique(a, b, n);
        idx += 3;
        p += 3;
        n += model_edge_add_unique(a, c, n);
        idx = (short *)((char *)idx + extra);
        n += model_edge_add_unique(b, c, n);
        p = (short *)((char *)p + extra);
    } while (cnt);
    return n;
}

/* Quads, four edges each. */
/* 0x0013ca08 */
static int model_add_quad_edges(short *p)
{
    int n = 0;
    int cnt;
    int sel;
    short *idx;
    int a, b, c, d;

    cnt = *p++;
    idx = p + 1;
    sel = ((unsigned int)cnt >> 14) & 3;
    cnt &= 0x3fff;
    do {
        a = p[0];
        b = idx[0];
        c = idx[1];
        d = p[3];
        cnt--;
        n += model_edge_add_unique(a, b, n);
        idx += 4;
        n += model_edge_add_unique(b, c, n);
        p += 4;
        n += model_edge_add_unique(c, d, n);
        idx = (short *)((char *)idx + (sel << 1));
        n += model_edge_add_unique(a, d, n);
        p = (short *)((char *)p + (sel << 1));
    } while (cnt);
    return n;
}

/* Triangles (56), quads (57), strips (64, 65); 4 stores a sub-list that 5 runs. Returns edges. */
/* 0x0013cb08 */
static int model_run_edge_commands(short *p)
{
    int n = 0;
    int cmd;
    int op;
    int len;

    for (cmd = *p++; cmd != 255; cmd = *p++) {
        op = cmd & 0xff;
        if (op < 8) {
            switch (op) {
            case 1:
                break;
            case 4:
                model_cmd_sub_streams[(cmd & 0xff00) >> 8] = p;
                model_cmd_depth = model_cmd_depth + 1;
                return n;
            case 5:
                n += model_run_edge_commands(model_cmd_sub_streams[(cmd & 0xff00) >> 8]);
                model_cmd_depth = model_cmd_depth - 1;
                break;
            }
        } else if (op < 16) {
            p++;
        } else if (op < 56) {
            len = *p;
            p++;
            p += len;
        } else {
            len = *(unsigned short *)p;
            p++;
            switch (op) {
            case 64:
                n += model_add_strip_edges(p);
                break;
            case 65:
                n += model_add_strip_edges3(p);
                break;
            case 56:
                n += model_add_triangle_edges(p);
                break;
            case 57:
                n += model_add_quad_edges(p);
                break;
            }
            p += len;
        }
    }
    return n;
}

/* One linked block per node with its unique edges (vertex index pairs). Returns the end. */
/* 0x0013cca8 */
static int *model_serialize_nodes(DcSceneNode *node, int *out)
{
    unsigned int flags;
    int *obj;
    int cnt;
    int *slot;
    unsigned short *tp;

    if (node) {
        do {
            out[1] = 0;
            slot = out + 1;
            flags = node->flags;
            cnt = 0;
            out[0] = 0;
            model_edge_out = model_edges = (unsigned short *)(out + 2);
            obj = node->obj;
            if ((flags & 8) == 0) {
                if (obj[0])
                    model_run_commands((short *)obj[0]);
                if (obj[1])
                    cnt = model_run_edge_commands(obj[1]);
            }
            *slot = cnt;
            tp = model_edge_out;
            out[0] = (int)tp;
            out = (int *)tp;
            out[0] = 0;
            out[1] = 0;
            if ((flags & 0x10) == 0)
                out = model_serialize_nodes(node->child, out);
            node = node->next;
        } while (node);
    }
    return out;
}

/* Scratch build, copied to a "WireData" heap block with links moved. Returns the copy. */
/* 0x0013cda8 */
int *model_load_wire_data(int src)
{
    char *base = model_wire_buf;
    char *end;
    int size;
    char *dst;
    int *out;
    int delta;

    model_edge_vtxs = gx_vertex_buffer;
    end = model_serialize_nodes(src, base);
    end += 8;
    size = end - base;
    dst = heap_block_tag(game_malloc(size), "WireData", -1);
    nj_copy8(dst, base, size);
    delta = (int)dst - (int)base;
    for (out = (int *)dst; *out; out = (int *)*out)
        *out += delta;
    return (int *)dst;
}

/* Or any heap block. */
/* 0x0013ce60 */
void model_free_block(void *blk)
{
    heap_free(blk, 0);
}

/* Link in the first word; 0 when shorter. */
/* 0x0013ce68 */
void *model_list_nth(void **list, int n)
{
    void **node = list;

    if (node != 0) {
        for (;;) {
            if (n <= 0)
                return node;
            node = (void **)*node;
            n--;
            if (node == 0)
                break;
        }
    }
    return 0;
}

/*
 * Rest pose, one model_kind_list entry per node. Translation, rotation (Euler, or quaternion with
 * flag 0x400) and scale unless flags 1, 2, 4 leave them out; flag 8 hides the shape.
 */
/* 0x0013ce98 */
static void model_node_draw_kinds(DcNodePosView *node)
{
    unsigned int flags;
    float *pos;

    do {
        flags = node->flags;
        if ((flags & 7) == 7) {
            if ((flags & 8) == 0)
                model_obj_draw(node->obj, (*(ModelKindEnt * *)&model_kind_list)); /* steer(cast) */
            (*(ModelKindEnt * *)&model_kind_list) = (*(ModelKindEnt * *)&model_kind_list)->next; /* steer(cast) */
            if ((flags & 0x10) == 0)
                model_node_draw_kinds(node->child);
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
                        quat_to_matrix_split(node->quat_w, node->rot);
                    else
                        nj_matrix_rotate_cur_ang3(node->rot, flags & 0x20);
                }
            }
            if ((flags & 4) == 0)
                vu0_matrix_scale(ulMatrixCur, node->scale[0], node->scale[1], node->scale[2]);
            if ((flags & 8) == 0)
                model_obj_draw(node->obj, (*(ModelKindEnt * *)&model_kind_list)); /* steer(cast) */
            (*(ModelKindEnt * *)&model_kind_list) = (*(ModelKindEnt * *)&model_kind_list)->next; /* steer(cast) */
            if ((flags & 0x10) == 0)
                model_node_draw_kinds(node->child);
            njPopMatrixEx();
        }
        node = node->next;
    } while (node);
}

/* Rest pose. */
/* 0x0013d008 */
void model_draw_kind_list(DcNodePosView *node, int kind_list)
{
    if (kind_list) {
        model_kind_list = kind_list;
        model_node_draw_kinds(node);
    }
}

/* Animated transforms. */
/* 0x0013d020 */
static void model_node_walk_list(DcSceneNode *node)
{
    unsigned int flags;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        if ((flags & 8) == 0)
            model_obj_draw(node->obj, (*(ModelKindEnt * *)&model_kind_list)); /* steer(cast) */
        (*(ModelKindEnt * *)&model_kind_list) = (*(ModelKindEnt * *)&model_kind_list)->next; /* steer(cast) */
        if ((flags & 0x10) == 0)
            model_node_walk_list(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* 0x0013d0b8 */
void model_node_walk_list_anim(int list, int clip, float time, int kind_list)
{
    anim_start((void *)clip, time);
    if (kind_list) {
        model_kind_list = kind_list;
        model_node_walk_list(list);
    }
}

/* Plus the animation cursor on each shape unless node flag 0x80. */
/* 0x0013d110 */
static void model_node_draw_list(DcSceneNode *node)
{
    unsigned int flags;
    int *obj;

    do {
        flags = node->flags;
        ulMatrixPush();
        anim_apply_node(node);
        obj = node->obj;
        if ((flags & 0x80) == 0) {
            if (obj && (flags & 8) == 0) {
                if (**anim_cur_cursor)
                    anim_cursor_play(obj);
                model_obj_draw(obj, (*(ModelKindEnt * *)&model_kind_list)); /* steer(cast) */
            }
            anim_cursor_next();
        } else {
            if (obj) {
                if ((flags & 8) == 0)
                    model_obj_draw(obj, (*(ModelKindEnt * *)&model_kind_list)); /* steer(cast) */
            }
        }
        (*(ModelKindEnt * *)&model_kind_list) = (*(ModelKindEnt * *)&model_kind_list)->next; /* steer(cast) */
        if ((flags & 0x10) == 0)
            model_node_draw_list(node->child);
        ulMatrixPop();
        node = node->next;
    } while (node);
}

/* Cursor clip clip_b too. */
/* 0x0013d210 */
static void model_node_start_draw(float time, int tree, int clip, int clip_b, int list)
{
    if (list) {
        anim_start(clip, time);
        anim_select_cursor(0);
        anim_cursor_start(clip_b, time);
        model_kind_list = list;
        model_node_draw_list(tree);
    }
}

/* 0x0013d2a0 */
static void model_on_frame_submit(void)
{
}

/* 0x0013d2a8 */
static void model_on_frame_flip(void)
{
}

/* Messages 1 and 2 with empty handlers. */
/* 0x0013d2b0 */
void model_register_callbacks(void)
{
    draw_clear_poly_count();
    listener_add(1, model_on_frame_submit);
    listener_add(2, model_on_frame_flip);
}
