/*
 * Pass editor: debug tool editing a copy of the stage's camera passes: pass list, part list, roll
 * and camera/look-at curve editors, preview drawing, simulation, save to a PC file.
 */

#include "system.h"
#include "types_game.h"
#include "color.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_window.h"
#include "nj_system.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "passedit.h"
#include "bspline.h"
#include "debug.h"
#include "debugdraw.h"
#include "debugmenu.h"
#include "game.h"
#include "pad.h"
#include "pass.h"
#include "pcfile.h"
#include "stage.h"
#include "task.h"
#include "yuki_tool.h"
#include "game_flow_editor.h"

#define FREE(p) \
    do { \
        yuki_free(p); \
        p = 0; \
    } while (0)

/* Always 0 (bit 0 shifted out): guard for tests that never pass. */
#define HELP_ZERO(p) ((((int)(p)) & 1) >> 1)

extern void *pass_edit_task;
extern char str_pass_edit_menu[];
extern char str_pass_edit[];
extern char str_pass_edit_stg_pas[];
extern char str_pass_edit_u_update[];
extern char str_pass_edit_d_downdate[];
extern char str_pass_edit_s_save_pc_f[];
extern char str_pass_edit_o_pass_draw[];
extern char str_pass_edit_g_simulatio[];
extern char str_pass_edit_arrow_up_down_cursor[];
extern char str_pass_edit_arrow_left_right_change[];
extern ScreenView screen_layout;
extern char str_pass_edit_char[];
extern char str_pass_edit_enter_exit[];
extern char str_pass_edit_arrow_left_right_cursor[];
extern char str_pass_edit_symboledit[];
extern unsigned char kbd_state[];
extern char str_pass_edit_gamecount[];
extern char str_pass_edit_angz_nothing[];
extern char str_pass_edit_angz_absolute[];
extern char str_pass_edit_angz_relative_for[];
extern char str_pass_edit_no_count_angz[];
extern char str_pass_edit_fmt[];
extern char str_pass_edit_fmt_2[];
extern char str_pass_edit_passeditor_pass_p[];
extern char str_pass_edit_insert_insert_ne[];
extern char str_pass_edit_delete_delete_ke[];
extern char str_pass_edit_n_set_angz[];
extern char str_pass_edit_a_set_angz[];
extern char str_pass_edit_r_set_angz[];
extern char str_pass_edit_c_arrow_left_right[];
extern char str_pass_edit_z_arrow_left_right[];
extern char str_pass_edit_p_point_dra[];
extern char str_pass_edit_enter_exit_2[];
extern char str_pass_edit_passeditor_pass[];
extern char str_pass_edit_nothing[];
extern char str_pass_edit_absolute[];
extern char str_pass_edit_relative_for[];
extern char str_pass_edit_cam[];
extern char str_pass_edit_int[];
extern char str_pass_edit_angx[];
extern char str_pass_edit_angy[];
extern char str_pass_edit_angz[];
extern char str_pass_edit_liner[];
extern char str_pass_edit_bezier[];
extern char str_pass_edit_bspline[];
extern char str_pass_edit_cardinal_tension[];
extern char str_pass_edit_disang_base[];
extern char str_pass_edit_no_x_y_z[];
extern char str_pass_edit_no_distance_degx_degy[];
extern char str_pass_edit_fmt_3[];
extern char str_pass_edit_fmt_4[];
extern char str_pass_edit_fmt_5[];
extern void ulMatrixPush(void);
extern char str_pass_edit_arrow_left_right_x[];
extern char str_pass_edit_page_up_down_y[];
extern char str_pass_edit_arrow_up_down_z[];
extern char str_pass_edit_space_exit[];
extern char str_pass_edit_passeditor_pass_2[];
extern char str_pass_edit_insert_insert_ne_2[];
extern char str_pass_edit_delete_delete_ct[];
extern char str_pass_edit_n_set_data[];
extern char str_pass_edit_a_set_data[];
extern char str_pass_edit_r_set_data[];
extern char str_pass_edit_n1_set_data[];
extern char str_pass_edit_n2_3_4_set[];
extern char str_pass_edit_c_change_sp[];
extern char str_pass_edit_j_joint_to[];
extern char str_pass_edit_space_edit_ctrl[];
extern char str_pass_edit_e_arrow_left_right[];
extern char str_pass_edit_t_arrow_left_right[];
extern char str_pass_edit_b_edit_base[];
extern char str_pass_edit_passeditor_pass_3[];
extern char str_pass_edit_passeditor_pass_4[];
extern char str_pass_edit_no_cam_int_angz[];
extern char str_pass_edit_n4[];
extern char str_pass_edit_passeditor_pass_5[];
extern char str_pass_edit_no_symbolname_partsnum_startcou[];
extern char str_pass_edit_fmt_6[];
extern unsigned int game_flags;
extern char str_pass_edit_insert_insert_new_pa[];
extern char str_pass_edit_delete_delete_pass[];
extern char str_pass_edit_e_edit_pass[];
extern char str_pass_edit_n_edit_symbol_n[];
extern char str_pass_edit_f_increment_sta[];
extern char str_pass_edit_l_decrement_sta[];
extern int game_work;
extern char *system1_work;
extern struct TaskDesc pass_edit_desc;
extern struct TaskDesc pass_sim_desc;

static void pass_edit_run_key(PassEditor *ed);
static void pass_edit_run_help(PassEditor *ed);
static void pass_edit_spline_key(PassEditor *ed);
static void pass_edit_spline_help(PassEditor *ed);
static void pass_edit_part_list_draw_rows(int win);
static void pass_edit_part_list_key(PassEditor *ed);
static void pass_edit_part_list_draw(PassEditor *ed);
static void pass_edit_list_draw(int win);
static void pass_edit_task_disp(PassEditWork *ed);
static void pass_edit_task_exec(PassEditWork *ed);
static char *pass_edit_task_create(void);
static PassSimWork *pass_sim_create(char *parent, int start, int end);

/* 0x001479b8 */
static void pass_edit_menu_toggle(void)
{
    if (pass_edit_task) {
        TASK_END(pass_edit_task);
    } else {
        pass_edit_task = pass_edit_task_create();
        if (pass_edit_task)
            menu_item_set_value(str_pass_edit_menu, str_pass_edit, 1);
    }
}

/* 0x00147a30 */
static void pass_edit_menu_init(void)
{
    menu_item_add(str_pass_edit_menu, str_pass_edit, pass_edit_menu_toggle, 0, 0);
}

/* Whether any part of the n passes is relative to pass idx. */
/* 0x00147a58 */
static int pass_symbol_used(PassEnt *tbl, int n, int idx)
{
    int i = 0;
    int j;
    int sym = tbl[idx].sym;
    PassPart *p;

    if (n > 0) {
        do {
            j = 0;
            p = tbl->parts;
            if (tbl->nparts > 0) {
                do {
                    if ((p->cam.flags & 3) == 0 && p->cam.ref == sym)
                        return 1;
                    if ((p->in.flags & 3) == 0 && p->in.ref == sym)
                        return 1;
                    j++;
                    if ((p->run.flags & 0xc000) == 0 && p->run.ref == sym)
                        return 1;
                    p++;
                } while (j < tbl->nparts);
            }
            i++;
            tbl++;
        } while (i < n);
    }
    return 0;
}

/*
 * Own buffers for key times and points (three per key for Bezier); B-spline knots rebuilt. Unused
 * curve copied as is.
 */
/* 0x00147b08 */
static void pass_spline_clone(PassSpline *dst, PassSpline *src)
{
    unsigned np;

    nj_copy8(dst, src, 44);
    if ((dst->flags & 1) == 0) {
        dst->rate = yuki_alloc(dst->nkey << 2);
        np = dst->nkey;
        if (dst->type == 1)
            np = np * 3;
        dst->pts = yuki_alloc(np * 12);
        nj_copy8(dst->rate, src->rate, dst->nkey << 2);
        nj_copy8(dst->pts, src->pts, np * 12);
        if (dst->type == 2)
            bspline_make_knots(dst->nkey, 3, &dst->knots);
    }
}

/* Angles unless no roll (flag 0x4000). */
/* 0x00147be0 */
static void pass_run_clone(PassRun *dst, PassRun *src)
{
    nj_copy8(dst, src, 20);
    dst->key_count = yuki_alloc(dst->nkey << 2);
    nj_copy8(dst->key_count, src->key_count, dst->nkey << 2);
    if ((dst->flags & 0x4000) == 0) {
        dst->key_angz = yuki_alloc(dst->nkey << 2);
        nj_copy8(dst->key_angz, src->key_angz, dst->nkey << 2);
    }
}

/* 0x00147c78 */
static void pass_clone(PassEnt *dst, PassEnt *src)
{
    int i;
    PassPart *src_part;
    PassPart *dst_part;

    src_part = src->parts;
    nj_copy8(dst, src, 20);
    dst->parts = yuki_alloc(dst->nparts * 108);
    dst_part = dst->parts;
    for (i = 0; i < dst->nparts; i++, dst_part++, src_part++) {
        nj_copy8(dst_part, src_part, 108);
        pass_spline_clone(&dst_part->cam, &src_part->cam);
        pass_spline_clone(&dst_part->in, &src_part->in);
        pass_run_clone(&dst_part->run, &src_part->run);
    }
}

/* 0x00147d30 */
static PassEnt *pass_table_clone(PassEnt *src, int n)
{
    PassEnt *tbl;
    PassEnt *dst;

    tbl = yuki_alloc(n * 20);
    dst = tbl;
    if (n > 0) {
        do {
            pass_clone(dst, src);
            n--;
            src++;
            dst++;
        } while (n != 0);
    }
    return tbl;
}

/* Nothing for an unused curve. */
/* 0x00147da8 */
static void pass_spline_free(PassSpline *spline)
{
    if ((spline->flags & 1) == 0) {
        if (spline->type == 2)
            FREE(spline->knots);
        FREE(spline->rate);
        FREE(spline->pts);
    }
}

/* 0x00147e18 */
static void pass_run_free(PassRun *run)
{
    FREE(run->key_count);
    if ((run->flags & 0x4000) == 0)
        FREE(run->key_angz);
}

/* 0x00147e68 */
static void pass_free(PassEnt *pass)
{
    int i = 0;
    PassPart *p = pass->parts;

    if (pass->nparts > 0) {
        do {
            i++;
            pass_spline_free(&p->cam);
            pass_spline_free(&p->in);
            pass_run_free(&p->run);
            p++;
        } while (i < pass->nparts);
    }
    yuki_free(pass->parts);
    pass->parts = 0;
}

/* 0x00147ee0 */
static void pass_table_free(PassEnt *p, int n)
{
    PassEnt *base = p;

    if (n > 0) {
        int k = n;

        do {
            pass_free(p);
            k--;
            p++;
        } while (k != 0);
    }
    yuki_free(base);
    base = 0;
}

/* Clamps passIdx/partIdx, then sets pass, part, spline (camera or look-at by sel) and run. */
/* 0x00147f40 */
static void pass_edit_clamp_selection(PassEditor *ed)
{
    int pi = ed->passIdx;
    int pj;
    int ti;
    int tj;
    PassEnt *pass;
    PassPart *part;

    if (pi < 0)
        pi = 0;
    pj = ed->npass - 1;
    ti = ed->partIdx;
    if (pi < pj)
        pj = pi;
    ed->passIdx = pj;
    if (ti < 0)
        ti = 0;
    pass = &ed->tbl[pj];
    tj = pass->nparts - 1;
    part = pass->parts;
    ed->pass = pass;
    if (ti < tj)
        tj = ti;
    ed->partIdx = tj;
    ed->part = part + tj;
    if (ed->sel == 0)
        ed->spline = &ed->part->in;
    else
        ed->spline = &ed->part->cam;
    ed->run = &ed->part->run;
}

/* "Update": edited passes become the game's table; editing continues on a fresh copy. */
/* 0x00147fd0 */
static void pass_edit_apply(PassEditor *ed)
{
    void *tbl;

    tbl = pass_table_get();
    pass_table_free(tbl, pass_table_count());
    pass_table_set(ed->tbl, ed->npass);
    tbl = pass_table_get();
    ed->tbl = pass_table_clone(tbl, pass_table_count());
    pass_edit_clamp_selection(ed);
}

/* "Downdate": discards edits, recopies the game's pass table. */
/* 0x00148040 */
static void pass_edit_reload_table(PassEditor *ed)
{
    void *src;

    pass_table_free(ed->tbl, ed->npass);
    src = pass_table_get();
    ed->tbl = pass_table_clone(src, pass_table_count());
    pass_edit_clamp_selection(ed);
}

/* In bytes, as pass_spline_write writes it. */
/* 0x00148098 */
static int pass_spline_size(PassSpline *spline)
{
    int size = 4;
    int i;
    int n;

    if (spline->flags & 1)
        return 4;
    if ((spline->flags & 2) == 0)
        size = 8;
    size += 8;
    for (i = 0; i < spline->nkey; i++)
        size += 4;
    n = spline->nkey;
    if (spline->type == 1)
        n = spline->nkey * 3;
    for (i = 0; i < n; i++)
        size += 12;
    if (spline->type == 3)
        size += 4;
    else if (spline->type == 4) {
        size += 4;
        size += 8;
    }
    return size;
}

/* In bytes, as pass_run_write writes it. */
/* 0x00148160 */
static int pass_run_size(PassRun *run)
{
    int size = 4;
    int i;

    if ((run->flags & 0xc000) == 0)
        size = 8;
    size += 4;
    for (i = 0; i < run->nkey; i++)
        size += 4;
    if ((run->flags & 0x4000) == 0) {
        for (i = 0; i < run->nkey; i++)
            size += 4;
    }
    return size;
}

/* In bytes, as pass_table_write writes them. */
/* 0x001481e0 */
static int pass_table_size(PassEnt *tbl, int n)
{
    int i;
    int size = 4;
    int j;
    PassPart *p;

    for (i = 0; i < n; i++, tbl++) {
        size += 12;
        p = tbl->parts;
        for (j = 0; j < tbl->nparts; j++, p++) {
            size += pass_spline_size(&p->cam);
            size += pass_spline_size(&p->in);
            size += pass_run_size(&p->run);
        }
    }
    return size;
}

/* Layout curve_track_load reads. Returns the next write position. */
/* 0x001482c0 */
static float *pass_spline_write(float *out, PassSpline *spline)
{
    int i;
    int npts;
    float *rate;
    PassVec3 *p;

    *(unsigned *)out = spline->flags;
    out++;
    if (spline->flags & 1)
        return out;
    if ((spline->flags & 2) == 0) {
        *(int *)out = spline->ref;
        out++;
    }
    rate = spline->rate;
    *(int *)out = spline->type;
    out++;
    *(int *)out = spline->nkey;
    out++;
    for (i = 0; i < spline->nkey; i++)
        *out++ = *rate++;
    npts = spline->type == 1 ? spline->nkey * 3 : spline->nkey;
    for (i = 0, p = spline->pts; i < npts; i++, p++) {
        *out++ = p->x;
        *out++ = p->y;
        *out++ = p->z;
    }
    if (spline->type == 3) {
        *out++ = spline->tension;
    } else if (spline->type == 4) {
        float *base = &spline->base.x;

        *out++ = base[0];
        *out++ = base[1];
        *out++ = base[2];
    }
    return out;
}

/* Layout keyframe_track_load reads. Returns the next write position. */
/* 0x00148400 */
static int *pass_run_write(int *out, PassRun *run)
{
    int i;
    int *src;

    *out++ = run->flags;
    if ((run->flags & 0xc000) == 0)
        *out++ = run->ref;
    *out++ = run->nkey;
    src = run->key_count;
    for (i = 0; i < run->nkey; i++, src++)
        *out++ = *src;
    if ((run->flags & 0x4000) == 0) {
        src = run->key_angz;
        for (i = 0; i < run->nkey; i++, src++)
            *out++ = *src;
    }
    return out;
}

/* Returns the next write position. */
/* 0x001484a8 */
static int *pass_table_write(int *out, PassEnt *tbl, int n)
{
    int i = 0;
    int j;
    PassPart *p;

    *out = n;
    out++;
    if (n > 0) {
        do {
            p = tbl->parts;
            *out++ = tbl->sym;
            *out++ = tbl->start;
            *out++ = tbl->nparts;
            for (j = 0; j < tbl->nparts; j++, p++) {
                out = pass_spline_write(out, &p->cam);
                out = pass_spline_write(out, &p->in);
                out = pass_run_write(out, &p->run);
            }
            i++;
            tbl++;
        } while (i < n);
    }
    return out;
}

/* "Save PC File": stg<name>.pas on the host PC. */
/* 0x001485a0 */
static void pass_edit_save_pc_file(void *tbl, int n)
{
    char path[256];
    int size;
    void *buf;

    size = pass_table_size(tbl, n);
    buf = yuki_alloc(size);
    pass_table_write(buf, tbl, n);
    sprintf(path, str_pass_edit_stg_pas, stage_get_name());
    pcfile_save_set(path, buf, size);
    yuki_free(buf);
}

/*
 * Shared keys: U update, D downdate, S save, O cycle path drawing, G simulate, right/left move game
 * time by step frames.
 */
/* 0x00148638 */
static void pass_edit_list_key(int key, int step, PassEditor *ed)
{
    PassEnt *pass;

    switch (key) {
    case 24:
        pass_edit_apply(ed);
        break;
    case 7:
        pass_edit_reload_table(ed);
        break;
    case 22:
        pass_edit_save_pc_file(ed->tbl, ed->npass);
        break;
    case 18:
        ed->drawmode = (ed->drawmode + 1) % 3;
        break;
    case 80:
    {
        int cnt = ed->gamecnt - step;

        if (cnt < 0)
            cnt = 0;
        ed->gamecnt = cnt;
        game_time_set((float)cnt);
        break;
    }
    case 79:
    {
        int cnt = ed->gamecnt + step;

        ed->gamecnt = cnt;
        game_time_set((float)cnt);
        break;
    }
    case 10:
        pass = &ed->tbl[ed->passIdx];
        pass_sim_create(ed, pass->start, pass->start + pass->len);
        break;
    }
}

/* 0x00148758 */
static void pass_edit_help_text(void)
{
    debug_puts(str_pass_edit_u_update);
    debug_newline();
    debug_puts(str_pass_edit_d_downdate);
    debug_newline();
    debug_puts(str_pass_edit_s_save_pc_f);
    debug_newline();
    debug_puts(str_pass_edit_o_pass_draw);
    debug_newline();
    debug_puts(str_pass_edit_g_simulatio);
    debug_newline();
    debug_puts(str_pass_edit_arrow_up_down_cursor);
    debug_newline();
    debug_puts(str_pass_edit_arrow_left_right_change);
    debug_newline();
}

/* Top left, cols wide, 15 lines; id -> *out. */
/* steer(cast) */
/* 0x001487f0 */
static void pass_edit_make_list_window(PassEditor *ed, int *out, char *title, int cols, PassEditCb draw_cb, PassEditCb close_cb)
{
    *out = ((int (*)())gw_window_open)(title, 95, (int)(screen_layout.left + 4.0f), (int)(screen_layout.top + 23.0f),
                        ed->cellw * cols, ed->cellh * 15);
    gw_window_set_color(*out, ARGB(0x80, 0, 0, 0));
    gw_window_set_handlers(*out, 0, draw_cb, close_cb);
    gw_window_set_slot(*out, 0, ed);
}

/* Four symbol characters, cursor one red. */
/* 0x001488c0 */
static void pass_edit_symbol_window_draw(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    int i;
    int x;
    int y;
    char *str;

    x = ed->cellw;
    y = ed->cellh;
    str = ed->symtxt;
    for (i = 0; i < 4; i++) {
        if (i == ed->symcur)
            gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0x80));
        else
            gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
        gw_win_printf(win, x, y, str_pass_edit_char, *str);
        str++;
        x += ed->cellw;
    }
}

/* Restores the editor's key and help handlers and reactivates its window. */
/* 0x00148990 */
static void pass_edit_symbol_window_activate(int win)
{
    int *p = gw_window_get_slot(win, 0);

    TASK_OF(p)->exec = p[9];
    TASK_OF(p)->disp = p[10];
    gw_window_activate(p[12]);
}

/*
 * Left/right move the cursor, letters/digits typed, Enter stores and closes. Keeps the window
 * active against the editor window.
 */
/* 0x001489c0 */
static void pass_edit_symbol_window_key(PassEditor *ed)
{
    int k;

    if (gw_window_is_active(ed->symwin)) {
        k = kbd_key2;
        switch (k) {
        case 80:
        {
            int cur = ed->symcur;

            cur--;
            if (cur < 0)
                cur = 0;
            ed->symcur = cur;
            break;
        }
        case 79:
        {
            int cur = ed->symcur;

            cur++;
            ed->symcur = cur < 4 ? cur : 3;
            break;
        }
        case 40:
            gw_window_close(ed->symwin);
            *ed->symptr = *(int *)ed->symtxt;
            break;
        default:
            if ((unsigned)(k - 4) < 26u) {
                ed->symtxt[ed->symcur] = k + 61;
                {
                    int sym_cur = ed->symcur;

                    sym_cur++;
                    ed->symcur = sym_cur < 4 ? sym_cur : 3;
                }
            } else if ((unsigned)(k - 30) < 9u) {
                ed->symtxt[ed->symcur] = k + 19;
                {
                    int sym_cur = ed->symcur;

                    sym_cur++;
                    ed->symcur = sym_cur < 4 ? sym_cur : 3;
                }
            } else if (k == 39) {
                ed->symtxt[ed->symcur] = '0';
                {
                    int sym_cur = ed->symcur;

                    sym_cur++;
                    ed->symcur = sym_cur < 4 ? sym_cur : 3;
                }
            }
            break;
        }
    } else if (gw_window_is_active(ed->win)) {
        gw_window_activate(ed->symwin);
    }
}

/* 0x00148af8 */
static void pass_edit_help_footer(void)
{
    debug_set_origin(0, -2);
    debug_puts(str_pass_edit_enter_exit);
    debug_newline();
    debug_puts(str_pass_edit_arrow_left_right_cursor);
    debug_newline();
}

/* Edits four-character *sym; key_cb/help_cb restored on close. */
/* 0x00148b38 */
static void pass_edit_symbol_window_open(PassEditor *ed, int *sym, PassEditCb key_cb, PassEditCb help_cb, int parent)
{
    float x = screen_layout.left + 4.0f;
    float y = screen_layout.top + 23.0f;

    ed->symcur = 0;
    ed->symcb1 = key_cb;
    ed->symcb2 = help_cb;
    *(int *)ed->symtxt = *sym;
    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_symbol_window_key;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_help_footer;
    ed->symptr = sym;
    ed->symwin = gw_window_open(str_pass_edit_symboledit, 95, (int)(x + (float)(ed->cellw * 35)), (int)y,
                              ed->cellw * 6, ed->cellh * 3, parent);
    gw_window_set_color(ed->symwin, ARGB(0x80, 0, 0, 0xff));
    gw_window_set_handlers(ed->symwin, 0, pass_edit_symbol_window_draw, pass_edit_symbol_window_activate);
    gw_window_set_slot(ed->symwin, 0, ed);
}

/* Insert: duplicates the cursor key (ed->sub). */
/* 0x00148c48 */
static void pass_edit_run_insert_key(PassEditor *ed)
{
    int *nc = 0;
    int k = 0;
    int *na;
    int j;

    na = yuki_alloc(++ed->run->nkey * 4);
    if ((ed->run->flags & 0x4000) == 0)
        nc = yuki_alloc(ed->run->nkey << 2);
    for (j = 0; j < ed->run->nkey; j++) {
        na[j] = ed->run->key_count[k];
        if ((ed->run->flags & 0x4000) == 0)
            nc[j] = ed->run->key_angz[k];
        if (j != ed->sub)
            k++;
    }
    pass_run_free(ed->run);
    ed->run->key_count = na;
    ed->run->key_angz = nc;
}

/*
 * Delete: while more than two remain. Removing the last key shortens the pass; first key time
 * reset to 0.
 */
/* 0x00148d58 */
static void pass_edit_run_delete_key(PassEditor *ed)
{
    int *nc = 0;
    int k = 0;
    int n = ed->run->nkey;

    if (n > 2) {
        int new_n = n - 1;
        int j;
        int *na;
        int last;
        PassRun *cur_run;

        ed->run->nkey = new_n;
        na = yuki_alloc(new_n * 4);
        cur_run = ed->run;
        if ((cur_run->flags & 0x4000) == 0) {
            nc = yuki_alloc(cur_run->nkey << 2);
            cur_run = ed->run;
        }
        last = cur_run->key_count[cur_run->nkey - 1];
        for (j = 0; j < n; j++) {
            na[k] = cur_run->key_count[j];
            if ((ed->run->flags & 0x4000) == 0)
                nc[k] = cur_run->key_angz[j];
            if (j != ed->sub)
                k++;
        }
        pass_run_free(ed->run);
        {
            PassRun *run = ed->run;
            int sub, lim;

            run->key_angz = nc;
            run->key_count = na;
            if (ed->sub == new_n)
                ed->pass->len -= last - na[run->nkey - 1];
            sub = ed->sub;
            lim = run->nkey - 1;
            if (lim < sub)
                sub = lim;
            ed->sub = sub;
            run->key_count[0] = 0;
        }
    }
}

/* "N": no roll (angles freed). */
/* 0x00148ed8 */
static void pass_edit_run_reset(PassEditor *ed)
{
    if ((ed->run->flags & 0x4000) == 0) {
        FREE(ed->run->key_angz);
        ed->run->flags = 0x4000;
    }
}

/*
 * "A"/"R": absolute (0x8000) or relative (0x10000) roll; no roll -> zero angles. Relative asks for
 * the symbol.
 */
/* 0x00148f28 */
static void pass_edit_run_set_flag(PassEditor *ed, unsigned mask)
{
    PassRun *run = ed->run;
    int *p;
    int i;

    if ((run->flags & mask) == 0) {
        if ((run->flags & 0x4000) == 0) {
            run->flags = mask;
        } else {
            p = yuki_alloc(run->nkey << 2);
            run = ed->run;
            i = 0;
            run->key_angz = p;
            if (run->nkey > 0) {
                do {
                    *p = 0;
                    i++;
                    p++;
                } while (i < ed->run->nkey);
            }
            run->flags = mask;
        }
    }
    if (mask != 0x10000)
        return;
    pass_edit_symbol_window_open(ed, &run->flags + 1, pass_edit_run_key, pass_edit_run_help, ed->win);
}

/*
 * C + left/right: cursor key time by 1 frame (100 with Shift), between neighbours; last key moves
 * the pass length.
 */
/* 0x00148ff0 */
static void pass_edit_run_move_key(PassEditor *ed, int key)
{
    int last = 0;
    int *p = &ed->run->key_count[ed->sub];
    int prev, next, step;

    if (ed->sub > 0) {
        prev = p[-1];
        if (ed->sub == ed->run->nkey - 1) {
            next = -1;
            last = p[0];
        } else
            next = p[1];
        step = 1;
        if (kbd_state[7] & 0x22)
            step = 100;
        if (key == 80) {
            *p = (*p - step < prev) ? prev : *p - step;
            if (next < 0)
                ed->pass->len -= last - *p;
        } else if (key == 79) {
            *p = (next >= 0) ? ((*p + step < next) ? *p + step : next) : *p + step;
            if (next < 0)
                ed->pass->len += *p - last;
        }
    }
}

/* Z + left/right: cursor key roll by 0.1 degree (10 with Shift). */
/* 0x001490d8 */
static void pass_edit_run_turn_key(PassEditor *ed, int key)
{
    int *p = &ed->run->key_angz[ed->sub];
    int step;

    if ((ed->run->flags & 0x4000) == 0) {
        step = 18;
        if (kbd_state[7] & 0x22)
            step = 1820;
        if (key == 80)
            *p -= step;
        else if (key == 79)
            *p += step;
    }
}

/*
 * Game time, roll mode, one row per key (number, time, roll in degrees); cursor key green, kept in
 * view.
 */
/* 0x00149150 */
static void pass_edit_run_draw(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    int y = ed->cellh;
    int x = ed->cellw;
    int hw[2];
    PassRun *run;
    int rows;
    int top;
    int i;
    unsigned flags;

    gw_window_get_size(win, &hw[0], &hw[1]);
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    gw_win_printf(win, x, y, str_pass_edit_gamecount, (int)game_time_get());
    run = ed->run;
    flags = run->flags;
    y += ed->cellh;
    if (flags & 0x4000) {
        gw_win_printf(win, x, y, str_pass_edit_angz_nothing);
    } else if (flags & 0x8000) {
        gw_win_printf(win, x, y, str_pass_edit_angz_absolute);
    } else if (flags & 0x10000) {
        char *sym = (char *)&run->ref;

        gw_win_printf(win, x, y, str_pass_edit_angz_relative_for, sym[0], sym[1], sym[2], sym[3]);
    }
    y += ed->cellh;
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    rows = hw[1] / ed->cellh - 4;
    top = ed->cur;
    if (ed->sub < top) {
        ed->cur = ed->sub;
        top = ed->sub;
    }
    if (ed->sub >= top + rows) {
        int row_top = ed->sub - rows + 1;

        if (row_top < 0)
            row_top = 0;
        ed->cur = row_top;
    }
    gw_win_printf(win, x, y, str_pass_edit_no_count_angz);
    i = ed->cur;
    y += ed->cellh;
    if (i < i + rows) {
        do {
            PassRun *run;

            y += ed->cellh;
            if (i == ed->sub)
                gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0));
            else
                gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
            run = ed->run;
            if (run->flags & 0x4000) {
                gw_win_printf(win, x, y, str_pass_edit_fmt, i, run->key_count[i]);
            } else {
                gw_win_printf(win, x, y, str_pass_edit_fmt_2, i, run->key_count[i],
                             (float)run->key_angz[i] * DEG_PER_ANG);
            }
            if (i == ed->run->nkey - 1)
                break;
            i++;
        } while (i < ed->cur + rows);
    }
}

/* Editor window -> part list of the current pass. */
/* 0x00149448 */
static void pass_edit_part_window_init(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    char *sym = (char *)ed->pass;

    ((int (*)(char *, char *, ...))sprintf)(ed->title, str_pass_edit_passeditor_pass_p, sym[0], sym[1], sym[2], sym[3]); /* steer(cast) */
    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_part_list_key;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_part_list_draw;
    gw_window_set_title(ed->win, ed->title);
    gw_window_set_draw_handler(ed->win, pass_edit_part_list_draw_rows);
}

/*
 * Up/down cursor (10 with Shift), Insert/Delete, N/A/R roll mode, P point preview, Enter back to
 * parts; C/Z + left/right edit the key, else shared keys.
 */
/* 0x001494d0 */
static void pass_edit_run_key(PassEditor *ed)
{
    int step;
    int key;

    if (gw_window_is_active(ed->win)) {
        step = 1;
        key = kbd_state[6];
        if (kbd_state[7] & 0x22)
            step = 10;
        switch (key) {
        case 82:
        {
            int sub = ed->sub - step;

            if (sub < 0)
                sub = 0;
            ed->sub = sub;
            break;
        }
        case 81:
        {
            int sub = ed->sub + step;
            int max_sub = ed->run->nkey - 1;

            if (max_sub < sub)
                sub = max_sub;
            ed->sub = sub;
            break;
        }
        case 73:
            pass_edit_run_insert_key(ed);
            break;
        case 76:
            pass_edit_run_delete_key(ed);
            break;
        case 17:
            pass_edit_run_reset(ed);
            break;
        case 4:
            pass_edit_run_set_flag(ed, 0x8000);
            break;
        case 21:
            pass_edit_run_set_flag(ed, 0x10000);
            break;
        case 19:
            ed->run_preview = (ed->run_preview + 1) % 3;
            break;
        case 40:
            pass_edit_part_window_init(ed->win);
            break;
        default:
            if (pad_keyboard_key_down(6))
                pass_edit_run_move_key(ed, key);
            else if (pad_keyboard_key_down(29))
                pass_edit_run_turn_key(ed, key);
            else
                pass_edit_list_key(key, step, ed);
            break;
        }
    }
}

/*
 * Point preview: camera at the cursor key's time; path drawing: current part's path (edited
 * passes). Then the key help.
 */
/* 0x001496e8 */
static void pass_edit_run_help(PassEditor *ed)
{
    int cnt;
    void *tbl;
    int total;
    int i;
    int k;
    PassPart *part;
    PassRun *run;
    int sum;

    if (gw_window_is_active(ed->win)) {
        if (ed->run_preview) {
            cnt = pass_table_count();
            tbl = pass_table_get();
            i = ed->partIdx;
            part = ed->pass->parts;
            total = ed->pass->start;
            for (k = 0; k < i; k++) {
                run = &part->run;
                sum = total + 1;
                part++;
                total = sum + run->key_count[run->nkey - 1];
            }
            if (HELP_ZERO(part))
                str_pass_edit_insert_insert_ne[0] = 0;
            if (HELP_ZERO(part))
                str_pass_edit_delete_delete_ke[0] = 0;
            if (HELP_ZERO(part))
                str_pass_edit_n_set_angz[0] = 0;
            if (HELP_ZERO(part))
                str_pass_edit_enter_exit_2[0] = 0;
            run = &part->run;
            pass_table_set(ed->tbl, ed->npass);
            if (ed->run_preview == 1) {
                if (HELP_ZERO(run))
                    str_pass_edit_enter_exit_2[0] = 0;
                pass_draw_path(ed->passIdx, (float)(total + run->key_count[ed->sub]), 1.0f, 1.0f);
            }
            else
                pass_draw_part_path(ed->passIdx, (float)(total + run->key_count[ed->sub]), 1.0f, 1.0f);
            pass_table_set(tbl, cnt);
        }
        if (ed->drawmode) {
            int path_total;
            PassRun *path_run;
            PassPart *path_part;
            int path_idx;
            int path_left;

            if (HELP_ZERO(ed))
                str_pass_edit_insert_insert_ne[0] = 0;
            if (HELP_ZERO(ed))
                str_pass_edit_enter_exit_2[0] = 0;
            cnt = pass_table_count();
            tbl = pass_table_get();
            path_idx = ed->partIdx;
            path_part = ed->pass->parts;
            path_total = ed->pass->start;
            if (path_idx > 0) {
                path_left = path_idx;
                do {
                    path_left--;
                    path_run = &path_part->run;
                    path_total += 1 + path_run->key_count[path_run->nkey - 1];
                    path_part++;
                } while (path_left != 0);
            }
            path_run = &path_part->run;
            pass_table_set(ed->tbl, ed->npass);
            if (ed->drawmode == 1)
                pass_draw_path(ed->passIdx, (float)path_total, pass_table_end(ed->passIdx) / 20.0f,
                               (float)path_run->key_count[path_run->nkey - 1]);
            else
                pass_draw_part_path(ed->passIdx, (float)path_total, pass_table_end(ed->passIdx) / 20.0f,
                                    (float)path_run->key_count[path_run->nkey - 1]);
            pass_table_set(tbl, cnt);
        }
        debug_set_origin(0, -16);
        debug_puts(str_pass_edit_insert_insert_ne);
        debug_newline();
        debug_puts(str_pass_edit_delete_delete_ke);
        debug_newline();
        debug_puts(str_pass_edit_n_set_angz);
        debug_newline();
        debug_puts(str_pass_edit_a_set_angz);
        debug_newline();
        debug_puts(str_pass_edit_r_set_angz);
        debug_newline();
        debug_puts(str_pass_edit_c_arrow_left_right);
        debug_newline();
        debug_puts(str_pass_edit_z_arrow_left_right);
        debug_newline();
        debug_puts(str_pass_edit_p_point_dra);
        debug_newline();
        debug_puts(str_pass_edit_enter_exit_2);
        debug_newline();
        pass_edit_help_text();
    }
}

/* 0x00149ab0 */
static void pass_edit_run_enter(PassEditor *ed)
{
    char *sym;
    PassPart *part;

    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_run_key;
    sym = (char *)ed->pass;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_run_help;
    ed->sub = 0;
    ed->cur = 0;
    part = ed->pass->parts + ed->partIdx;
    ed->part = part;
    ed->run = &part->run;
    sprintf(ed->title, str_pass_edit_passeditor_pass, ed->passIdx, sym[0], sym[1], sym[2], sym[3], ed->partIdx);
    gw_window_set_title(ed->win, ed->title);
    gw_window_set_draw_handler(ed->win, pass_edit_run_draw);
}

/*
 * Insert: duplicates the cursor key (ed->cpidx; Bezier: key points only, not handles); rebuilds
 * B-spline knots.
 */
/* 0x00149b60 */
static void pass_edit_spline_insert_key(PassEditor *ed)
{
    PassSpline *spline = ed->spline;
    char *src_pts;
    char *np;
    char *nr;
    char *src_rate;
    int stride = 1;
    int i;
    int j;
    char *dp;
    char *dr;

    src_pts = (char *)spline->pts;
    src_rate = (char *)spline->rate;

    if (spline->type == 1)
        stride = 3;
    if (ed->cpidx % stride == 0) {
        {
            int k = stride * 12;

            spline->nkey = spline->nkey + 1;
            do { } while (0);
            np = yuki_alloc(spline->nkey * k);
        }
        nr = yuki_alloc(spline->nkey << 2);
        i = 0;
        dr = nr;
        dp = np;
        if (spline->nkey > 0) {
            do {
                if (i / stride != ed->cpidx) {
                    j = 0;
                    if (stride != 0) {
                        do {
                            j++;
                            nj_copy8(dp, src_pts, 12);
                            src_pts += 12;
                        } while (j < stride);
                    }
                    {
                        char *rate = src_rate;

                        src_rate += 4;
                        nj_copy8(dr, rate, 4);
                    }
                } else {
                    j = 0;
                    if (stride != 0) {
                        do {
                            nj_copy8(dp, src_pts + j * 12, 12);
                            j++;
                        } while (j < stride);
                    }
                    nj_copy8(dr, src_rate, 4);
                }
                i++;
                dr += 4;
                dp += stride * 12;
            } while (i < spline->nkey);
        }
        pass_spline_free(spline);
        spline->rate = (float *)nr;
        spline->pts = (PassVec3 *)np;
        if (spline->type == 2)
            bspline_make_knots(spline->nkey, 3, &spline->knots);
        return;
    }
}

/*
 * Delete: keeps more than two keys (linear, Bezier, polar) or four (B-spline, cardinal). First key
 * time reset to 0, last to 1.
 */
/* 0x00149d80 */
static void pass_edit_spline_delete_key(PassEditor *ed)
{
    PassSpline *spline = ed->spline;
    char *src_pts = (char *)spline->pts;
    char *src_rate = (char *)spline->rate;
    int stride = 1;
    int n;
    int i;
    int j;
    char *np;
    char *nr;
    char *dp;
    float *dr;

    if (spline->type == 1)
        stride = 3;
    switch (spline->type) {
    case 1:
        if (ed->cpidx % 3 != 0)
            return;
    case 0:
    case 4:
        n = spline->nkey;
        if (n < 3)
            return;
        break;
    case 2:
    case 3:
        n = spline->nkey;
        if (n < 5)
            return;
        break;
    default:
        n = spline->nkey;
    }
    {
        int k = stride * 12;

        np = yuki_alloc((n - 1) * k);
    }
    nr = yuki_alloc((spline->nkey - 1) * 4);
    i = 0;
    n = spline->nkey;
    dp = np;
    dr = (float *)nr;
    if (n > 0) {
        do {
            if (i * stride != ed->cpidx) {
                j = 0;
                if (stride != 0) {
                    do {
                        nj_copy8(dp, src_pts + j * 12, 12);
                        j++;
                        dp += 12;
                    } while (j < stride);
                }
                nj_copy8(dr++, src_rate, 4);
                n = spline->nkey;
            }
            i++;
            src_rate += 4;
            src_pts += stride * 12;
        } while (i < n);
    }
    {
        int nn;

            pass_spline_free(spline);
            nn = spline->nkey - 1;

        *(float *)nr = 0;
        ((float *)nr)[nn - 1] = 1.0f;
        spline->pts = (PassVec3 *)np;
        spline->nkey = nn;
        spline->rate = (float *)nr;
        if (spline->type == 2)
            bspline_make_knots(nn, 3, &spline->knots);
    }
    {
        int max_cp = spline->nkey * stride - stride;
        int cp = ed->cpidx;

        if (max_cp < cp)
            cp = max_cp;
        ed->cpidx = cp;
    }
}

/* "N": channel unused, keys freed. */
/* 0x0014a008 */
static void pass_edit_spline_reset(PassEditor *ed)
{
    if ((ed->spline->flags & 1) == 0) {
        FREE(ed->spline->pts);
        FREE(ed->spline->rate);
        ed->spline->flags = 1;
    }
}

/*
 * "A"/"R": absolute (2) or relative; an unused channel first gets a two-key linear curve at the
 * origin. Relative asks for the symbol.
 */
/* 0x0014a068 */
static void pass_edit_spline_set_mode(PassEditor *ed, unsigned mask)
{
    PassSpline *spline = ed->spline;
    float *rate;
    float *pts;
    float *rate_p;

    if ((spline->flags & mask) == 0) {
        if ((spline->flags & 1) == 0) {
            spline->flags = mask;
        } else {
            spline->rate = yuki_alloc(8);
            pts = yuki_alloc(24);
            rate = spline->rate;
            rate[0] = 0;
            rate[1] = 1.0f;
            rate_p = pts + 3;
            rate_p[0] = 0;
            pts[0] = 0;
            pts[1] = 0;
            pts[2] = 0;
            spline->nkey = 2;
            spline->type = 0;
            rate_p[1] = 0;
            rate_p[2] = 0;
            spline->pts = (PassVec3 *)pts;
            spline->flags = mask;
        }
    }
    if (mask == 0xfffffffc)
        pass_edit_symbol_window_open(ed, &spline->ref, pass_edit_spline_key, pass_edit_spline_help, ed->win);
}

/* Relative curves only. */
/* 0x0014a150 */
static void pass_edit_toggle_flags(PassEditor *ed, unsigned mask)
{
    unsigned *flags = ed->spline;
    unsigned val = *flags;

    if (val & 0xfffffffc) {
        if (val & mask)
            *flags = val & ~mask;
        else
            *flags = val | mask;
    }
}

/*
 * E + left/right: cursor key time by 0.001 of the part (0.1 with Shift), between neighbours.
 * First/last keys and Bezier handles fixed.
 */
/* 0x0014a190 */
static void pass_edit_edit_rate(PassEditor *ed, int key)
{
    PassSpline *spline = ed->spline;
    float step;
    float *cur;
    float v;
    float hi;
    int idx = ed->cpidx;
    float lo;
    int stride;
    float *rate = spline->rate;

    if (spline->type != 1)
        stride = 1;
    else
        stride = 3;
    cur = rate + idx;
    if (idx % stride == 0 && idx > 0 && idx < spline->nkey * stride - stride) {
        lo = rate[idx - stride];
        step = 0.001f;
        hi = rate[idx + stride];
        if (kbd_mods & 0x22)
            step = 0.1f;
        if (key == 80) {
            v = *cur - step;
            hi = lo <= v ? v : lo;
            *cur = hi;
        } else if (key == 79) {
            if (0.0f <= hi) {
                v = *cur + step;
                lo = v < hi ? v : hi;
                *cur = lo;
            } else {
                *cur = *cur + step;
            }
        }
    }
}

/* Left/right: *val by step (300x with Shift). */
/* 0x0014a2b0 */
static void pass_edit_nudge_float(float step, float *val, int key)
{
    if (kbd_mods & 0x22)
        step = step * 300.0f;
    if (key == 80)
        *val = *val - step;
    else if (key == 79)
        *val = *val + step;
}

/*
 * "C": next kind (linear, Bezier, B-spline, cardinal, polar), four keys or more. Bezier handles
 * start on the key point; tension reset to 0.3, polar pivot to the origin.
 */
/* 0x0014a308 */
static void pass_edit_spline_next_type(PassEditor *ed)
{
    PassSpline *spline = ed->spline;

    if (spline->nkey >= 4) {
        int ss;
        int ds;
        int i;
        int type;
        PassVec3 *src;
        PassVec3 *dst;
        PassVec3 *new_pts;
        PassVec3 *base;

        ss = (spline->type == 1) ? 3 : 1;
        ds = ((spline->type + 1) % 5 == 1) ? 3 : 1;
        {
            int sz = ds * sizeof(PassVec3);

            new_pts = yuki_alloc(spline->nkey * sz);
        }
        src = spline->pts;
        dst = new_pts;
        for (i = 0; i < spline->nkey; i++) {
            dst->x = src->x;
            dst->y = src->y;
            dst->z = src->z;
            src += ss;
            dst += ds;
        }
        if (ds == 3 && spline->nkey > 0) {
            float *key_pt = &new_pts[2].x;

            i = spline->nkey;

            do {
                i--;
                key_pt[-3] = key_pt[-6];
                key_pt[-2] = key_pt[-5];
                key_pt[-1] = key_pt[-4];
                key_pt[0] = key_pt[-6];
                key_pt[1] = key_pt[-5];
                key_pt[2] = key_pt[-4];
                key_pt += 9;
            } while (i != 0);
        }
        do { yuki_free(spline->pts); } while (0);
        spline->pts = new_pts;
        if (spline->type == 2) {
            yuki_free(spline->knots);
            spline->knots = 0;
        }
        type = (spline->type + 1) % 5;
        spline->tension = 0.3f;
        spline->type = type;
        if (type == 2)
            bspline_make_knots(spline->nkey, 3, &spline->knots);
        base = &spline->base;
        base->x = 0;
        base->y = 0;
        base->z = 0;
    }
}

static inline void psj_vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

/*
 * "J": first key to where the previous part's same curve ends (polar: sampled at its end); a polar
 * curve gets distance and angles from its pivot.
 */
/* 0x0014a4c8 */
static void pass_edit_spline_joint(PassEditor *ed)
{
    PassSpline *spline = ed->spline;
    PassSpline *prev;
    PassVec3 end_pos;
    PassVec3 delta;
    int ang[2];
    int cnt;
    void *tbl;
    int total;
    int k;
    int stride;
    PassRun *run;
    PassPart *part;
    PassVec3 *pp;
    PassEnt *pass;
    int idx;

    if (spline->flags & 1)
        return;
    if (ed->partIdx <= 0)
        return;
    if (ed->sel)
        prev = (PassSpline *)((char *)ed->part - 108);
    else
        prev = (PassSpline *)((char *)ed->part - 64);
    if (prev->flags & 1)
        return;
    if (prev->type != 4) {
        stride = 3;
        if (prev->type != 1)
            stride = 1;
        pp = (PassVec3 *)((stride * prev->nkey - stride) * (int)sizeof(PassVec3) + (int)prev->pts);
        end_pos.x = pp->x;
        do { } while (0);
        end_pos.y = pp->y;
        end_pos.z = pp->z;
    } else {
        cnt = pass_table_count();
        tbl = pass_table_get();
        do { } while (0);
        pass = ed->pass;
        do { idx = ed->partIdx; } while (0);
        total = pass->start;
        k = idx - 1;
        part = pass->parts;
        if (k > 0) {
            PassRun *cur_run = &part->run;
            int j = k;

            do {
                j--;
                total += 1 + cur_run->key_count[cur_run->nkey - 1];
                cur_run = (PassRun *)((char *)cur_run + 108);
                part++;
            } while (j != 0);
        }
        run = &part->run;
        pass_table_set(ed->tbl, ed->npass);
        if (ed->sel)
            sequence_eval(ed->passIdx, (float)(total + run->key_count[run->nkey - 1]), 0, &end_pos, 0);
        else
            sequence_eval(ed->passIdx, (float)(total + run->key_count[run->nkey - 1]), 0, 0, &end_pos);
        pass_table_set(tbl, cnt);
    }
    if (spline->type != 4) {
        spline->pts->x = end_pos.x;
        spline->pts->y = end_pos.y;
        spline->pts->z = end_pos.z;
    } else {
        psj_vsub(&delta.x, &end_pos.x, &spline->base.x);
        spline->pts->x = vec3_length(&delta);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-delta.x, -delta.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(delta.y, sqrtf(delta.x * delta.x + delta.z * delta.z)));
        } while (0);
        do { } while (0);
        spline->pts->z = (float)ang[1] * DEG_PER_ANG;
        spline->pts->y = (float)ang[0] * DEG_PER_ANG;
    }
}

/*
 * Game time, mode (unused, absolute, relative: symbol, camera/look-at, angles), kind, one row per
 * point (number, position, key time, frames); cursor point green, kept in view.
 */
/* 0x0014a7f0 */
static void pass_edit_spline_draw(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    int x = ed->cellw;
    int y = ed->cellh;
    int win_w;
    int win_h;
    PassSpline *spline;
    char *sym;
    int n;
    int rows;
    int i;
    float *rate;
    PassVec3 *pt;
    int count;

    gw_window_get_size(win, &win_w, &win_h);
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    gw_win_printf(win, x, y, str_pass_edit_gamecount, (int)game_time_get());
    spline = ed->spline;
    sym = (char *)&spline->ref;
    y += ed->cellh;
    if (spline->flags & 1)
        gw_win_printf(win, x, y, str_pass_edit_nothing);
    else if (spline->flags & 2)
        gw_win_printf(win, x, y, str_pass_edit_absolute);
    else {
        gw_win_printf(win, x, y, str_pass_edit_relative_for, sym[0], sym[1], sym[2], sym[3]);
        if (ed->spline->flags & 4)
            gw_win_printf(win, x + ed->cellw * 18, y, str_pass_edit_cam);
        else
            gw_win_printf(win, x + ed->cellw * 18, y, str_pass_edit_int);
        if (ed->spline->flags & 0x10)
            gw_win_printf(win, x + ed->cellw * 23, y, str_pass_edit_angx);
        if (ed->spline->flags & 0x20)
            gw_win_printf(win, x + ed->cellw * 28, y, str_pass_edit_angy);
        if (ed->spline->flags & 0x40)
            gw_win_printf(win, x + ed->cellw * 33, y, str_pass_edit_angz);
    }
    y += ed->cellh;
    switch (ed->spline->type) {
    case 0:
        gw_win_printf(win, x, y, str_pass_edit_liner);
        break;
    case 1:
        gw_win_printf(win, x, y, str_pass_edit_bezier);
        break;
    case 2:
        gw_win_printf(win, x, y, str_pass_edit_bspline);
        break;
    case 3:
        gw_win_printf(win, x, y, str_pass_edit_cardinal_tension, ed->spline->tension);
        break;
    case 4:
        gw_win_printf(win, x, y, str_pass_edit_disang_base, ed->spline->base.x, ed->spline->base.y,
                     ed->spline->base.z);
        break;
    }
    y += ed->cellh;
    spline = ed->spline;
    if (spline->flags & 1)
        return;
    n = spline->nkey;
    if (spline->type == 1)
        n = n * 3;
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    rows = win_h / ed->cellh - 5;
    {
        int cp = ed->cpidx;
        int top = ed->sp_top;

        if (cp < top) {
            ed->sp_top = cp;
            top = cp;
        }
        if (cp >= top + rows) {
            int row_top = cp - rows + 1;

            if (row_top < 0)
                row_top = 0;
            ed->sp_top = row_top;
        }
    }
    if (ed->spline->type != 4)
        gw_win_printf(win, x, y, str_pass_edit_no_x_y_z);
    else
        gw_win_printf(win, x, y, str_pass_edit_no_distance_degx_degy);
    {
        int top = ed->sp_top;

        pt = ed->spline->pts + top;
        rate = ed->spline->rate + top;
        i = top;
    }
    if (i < i + rows) {
        do {
            count = (int)(*rate * (float)ed->run->key_count[ed->run->nkey - 1]);
            y += ed->cellh;
            if (i == ed->cpidx)
                gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0));
            else
                gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
            if (ed->spline->type == 2) {
                gw_win_printf(win, x, y, str_pass_edit_fmt_3, i, pt->x, pt->y, pt->z);
                rate++;
            } else if (ed->spline->type == 1) {
                if (i % 3 == 0)
                    gw_win_printf(win, x, y, str_pass_edit_fmt_4, i / 3, pt->x, pt->y, pt->z, *rate++, count);
                else
                    gw_win_printf(win, x, y, str_pass_edit_fmt_5, pt->x, pt->y, pt->z);
            } else
                gw_win_printf(win, x, y, str_pass_edit_fmt_4, i, pt->x, pt->y, pt->z, *rate++, count);
            if (i == n - 1)
                break;
            i++;
            pt++;
        } while (i < ed->sp_top + rows);
    }
}

/* Red box at base (current position), points grey, selected green. */
/* 0x0014aec8 */
static void pass_edit_draw_points(PassSpline *spline, PassVec3 *base, int sel)
{
    PassVec3 *p = spline->pts;
    int n;
    int i;

    ulMatrixPush();
    do { } while (0);
    vu0_matrix_translate(ulMatrixCur, base->x, base->y, base->z);
    debug_draw_box(RGBA(0x80, 0, 0, 0x80), 10.0f);
    ulMatrixPop();
    n = spline->nkey;
    if (spline->type == 1)
        n = n * 3;
    if (n > 0) {
        i = 0;
        do {
            ulMatrixPush();
            do {
                vu0_matrix_translate(ulMatrixCur, p->x, p->y, p->z);
            } while (0);
            debug_draw_box(i == sel ? RGBA(0, 0x80, 0, 0x80) : RGBA(0x60, 0x60, 0x60, 0x80), 10.0f);
            p++;
            i++;
            ulMatrixPop();
        } while (i < n);
    }
}

/*
 * With path drawing on: the edited curve alone (made absolute, other curve off): path, points, and
 * a marker one frame further per call (ed->playpos).
 */
/* 0x0014afd8 */
static void pass_edit_spline_preview(PassEditor *ed)
{
    PassVec3 pos;
    int cnt;
    void *tbl;
    int total;
    int i;
    PassPart *part;
    PassRun *run;
    unsigned flags;
    unsigned saved;

    if (ed->drawmode) {
        cnt = pass_table_count();
        tbl = pass_table_get();
        part = ed->pass->parts;
        total = ed->pass->start;
        i = ed->partIdx;
        if (i > 0) {
            PassRun *cur_run = &part->run;
            int j = i;

            do {
                j--;
                total += 1 + cur_run->key_count[cur_run->nkey - 1];
                cur_run = (PassRun *)((char *)cur_run + 108);
                part++;
            } while (j != 0);
        }
        run = &part->run;
        pass_table_set(ed->tbl, ed->npass);
        flags = ed->spline->flags;
        if ((flags & 1) == 0) {
            if (ed->sel != 0) {
                saved = ed->part->in.flags;
                ed->spline->flags = 2;
                ed->part->in.flags = 1;
                pass_draw_part_path(ed->passIdx, (float)total, pass_table_end(ed->passIdx) / 20.0f,
                                    (float)run->key_count[run->nkey - 1]);
                sequence_eval(ed->passIdx, (float)(total + ed->playpos), 0, &pos, 0);
                ed->part->in.flags = saved;
            } else {
                saved = ed->part->cam.flags;
                ed->spline->flags = 2;
                ed->part->cam.flags = 1;
                pass_draw_part_path(ed->passIdx, (float)total, pass_table_end(ed->passIdx) / 20.0f,
                                    (float)run->key_count[run->nkey - 1]);
                sequence_eval(ed->passIdx, (float)(total + ed->playpos), 0, 0, &pos);
                ed->part->cam.flags = saved;
            }
            pass_edit_draw_points(ed->spline, &pos, ed->cpidx);
            ed->spline->flags = flags;
            ed->playpos = (ed->playpos + 1) % run->key_count[run->nkey - 1];
        }
        pass_table_set(tbl, cnt);
    }
}

/* Help handler of the point mover. */
/* 0x0014b230 */
static void pass_edit_point_move_draw(void *ed)
{
    pass_edit_spline_preview(ed);
    debug_set_origin(0, -4);
    debug_puts(str_pass_edit_arrow_left_right_x);
    debug_newline();
    debug_puts(str_pass_edit_page_up_down_y);
    debug_newline();
    debug_puts(str_pass_edit_arrow_up_down_z);
    debug_newline();
    debug_puts(str_pass_edit_space_exit);
    debug_newline();
}

/* While held: left/right X, Page Up/Down Y, up/down Z, 0.1 a frame (10 with Shift); Space back. */
/* 0x0014b2a0 */
static void pass_edit_point_move_key(PassEditor *ed)
{
    PassVec3 *pt;
    float step;
    int key;

    if (gw_window_is_active(ed->win)) {
        pt = ed->pt;
        step = 0.1f;
        key = kbd_state[6];
        if (kbd_state[7] & 0x22)
            step = 10.0f;
        if (pad_keyboard_key_down(82))
            pt->z = pt->z - step;
        if (pad_keyboard_key_down(81))
            pt->z = pt->z + step;
        if (pad_keyboard_key_down(80))
            pt->x = pt->x - step;
        if (pad_keyboard_key_down(79))
            pt->x = pt->x + step;
        if (pad_keyboard_key_down(75))
            pt->y = pt->y - step;
        if (pad_keyboard_key_down(78))
            pt->y = pt->y + step;
        if (key == 44) {
            FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_spline_key;
            FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_spline_help;
        }
    }
}

/* Like pass_edit_part_window_init, pass number in the title too. */
/* 0x0014b3e0 */
static void pass_edit_part_window_init_indexed(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    char *sym = (char *)ed->pass;

    ((int (*)(char *, char *, ...))sprintf)(ed->title, str_pass_edit_passeditor_pass_2, ed->passIdx, sym[0], sym[1], sym[2], sym[3]); /* steer(cast) */
    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_part_list_key;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_part_list_draw;
    gw_window_set_title(ed->win, ed->title);
    gw_window_set_draw_handler(ed->win, pass_edit_part_list_draw_rows);
}

/*
 * Up/down cursor (10 with Shift), T/E + left/right tension/key time, B/Space move pivot/point,
 * Insert/Delete, C kind, N/A/R mode, 1 camera/look-at, 2..4 pitch/yaw/roll, J join, Enter back.
 */
/* 0x0014b470 */
static void pass_edit_spline_key(PassEditor *ed)
{
    int step;
    int key;

    if (gw_window_is_active(ed->win)) {
        step = 1;
        key = kbd_state[6];
        if (kbd_state[7] & 0x22)
            step = 10;
        switch (key) {
        case 82:
        {
            int cp = ed->cpidx - step;

            if (cp < 0)
                cp = 0;
            ed->cpidx = cp;
            break;
        }
        case 81:
        {
            PassSpline *spline = ed->spline;
            int n = spline->nkey;
            int cp;
            int max_cp;

            if (spline->type == 1)
                n = n * 3;
            cp = ed->cpidx + step;
            max_cp = n - 1;
            ed->cpidx = max_cp < cp ? max_cp : cp;
            break;
        }
        case 23:
            pass_edit_nudge_float(0.01f, &ed->spline->tension, key);
            break;
        case 5:
            ed->pt = &ed->spline->base;
            FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_point_move_key;
            FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_point_move_draw;
            break;
        case 8:
            pass_edit_edit_rate(ed, key);
            break;
        case 44:
        {
            PassSpline *spline = ed->spline;

            FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_point_move_key;
            FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_point_move_draw;
            ed->pt = spline->pts + ed->cpidx;
            break;
        }
        case 73:
            pass_edit_spline_insert_key(ed);
            break;
        case 76:
            pass_edit_spline_delete_key(ed);
            break;
        case 6:
            pass_edit_spline_next_type(ed);
            break;
        case 17:
            pass_edit_spline_reset(ed);
            break;
        case 4:
            pass_edit_spline_set_mode(ed, 2);
            break;
        case 21:
            pass_edit_spline_set_mode(ed, 0xfffffffc);
            break;
        case 30:
        {
            unsigned flags = ed->spline->flags;

            if (flags & 0xfffffffc) {
                if (flags & 4)
                    ed->spline->flags = (flags & ~4u) | 8;
                else
                    ed->spline->flags = (flags & ~8u) | 4;
            }
            break;
        }
        case 31:
            pass_edit_toggle_flags(ed, 16);
            break;
        case 32:
            pass_edit_toggle_flags(ed, 32);
            break;
        case 33:
            pass_edit_toggle_flags(ed, 64);
            break;
        case 40:
            pass_edit_part_window_init_indexed(ed->win);
            break;
        case 13:
            pass_edit_spline_joint(ed);
            break;
        default:
            pass_edit_list_key(key, step, ed);
            break;
        }
    }
}

/* Help handler of the curve editor. */
/* 0x0014b738 */
static void pass_edit_spline_help(PassEditor *ed)
{
    if (gw_window_is_active(ed->win)) {
        pass_edit_spline_preview(ed);
        debug_set_origin(0, -21);
        debug_puts(str_pass_edit_insert_insert_ne_2);
        debug_newline();
        debug_puts(str_pass_edit_delete_delete_ct);
        debug_newline();
        debug_puts(str_pass_edit_n_set_data);
        debug_newline();
        debug_puts(str_pass_edit_a_set_data);
        debug_newline();
        debug_puts(str_pass_edit_r_set_data);
        debug_newline();
        debug_puts(str_pass_edit_n1_set_data);
        debug_newline();
        debug_puts(str_pass_edit_n2_3_4_set);
        debug_newline();
        debug_puts(str_pass_edit_c_change_sp);
        debug_newline();
        debug_puts(str_pass_edit_j_joint_to);
        debug_newline();
        debug_puts(str_pass_edit_space_edit_ctrl);
        debug_newline();
        debug_puts(str_pass_edit_e_arrow_left_right);
        debug_newline();
        debug_puts(str_pass_edit_t_arrow_left_right);
        debug_newline();
        debug_puts(str_pass_edit_b_edit_base);
        debug_newline();
        debug_puts(str_pass_edit_enter_exit_2);
        debug_newline();
        pass_edit_help_text();
    }
}

/* sel set: camera curve, else look-at. */
/* 0x0014b8a0 */
static void pass_edit_spline_enter(PassEditor *ed, int sel)
{
    PassEnt *pass;
    PassPart *part;
    char *sym;
    int idx;

    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_spline_key;
    idx = ed->partIdx;
    pass = ed->pass;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_spline_help;
    ed->cpidx = 0;
    ed->sp_top = 0;
    part = &pass->parts[idx];
    ed->playpos = 0;
    ed->sel = sel;
    ed->part = part;
    sym = (char *)pass;
    if (sel != 0) {
        ed->spline = &part->cam;
        sprintf(ed->title, str_pass_edit_passeditor_pass_3, ed->passIdx, sym[0], sym[1], sym[2], sym[3], idx);
    } else {
        ed->spline = &part->in;
        sprintf(ed->title, str_pass_edit_passeditor_pass_4, ed->passIdx, sym[0], sym[1], sym[2], sym[3], idx);
    }
    ed->run = &ed->part->run;
    gw_window_set_title(ed->win, ed->title);
    gw_window_set_draw_handler(ed->win, pass_edit_spline_draw);
}

/* Insert: duplicates the part; the pass grows by its length. */
/* 0x0014b998 */
static void pass_edit_part_duplicate(PassEditor *ed)
{
    PassPart *old;
    PassPart *src;
    PassPart *dst;
    int i;

    old = ed->pass->parts;
    ed->pass->nparts = ed->pass->nparts + 1;
    ed->pass->parts = yuki_alloc(ed->pass->nparts * 108);
    dst = ed->pass->parts;
    src = old;
    for (i = 0; i < ed->pass->nparts; i++, dst++) {
        if (i != ed->partIdx) {
            nj_copy8(dst, src++, 108);
        } else {
            nj_copy8(dst, src, 108);
            pass_spline_clone(&dst->cam, &src->cam);
            pass_spline_clone(&dst->in, &src->in);
            pass_run_clone(&dst->run, &src->run);
            ed->pass->len += dst->run.key_count[dst->run.nkey - 1];
        }
    }
    do {
        yuki_free(old);
    } while (0);
}

/* Delete: while more than one part; the pass shrinks by its length. */
/* 0x0014bab0 */
static void pass_edit_part_delete(PassEditor *ed)
{
    PassEnt *pass = ed->pass;
    PassPart *old = pass->parts;
    int n = pass->nparts;
    PassPart *src = old;

    if (n >= 2) {
        PassPart *dst;
        int i;

        pass->nparts = n - 1;
        ed->pass->parts = yuki_alloc((n - 1) * 108);
        dst = ed->pass->parts;
        for (i = 0; i < n; i++, src++) {
            if (i != ed->partIdx) {
                nj_copy8(dst++, src, 108);
            } else {
                pass_spline_free(&src->cam);
                pass_spline_free(&src->in);
                ed->pass->len -= src->run.key_count[src->run.nkey - 1];
                pass_run_free(&src->run);
            }
        }
        do {
            yuki_free(old);
        } while (0);
        {
            int max_idx = ed->pass->nparts - 1;
            int idx = ed->partIdx;

            if (max_idx < idx)
                idx = max_idx;
            ed->partIdx = idx;
        }
        {
            int max_idx = ed->pass->nparts - 1;
            int idx = ed->partIdx2;

            if (max_idx < idx)
                idx = max_idx;
            ed->partIdx2 = idx;
        }
    }
}

/* With the marked part (ed->partIdx2). */
/* 0x0014bbf0 */
static void pass_edit_swap_parts(PassEditor *ed)
{
    PassPart swap;
    PassPart *base = ed->pass->parts;

    nj_copy8(&swap, base + ed->partIdx, 108);
    nj_copy8(base + ed->partIdx, base + ed->partIdx2, 108);
    nj_copy8(base + ed->partIdx2, &swap, 108);
}

/*
 * "N" unused, "A" absolute (blank symbol); relative: "C" camera or "I" look-at, plus "X", "Y",
 * "Z" for the angles followed.
 */
/* 0x0014bc80 */
static void pass_mode_letters(PassSpline *spline, char *mode, char *tgt)
{
    unsigned flags = spline->flags;

    if (flags & 1) {
        mode[0] = 'N';
        tgt[0] = tgt[1] = tgt[2] = tgt[3] = '_';
        return;
    }
    if (flags & 2) {
        mode[0] = 'A';
        tgt[0] = tgt[1] = tgt[2] = tgt[3] = '_';
        return;
    }
    if (flags & 4) {
        mode[0] = 'C';
        flags = spline->flags;
    } else if (flags & 8) {
        mode[0] = 'I';
        flags = spline->flags;
    }
    if (flags & 0x10) {
        mode[1] = 'X';
        flags = spline->flags;
    }
    if (flags & 0x20) {
        mode[2] = 'Y';
        flags = spline->flags;
    }
    if (flags & 0x40)
        mode[3] = 'Z';
}

/*
 * Game time, one row per part (number, curve and roll modes and symbols, length); cursor green,
 * mark yellow. No or absolute roll blanks the roll symbol.
 */
/* 0x0014bd20 */
static void pass_edit_part_list_draw_rows(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    int x = ed->cellw;
    int y = ed->cellh;
    int win_w;
    int win_h;
    int rows;
    int i;
    PassPart *p;

    gw_window_get_size(win, &win_w, &win_h);
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    gw_win_printf(win, x, y, str_pass_edit_gamecount, (int)game_time_get());
    y += ed->cellh;
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    rows = win_h / ed->cellh - 4;
    {
        int idx = ed->partIdx;
        int top = ed->part_top;

        if (idx < top) {
            ed->part_top = idx;
            top = idx;
        }
        if (ed->partIdx >= top + rows) {
            int row_top = ed->partIdx - rows + 1;

            if (row_top < 0)
                row_top = 0;
            ed->part_top = row_top;
        }
    }
    gw_win_printf(win, x, y, str_pass_edit_no_cam_int_angz);
    {
        int top = ed->part_top;

        p = ed->pass->parts + top;
        i = top;
    }
    y += ed->cellh;
    if (i < i + rows) {
        do {
            char cm[5] = "____";
            char im[5] = "____";
            char rm[5] = "____";
            char *cr = (char *)&p->cam.ref;
            char *ir = (char *)&p->in.ref;
            char *rr = (char *)&p->run.ref;
            PassRun *run = &p->run;
            unsigned int flags;
            char u;

            y += ed->cellh;
            pass_mode_letters(&p->cam, cm, cr);
            pass_mode_letters(&p->in, im, ir);
            flags = run->flags;
            do {
                if (flags & 0x4000) {
                    flags = 'N';
                    goto set;
                }
                if (flags & 0x8000) {
                    flags = 'A';
                    do {
set:
                        u = '_';
                    } while (0);
                    rm[0] = flags;
                    rr[3] = u;
                    rr[2] = u;
                    rr[1] = u;
                    ((char *)&p->run.ref)[0] = u;
                } else {
                    rm[0] = 'R';
                }
                if (i == ed->partIdx)
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0));
                else if (i == ed->partIdx2)
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0x80));
                else
                    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
            } while (0);
            gw_win_printf(win, x, y, str_pass_edit_n4, i, cm, cr[0], cr[1], cr[2], cr[3],
                         im, ir[0], ir[1], ir[2], ir[3], rm, rr[0], rr[1], rr[2], rr[3],
                         run->key_count[run->nkey - 1]);
            if (i == ed->pass->nparts - 1)
                break;
            i++;
            p++;
        } while (i < ed->part_top + rows);
    }
}

/* Editor window -> pass list. */
/* 0x0014c0b0 */
static void pass_edit_list_window_init(int win)
{
    char *ed = gw_window_get_slot(win, 0);

    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_task_exec;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_task_disp;
    gw_window_set_title(FIELD_AT(ed, 48, int), str_pass_edit_passeditor_pass_5);
    gw_window_set_draw_handler(FIELD_AT(ed, 48, int), pass_edit_list_draw);
}

/*
 * Up/down cursor (10 with Shift), Insert/Delete, C/I/R camera/look-at/roll editor, M mark, P swap
 * with mark, Enter back to passes; else shared keys.
 */
/* 0x0014c110 */
static void pass_edit_part_list_key(PassEditor *ed)
{
    int step;
    int key;

    if (gw_window_is_active(ed->win)) {
        step = 1;
        key = kbd_state[6];
        if (kbd_state[7] & 0x22)
            step = 10;
        switch (key) {
        case 82:
        {
            int idx = ed->partIdx - step;

            if (idx < 0)
                idx = 0;
            ed->partIdx = idx;
            break;
        }
        case 81:
        {
            int idx = ed->partIdx + step;
            int max_idx = ed->pass->nparts - 1;

            if (max_idx < idx)
                idx = max_idx;
            ed->partIdx = idx;
            break;
        }
        case 73:
            pass_edit_part_duplicate(ed);
            break;
        case 76:
            pass_edit_part_delete(ed);
            break;
        case 6:
            pass_edit_spline_enter(ed, 1);
            break;
        case 12:
            pass_edit_spline_enter(ed, 0);
            break;
        case 21:
            pass_edit_run_enter(ed);
            break;
        case 16:
            ed->partIdx2 = ed->partIdx;
            break;
        case 19:
            pass_edit_swap_parts(ed);
            break;
        case 40:
            pass_edit_list_window_init(ed->win);
            break;
        default:
            pass_edit_list_key(key, step, ed);
            break;
        }
    }
}

/* With path drawing on: the selected part's path (edited passes); then key help. */
/* 0x0014c280 */
static void pass_edit_part_list_draw(PassEditor *ed)
{
    int cnt;
    void *tbl;
    int total;
    int k;
    int i;
    int row_top;
    PassPart *part;
    PassRun *run;

    if (gw_window_is_active(ed->win)) {
        if (ed->drawmode) {
            cnt = pass_table_count();
            tbl = pass_table_get();
            part = ed->pass->parts;
            total = ed->pass->start;
            i = ed->partIdx;
            for (k = 0; k < i; k++) {
                run = &part->run;
                row_top = total + 1;
                part++;
                total = row_top + run->key_count[run->nkey - 1];
            }
            run = &part->run;
            pass_table_set(ed->tbl, ed->npass);
            if (ed->drawmode == 1)
                pass_draw_path(ed->passIdx, (float)total, pass_table_end(ed->passIdx) / 20.0f,
                               (float)run->key_count[run->nkey - 1]);
            else
                pass_draw_part_path(ed->passIdx, (float)total, pass_table_end(ed->passIdx) / 20.0f,
                                    (float)run->key_count[run->nkey - 1]);
            pass_table_set(tbl, cnt);
        }
        debug_set_origin(0, -14);
        debug_puts("[INSERT] ......... Insert New Spline Part");
        debug_newline();
        debug_puts("[DELETE] ......... Delete Spline Part");
        debug_newline();
        debug_puts("[C] .............. Edit CAM Spline");
        debug_newline();
        debug_puts("[I] .............. Edit INT Spline");
        debug_newline();
        debug_puts("[R] .............. Edit RUN Data(AngZ & count)");
        debug_newline();
        debug_puts("[M/P] ............ Mark & Swap Spline Part");
        debug_newline();
        debug_puts("[ENTER] .......... Exit");
        debug_newline();
        pass_edit_help_text();
    }
}

/* Cursor on its first part. */
/* 0x0014c520 */
static void pass_edit_part_list_enter(PassEditor *ed)
{
    char *sym;

    FIELD_BEFORE(ed, 48, PassEditCb) = pass_edit_part_list_key;
    FIELD_BEFORE(ed, 44, PassEditCb) = pass_edit_part_list_draw;
    ed->partIdx = 0;
    ed->part_top = 0;
    ed->partIdx2 = 0;
    ed->pass = ed->tbl + ed->passIdx;
    sym = (char *)ed->pass;
    sprintf(ed->title, str_pass_edit_passeditor_pass_2, ed->passIdx, sym[0], sym[1], sym[2], sym[3]);
    gw_window_set_title(ed->win, ed->title);
    gw_window_set_draw_handler(ed->win, pass_edit_part_list_draw_rows);
}

/*
 * Game time, one row per pass (number, symbol, parts, start, length). Selected green, red if
 * others are relative to it; those yellow.
 */
/* 0x0014c5c8 */
static void pass_edit_list_draw(int win)
{
    PassEditor *ed = gw_window_get_slot(win, 0);
    int y = ed->cellh;
    int x = ed->cellw;
    int win_w;
    int win_h;
    int rows;
    int i;
    int top;
    PassEnt *pass;

    gw_window_get_size(win, &win_w, &win_h);
    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
    gw_win_printf(win, x, y, str_pass_edit_gamecount, (int)game_time_get());
    rows = win_h / ed->cellh - 4;
    y = ed->cellh * 2;
    if (ed->passIdx < ed->top)
        ed->top = ed->passIdx;
    if (ed->passIdx >= ed->top + rows) {
        top = ed->passIdx - rows + 1;
        if (top < 0)
            top = 0;
        ed->top = top;
    }
    pass = ed->tbl + ed->top;
    gw_win_printf(win, x, y, str_pass_edit_no_symbolname_partsnum_startcou, game_time_get());
    i = ed->top;
    y += ed->cellh;
    if (i < i + rows) {
        do {
            if (i == ed->passIdx) {
                if (pass_symbol_used(ed->tbl, ed->npass, i))
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0, 0x80));
                else
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0));
            } else {
                if (pass_symbol_used(ed->tbl, ed->npass, i))
                    gw_window_set_draw_color(win, ARGB(0x80, 0, 0x80, 0x80));
                else
                    gw_window_set_draw_color(win, ARGB(0x80, 0x60, 0x60, 0x60));
            }
            gw_win_printf(win, x, y, str_pass_edit_fmt_6, i, ((char *)pass)[0], ((char *)pass)[1],
                         ((char *)pass)[2], ((char *)pass)[3], pass->nparts, pass->start, pass->len);
            y += ed->cellh;
            if (i == ed->npass - 1)
                break;
            i++;
            pass++;
        } while (i < ed->top + rows);
    }
}

/* 0x0014c828 */
static int pass_edit_insert_pass(PassEditWork *ed)
{
    PassEditEnt *old = ed->tbl;
    PassEditEnt *src = old;
    PassEditEnt *dst;
    int i;

    ed->npass = ed->npass + 1;
    ed->tbl = yuki_alloc(ed->npass * 20);
    dst = ed->tbl;
    for (i = 0; i < ed->npass; i++, dst++) {
        if (i != ed->passIdx) {
            nj_copy8(dst, src++, 20);
        } else {
            pass_clone(dst, src);
        }
    }
    return ((int (*)(void *))yuki_free)(old); /* steer(cast) */
}

/* Keeps at least one pass. */
/* 0x0014c8f0 */
static void pass_edit_delete_pass(PassEditWork *ed)
{
    int n = ed->npass;
    PassEditEnt *old = ed->tbl;
    PassEditEnt *src = old;

    if (n >= 2) {
        PassEditEnt *dst;
        int i;

        ed->npass = n - 1;
        ed->tbl = yuki_alloc((n - 1) * 20);
        dst = ed->tbl;
        for (i = 0; i < n; i++, src++) {
            if (i != ed->passIdx) {
                nj_copy8(dst++, src, 20);
            } else {
                pass_free(src);
            }
        }
        do {
            yuki_free(old);
        } while (0);
        ed->passIdx = ed->npass - 1 < ed->passIdx ? ed->npass - 1 : ed->passIdx;
    }
}

/* Drops the edits, restores game time and its running state, ends the task, unticks "PassEdit". */
/* 0x0014c9d8 */
static void pass_edit_close(int win)
{
    PassEditWork *ed;

    if (pass_edit_task != 0) {
        ed = gw_window_get_slot(win, 0);
        pass_table_free(ed->tbl, ed->npass);
        game_time_set((float)ed->countSave);
        if (ed->countFlag != 0)
            GameCountStop();
        else
            GameCountStart();
        if (pass_edit_task != 0)
            TASK_OF(pass_edit_task)->flags |= TASK_F_END;
        pass_edit_task = 0;
        menu_item_set_value(str_pass_edit_menu, str_pass_edit, 0);
    }
}

/* Pass list window 70 wide; saves game time and its running state, then starts it. */
/* 0x0014caa8 */
static void pass_edit_task_init(PassEditWork *ed)
{
    int count;

    ed->npass = pass_table_count();
    ed->tbl = pass_table_clone(pass_table_get(), pass_table_count());
    gw_get_cell_size(&ed->cellw, &ed->cellh);
    ((void (*)(PassEditWork *, int *, char *, int, PassEditFn, PassEditFn, int))pass_edit_make_list_window)(ed, &ed->win, str_pass_edit_passeditor_pass_5, 70, pass_edit_list_draw, pass_edit_close, 0); /* steer(cast) */
    ed->top = 0;
    ed->passIdx = 0;
    count = (int)game_time_get();
    ed->countFlag = game_flags & 1;
    ed->countSave = count;
    ed->countSave2 = count;
    GameCountStart();
    ed->run_preview = 0;
    ed->drawmode = 1;
}

/* 0x0014cb68 */
static void pass_edit_task_release(PassEditWork *ed)
{
    gw_window_close(ed->win);
}

/* Also the pass list's help handler: with path drawing on, the selected pass; then key help. */
/* 0x0014cb70 */
static void pass_edit_task_disp(PassEditWork *ed)
{
    int cnt;
    PassEditEnt *pts;
    float z;

    if (gw_window_is_active(ed->win)) {
        if (ed->drawmode != 0) {
            cnt = pass_table_count();
            pts = pass_table_get();
            pass_table_set(ed->tbl, ed->npass);
            if (ed->drawmode == 1) {
                z = 0.0f;
                pass_draw_path(ed->passIdx, z, pass_table_end(ed->passIdx) / 20.0f, z);
            } else {
                z = 0.0f;
                pass_draw_part_path(ed->passIdx, z, pass_table_end(ed->passIdx) / 20.0f, z);
            }
            pass_table_set(pts, cnt);
        }
        debug_set_origin(0, -13);
        debug_puts(str_pass_edit_insert_insert_new_pa);
        debug_newline();
        debug_puts(str_pass_edit_delete_delete_pass);
        debug_newline();
        debug_puts(str_pass_edit_e_edit_pass);
        debug_newline();
        debug_puts(str_pass_edit_n_edit_symbol_n);
        debug_newline();
        debug_puts(str_pass_edit_f_increment_sta);
        debug_newline();
        debug_puts(str_pass_edit_l_decrement_sta);
        debug_newline();
        pass_edit_help_text();
    }
}

/*
 * Also the pass list's key handler: up/down (10 with Shift), F/L lower/raise start time (help text
 * has them swapped), Insert/Delete, E part list, N symbol.
 */
/* 0x0014cd08 */
static void pass_edit_task_exec(PassEditWork *ed)
{
    int key;
    int step;

    if (gw_window_is_active(ed->win)) {
        step = 1;
        key = kbd_state[6];
        if (kbd_state[7] & 0x22)
            step = 10;
        switch (key) {
        case 0x52: {
            int x = ed->passIdx - step;

            if (x < 0)
                x = 0;
            ed->passIdx = x;
            break;
        }
        case 0x51: {
            int x = ed->passIdx + step;

            if (ed->npass - 1 < x)
                x = ed->npass - 1;
            ed->passIdx = x;
            break;
        }
        case 0x49:
            pass_edit_insert_pass(ed);
            break;
        case 0x4c:
            pass_edit_delete_pass(ed);
            break;
        case 0x11:
            pass_edit_symbol_window_open(ed, &ed->tbl[ed->passIdx].sym, pass_edit_task_exec, pass_edit_task_disp, ed->win);
            break;
        case 8:
            pass_edit_part_list_enter(ed);
            break;
        case 9: {
            int x = ed->tbl[ed->passIdx].start - step;

            if (x < 0)
                x = 0;
            ed->tbl[ed->passIdx].start = x;
            break;
        }
        case 0xf: {
            ed->tbl[ed->passIdx].start = ed->tbl[ed->passIdx].start + step;
            break;
        }
        default:
            pass_edit_list_key(key, step, ed);
            break;
        }
    }
}

/* Only while a game runs, else 0. */
/* 0x0014cea0 */
static char *pass_edit_task_create(void)
{
    char *work;

    if (game_work == 0)
        return 0;
    work = task_create(system1_work, &pass_edit_desc, 384);
    TASK_FLAGS_OF(work) |= TASK_F_RUN_PAUSED;
    return work;
}

/* 0x0014cef0 */
static void pass_sim_init(void)
{
}

/* Resumes the editor, restores game time. */
/* 0x0014cef8 */
static void pass_sim_release(PassSimWork *w)
{
    if (w->parent != 0)
        TASK_FLAGS_OF(w->parent) &= ~TASK_F_SUSPENDED;
    game_time_set((float)w->saved);
    GameCountStart();
}

/* Game time one frame further per frame; ends at the pass end or on Enter. */
/* 0x0014cf38 */
static void pass_sim_exec(PassSimWork *w)
{
    if (kbd_key != 40) {
        game_time_set((float)w->cur++);
        if (w->cur <= w->end)
            return;
    }
    if (w != 0)
        TASK_FLAGS_OF(w) |= TASK_F_END;
}

/* 0x0014cfa8 */
static void pass_sim_disp(void)
{
}

/* "G": plays start..end; editor suspended (task flag 2), game time saved. */
/* 0x0014cfb0 */
static PassSimWork *pass_sim_create(char *parent, int start, int end)
{
    PassSimWork *w = task_create_sibling(parent, &pass_sim_desc, 16);

    TASK_FLAGS_OF(w) |= TASK_F_RUN_PAUSED;
    do {
        if (parent != 0)
            TASK_FLAGS_OF(parent) |= TASK_F_SUSPENDED;
    } while (0);
    w->saved = (int)game_time_get();
    w->cur = start;
    w->end = end;
    w->parent = parent;
    return w;
}
