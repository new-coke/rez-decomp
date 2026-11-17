/*
 * Map objects (stage scenery): a map task places a map model's animated parts, culls them by slot
 * spheres, syncs animation to beat, bar or map frame, and draws them through per-part hooks.
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "map.h"
#include "anim.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "yuki_tool.h"
#include "system.h"

typedef struct { char *list; char pad_04[4]; float ref[3]; } DcObjectShape;
typedef struct DcNodeChildView DcNodeChildView;
typedef struct MapDcObjectShape { char pad_00[8]; float center; char pad_0c[8]; float radius; } MapDcObjectShape;

struct DcNodeChildView {
    char pad_00[4];
    MapDcObjectShape *s;
    char pad_08[36];
    DcNodeChildView *child;
    DcNodeChildView *next;
};

typedef struct { float *pos; int rot; float *scale; } DcNodeBufs;
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
/* steer(alias): the code that matched reads DcObject through these member types; one view
 * shared by those sites (src/game/map.c). */
typedef struct DcObjectRootView {
    char pad_00[4];
    int root;
    int clip;
    char pad_0c[48];
    float time;
    char pad_40[16];
    float radius;
} DcObjectRootView;
#define MAX_MAP_DRAW_PART(a, b) ((a) > (b) ? (a) : (b))
#define MAX_MAP_DRAW_SORTED(x, y) ((x) <= (y) ? (y) : (x))
#define MAX_F(a, b) ((a) >= (b) ? (a) : (b))

extern char map_node_pos_buf[];
extern char map_node_rot_buf[];
extern char map_node_scl_buf[];
extern char map_color_eval_desc[];
extern int map_debug_part;
extern int map_draw_num;
extern int ulMatrixPush(void);
extern int debug_disp_level;
extern int map_debug_repeat;
extern PadSystem pad_system;
extern void debug_set_color(unsigned c);
extern struct TaskDesc map_task_desc;
extern struct TaskDesc map_task_m1_f1_desc;
extern struct TaskDesc map_task_m1_f0_desc;
extern struct TaskDesc map_task_m2_f1_desc;
extern struct TaskDesc map_task_m2_f0_desc;
extern int **anim_cur_cursor;
extern void dc_object_calc_radius(DcObjectRootView *w, float *radius, float *radius_pts);
extern int dc_object_node_radius(DcNodeChildView *node, DcNodeBufs *bufs, int idx, float *radius, float *radius_pts, float x, float y, float z, float scale);
extern float dc_object_shape_radius(DcObjectShape *w);

/* Samples each vertex list. */
/* 0x00132100 */
float dc_object_shape_radius(DcObjectShape *w)
{
    char *p = w->list;
    float best = 0.0f;

    if (p != 0 && *(int *)p != 255) {
        do {
            int cnt;
            float *pt;
            int n;
            int step;
            float dist;

            pt = (float *)(p + 8);
            n = *(short *)(p + 6);
            cnt = (short)(*(unsigned short *)(p + 2) + 1);
            step = (cnt - 1) / n;

            if (n-- > 0) {
                do {
                    dist = ulVec3Distance(pt, w->ref);
                    if (best >= dist)
                        dist = best;
                    pt += step;
                    best = dist;
                } while (n-- > 0);
            }
            p += cnt * 4;
        } while (*(int *)p != 255);
    }
    return best;
}

/* *radius from shape spheres, *radius_pts from farthest vertices. Returns the next node index. */
/* 0x00132210 */
int dc_object_node_radius(DcNodeChildView *node, DcNodeBufs *bufs, int idx, float *radius, float *radius_pts, float x, float y, float z, float scale)
{
    float *pos;
    float *w;
    float max_scale;
    float len_sq;
    float centre_len;
    float shape_r;
    float val;
    MapDcObjectShape *shape;

    do {
        pos = bufs->pos + idx * 3;
        w = bufs->scale + idx * 3;
        shape = node->s;
        x += pos[0] * scale;
        y += pos[1] * scale;
        z += pos[2] * scale;
        max_scale = MAX(MAX(w[0], w[1]), w[2]);
        if (shape != 0) {
            centre_len = vec3_length(&shape->center);
            len_sq = x * x + y * y + z * z;
            val = sqrtf(len_sq) + (centre_len + shape->radius) * scale * max_scale;
            *radius = MAX(*radius, val);
            shape_r = dc_object_shape_radius(shape);
            val = sqrtf(len_sq) + (centre_len + shape_r) * scale * max_scale;
            if (val <= *radius_pts)
                val = *radius_pts;
            *radius_pts = val;
        }
        idx++;
        if (node->child != 0)
            idx = dc_object_node_radius(node->child, bufs, idx, radius, radius_pts, x, y, z, scale * max_scale);
        node = node->next;
    } while (node != 0);
    return idx;
}

/* Current pose: spheres -> *radius, vertices -> *radius_pts. Without a clip: stored radius. */
/* 0x00132468 */
void dc_object_calc_radius(DcObjectRootView *w, float *radius, float *radius_pts)
{
    void *bufs[3];
    float z;

    if (w->clip == 0) {
        *radius = *radius_pts = w->radius;
        return;
    }
    dc_object_update(w, w->time);
    bufs[0] = map_node_pos_buf;
    bufs[1] = map_node_rot_buf;
    bufs[2] = map_node_scl_buf;
    anim_eval_hierarchy(w->root, w->clip, bufs, w->time);
    z = 0.0f;
    *radius_pts = z;
    *radius = z;
    dc_object_node_radius(w->root, bufs, 0, radius, radius_pts, z, z, z, 1.0f);
}

/* Slot clipping and view culling off: always draws. */
/* 0x00132538 */
void map_hook_show(int *w)
{
    map_slot_set_clip(w[20], w[14], 0);
    map_part_set_cull_flag(w[20], w[15], 0);
}

/* 0x00132578 */
void map_hook_set_display_list3(int *w)
{
    map_part_set_display_list(w[20], w[15], 3);
}

/* 0x00132588 */
void map_hook_set_beat_phase(float *w, int div)
{
    w[17] = yuki_beat_phase(div);
}

/* 0x001325b8 */
void map_hook_beat_phase16(int w)
{
    map_hook_set_beat_phase(w, 16);
}

/* 0x001325c0 */
void map_hook_beat_phase8(int w)
{
    map_hook_set_beat_phase(w, 8);
}

/* 0x001325c8 */
void map_hook_beat_phase4(int w)
{
    map_hook_set_beat_phase(w, 4);
}

/* scale: 0..1 of its length. */
/* 0x001325d0 */
void map_part_eval_color(float scale, unsigned int *w, int x, int y, int z)
{
    int bufs[3];
    unsigned int n = w[1] - 1;

    bufs[0] = x;
    bufs[1] = y;
    bufs[2] = z;
    anim_eval_hierarchy(map_color_eval_desc, w, bufs, scale * (float)n);
}

/*
 * Skipped while map_debug_part singles out another part or its slot is culled; sphere and far
 * plane culling, distance fade, colour animation tint. Counts into map_draw_num.
 */
/* steer(cast) */
/* 0x00132640 */
void map_draw_part(MapHookCtx *w, float time, float r, float g, float b, float a)
{
    float view_pos[4];
    float eye[4];
    float anim_pos[4];
    float anim_scl[4];
    float anim_rot[4];
    float col[4];
    MapPart *node = w->part;
    float *sc = w->scale;
    float sz = node->radius;
    float far_dist;
    float x;
    int saved;

    if (map_debug_part != w->part_idx) {
        if (map_debug_part != -1)
            return;
    }
    if (w->slot_clip && w->slot_culled)
        return;
    camera_get_eye(eye);
    if (sc[0] >= sc[1] ? sc[0] <= sc[2] : sc[1] <= sc[2])
        sz *= sc[2];
    else if (sc[0] >= sc[1])
        sz *= sc[0];
    else
        sz *= sc[1];
    if (w->cull) {
        if (!view_sphere_visible(w->pos, sz))
            return;
    }
    ulMatrixPush();
    do {
        float *p = w->pos;

        vu0_matrix_translate(ulMatrixCur, p[0], p[1], p[2]);
    } while (0);
    map_rotate_zyx((int *)w->rot);
    vu0_matrix_scale(ulMatrixCur, sc[0], sc[1], sc[2]);
    ulMatrixGetTranslation(ulMatrixCur, view_pos);
    far_dist = camera_get_far();
    if (w->cull && w->far_clip) {
        if (!(-view_pos[2] - sz <= far_dist))
            goto pop;
    }
    ulGxSetTextureList(w->tex);
    if (w->far_fade) {
        x = 1.0f - (-view_pos[2] - sz) / far_dist;
        r *= (((x >= 0.0f) ? x : 0.0f) < 1.0f) ? ((x >= 0.0f) ? x : 0.0f) : 1.0f;
    }
    if (w->color_anim) {
        map_part_eval_color(w->beat_pos, (unsigned int *)w->color_anim, anim_pos, anim_rot, anim_scl);
        r *= anim_scl[0];
        g *= anim_pos[0];
        b *= anim_pos[1];
        a *= anim_pos[2];
    }
    if (!w->wire || !w->obj->enable) {
        saved = 0;
        if (!(fabsf(r - 1.0f) < FLT_EPSILON)) {
            saved = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
            col[0] = (((r >= 0.0f) ? r : 0.0f) < 1.0f) ? ((r >= 0.0f) ? r : 0.0f) : 1.0f;
            col[1] = (((g >= 0.0f) ? g : 0.0f) < 1.0f) ? ((g >= 0.0f) ? g : 0.0f) : 1.0f;
            col[2] = (((b >= 0.0f) ? b : 0.0f) < 1.0f) ? ((b >= 0.0f) ? b : 0.0f) : 1.0f;
            col[3] = (((a >= 0.0f) ? a : 0.0f) < 1.0f) ? ((a >= 0.0f) ? a : 0.0f) : 1.0f;
            draw_push_strip_flags();
            ((void (*)(unsigned int, int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_push_override_color();
            draw_set_override_color(col);
        }
        model_object_draw(node, time);
        if (!(fabsf(r - 1.0f) < FLT_EPSILON)) {
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(saved);
        }
        map_draw_num++;
    } else {
        int ir = (int)(r * 255.0f);
        int ig = (int)(g * 255.0f);
        int ib = (int)(b * 255.0f);
        int ia = (int)(a * 255.0f);
        int cr;

        if (fabsf(r - 1.0f) < FLT_EPSILON)
            ir = 255;
        cr = MIN(MAX_MAP_DRAW_PART(ir, 0), 255);
        draw_set_color((((cr + 1) >> 1) << 24) | ((MIN(MAX_MAP_DRAW_PART(ia, 0), 255) & 0xff) << 16) | ((MIN(MAX_MAP_DRAW_PART(ib, 0), 255) & 0xff) << 8) | (MIN(MAX_MAP_DRAW_PART(ig, 0), 255) & 0xff));
        draw_set_blend(4, 5);
        model_object_draw(node, time);
        map_draw_num++;
    }
pop:
    ulMatrixPop();
}

/* As map_draw_part; the tint darkens the part's own colour instead of scaling an override. */
/* steer(cast) */
/* 0x00132b90 */
void map_draw_part_add(MapHookCtx *w, float time, float r, float g, float b, float a)
{
    float view_pos[4];
    float eye[4];
    float anim_pos[4];
    float anim_scl[4];
    float anim_rot[4];
    float col[4];
    MapPart *node = w->part;
    float *sc = w->scale;
    float sz = node->radius;
    float far_dist;
    float x;
    int saved;

    if (map_debug_part != w->part_idx) {
        if (map_debug_part != -1)
            return;
    }
    if (w->slot_clip && w->slot_culled)
        return;
    camera_get_eye(eye);
    if (sc[0] >= sc[1] ? sc[0] <= sc[2] : sc[1] <= sc[2])
        sz *= sc[2];
    else if (sc[0] >= sc[1])
        sz *= sc[0];
    else
        sz *= sc[1];
    if (w->cull) {
        if (!view_sphere_visible(w->pos, sz))
            return;
    }
    ulMatrixPush();
    do {
        float *p = w->pos;

        vu0_matrix_translate(ulMatrixCur, p[0], p[1], p[2]);
    } while (0);
    map_rotate_zyx((int *)w->rot);
    vu0_matrix_scale(ulMatrixCur, sc[0], sc[1], sc[2]);
    ulMatrixGetTranslation(ulMatrixCur, view_pos);
    far_dist = camera_get_far();
    if (w->cull && w->far_clip) {
        if (!(-view_pos[2] - sz <= far_dist))
            goto pop;
    }
    ulGxSetTextureList(w->tex);
    if (w->far_fade) {
        x = 1.0f - (-view_pos[2] - sz) / far_dist;
        r *= (((x >= 0.0f) ? x : 0.0f) < 1.0f) ? ((x >= 0.0f) ? x : 0.0f) : 1.0f;
    }
    if (w->color_anim) {
        map_part_eval_color(w->beat_pos, (unsigned int *)w->color_anim, anim_pos, anim_rot, anim_scl);
        r *= anim_scl[0];
        g *= anim_pos[0];
        b *= anim_pos[1];
        a *= anim_pos[2];
    }
    if (!w->wire || !w->obj->enable) {
        saved = 0;
        if (!(fabsf(r - 1.0f) < FLT_EPSILON)) {
            saved = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            col[0] = r - 1.0f;
            col[1] = g - 1.0f;
            col[2] = b - 1.0f;
            col[3] = a - 1.0f;
            draw_push_strip_flags();
            ((void (*)(unsigned int, int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_push_override_color();
            draw_set_override_color(col);
        }
        model_object_draw(node, time);
        if (!(fabsf(r - 1.0f) < FLT_EPSILON)) {
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(saved);
        }
        map_draw_num++;
    } else {
        int cr = (int)((float)color_get_a(w->color) - (1.0f - r) * 255.0f);
        int cg = (int)((float)((int (*)(unsigned int))color_get_r)(w->color) - (1.0f - g) * 255.0f);
        int cb = (int)((float)color_get_g(w->color) - (1.0f - b) * 255.0f);
        int ca = (int)((float)color_get_b(w->color) - (1.0f - a) * 255.0f);

        if (fabsf(r - 1.0f) < FLT_EPSILON)
            cr = color_get_a(w->color);
        cg = cg > -1 ? cg : 0;
        cb = cb > -1 ? cb : 0;
        cg = cg < 256 ? cg : 255;
        cr = cr > -1 ? cr : 0;
        cb = cb < 256 ? cb : 255;
        ca = ca > -1 ? ca : 0;
        cr = cr < 256 ? cr : 255;
        ca = ca < 256 ? ca : 255;
        draw_set_color((((cr + 1) >> 1) << 24) | ((ca & 0xff) << 16) | ((cb & 0xff) << 8) | (cg & 0xff));
        draw_set_blend(4, 5);
        model_object_draw(node, time);
        map_draw_num++;
    }
pop:
    ulMatrixPop();
}

/* 0x00133058 */
void map_draw_part_rgba(MapObj *w, float time, float col_r, float col_g, float col_b, float col_a)
{
    if (w->far_clip) {
        map_draw_part((MapHookCtx *)w, w->p4->time, col_r, col_g, col_b, col_a); /* steer(cast) */
    } else {
        map_draw_part_add((MapHookCtx *)w, w->p4->time, col_r, col_g, col_b, col_a); /* steer(cast) */
    }
}

/* 0x00133078 */
void map_hook_draw(MapObj *w)
{
    map_draw_part_rgba(w, w->p4->time, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x00133098 */
void map_hook_draw_ent_time(MapObj *w)
{
    map_draw_part_rgba(w, w->sort_keys.f, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x001330b8 */
void map_hook_draw_ent_fade(MapObj *w)
{
    map_draw_part_rgba(w, w->p4->time, w->sort_keys.f, 1.0f, 1.0f, 1.0f);
}

/* Returns the part's display list. */
/* 0x001330d8 */
int map_part_ctx_fill(MapObj *w, MapHookCtx *out, int idx)
{
    MapDef *def = &w->model->defs[idx];
    MapPart *part = &w->parts[def->part_idx];
    MapPair *ent = &w->ents[idx];
    MapSlot *slot = &w->slots[def->slot_idx];
    int dlist;
    int val;
    float beat_pos;

    out->idx = idx;
    out->pos = def->pos;
    out->rot = def->rot;
    out->scale = def->scale;
    val = part->color;
    out->color = val;
    *(void **)&out->wire = *(void **)&part->wire;
    out->part = part;
    out->follow_frame = part->follow_frame;
    out->part_idx = def->part_idx;
    out->ent_key = ent->key;
    out->ent_val = ent->val;
    out->slot_culled = slot->culled;
    out->slot_clip = slot->clip;
    out->cull = part->cull;
    out->far_clip = w->far_clip;
    out->far_fade = w->far_fade;
    val = def->slot_idx;
    beat_pos = w->beat_pos;
    ((union { int i; } *)&out->color_anim)->i = part->color_anim;  /* steer(alias) */
    dlist = part->dlist;
    ((union { float f; } *)&out->beat_pos)->f = beat_pos;        /* steer(alias) */
    out->slot_idx = val;
    do { out->obj = w; } while (0);     /* steer(barrier) */
    return dlist;
}

/* mode 0 init, 1 release, 2 update, 3 draw (part's display list). Keeps each hook's entry value. */
/* 0x001331c0 */
void map_run_part_hooks(MapObj *w, int mode)
{
    MapHookCtx ctx;
    MapModelInfo *model;
    MapDef *def;
    MapPart *part;
    int i, dlist;
    void *hook;

    model = w->model;
    i = 0;
    ctx.tex = w->p4;
    *(void **)&ctx.data = (void *)w->data;
    for (def = model->defs; i < model->ndef; i++, def++) {
        part = &w->parts[def->part_idx];
        dlist = map_part_ctx_fill(w, &ctx, i);
        hook = (void *)part->user.hook;
        switch (mode) {
        case 0:
            hook = ((void **)hook)[0];
            if (hook)
                ((void (*)(MapHookCtx *))hook)(&ctx);
            break;
        case 1:
            hook = ((void **)hook)[1];
            if (hook)
                ((void (*)(MapHookCtx *))hook)(&ctx);
            break;
        case 2:
            hook = ((void **)hook)[2];
            if (hook)
                ((void (*)(MapHookCtx *))hook)(&ctx);
            break;
        case 3:
            hook = ((void **)hook)[3];
            if (hook) {
                dlist = RenderSetCurrentDisplayList(dlist);
                ((void (*)(MapHookCtx *))hook)(&ctx);
                RenderSetCurrentDisplayList(dlist);
            }
            break;
        }
        w->ents[i].val = ctx.ent_val;
    }
}

/* Stable insertion sort, ascending key[k]. */
/* 0x00133328 */
void map_key_insertion_sort(MapSortKey *keys, int n, int k)
{
    MapSortKey key_save;
    MapSortKey *cur, *prev;
    int i, j;
    float key;

    for (i = 1; i < n; i++) {
        cur = &keys[i];
        j = i - 1;
        key = cur->key[k];
        key_save = *cur;
        prev = &keys[j];
        do {
            if (!(key < prev->key[k])) {
                break;
            }
            prev[1] = prev[0];
            prev--;
            j--;
        } while (j >= 0);
        keys[j + 1] = key_save;
    }
}

/* Runs of the same part (key[1]) by distance (key[0]). */
/* 0x001333f8 */
void map_key_sort_runs(MapSortKey *keys, int n)
{
    MapSortKey *p = keys;
    int cnt = 1;
    MapSortKey *grp = keys;
    int i;

    for (i = 1; i < n; i++, p++, cnt++) {
        if (p[1].key[1] != p->key[1]) {
            if (cnt > 1) {
                map_key_insertion_sort(grp, cnt, 0);
            }
            grp = &keys[i];
            cnt = 0;
        }
    }
    if (cnt > 1) {
        map_key_insertion_sort(grp, cnt, 0);
    }
}

/* Sort type 1 back to front; 2 back to front within same-part runs; 3 grouped by part. */
/* 0x001334a8 */
void map_draw_sorted(MapObj *w)
{
    MapHookCtx ctx;
    float view[4];
    MapModelInfo *model = w->model;
    MapSortKey *out = (MapSortKey *)w->sort_keys.p;
    MapDef *def = model->defs;
    void (*hook)(MapHookCtx *);
    float radius;
    int i, nsel, old, ret;

    nsel = 0;
    i = 0;
    camera_get_eye(view);
    for (; i < model->ndef; i++, def++) {
        MapSlot *slot = &w->slots[def->slot_idx];
        MapPart *part = &w->parts[def->part_idx];
        float *scale = def->scale;

        if (slot->clip != 0 && slot->culled != 0)
            continue;
        radius = part->radius;
        radius *= MAX_MAP_DRAW_SORTED(scale[2], MAX_MAP_DRAW_SORTED(scale[1], scale[0]));
        if (part->cull != 0 && !view_sphere_visible(def->pos, radius))
            continue;
        out->tag = i;
        out->key[1] = (float)def->part_idx;
        out->key[0] = -vec3_distance_squared(def->pos, view);
        out++;
        nsel++;
    }
    out = (MapSortKey *)w->sort_keys.p;
    switch (w->sort_type) {
    case 1:
        map_key_insertion_sort(out, nsel, 0);
        break;
    case 3:
        map_key_insertion_sort(out, nsel, 0);
        map_key_insertion_sort(out, nsel, 1);
        break;
    case 2:
        map_key_sort_runs(out, nsel);
        break;
    }
    ctx.tex = w->p4;
    ctx.data = w->data;
    for (i = 0; i < nsel; i++, out++) {
        MapPart *part = &w->parts[model->defs[out->tag].part_idx];

        hook = part->user.hook[3];
        if (hook) {
            ret = map_part_ctx_fill(w, &ctx, out->tag);
            old = RenderSetCurrentDisplayList(ret);
            hook(&ctx);
            RenderSetCurrentDisplayList(old);
        }
        w->ents[out->tag].val = ctx.ent_val;
    }
}

/*
 * prm {mode, own wires, model}: mode 0 static, 1 animated, 2 bar-synced. Each slot gets a radius
 * covering its definitions; then the init hooks.
 */
/* 0x00133760 */
void map_task_init(MapObj *w, int *prm)
{
    int mode;
    int simple_mode;
    MapModel *model;
    MapSlot *sl;
    MapPart *pt;
    MapAnim *an;
    MapDef *df;
    int i;
    int base, node_data, clip, clip_b, clip_c;
    float r;

    mode = prm[0];
    model = (MapModel *)prm[2];
    w->mode = mode;
    w->model = (MapModelInfo *)model;
    w->frame = 0;
    an = (MapAnim *)(w->slots = yuki_alloc(model->nslot * 12));
    for (i = 0; i < model->nslot; i++, an = (MapAnim *)((MapSlot *)an + 1)) {
        ((MapSlot *)an)->culled = 0;
        ((MapSlot *)an)->clip = 1;
        ((MapSlot *)an)->radius = 0;
    }
    an = model->anm;
    w->enable = 1;
    i = 0;
    w->own_wires = prm[1];
    pt = w->parts = yuki_alloc(model->npart * 140);
    if (model->npart > 0) {
        simple_mode = mode < 2;
        for (; i < model->npart; i++, pt++, an++) {
            int node_ofs = an->node_ofs;

            pt->bar_sync = 0;
            base = w->data;
            node_data = base + node_ofs;
            if (mode <= 0) {
                clip = 0;
                if (an->clip_ofs)
                    clip = base + an->clip_ofs;
                clip_b = 0;
                clip_c = 0;
            } else {
                if (!simple_mode && an->bar_clip_ofs) {
                    clip = base + an->bar_clip_ofs;
                    clip_b = an->bar_clip_b_ofs ? base + an->bar_clip_b_ofs : 0;
                    if (an->bar_clip_c_ofs)
                        clip_c = w->data + an->bar_clip_c_ofs;
                    else
                        clip_c = 0;
                    pt->bar_sync = 1;
                } else {
                    if (an->clip_ofs)
                        clip = w->data + an->clip_ofs;
                    else
                        clip = 0;
                    if (an->clip_b_ofs)
                        clip_b = w->data + an->clip_b_ofs;
                    else
                        clip_b = 0;
                    if (an->clip_c_ofs)
                        clip_c = w->data + an->clip_c_ofs;
                    else
                        clip_c = 0;
                }
            }
            pt->dlist = 4;
            *(unsigned *)&pt->color = RGBA(0xff, 0xff, 0xff, 0xff);
            if (mode == 0) {
                pt->follow_frame = 0;
                pt->wire = 0;
                dc_object_init(pt, node_data, clip, 4096);
                dc_object_rebuild(pt);
            } else {
                int res = 0;

                if (w->own_wires && an->has_wire)
                    res = model_load_wire_data(node_data);
                *(void **)&pt->wire = (void *)res;
                pt->follow_frame = an->follow_frame;
                if (clip_c == 0 || clip_b == 0)
                    dc_object_init(pt, node_data, clip, 4096);
                else
                    dc_object_init_cursor(pt, node_data, clip, clip_b, clip_c, 4096);
                dc_object_set_wire(pt, pt->wire);
                if (an->has_wire == 0)
                    dc_object_rebuild(pt);
                pt->color_anim = 0;
                if (!simple_mode && an->color_ofs)
                    pt->color_anim = w->data + an->color_ofs;
            }
            *(void **)&pt->ndef = 0;
            *(void **)&pt->cull = (void *)1;
            if (an->vec)
                pt->user.v4 = *(MapVec4 *)an->vec;
            else
                nj_fill8(&pt->user, 0, 16);
        }
    }
    df = model->defs;
    w->ents = yuki_alloc(model->ndef * 8);
    for (i = 0; i < model->ndef; i++, df++) {
        w->ents[i].key = w->parts[df->part_idx].ndef++;
        w->ents[i].val = 0;
        an = (MapAnim *)&w->slots[df->slot_idx];
        r = ulVec3Distance(model->sphere[df->slot_idx], df->pos) + w->parts[df->part_idx].radius;
        *(float *)&((MapSlot *)an)->radius = MAX_F(*(float *)&((MapSlot *)an)->radius, r);
    }
    w->far_fade = 0;
    w->far_clip = 1;
    w->unk_3c = 0;
    w->sort_type = 0;
    pt = yuki_alloc(model->ndef * 12);
    map_debug_part = -1;
    *(void **)&w->sort_keys = pt; /* steer(alias) */
    map_run_part_hooks(w, 0);
}

/* Frees owned wire data too. */
/* 0x00133b98 */
void map_task_release(MapObj *w)
{
    int i;

    map_run_part_hooks(w, 1);
    yuki_free(w->sort_keys.p);
    w->sort_keys.p = 0;
    yuki_free(w->ents);
    do { w->ents = 0; } while (0);
    for (i = 0; w->model->npart > i; i++) {
        if (w->own_wires) {
            if (w->parts[i].wire) {
                model_free_block(w->parts[i].wire);
            }
        }
        dc_object_release(&w->parts[i]);
    }
    yuki_free(w->parts);
    w->parts = 0;
    yuki_free(w->slots);
    w->slots = 0;
}

/* 16-beat cycle position (0..1); parts advance to bar position, map frame, or own clock. */
/* 0x00133c60 */
void map_task_exec(MapObj *w)
{
    float interval, bar;
    int i;
    MapPart *p;

    interval = beat_clock_get_interval();
    bar = (interval - (float)beat_clock_get_frames_to_beat()) / interval * 0.0625f;
    bar += (float)(beat_clock_get_haku() % 16) * 0.0625f;
    w->beat_pos = bar;
    bar = beat_clock_get_bar();
    p = w->parts;
    for (i = 0; i < w->model->npart; i++, p++) {
        if (p->bar_sync) {
            p->time = p->length * bar;
        } else if (p->follow_frame) {
            p->time = w->frame;
        } else {
            dc_object_advance_time(p);
        }
    }
    map_run_part_hooks(w, 2);
}

/*
 * Display list 4. Debug flag 8: draw count, frame, sort type; pad 0 left stick singles out a part
 * to check its radius (red when too small), stick up shows all.
 */
/* 0x00133d78 */
void map_task_disp(MapObj *w)
{
    float radius[2];
    int old;
    int i;

    old = RenderSetCurrentDisplayList(4);
    if (w->far_fade || !w->far_clip)
        model_clip_far_off();
    else
        model_clip_far_on();
    for (i = 0; i < ((MapModel *)w->model)->nslot; i++)
        w->slots[i].culled = !view_sphere_visible(((MapModel *)w->model)->sphere[i], *(float *)&w->slots[i].radius);
    map_draw_num = 0;
    ulGxSetTextureList(w->p4);
    if (w->sort_type == 0)
        map_run_part_hooks(w, 3);
    else
        map_draw_sorted(w);
    if (debug_flags_test(8) && debug_disp_level == 0) {
        debug_set_origin(0, -1);
        debug_printf("MapDrawNum %d / %d", map_draw_num, w->model->ndef);
        debug_set_origin(0, -2);
        debug_printf("Frame      %f", w->frame);
        debug_set_origin(0, -3);
        debug_printf("sort_type %d", w->sort_type);
        debug_set_origin(0, -6);
        if (map_debug_part == -1)
            debug_printf("draw all");
        else {
            MapPart *part = &w->parts[map_debug_part];

            dc_object_calc_radius(part, &radius[0], &radius[1]);
            if (part->radius < radius[0] || part->radius < radius[1] || radius[0] < radius[1])
                debug_set_color(RGBA(0x80, 0, 0, 0x80));
            debug_printf("%d(%f:%f:%f)", map_debug_part, part->radius, radius[0], radius[1]);
            debug_set_color(COL_WHITE);
        }
        if (map_debug_repeat > 0)
            map_debug_repeat--;
        else {
            short x = ((short *)pad_system.pad[0])[46];

            if (x > 64) {
                if (map_debug_part == w->model->npart - 1)
                    map_debug_part = -1;
                else
                    map_debug_part = map_debug_part + 1;
                map_debug_repeat = 30;
            } else if (x < -64) {
                if (map_debug_part == -1)
                    map_debug_part = w->model->npart - 1;
                else
                    map_debug_part = map_debug_part - 1;
                map_debug_repeat = 30;
            }
        }
        if (((short *)pad_system.pad[0])[47] < -64)
            map_debug_part = -1;
    }
    model_clip_far_off();
    RenderSetCurrentDisplayList(old);
}

/* Mode 0. */
/* 0x001340b0 */
void map_task_create(void *parent, int model)
{
    int arg[3];

    arg[0] = 0;
    arg[1] = 0;
    arg[2] = model;
    task_create_inherit_arg(parent, &map_task_desc, 72, arg);
}

/* Mode 1; parts load their own wire data. */
/* 0x001340e8 */
void map_task_create_mode1(void *parent, int model)
{
    int arg[3];

    arg[0] = 1;
    arg[1] = 1;
    arg[2] = model;
    task_create_inherit_arg(parent, &map_task_m1_f1_desc, 72, arg);
}

/* Mode 1; wire data bound by the map manager. */
/* 0x00134120 */
void map_task_create_mode1_wires(void *parent, int model)
{
    int arg[3];

    arg[0] = 1;
    arg[1] = 0;
    arg[2] = model;
    task_create_inherit_arg(parent, &map_task_m1_f0_desc, 72, arg);
}

/* Mode 2; parts load their own wire data. */
/* 0x00134158 */
int map_task_create_mode2(void *parent, int model)
{
    int arg[3];

    arg[0] = 2;
    arg[1] = 1;
    arg[2] = model;
    return task_create_inherit_arg(parent, &map_task_m2_f1_desc, 72, arg);
}

/* Mode 2; wire data bound by the map manager. */
/* 0x00134198 */
void map_task_create_mode2_wires(void *parent, int model)
{
    int arg[3];

    arg[0] = 2;
    arg[1] = 0;
    arg[2] = model;
    task_create_inherit_arg(parent, &map_task_m2_f0_desc, 72, arg);
}

/* For frame-following parts. Returns the previous one. */
/* 0x001341d0 */
float map_set_frame(MapObj *w, float v)
{
    float old = w->frame;

    w->frame = v;
    return old;
}

/* 0x001341e0 */
void map_part_get_vector(MapObj *w, int i, MapVec4 *out)
{
    MapPart *part = &w->parts[i];

    *out = part->user.v4;
}

/* 0x00134218 */
void map_part_set_vector(MapObj *w, int i, MapVec4 *in)
{
    MapPart *part = &w->parts[i];

    part->user.v4 = *in;
}

/* Returns the previous setting. */
/* 0x00134250 */
int map_part_set_follow_frame(MapObj *w, int i, int follow)
{
    MapPart *part = &w->parts[i];
    int old = part->follow_frame;

    part->follow_frame = follow;
    return old;
}

/* Returns the previous setting. */
/* 0x00134270 */
int map_slot_set_clip(MapObj *w, int i, int clip)
{
    MapSlot *slot = &w->slots[i];
    int old = slot->clip;

    slot->clip = clip;
    return old;
}

/* Returns the previous setting. */
/* 0x00134290 */
int map_part_set_cull_flag(MapObj *w, int i, int cull)
{
    MapPart *part = &w->parts[i];
    int old = part->cull;

    part->cull = cull;
    return old;
}

/* 0x001342b0 */
void *map_get_model(MapObj *w)
{
    return w->model;
}

/* Returns the previous one. */
/* 0x001342b8 */
int map_part_set_wire(MapObj *w, int i, int wire)
{
    MapPart *part = &w->parts[i];
    int old = part->wire;

    part->wire = wire;
    dc_object_set_wire(part, wire);
    return old;
}

/* Returns the previous one. */
/* 0x00134300 */
int map_part_set_color(MapObj *w, int i, int col)
{
    MapPart *part = &w->parts[i];
    int old = part->color;

    part->color = col;
    return old;
}

/* Returns the previous one. */
/* 0x00134320 */
int map_part_set_display_list(MapObj *w, int i, int dlist)
{
    MapPart *part = &w->parts[i];
    int old = part->dlist;

    part->dlist = dlist;
    return old;
}

/* Returns the previous setting. */
/* 0x00134340 */
int map_set_enable(MapObj *w, int enable)
{
    int old = w->enable;

    w->enable = enable;
    return old;
}

/* Returns the previous setting. */
/* 0x00134350 */
int map_set_far_clip(MapObj *w, int far_clip)
{
    int old = w->far_clip;

    w->far_clip = far_clip;
    return old;
}

/* Returns the previous setting. */
/* 0x00134360 */
int map_set_far_fade(MapObj *w, int far_fade)
{
    int old = w->far_fade;

    w->far_fade = far_fade;
    return old;
}

/* 0 definition order, 1..3 sorted (map_draw_sorted). Returns the previous one. */
/* 0x00134370 */
int map_set_sort_type(MapObj *w, int sort_type)
{
    int old = w->sort_type;

    w->sort_type = sort_type;
    return old;
}

/* Bar-synced maps: frame-following bar parts back to regular clips; alternate colour animations. */
/* 0x00134380 */
void map_rebind_anims(MapObj *w)
{
    MapPart *p = w->parts;
    MapModelInfo *model = w->model;
    MapAnim *anim = model->anm;
    int i, n;

    if (w->mode >= 2) {
        for (i = 0, n = model->npart; i < n; p++, anim++) {
            if (p->bar_sync) {
                if (anim->follow_frame) {
                    if (anim->clip_ofs == 0) {
                        dc_object_update(p, 0.0f);
                        dc_object_set_clip(p, 0);
                        p->bar_sync = 0;
                    } else if (anim->clip_b_ofs == 0 || anim->clip_c_ofs == 0) {
                        dc_object_update(p, 0.0f);
                        dc_object_set_clip(p, (DcClip *)(w->data + anim->clip_ofs));
                        p->bar_sync = 0;
                    } else {
                        dc_object_set_clip_cursor(p, (DcClip *)(w->data + anim->clip_ofs), w->data + anim->clip_b_ofs, w->data + anim->clip_c_ofs);
                        p->bar_sync = 0;
                    }
                    n = model->npart;
                }
            }
            if (anim->color_alt_ofs) {
                p->color_anim = w->data + anim->color_alt_ofs;
            }
            i++;
        }
    }
}

/* Bar-synced maps: bar clips where present, else regular; regular colour animation. */
/* 0x001344b0 */
void map_rebind_anims_mode2(MapObj *w)
{
    MapPart *p = w->parts;
    MapModelInfo *model = w->model;
    MapAnim *anim = model->anm;
    int i;

    if (w->mode >= 2) {
        for (i = 0; i < model->npart; i++, p++, anim++) {
            p->bar_sync = anim->bar_clip_ofs != 0;
            if (p->bar_sync) {
                if (anim->bar_clip_ofs == 0) {
                    dc_object_update(p, 0.0f);
                    dc_object_set_clip(p, 0);
                } else if (anim->bar_clip_c_ofs == 0 || anim->bar_clip_b_ofs == 0) {
                    dc_object_update(p, 0.0f);
                    dc_object_set_clip(p, (DcClip *)(w->data + anim->bar_clip_ofs));
                } else {
                    dc_object_set_clip_cursor(p, (DcClip *)(w->data + anim->bar_clip_ofs), w->data + anim->bar_clip_b_ofs, w->data + anim->bar_clip_c_ofs);
                }
            } else {
                if (anim->clip_ofs == 0) {
                    dc_object_update(p, 0.0f);
                    dc_object_set_clip(p, 0);
                } else if (anim->clip_c_ofs == 0 || anim->clip_b_ofs == 0) {
                    dc_object_update(p, 0.0f);
                    dc_object_set_clip(p, (DcClip *)(w->data + anim->clip_ofs));
                } else {
                    dc_object_set_clip_cursor(p, (DcClip *)(w->data + anim->clip_ofs), w->data + anim->clip_b_ofs, w->data + anim->clip_c_ofs);
                }
            }
            *(void **)&p->color_anim = anim->color_ofs ? (void *)(w->data + anim->color_ofs) : 0;
        }
    }
}

/* 0x00134630 */
void map_parts_reset_time(MapObj *w)
{
    int i;

    for (i = 0; i < w->model->npart; i++) {
        dc_object_update(&w->parts[i], 0.0f);
    }
}

/* 0x001346a0 */
void map_part_set_radius(MapObj *w, int i, float v)
{
    w->parts[i].radius = v;
}

/* 0x001346b8 */
void map_set_part_radii(MapObj *w, float *radii)
{
    int i;
    int n = w->model->npart;

    for (i = 0; i < n; i++) {
        w->parts[i].radius = *radii++;
    }
}

/* Nodes without 0x80 play the cursor on their shape unless hidden (8); 0x10 skips children. */
/* 0x00134700 */
void map_node_anim_walk(DcNode *node)
{
    unsigned flags;

    do {
        flags = node->flags;
        if (!(flags & 0x80)) {
            if (node->data && !(flags & 8)) {
                if (**anim_cur_cursor != 0) {
                    anim_cursor_play(node->data);
                }
            }
            anim_cursor_next();
        }
        if (!(flags & 0x10)) {
            map_node_anim_walk(node->child);
        }
        node = node->next;
    } while (node);
}

/* Cursor animation (mode 2). */
/* 0x00134798 */
void dc_object_update(MapPart *part, float time)
{
    if (part->flags & 2) {
        anim_start(part->clip, time);
        anim_set_slot_table(part->clip_c);
        anim_select_cursor(0);
        anim_cursor_start(part->clip_b, time);
        map_node_anim_walk(part->root);
    }
}

/* z, then y, then x. */
/* 0x00134810 */
void map_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}
