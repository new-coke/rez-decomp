/*
 * Shape transition: dims the screen to grey, closes in a border and plays an animated shape from
 * the system data bank, then eases out and removes itself. Also a small 2D strip renderer.
 */

#include "types_game.h"
#include "color.h"
#include "task.h"
#include "libc.h"
#include "nj_cnk_direct.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "fade_shape.h"
#include "anim.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "screen.h"
#include "system.h"

typedef struct ShapeNode {
    int eval_flags;
    int *data;              /* 0x04 */
    char pad_08[36];
    struct ShapeNode *child;    /* 0x2c */
    struct ShapeNode *next;     /* 0x30 */
} ShapeNode;

typedef struct SrcVtx {
    char pad_00[16];
    float x;                /* 0x10 */
    float y;                /* 0x14 */
    char pad_18[40];
} SrcVtx;

extern int fade_shape_motion_offsets[];
extern void (*fade_shape_exec_funcs[])(FadeShapeWork *n);
extern void (*fade_shape_disp_funcs[])(FadeShapeWork *n);
extern int system_pnb;
extern void *scr_root;
extern struct TaskDesc fade_shape_desc;

extern int fade_shape_object_offsets[];
extern short *fade_shape_cmd_slots[];
extern SrcVtx gx_vertex_buffer[];
extern int **anim_cur_cursor;
extern int ulMatrixPush(void);

static void fade_shape_scan_tree(ShapeNode *node);

/* Grey fade (to 240) and border (to 50) ease in, motion first to last; sets busy, held. */
/* 0x001160d0 */
static void fade_shape_state_in(FadeShapeWork *w)
{
    float t;
    float x;
    unsigned int *p;

    switch ((unsigned int)w->phase) {
    case 0:
        w->phase++;
        w->frm = 1.0f;
        w->count++;
    case 1:
        if (w->in_len > 0) {
            t = w->frm / w->in_len;
            x = ease_sine_in_out(t);
            screen_set_fade_alpha((int)(x * 240.0f));
            screen_set_fade_color(COL_WHITE);
            screen_set_margin((int)(x * 50.0f));
            p = (unsigned int *)(w->base + fade_shape_motion_offsets[w->kind]);
            w->mot_frm = (float)p[1] * t;
        }
        w->frm += 1.0f;
        if (w->in_len < w->frm) {
            w->flags |= 3;
            do {
                w->count = w->phase + 1;
            } while (0);
        }
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        break;
    }
}

/* Over out_len: fade, border and shape alpha ease down, then the task dies. */
/* 0x00116270 */
static void fade_shape_state_out(FadeShapeWork *w)
{
    float t;
    float x;

    switch ((unsigned int)w->phase) {
    case 0:
        w->phase++;
        w->frm = 1.0f;
        w->count++;
    case 1:
        if (w->out_len > 0) {
            t = w->frm / w->out_len;
            x = ease_sine_in_out_rev(t);
            screen_set_fade_alpha((int)(x * 240.0f));
            screen_set_fade_color(COL_WHITE);
            screen_set_margin((int)(x * 50.0f));
            w->alpha = (int)(x * 255.0f);
        }
        w->frm += 1.0f;
        if (w->out_len < w->frm)
            w->count = w->phase + 1;
        if (w->pstate == w->state && w->phase == w->count)
            break;
        w->phase++;
    case 2:
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
        break;
    }
}

/* Clears held, latches the requested state and step, runs fade_shape_exec_funcs[state]. */
/* 0x001163e0 */
static void fade_shape_exec(FadeShapeWork *w)
{
    void (*fn)(FadeShapeWork *);

    w->flags &= ~2;
    do {
        w->pstate = w->state;
        w->phase = w->count;
    } while (0);
    fn = fade_shape_exec_funcs[w->state];
    if (fn != 0)
        fn(w);
}

/* Switches the screen renderer (previous kept in w->saved for release), then state 1. */
/* 0x00116440 */
static void fade_shape_state_start(FadeShapeWork *w)
{
    switch ((unsigned int)w->phase) {
    case 0:
        w->saved = screen_swap_render(1);
        w->phase++;
        w->count++;
    case 1:
        do {
            w->state = 1;
            w->count = 0;
        } while (0);
        if (w->pstate == 1 && w->phase == 0)
            break;
        w->phase++;
    case 2:
        break;
    }
}

/* 0x00116530 */
static void fade_shape_disp(FadeShapeWork *w)
{
    void (*fn)(FadeShapeWork *) = fade_shape_disp_funcs[w->pstate];

    if (fn != 0)
        fn(w);
}

/* Opaque; shapes from system_pnb; display list 12. */
/* 0x00116570 */
static void fade_shape_init(FadeShapeWork *w)
{
    w->flags = 0;
    w->alpha = 255;
    do {
        w->frm = 0;
    } while (0);
    w->mot_frm = 0;
    w->p4 = 0;
    w->base = system_pnb;
    w->kind = 0;
    do {
        w->state = 0;
        w->count = 0;
    } while (0);
    task_set_dlist(w, 12);
}

/* 0x001165a8 */
static void fade_shape_release(FadeShapeWork *w)
{
    screen_set_fade_alpha(0);
    screen_set_margin(0);
    screen_swap_render(w->saved);
}

/* Under scr_root; parent unused. */
/* 0x001165e0 */
static int fade_shape_create(void *parent, int prm)
{
    return task_create_inherit_arg(scr_root, &fade_shape_desc, 56, prm);
}

/* 0x00116610 */
static int fade_shape_create_default(void *parent)
{
    return fade_shape_create(parent, 0);
}

/* Shape 0; prepares its command lists. */
/* 0x00116630 */
static void fade_shape_start_in(FadeShapeWork *w, int len)
{
    int tree;

    w->in_len = len;
    tree = w->base + fade_shape_object_offsets[0];
    w->kind = 0;
    fade_shape_scan_tree(tree);
}

/* 0x00116658 */
static void fade_shape_start_out(FadeShapeWork *w, int len)
{
    w->out_len = len;
    do {
        w->state = 2;
        w->count = 0;
    } while (0);
}

/* 0x00116678 */
static int fade_shape_is_busy(FadeShapeWork *w)
{
    return w->flags & 1;
}

/* 0x00116688 */
static int fade_shape_is_held(FadeShapeWork *w)
{
    return w->flags & 2;
}

/* Gone or marked for removal. */
/* 0x00116698 */
static int task_is_dead(void *w)
{
    int dead = 0;

    if (w == 0 || (TASK_OF(w)->flags & TASK_F_END))
        dead = 1;
    return dead;
}

/*
 * 16-bit commands ending at 0xff: 4 records sub-list starts in fade_shape_cmd_slots, 5 calls a
 * sub-list, 8 has its argument cleared; others skipped by length.
 */
/* 0x001166c0 */
static void fade_shape_scan_commands(short *p)
{
    int cmd;
    int arg;

    cmd = (unsigned short)*p;
    while (*p != 255) {
        arg = p[1];
        switch (cmd & 0xff) {
        case 0:
            p += 1;
            break;
        case 8:
            p[1] = 0;
            p += 2;
            break;
        case 4:
            p += 1;
            fade_shape_cmd_slots[(cmd & 0xf00) >> 8] = p;
            return;
        case 5:
            p += 1;
            fade_shape_scan_commands(fade_shape_cmd_slots[(cmd & 0xf00) >> 8]);
            break;
        default:
            p = (short *)((char *)p + (arg * 2 + 4));
            break;
        }
        cmd = (unsigned short)*p;
    }
}

/* Siblings through next, children through child. */
/* 0x001167d0 */
static void fade_shape_scan_tree(ShapeNode *node)
{
    do {
        int *data = node->data;

        if (data != 0) {
            int cmds = data[1];
            ShapeNode *ch;

            if (cmds != 0)
                fade_shape_scan_commands(cmds);
            ch = node->child;
            if (ch != 0)
                fade_shape_scan_tree(ch);
        }
        node = node->next;
    } while (node != 0);
}

/* Does nothing, so polygon lists draw nothing. */
/* 0x00116838 */
static void fade_shape_poly_vertex(void)
{
}

/*
 * vert: gx_vertex_buffer index, u, v; mapped to 640x512, col with full RGB. flag continues the
 * strip, but out is not kept: reused vertices, z and colour are stale stack (original quirk).
 */
/* 0x00116840 */
void fade_shape_triangle(short *vert, unsigned int col, int flag)
{
    Vtx out[3];
    SrcVtx *p0 = &gx_vertex_buffer[vert[0]];
    SrcVtx *p1 = &gx_vertex_buffer[vert[3]];
    SrcVtx *p2 = &gx_vertex_buffer[vert[6]];

    col |= 0x808080;
    if (flag == 0) {
        out[0].x = (p0->x - 32768.0f) * 0.0625f + 320.0f;
        out[0].y = (p0->y - 32768.0f) * 0.125f + 256.0f;
        out[0].z = 1.0f;
        out[0].u = (float)vert[1] * 0.00390625f;
        out[0].v = (float)vert[2] * 0.00390625f;
        out[0].col = col;
        out[1].x = (p1->x - 32768.0f) * 0.0625f + 320.0f;
        out[1].y = (p1->y - 32768.0f) * 0.125f + 256.0f;
        out[1].z = 1.0f;
        out[1].u = (float)vert[4] * 0.00390625f;
        out[1].v = (float)vert[5] * 0.00390625f;
        out[1].col = col;
        out[2].x = (p2->x - 32768.0f) * 0.0625f + 320.0f;
        out[2].y = (p2->y - 32768.0f) * 0.125f + 256.0f;
        out[2].z = 1.0f;
        out[2].u = (float)vert[7] * 0.00390625f;
        out[2].v = (float)vert[8] * 0.00390625f;
        out[2].col = col;
    } else {
        out[0] = out[1];
        out[1] = out[2];
        out[2].x = (p2->x - 32768.0f) * 0.0625f + 320.0f;
        out[2].y = (p2->y - 32768.0f) * 0.125f + 256.0f;
        out[2].u = (float)vert[7] * 0.00390625f;
        out[2].v = (float)vert[8] * 0.00390625f;
    }
    ulGxAddTexStrip(out, 4);
}

/* Record count, then records of n vertex words. */
/* 0x00116ac8 */
static void fade_shape_poly_list(short *p, int col)
{
    int cnt;
    int n;
    short *vert;

    p += 2;
    cnt = *p;
    p++;
    while (cnt--) {
        n = *p;
        vert = p + 1;
        if (n < 0)
            n = -n;
        p = vert + n;
        n -= 2;
        while (n--) {
            ((void (*)(short *, int))fade_shape_poly_vertex)(vert, col); /* steer(cast) */
            vert++;
        }
    }
}

/* Record count, then records of n three-short vertices; one triangle per vertex after the first. */
/* 0x00116b90 */
void fade_shape_fan_list(short *p, int col)
{
    int cnt;
    int n;
    short *vert;

    p += 2;
    cnt = *p;
    ulGxBeginTexStrip(0);
    p++;
    while (cnt--) {
        vert = p + 1;
        n = *p;
        n = (int)fabsf((float)n);
        p = (short *)((char *)vert + n * 6);
        n -= 3;
        fade_shape_triangle(vert, col, 0);
        while (n--) {
            fade_shape_triangle(vert, col, 1);
            vert += 3;
        }
    }
    ulGxEndTexStrip();
}

/* As fade_shape_scan_commands, plus 8 selects a texture, 64 a polygon list, 65 a strip list. */
/* 0x00116c80 */
static void fade_shape_draw_commands(short *p, int col)
{
    int cmd;
    int len;

    cmd = (unsigned short)*p;
    while (*p != 255) {
        len = p[1];
        switch (cmd & 0xff) {
        case 0:
            p += 1;
            break;
        case 8:
            ulGxSelectTexture(ulGxGetTextureList(), (unsigned short)p[1] & 0x1fff);
            p += 2;
            break;
        case 4:
            p += 1;
            fade_shape_cmd_slots[(cmd & 0xf00) >> 8] = p;
            return;
        case 5:
            fade_shape_draw_commands(fade_shape_cmd_slots[(cmd & 0xf00) >> 8], col);
            p += 1;
            break;
        case 64:
            fade_shape_poly_list(p, col);
            p = (short *)((char *)p + (len * 2 + 4));
            break;
        case 65:
            fade_shape_fan_list(p, col);
            p = (short *)((char *)p + (len * 2 + 4));
            break;
        default:
            p = (short *)((char *)p + (len * 2 + 4));
            break;
        }
        cmd = (unsigned short)*p;
    }
}

/* Hidden by flag bit 3. Vertices into gx_vertex_buffer. */
/* 0x00116de0 */
static void fade_shape_draw_object(int *obj, int aux, int motion, float frame, int color)
{
    int *node;
    int tex;
    char *col;

    ulMatrixPush();
    if (!(obj[0] & 8)) {
        node = (int *)obj[1];
        col = gx_vertex_buffer;
        tex = node[0];
        anim_select_cursor(0);
        anim_cursor_start(motion, frame);
        if (**anim_cur_cursor != 0)
            anim_cursor_play(node);
        ulGxSetMatrix(0, ulMatrixCur);
        if (tex != 0)
            nj_cnk_wire_vlist(tex, col);
        fade_shape_draw_commands((short *)((int *)obj[1])[1], color);
    }
    ulMatrixPop();
}

/* Flat before the camera; opaque at 255, else blended; restores alpha and depth test after. */
/* 0x00116ec8 */
void fade_shape_draw(int obj, int aux, int motion, int slots, int alpha, float frame)
{
    int *tex_list;
    float inv_dist;

    alpha &= 0xff;
    anim_set_slot_table(slots);
    psGsSetAlphaTest(0, 5, 128, 1);
    psGsSetDepthTest(1, 1);
    psGsSetZMask(1);
    if (alpha == 255)
        draw_set_blend(1, 0);
    else
        draw_set_blend(4, 5);
    tex_list = screen_get_tex_list();
    ulGxSetTextureList(tex_list);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    psGsWriteReg(6, *(long *)(*(int *)(*tex_list + 8)) & ~((long)1 << 34));
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -1.0f);
    inv_dist = 1.0f / cam_dist;
    vu0_matrix_scale(ulMatrixCur, inv_dist, inv_dist, 1.0f);
    fade_shape_draw_object(obj, aux, motion, frame, alpha << 24);
    ulMatrixPop();
    psGsSetDepthTest(1, 2);
    psGsSetZMask(0);
    psGsSetAlphaTest(1, 5, 128, 1);
}
