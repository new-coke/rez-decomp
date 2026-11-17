/*
 * Debug GUI console: coloured lines in a ring buffer, 'Console' window with log, backlog, command
 * line.
 */

#include "types_gw.h"
#include <stdarg.h>
#include "libc.h"
#include "gw_console.h"
#include "gw_command.h"
#include "gw_desktop.h"
#include "gw_draw.h"
#include "gw_font.h"
#include "gw_heap.h"
#include "gw_keyboard.h"
#include "gw_menu.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "ul_gx_prim.h"

static void gw_console_add_menu(void);
static void gw_console_toggle(int visible);
static void gw_console_line_init(GwConLine *line);
static void gw_console_print(char *s);

extern GwConsole gw_console_state;
extern char *gw_console_buf;
extern GwConLine **gw_console_lines;
extern char str_gw_console_empty[8];
extern struct GwVarNode *gw_varedit_root;
extern GwVarDef gw_console_vardefs[];
extern int gw_console_menu_defs;
extern int gw_console_font_menu_defs;
extern GwKbd gw_keyboard_state;
extern char *gw_console_last_text;

/*
 * cfg = {buffer bytes, line count (power of two)}. Empty first line, window bottom left, 50 chars
 * wide; adds the desktop menu entry and editable variables.
 */
/* 0x00196c88 */
void gw_console_init(int *cfg)
{
    GwConsole *con;
    GwConLine *rec;
    int rect[4];
    int cell[2];
    int one;
    int i;

    con = &gw_console_state;
    con->bufsize = cfg[0];
    con->nlines = cfg[1];
    gw_console_buf = gw_dc_calloc(1, con->bufsize, "gw_console.c", 202);
    gw_console_last_text = 0;
    gw_console_lines = gw_dc_calloc(4, con->nlines, "gw_console.c", 204);
    for (i = 63; i >= 0; i--)
        con->hist[i] = 0;
    one = 1;
    con->hist_idx = 0;
    con->hist_next = one;
    gw_line_clear(&con->line);
    con->visible = 0;
    con->follow = 0;
    gw_console_set_serial_out(1);
    con->attr = gw_get_default_color();
    con->font = gw_get_cell_size(0, 0);
    rec = (GwConLine *)gw_console_buf;
    rec->len = strlen(str_gw_console_empty);
    rec->attr = con->attr;
    strcpy(rec->text, str_gw_console_empty);
    con->count = one;
    con->head = 0;
    con->top = 0;
    con->tail = 0;
    gw_console_lines[0] = rec;
    con->used = (rec->len + 27) & 0xfffffff0;
    rec = (GwConLine *)(gw_console_buf + con->used);
    gw_console_line_init(rec);
    gw_console_lines[con->count] = rec;
    gw_get_desktop_rect(rect);
    gw_font_get_cell_size(con->font, &cell[0], &cell[1]);
    con->x = rect[0];
    con->w = cell[0] * 50 + 5;
    con->h = 160;
    con->y = rect[3] - 160;
    gw_console_add_menu();
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_get_root(), gw_console_vardefs);
}

/* 0x00196e68 */
void gw_console_exit(void)
{
    gw_dc_free(gw_console_buf, "gw_console.c", 293);
    gw_dc_free(gw_console_lines, "gw_console.c", 294);
    gw_console_buf = 0;
    gw_console_lines = 0;
}

/* 0x00196eb8 */
void gw_console_poll(void)
{
}

/* 0x00196ec0 */
int gw_console_is_visible(void)
{
    return gw_console_state.visible;
}

/* Returns whether it was open. */
/* 0x00196ed0 */
int gw_console_set_visible(int visible)
{
    int old;

    old = gw_console_state.visible;
    if (visible != old)
        gw_console_toggle(visible);
    return old;
}

/* Returns the old setting. */
/* 0x00196f08 */
int gw_console_set_serial_out(int enable)
{
    int old;

    old = gw_console_state.serial_out;
    gw_console_state.serial_out = enable;
    return old;
}

/* Returns the old colour. */
/* 0x00196f20 */
int gw_console_set_attr(int attr)
{
    int old;

    old = gw_console_state.attr;
    gw_console_state.attr = attr;
    return old;
}

/* 0x00196f38 */
void gw_console_puts(char *s)
{
    gw_console_print(s);
}

/* Up to 256 bytes. */
/* 0x00196f40 */
void gw_console_printf(char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    gw_console_print(buf);
}

/* Bottom up from line, back through the ring to the oldest line or the window top. */
/* 0x00196f88 */
static void gw_console_draw_lines(void *win, int line)
{
    int size[2];
    int ch;
    int y;
    GwConLine *rec;
    GwConsole *con;

    con = &gw_console_state;
    ch = gw_font_cell_h;
    gw_window_get_size(win, &size[0], &size[1]);
    y = size[1] - 1;
    for (;;) {
        rec = gw_console_lines[line];
        if (rec == 0)
            break;
        if (size[1] < ch)
            break;
        gw_font_set_color(rec->attr);
        y -= ch;
        gw_win_text(win, 1, y, rec->text);
        size[1] -= ch;
        if (line == con->tail)
            break;
        line = (line - 1) & (con->nlines - 1);
    }
}

/*
 * Keeps the window on screen and in the desktop (remembering it); command line at the bottom,
 * backlog at the top while scrolled back, log between (three lines high then).
 */
/* 0x00197068 */
static void gw_console_layout(void)
{
    GwConsole *con;
    int sh;
    int y;
    int height;
    int cw;
    int sw;
    int ch;
    int rect[4];
    int x;
    int width;
    int inner;

    con = &gw_console_state;
    gw_get_desktop_rect(rect);
    gw_window_get_pos(con->frame, &x, &y);
    gw_window_get_size(con->frame, &width, &height);
    gw_font_get_cell_size(con->font, &cw, &ch);
    gw_get_screen_size(&sw, &sh);
    if (sw < width)
        width = sw;
    if (sh < height)
        height = sh;
    if (rect[2] < x + width)
        x = rect[2] - width;
    if (x < rect[0])
        x = rect[0];
    if (rect[3] < y + height)
        y = rect[3] - height;
    if (y < rect[1])
        y = rect[1];
    gw_window_set_pos(con->frame, x, y);
    gw_window_set_size(con->frame, width, height);
    con->x = x;
    con->y = y;
    con->w = width;
    con->h = height;
    x = 2;
    y = 2;
    width = width - 4;
    {
        int cmd_h = ch + 2;
        gw_window_set_pos(con->command_win, x, height - cmd_h);
    }
    gw_window_set_size(con->command_win, width, ch);
    inner = height - 4;
    height = inner - ch;
    if (con->follow) {
        height -= 6;
        height -= ch * 3;
        gw_window_set_pos(con->backlog_win, x, y);
        gw_window_set_size(con->backlog_win, width, height);
        {
            int next_y = y + 3;
            y = next_y + height;
        }
        height = ch * 3 + 3;
    } else
        gw_window_set_size(con->backlog_win, 0, 0);
    height = height - 3;
    gw_window_set_pos(con->log_win, x, y);
    gw_window_set_size(con->log_win, width, height);
}

/* 0x001972a8 */
static void gw_console_fill_rect(unsigned col, float z, float x, float y, float width, float height)
{
    GwVtx vtx[4];
    float x1;
    float y1;

    x1 = x + width;
    y1 = y + height;
    vtx[0].y = y;
    vtx[2].y = y;
    vtx[0].z = z;
    vtx[0].col = col;
    vtx[1].x = x;
    vtx[1].y = y1;
    vtx[1].z = z;
    vtx[3].y = y1;
    vtx[1].col = col;
    vtx[2].x = x1;
    vtx[2].z = z;
    vtx[2].col = col;
    vtx[3].x = x1;
    vtx[3].z = z;
    vtx[3].col = col;
    vtx[0].x = x;
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
}

/* 0x00197328 */
static void gw_console_toggle_visible(void)
{
    gw_console_set_visible(!gw_console_is_visible());
}

/* From gw_console_menu_defs. */
/* 0x00197348 */
static void gw_console_add_menu(void)
{
    GwConsole *con;
    int root;

    con = &gw_console_state;
    root = gw_desktop_get_menu();
    con->menu = gw_menu_build(0, &gw_console_menu_defs);
    gw_menu_append(root, con->menu);
}

/* 0x00197398 */
static void gw_console_select_font(int *item)
{
    gw_console_state.font = item[3];
}

/* Frees its item list. */
/* 0x001973a8 */
static void gw_console_popup_on_destroy(int win)
{
    int list;

    list = gw_window_get_slot(win, 0);
    if (list)
        gw_menu_free_all(list);
}

/* Right-click: 'Font Menu' pop-up at the pointer. */
/* 0x001973d8 */
static void gw_console_popup_menu(int parent)
{
    int x;
    int y;
    int list;
    int menu;

    if (gw_window_is_grabbed(parent)) {
        gw_mouse_get_pos(&x, &y);
        if (gw_mouse_get_stat() & 0x200) {
            list = gw_menu_build(0, &gw_console_font_menu_defs);
            menu = gw_menu_popup_create(list, "Font Menu", x, y, parent);
            gw_window_set_handlers(menu, 0, 0, gw_console_popup_on_destroy);
            gw_window_set_slot(menu, 0, list);
        }
    }
}

/* Starts scrolling back: shows the backlog, its top three lines before the newest. */
/* 0x00197480 */
static void gw_console_follow_on(int win, GwConsole *con)
{
    gw_window_replace_flags(win, gw_window_get_flags(win) & ~0x80);
    con->follow = 1;
    con->top = (con->head - 3) & (con->nlines - 1);
}

/* Stops scrolling back. */
/* 0x001974e0 */
static void gw_console_follow_off(int win, GwConsole *con)
{
    gw_window_replace_flags(win, gw_window_get_flags(win) | 0x80);
    con->follow = 0;
    gw_console_layout();
}

/* Page Up one line back (starts scrolling back), Page Down one forward, End stops. */
/* 0x00197520 */
static void gw_console_scroll_keys(int win)
{
    GwConsole *con;
    int bottom;

    con = &gw_console_state;
    if (gw_keyboard_state.key == 75) {
        con->top = (con->top - 1) & (con->nlines - 1);
        if (con->follow == 0)
            gw_console_follow_on(win, con);
    }
    if (gw_keyboard_state.key == 78) {
        bottom = (con->head - 3) & (con->nlines - 1);
        if (con->top == bottom || (con->top = (con->top + 1) & (con->nlines - 1)) == bottom)
            gw_console_follow_off(win, con);
    }
    if (con->follow) {
        if (gw_keyboard_state.key_press == 77)
            gw_console_follow_off(win, con);
    }
}

/* Lines up to the scrolled-back position. */
/* 0x00197620 */
static void gw_console_draw_backlog(int win)
{
    int cell[2];
    int old;

    gw_font_get_cell_size(gw_console_state.font, &cell[0], &cell[1]);
    old = gw_font_set(gw_console_state.font);
    gw_console_draw_lines(win, gw_console_state.top);
    gw_font_set(old);
}

/* The newest lines. */
/* 0x00197688 */
static void gw_console_draw_log(int win)
{
    int cell[2];
    int old;

    gw_font_get_cell_size(gw_console_state.font, &cell[0], &cell[1]);
    old = gw_font_set(gw_console_state.font);
    gw_console_draw_lines(win, gw_console_state.head);
    gw_font_set(old);
}

/* 0x001976f0 */
static void gw_str_cut_newline(char *s)
{
    char c;

    while (*s) {
        c = *(unsigned char *)s;
        if (c == 10) {
            *s = 0;
            return;
        }
        s++;
    }
}

/* Cursor at its end. */
/* 0x00197730 */
static void gw_console_recall_history(GwConsole *con)
{
    char *entry;

    entry = (char *)con->hist[con->hist_idx];
    if (entry) {
        strcpy((char *)&con->line, entry);
        gw_str_cut_newline((char *)&con->line);
        con->line.cursor = strlen((char *)&con->line);
    }
}

/*
 * Enter: echo to the log, record in the 64-entry history, run, clear. Up/Down (no modifiers) step
 * the history. Entries point at the echoed log text, so live only as long as that line.
 */
/* 0x00197790 */
static void gw_console_command_input(void *win)
{
    GwConsole *con;
    GwLine *line;
    int i;
    int n;

    con = &gw_console_state;
    line = &con->line;
    if (gw_line_edit_key(line) == 2) {
        gw_console_printf(">%s\n", line);
        i = con->hist_next;
        ((char **)con->hist)[i] = gw_console_last_text + 1;
        con->hist_idx = i;
        n = (i + 1) & 63;
        con->hist_next = n;
        gw_command_exec(line);
        gw_line_clear(line);
    }
    if (gw_keyboard_state.mods == 0) {
        if (gw_keyboard_state.key_press == 82) {
            gw_console_recall_history(con);
            con->hist_idx = (con->hist_idx - 1) & 63;
            if (((char **)con->hist)[con->hist_idx] == 0)
                con->hist_idx = (con->hist_idx + 1) & 63;
        }
        if (gw_keyboard_state.key_press == 81) {
            if (con->hist_idx != ((con->hist_next - 1) & 63))
                con->hist_idx = (con->hist_idx + 1) & 63;
            gw_console_recall_history(con);
        }
    }
}

/* Translucent cursor block while the cursor column is inside the window. */
/* 0x001978c0 */
static void gw_console_draw_command(void *win)
{
    GwConsole *con;
    int cw;
    int ch;
    int ww;
    int wh;
    int x;
    int y;
    int old;
    float z;

    con = &gw_console_state;
    old = gw_font_set(con->font);
    x = 0;
    y = 0;
    gw_font_get_cell_size(con->font, &cw, &ch);
    gw_window_get_size(win, &ww, &wh);
    gw_win_printf(win, x, y, (char *)&con->line);
    if (cw * (con->line.cursor + 1) < ww) {
        z = gw_float_bits_add(gw_window_get_scale(win), 1);
        gw_window_get_abs_pos(win, &x, &y);
        x += con->line.cursor * cw;
        gw_console_fill_rect(0x38643c3c, z, (float)x, (float)y, (float)cw, (float)ch);
    }
    gw_font_set(old);
}

/*
 * Shift + arrows resize (left/right width, up grows, down shrinks from the top); then layout and
 * input.
 */
/* 0x001979e8 */
static void gw_console_update(void *win)
{
    GwConsole *con;
    int x;
    int y;
    int fw;
    int fh;

    con = &gw_console_state;
    if (gw_window_has_active(win)) {
        if (gw_keyboard_state.mods & 0x22) {
            gw_window_get_pos(con->frame, &x, &y);
            gw_window_get_size(con->frame, &fw, &fh);
            if (gw_keyboard_is_down(80))
                fw -= 4;
            if (gw_keyboard_is_down(79))
                fw += 4;
            if (gw_keyboard_is_down(82))
                fh += 4;
            if (gw_keyboard_is_down(81)) {
                y += 4;
                fh -= 4;
            }
            gw_window_set_pos(con->frame, x, y);
            gw_window_set_size(con->frame, fw, fh);
        }
        gw_console_layout();
        gw_console_popup_menu(con->log_win);
        gw_console_scroll_keys(con->backlog_win);
        gw_console_popup_menu(con->backlog_win);
        gw_console_command_input(con->command_win);
        gw_console_popup_menu(con->command_win);
    }
}

/* 0x00197b00 */
static void gw_console_window_draw(void)
{
}

/*
 * Opens at the saved rectangle with its children and the saved scroll state; menu entry ticked
 * while open.
 */
/* 0x00197b08 */
static void gw_console_toggle(int visible)
{
    GwConsole *con;

    con = &gw_console_state;
    if (con->frame) {
        gw_window_close(con->frame);
        con->frame = 0;
        con->visible = 0;
        gw_item_set_value((void *)con->menu, 0);
    } else {
        con->frame = gw_window_open("Console", 0x1004b, con->x, con->y, con->w, con->h, 0);
        gw_window_set_handlers(con->frame, gw_console_update, gw_console_window_draw, 0);
        con->backlog_win = gw_window_open("Console Backlog", 0x401a8, 0, 0, 0, 0, con->frame);
        gw_window_set_handlers(con->backlog_win, 0, gw_console_draw_backlog, 0);
        con->log_win = gw_window_open("Console Log", 0x40128, 0, 0, 0, 0, con->frame);
        gw_window_set_handlers(con->log_win, 0, gw_console_draw_log, 0);
        con->command_win = gw_window_open("Console Command", 0x40128, 0, 0, 0, 0, con->frame);
        gw_window_set_handlers(con->command_win, 0, gw_console_draw_command, 0);
        gw_console_layout();
        con->visible = 1;
        gw_item_set_value((void *)con->menu, 1);
        if (con->follow)
            gw_console_follow_on(con->backlog_win, con);
    }
}

/* 0x00197ca8 */
static void gw_console_line_release(int line)
{
}

/* Clears its line table slot. */
/* 0x00197cb0 */
static void gw_console_drop_line(int *slot)
{
    gw_console_line_release(*slot);
    *slot = 0;
}

/* Default colour. */
/* 0x00197ce0 */
static void gw_console_line_init(GwConLine *line)
{
    line->attr = 0x09111111;
    line->len = 0;
    line->text[0] = 0;
}

/* Drops the oldest lines starting inside [off, off + size). Returns whether it fits the buffer. */
/* 0x00197cf8 */
static int gw_console_make_room(int off, unsigned size)
{
    GwConsole *con;
    GwConLine **tbl;
    char *start;
    char *end;
    GwConLine *line;
    int i;

    con = &gw_console_state;
    size = off + ((size + 15) & 0xfffffff0);
    i = con->tail;
    tbl = gw_console_lines;
    start = gw_console_buf + off;
    end = gw_console_buf + size;
    while ((line = tbl[i]) != 0 && !((char *)line < start) && (char *)line < end) {
        gw_console_drop_line(&tbl[i]);
        i = (i + 1) & (con->nlines - 1);
        tbl = gw_console_lines;
    }
    con->tail = i;
    return end < gw_console_buf + con->bufsize;
}

/*
 * Each newline completes the current line in the current colour; drops old lines for room and
 * wraps at the buffer end. Serial output measures the string but sends nothing.
 */
/* 0x00197dd8 */
static void gw_console_print(char *s)
{
    char *end;
    int step;
    int n;
    int more;
    int len;
    GwConsole *con;
    char *text;
    GwConLine *rec;
    int i;
    int need;
    int nl;

    con = &gw_console_state;
    if (con->serial_out) {
        end = s;
        if (*s) {
            do {
                end++;
            } while (*end);
        }
    }
    if (gw_console_buf == 0)
        return;
    nl = 10;
    do {
        more = 1;
        len = gw_line_length(s);
        if (s[len] != nl) {
            if (len == 0)
                return;
            more = 0;
        }
        rec = (GwConLine *)(gw_console_buf + con->used);
        step = rec->len;
        need = len + 12;
        n = step + need;
        if (!gw_console_make_room(con->used, n)) {
            rec = (GwConLine *)gw_console_buf;
            con->used = 0;
            gw_console_line_init(rec);
            gw_console_make_room(con->used, n);
            con->head = con->count = (con->count + 1) & (con->nlines - 1);
        }
        do {
            i = (con->count + 1) & (con->nlines - 1);
            gw_console_drop_line(&gw_console_lines[i]);
        } while (0);
        i = con->tail;
        if (gw_console_lines[i] == 0) {
            do {
                i = (i + 1) & (con->nlines - 1);
            } while (gw_console_lines[i] == 0);
        }
        n = step + len;
        step = len + 1;
        text = rec->text;
        con->tail = i;
        strcat(text, s);
        rec->len = n;
        gw_console_last_text = text;
        rec->attr = con->attr;
        gw_console_lines[con->count] = rec;
        if (more) {
            int new_used = (con->used + n + 27) & 0xfffffff0;

            con->used = new_used;
            if (!gw_console_make_room(new_used, 12)) {
                con->used = 0;
                gw_console_make_room(0, 12);
            }
            rec = (GwConLine *)(gw_console_buf + con->used);
            gw_console_line_init(rec);
            con->head = con->count;
            con->count = (con->count + 1) & (con->nlines - 1);
            gw_console_lines[con->count] = rec;
        }
        s += step;
    } while (more);
}
