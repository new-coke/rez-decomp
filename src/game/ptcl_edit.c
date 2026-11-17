/*
 * Particle editor (debug, "PtclEdit" task): edits ptcl.dat entries in a variable-editor window
 * (emission, blend, shooter shape, gravity, black hole, texture; texture/colour sub-entries),
 * previews in front of a stored camera position, applies to the game or saves ptcl.dat to the PC.
 */

#include "types_game.h"
#include "system.h"
#include "types_gw.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_varedit.h"
#include "gw_varedit_win.h"
#include "gw_window.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "ptcl_edit.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "debugdraw.h"
#include "debugmenu.h"
#include "lockon.h"
#include "node_draw_state.h"
#include "pcfile.h"
#include "ptcl.h"
#include "task.h"
#include "yuki_tool.h"

typedef struct { float x, y, z, u, v; unsigned c; } GradVtx;
#define FREE(p) do { yuki_free(p); (p) = 0; } while (0)

#define WF(off) ((int *)((char *)w + (off)))

extern PtclEdit *ptcl_edit_work;
extern int ptcl_edit_vars_root;
extern int ptcl_texture;
extern void ptcl_data_set(PtclData *, PtclEntData *, int);
extern GwVarDef ptcl_edit_vardefs[];
extern GwVarDef ptcl_edit_ptcl_vardefs[];
extern GwVarDef ptcl_edit_blend_vardefs[];
extern GwVarDef ptcl_edit_shooter_vardefs[];
extern GwVarDef ptcl_edit_gravity_vardefs[];
extern GwVarDef ptcl_edit_black_hall_vardefs[];
extern GwVarDef ptcl_edit_texture_vardefs[];
extern char str_ptcl_edit_ptcl[];
extern char str_ptcl_edit_shooter[];
extern char str_ptcl_edit_gravity[];
extern char str_ptcl_edit_black_hall[];
extern char str_ptcl_edit_texture[];
extern char str_ptcl_edit_window[];
extern char str_ptcl_edit_texture_window[];
extern int ptcl_data_get_count(int);
extern char str_ptcl_edit_menu[];
extern unsigned char kbd_state[];
extern void ulMatrixPush(void);
extern struct TaskDesc ptcl_edit_desc;

static void ptcl_edit_rotate_yxz(int *ang);

/* 0x00159908 */
static PtclEnt *ptcl_ent_next(PtclEnt *node, int n)
{
    while (n-- > 0)
        node = node->next;
    return node;
}

/* 0x00159940 */
static void ptcl_edit_close_callback(void)
{
    PtclEdit *w = ptcl_edit_work;

    if (w) {
        ptcl_edit_work = 0;
        do { TASK_FLAGS(w) |= TASK_F_END; } while (0);
    }
    gw_varedit_free_root(&ptcl_edit_vars_root);
}

/*
 * Selected sub-entry's texture rect, tinted by the entry's colour there (start -> middle -> end
 * over the sub-entries). Packed red in bits 16..23, blue in 0..7: reversed from GS order.
 */
/* 0x00159968 */
static void ptcl_edit_draw_gradient_quad(int win)
{
    GradVtx vt[4];
    int rect[4];
    PtclEdit *w = ptcl_edit_work;
    PtclSub *sub;
    PtclEntData *ed;
    float t, rate;
    float *uv;
    unsigned char *to, *from;
    float left, top, right, bottom;
    int r, g, b;
    unsigned col;

    gw_window_get_abs_pos(win, &rect[0], &rect[1]);
    sub = &w->sub;
    uv = (float *)sub;
    gw_window_get_size(win, &rect[2], &rect[3]);
    ed = &w->ed;
    ulGxSetTextureList(ptcl_texture);
    ulGxSelectTexture(ulGxGetTextureList(), sub->tex_uv[0]);
    draw_push_blend();
    left = (float)rect[0];
    top = (float)rect[1];
    right = (float)(rect[0] + rect[2]);
    bottom = (float)(rect[1] + rect[3]);
    vt[0].x = vt[1].x = left;
    vt[2].x = vt[3].x = right;
    vt[0].y = vt[2].y = top;
    vt[1].y = vt[3].y = bottom;
    vt[0].z = vt[1].z = vt[2].z = vt[3].z = 6.6666665f;
    t = (float)ptcl_edit_work->subidx / (float)ed->nsub;
    vt[0].u = vt[1].u = uv[1];
    vt[2].u = vt[3].u = uv[3];
    vt[0].v = vt[2].v = uv[2];
    vt[1].v = vt[3].v = uv[4];
    from = (unsigned char *)&ed->col[0];
    to = (unsigned char *)&ed->col[2];
    if (t < 0.5f) {
        rate = t + t;
        to = (unsigned char *)&ed->col[1];
    } else {
        rate = (t - 0.5f) + (t - 0.5f);
        from = (unsigned char *)&ed->col[1];
    }
    r = (int)((float)to[0] * rate + (float)from[0] * (1.0f - rate));
    b = (int)((float)to[2] * rate + (float)from[2] * (1.0f - rate));
    g = (int)((float)to[1] * rate + (float)from[1] * (1.0f - rate));
    col = 0x80000000u | ((r + 1) >> 1 & 0xff) << 16 | ((g + 1) >> 1 & 0xff) << 8 | ((b + 1) >> 1 & 0xff);
    vt[0].c = vt[1].c = vt[2].c = vt[3].c = col;
    ulGxBeginTexStrip(1);
    ulGxAddTexStrip(vt, 4);
    ulGxEndTexStrip();
    draw_pop_blend();
}

/* With a new copy of its sub-entry table. */
/* 0x00159be8 */
static void ptcl_ent_copy(PtclEntData *src, PtclEntData *dst)
{
    nj_copy8(dst, src, 172);
    dst->sub = yuki_alloc(dst->nsub * 20);
    nj_copy8(dst->sub, src->sub, dst->nsub * 20);
}

/* 0x00159c58 */
static void ptcl_ent_free_sub(int *ent)
{
    yuki_free(ent[11]);
    ent[11] = 0;
}

/* Into the current entry and its selected sub-entry. */
/* 0x00159c88 */
static void ptcl_edit_store_cur(void)
{
    PtclEnt *node = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);

    node->flag = ptcl_edit_work->flag;
    node->data = ptcl_edit_work->ed;
    node->data.sub[ptcl_edit_work->subidx] = ptcl_edit_work->sub;
}

/* From the current entry and its selected sub-entry. */
/* 0x00159dd0 */
static void ptcl_edit_load_cur(void)
{
    PtclEnt *node = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);

    ptcl_edit_work->flag = node->flag;
    ptcl_edit_work->ed = node->data;
    ptcl_edit_work->sub = node->data.sub[ptcl_edit_work->subidx];
}

/* 0x00159f18 */
static void ptcl_edit_var_changed(void)
{
    ptcl_edit_store_cur();
}

/* Wraps the entry number, selects sub-entry 0, loads it. */
/* 0x00159f20 */
static void ptcl_edit_wrap_cur(void)
{
    PtclEdit *w = ptcl_edit_work;

    w->cur = (w->cur + w->count) % w->count;
    w->subidx = 0;
    ptcl_edit_load_cur();
}

/* Wraps the texture number into the particle texture count. */
/* 0x00159f50 */
static void ptcl_edit_wrap_field(void)
{
    PtclEdit *w = ptcl_edit_work;
    int *tex = ptcl_texture;

    w->sub.tex_uv[0] = (w->sub.tex_uv[0] + tex[5]) % tex[5];
    ptcl_edit_var_changed();
}

/* Wraps the sub-entry number, loads that one. */
/* 0x00159f88 */
static void ptcl_edit_wrap_sub_idx(void)
{
    PtclEnt *node = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);

    ptcl_edit_work->subidx = (ptcl_edit_work->subidx + node->data.nsub) % node->data.nsub;
    ptcl_edit_load_cur();
}

/* Preview end: forgets it, counts it off. */
/* 0x00159fe0 */
static void ptcl_ent_task_gone(void *ptcl, PtclEnt *node)
{
    PtclEdit *w = ptcl_edit_work;

    node->task = 0;
    w->busy--;
}

/* Space: ends running previews, or plays the current entry (runs while paused). */
/* 0x0015a000 */
static void ptcl_edit_preview_cur(void)
{
    PtclEnt *node;
    void *task;
    PtclEdit *ed;

    if (ptcl_edit_work->busy != 0) {
        task_end_children(ptcl_edit_work);
        return;
    }
    node = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);
    task = ptcl_task_create(ptcl_edit_work, node, ptcl_ent_task_gone, node);
    ed = ptcl_edit_work;
    TASK_FLAGS(task) |= TASK_F_RUN_PAUSED;
    ed->busy++;
    node->task = task;
}

/* L: looping preview. */
/* 0x0015a090 */
static void ptcl_edit_preview_cur_loop(void)
{
    PtclEnt *node;
    void *task;
    PtclEdit *ed;

    if (ptcl_edit_work->busy != 0) {
        task_end_children(ptcl_edit_work);
        return;
    }
    node = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);
    task = ptcl_task_create_loop(ptcl_edit_work, node, ptcl_ent_task_gone, node);
    ed = ptcl_edit_work;
    TASK_FLAGS(task) |= TASK_F_RUN_PAUSED;
    ed->busy++;
    node->task = task;
}

/* S: the first running preview stops emitting. */
/* 0x0015a120 */
static void ptcl_edit_stop_child(void)
{
    char *w = (char *)ptcl_edit_work;
    char *child = (char *)TASK_OF(w)->child;
    void *work = child ? child + 64 : 0;

    if (work)
        ptcl_task_stop(work);
}

/* Ends running previews, or plays every ticked entry at once. */
/* 0x0015a150 */
static void ptcl_edit_preview_all(void)
{
    PtclEnt *node;
    void *task;
    PtclEdit *ed;

    if (ptcl_edit_work->busy != 0) {
        task_end_children(ptcl_edit_work);
        return;
    }
    for (node = ptcl_edit_work->head; node; node = node->next) {
        if (node->flag) {
            task = ptcl_task_create(ptcl_edit_work, node, ptcl_ent_task_gone, node);
            ed = ptcl_edit_work;
            TASK_FLAGS(task) |= TASK_F_RUN_PAUSED;
            ed->busy++;
            node->task = task;
        }
    }
}

/* Camera position and angles (no roll) -> the entry's emitter placement. */
/* 0x0015a200 */
static void ptcl_edit_grab_camera(void)
{
    float eye[3];
    int rot[3];
    float *de;
    int *dr;

    camera_get_eye(eye);
    camera_get_angles(rot);
    rot[2] = 0;
    de = ptcl_edit_work->ed.eye;
    dr = ptcl_edit_work->ed.rot;
    de[0] = eye[0];
    de[1] = eye[1];
    de[2] = eye[2];
    dr[0] = rot[0];
    dr[1] = rot[1];
    dr[2] = rot[2];
    ptcl_edit_var_changed();
}

/* 0x0015a270 */
static void ptcl_edit_menu_set_camera_pos(void)
{
    camera_move_eye((*(char * *)&ptcl_edit_work) + 72); /* steer(cast) */
}

/* Copy inserted before the current entry; entry count bumped. */
/* 0x0015a280 */
static void ptcl_edit_insert_entry(void)
{
    PtclEnt *node = yuki_alloc(188);
    PtclEnt *cur = ptcl_ent_next(ptcl_edit_work->head, ptcl_edit_work->cur);

    ptcl_ent_copy(cur, node);
    node->next = cur;
    node->task = 0;
    node->prev = cur->prev;
    node->flag = 0;
    if (cur->prev)
        cur->prev->next = node;
    else
        ptcl_edit_work->head = node;
    cur->prev = node;
    ptcl_edit_work->count++;
    ptcl_edit_wrap_cur();
}

/* 0x0015a320 */
static void ptcl_edit_delete_entry(void)
{
    PtclEdit *w = ptcl_edit_work;
    PtclEnt *node;

    if (w->count > 0) {
        node = ptcl_ent_next(w->head, w->cur);
        if (node->prev)
            node->prev->next = node->next;
        else
            *(void **)&ptcl_edit_work->head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            ptcl_edit_work->cur--;
        ptcl_ent_free_sub(node);
        FREE(node);
        ptcl_edit_work->count--;
        ptcl_edit_wrap_cur();
    }
}

/* A copy of the last one. */
/* 0x0015a3e0 */
static void ptcl_edit_add_sub(void)
{
    PtclEntData *ed = &ptcl_edit_work->ed;
    PtclSub *old = ed->sub;

    ed->sub = yuki_alloc((ed->nsub + 1) * sizeof(PtclSub));
    nj_copy8(ed->sub, old, ed->nsub * sizeof(PtclSub));
    nj_copy8(ed->sub + ed->nsub, old + ed->nsub - 1, sizeof(PtclSub));
    ed->nsub++;
    do { yuki_free(old); } while (0);
    ptcl_edit_var_changed();
}

/* Keeps the selection in range. */
/* 0x0015a480 */
static void ptcl_edit_delete_sub(void)
{
    PtclEntData *ed = &ptcl_edit_work->ed;
    PtclSub *old = ed->sub;
    PtclSub *nw;
    PtclEdit *w;

    nw = yuki_alloc((ed->nsub - 1) * 20);
    ed->sub = nw;
    nj_copy8(nw, old, (ed->nsub - 1) * 20);
    ed->nsub--;
    FREE(old);
    w = ptcl_edit_work;
    w->subidx = (w->subidx + ed->nsub) % ed->nsub;
    w->sub = ed->sub[w->subidx];
    ptcl_edit_var_changed();
}

/* Replaces the game's particle table with a copy of the edited entries. */
/* 0x0015a568 */
static void ptcl_edit_commit(void)
{
    PtclEnt *node;
    PtclEntData *buf;
    PtclEntData *ent;
    int i = 0;

    ptcl_data_free(0);
    node = ptcl_edit_work->head;
    buf = yuki_alloc(ptcl_edit_work->count * 172);
    if (ptcl_edit_work->count > 0) {
        ent = buf;
        do {
            ptcl_ent_copy(node, ent);
            i++;
            ent++;
            node = node->next;
        } while (i < ptcl_edit_work->count);
    }
    ptcl_data_set(0, buf, ptcl_edit_work->count);
}

/* "ptcl.dat" on the PC: entry count, then each entry and its sub-entry table. */
/* 0x0015a618 */
static void ptcl_edit_save_file(void)
{
    char *buf;
    char *dst;
    PtclEnt *node = ptcl_edit_work->head;
    int size = ptcl_edit_work->count * 172 + 4;
    PtclEdit *w;

    if (node) {
        do {
            size += node->data.nsub * 20;
            node = node->next;
        } while (node);
    }
    buf = yuki_alloc(size);
    w = ptcl_edit_work;
    node = w->head;
    nj_copy8(buf, &w->count, 4);
    dst = buf + 4;
    if (node) {
        do {
            nj_copy8(dst, node, 172);
            dst += 172;
            nj_copy8(dst, node->data.sub, node->data.nsub * 20);
            dst += node->data.nsub * 20;
            node = node->next;
        } while (node);
    }
    pcfile_save_set("ptcl.dat", buf, size);
    do {
        yuki_free(buf);
    } while (0);
}

/*
 * Copies the game's particle table to edit; opens the editor and texture preview windows. Runs
 * while paused.
 */
/* 0x0015a738 */
static void ptcl_edit_init(PtclEdit *w)
{
    PtclEnt *last, *prev;
    int i;
    int lx, ly;
    int sub, win;
    int *flag, *num, *shape, *gravity, *black_hole;

    w->cur = 0;
    w->subidx = 0;
    w->busy = 0;
    ptcl_edit_work = w;
    TASK_FLAGS(w) |= TASK_F_RUN_PAUSED;
    w->count = ptcl_data_get_count(0);
    w->head = 0;
    i = 0;
    if (w->count > 0) {
        do { last = 0; } while (0); /* steer(barrier) */
        do {
            prev = last;
            w->head = yuki_alloc(188);
            i++;
            ptcl_ent_copy(ptcl_data_get_entry(0, w->count - i), w->head);
            last = w->head;
            last->task = 0;
            last->next = prev;
            last->prev = 0;
            last->flag = 0;
            if (prev)
                prev->prev = last;
        } while (i < w->count);
    } else
        last = 0;
    last->prev = 0;
    flag = WF(8); num = WF(12); shape = WF(68); gravity = WF(160); black_hole = WF(164);
    ptcl_edit_wrap_cur();
    gw_varedit_init_root(&ptcl_edit_vars_root);
    ptcl_edit_vardefs[0].ptr = WF(208);
    ptcl_edit_vardefs[8].ptr = flag;
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, ptcl_edit_vardefs); /* steer(cast) */
    ptcl_edit_ptcl_vardefs[0].ptr = num;
    ptcl_edit_ptcl_vardefs[1].ptr = WF(16);
    ptcl_edit_ptcl_vardefs[2].ptr = WF(20);
    ptcl_edit_ptcl_vardefs[3].ptr = WF(24);
    ptcl_edit_ptcl_vardefs[4].ptr = WF(28);
    ptcl_edit_ptcl_vardefs[5].ptr = WF(32);
    ptcl_edit_ptcl_vardefs[6].ptr = WF(36);
    ptcl_edit_ptcl_vardefs[7].ptr = WF(40);
    ptcl_edit_ptcl_vardefs[8].ptr = WF(44);
    ptcl_edit_ptcl_vardefs[9].ptr = WF(48);
    ptcl_edit_blend_vardefs[0].ptr = WF(60);
    ptcl_edit_blend_vardefs[1].ptr = WF(64);
    ptcl_edit_shooter_vardefs[0].ptr = WF(72);
    ptcl_edit_shooter_vardefs[1].ptr = WF(76);
    ptcl_edit_shooter_vardefs[2].ptr = WF(80);
    ptcl_edit_shooter_vardefs[3].ptr = WF(84);
    ptcl_edit_shooter_vardefs[4].ptr = WF(88);
    ptcl_edit_shooter_vardefs[5].ptr = WF(92);
    ptcl_edit_shooter_vardefs[6].ptr = shape;
    ptcl_edit_shooter_vardefs[7].ptr = WF(96);
    ptcl_edit_shooter_vardefs[8].ptr = WF(100);
    ptcl_edit_shooter_vardefs[9].ptr = WF(104);
    ptcl_edit_shooter_vardefs[10].ptr = WF(108);
    ptcl_edit_shooter_vardefs[11].ptr = WF(112);
    ptcl_edit_shooter_vardefs[12].ptr = WF(116);
    ptcl_edit_shooter_vardefs[13].ptr = WF(120);
    ptcl_edit_shooter_vardefs[14].ptr = WF(124);
    ptcl_edit_shooter_vardefs[15].ptr = WF(128);
    ptcl_edit_shooter_vardefs[16].ptr = WF(132);
    ptcl_edit_shooter_vardefs[17].ptr = WF(136);
    ptcl_edit_shooter_vardefs[18].ptr = WF(140);
    ptcl_edit_shooter_vardefs[19].ptr = WF(144);
    ptcl_edit_shooter_vardefs[20].ptr = WF(156);
    ptcl_edit_gravity_vardefs[0].ptr = gravity;
    ptcl_edit_black_hall_vardefs[0].ptr = black_hole;
    ptcl_edit_black_hall_vardefs[1].ptr = WF(168);
    ptcl_edit_black_hall_vardefs[2].ptr = WF(172);
    ptcl_edit_black_hall_vardefs[3].ptr = WF(176);
    ptcl_edit_black_hall_vardefs[4].ptr = WF(180);
    ptcl_edit_texture_vardefs[0].ptr = WF(184);
    ptcl_edit_texture_vardefs[1].ptr = WF(188);
    ptcl_edit_texture_vardefs[2].ptr = WF(192);
    ptcl_edit_texture_vardefs[3].ptr = WF(196);
    ptcl_edit_texture_vardefs[4].ptr = WF(200);
    ptcl_edit_texture_vardefs[5].ptr = WF(212);
    sub = gw_varedit_find_group(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, str_ptcl_edit_ptcl); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), sub, ptcl_edit_ptcl_vardefs); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), sub, ptcl_edit_blend_vardefs); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), gw_varedit_find_group(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, str_ptcl_edit_shooter), ptcl_edit_shooter_vardefs); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), gw_varedit_find_group(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, str_ptcl_edit_gravity), ptcl_edit_gravity_vardefs); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), gw_varedit_find_group(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, str_ptcl_edit_black_hall), ptcl_edit_black_hall_vardefs); /* steer(cast) */
    gw_varedit_add_table(((struct GwVarNode * *)&ptcl_edit_vars_root), gw_varedit_find_group(((struct GwVarNode * *)&ptcl_edit_vars_root), 0, str_ptcl_edit_texture), ptcl_edit_texture_vardefs); /* steer(cast) */
    win = gw_varedit_open(((char *)&ptcl_edit_vars_root), str_ptcl_edit_window); /* steer(cast) */
    w->win = win;
    gw_varedit_set_close_callback(win, (void *)ptcl_edit_close_callback);
    gw_window_show(w->win);
    gw_window_get_size(w->win, &lx, &ly);
    win = gw_window_open(str_ptcl_edit_texture_window, 7, 0, ly, 64, 64, 0);
    *(int *)((char *)w + 220) = win;
    gw_window_set_draw_handler(win, (void *)ptcl_edit_draw_gradient_quad);
    gw_window_show(*(struct GwWin **)((char *)w + 220));
}

/* 0x0015ae38 */
static void ptcl_edit_release(PtclEdit *w)
{
    PtclEnt *node;
    PtclEnt *next;

    if (ptcl_edit_work) {
        ptcl_edit_work = 0;
        gw_window_close(w->win);
    }
    gw_window_close(((int *)w)[55]);
    menu_item_set_value(0, str_ptcl_edit_menu, 0);
    if (w->head) {
        do {
            node = w->head;
            next = node->next;
            ptcl_ent_free_sub(node);
            do { yuki_free(w->head); } while (0);
            w->head = next;
        } while (w->head);
    }
}

/* Space preview, L loop, S stop. */
/* 0x0015aed0 */
static void ptcl_edit_exec(void)
{
    unsigned char *kbd = kbd_state;

    if (kbd[5] == 44)
        ptcl_edit_preview_cur();
    if (kbd[5] == 15)
        ptcl_edit_preview_cur_loop();
    if (kbd[5] == 22)
        ptcl_edit_stop_child();
}

/*
 * While nothing is previewed: emitter shape at its placement (line, disc or box of shooter size);
 * always an estimate of the entry's particle memory.
 */
/* steer(cast) */
/* 0x0015af40 */
static void ptcl_edit_disp(PtclEdit *w)
{
    PtclEntData *ed;
    float start[3];
    float end[3];
    float len;
    float x;
    float lim;
    int cnt;
    int interval;

    if (w->busy == 0) {
        ed = &w->ed;
        ulMatrixPush();
        do {
            float *pv = w->ed.eye;

            vu0_matrix_translate(ulMatrixCur, pv[0], pv[1], pv[2]);
        } while (0);
        ptcl_edit_rotate_yxz(w->ed.rot);
        switch ((unsigned)ed->shape) {
        case 0:
            len = ((float *)ed)[21];
            end[0] = len;
            start[1] = 0;
            start[2] = 0;
            start[0] = -len;
            end[1] = 0;
            end[2] = 0;
            lockon_draw_line_3d(start, end, RGBA(0xc0, 0xc0, 0xc0, 0x80), RGBA(0xc0, 0xc0, 0xc0, 0x80));
            start[0] = 0;
            start[1] = 0;
            start[2] = 0;
            end[2] = ((float *)ed)[21];
            end[0] = 0;
            end[1] = 0;
            lockon_draw_line_3d(start, end, RGBA(0xc0, 0xc0, 0xc0, 0x80), RGBA(0xc0, 0xc0, 0xc0, 0x80));
            break;
        case 1:
            start[0] = 0;
            start[1] = 0;
            start[2] = 0;
            end[2] = ((float *)ed)[21];
            end[0] = 0;
            end[1] = 0;
            lockon_draw_line_3d(start, end, RGBA(0xc0, 0xc0, 0xc0, 0x80), RGBA(0xc0, 0xc0, 0xc0, 0x80));
            vu0_matrix_rotate_x(ulMatrixCur, ANG_90);
            draw_ring_xz(((float *)ed)[21], 0x80c0c0c0, 16);
            break;
        case 2:
            debug_draw_box(RGBA(0xc0, 0xc0, 0xc0, 0x80), ((float *)ed)[21]);
            break;
        }
        ulMatrixPop();
    } else
        ed = &w->ed;
    {
        /* the entry's emitter words (PtclEmit): rate, interval, duration */
        int *em = (int *)((char *)w + 68);

        x = (float)w->ed.life[0] + fabsf((float)ed->life[1]);
        lim = (float)em[19];
        cnt = (int)x;
        if (lim < x)
            cnt = (int)lim;
        interval = em[18];
        cnt = (cnt + interval) / (interval + 1) * em[16];
    }
    debug_set_origin(0, -1);
    debug_printf("Memory Use : %d", cnt << 5);
}

/* 0x0015b140 */
static int ptcl_edit_create(void *parent)
{
    return task_create(parent, &ptcl_edit_desc, 0xe0);
}

/* 0x0015b168 */
static void ptcl_edit_menu_toggle(int item, int value, void *parent)
{
    PtclEdit *w = ptcl_edit_work;

    if (w) {
        TASK_END(w);
    } else {
        ptcl_edit_work = ptcl_edit_create(parent);
        if (ptcl_edit_work)
            menu_item_set_value(0, str_ptcl_edit_menu, 1);
    }
}

/* The editor opens under parent. */
/* 0x0015b1d8 */
void ptcl_edit_menu_register(void *parent)
{
    menu_item_add(0, str_ptcl_edit_menu, ptcl_edit_menu_toggle, parent, 0);
}

/* 0x0015b1f8 */
static void ptcl_edit_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
