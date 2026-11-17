/*
 * Message consoles: scrolling text windows reporting game events. Characters pop in large and
 * settle; lines scroll on the beat; the ConsoleCom task owns two console lists.
 */

#include "types_game.h"
#include "debug.h"
#include "libc.h"
#include "billboard.h"
#include "callback.h"
#include "config.h"
#include "console.h"
#include "float_bits.h"
#include "heap.h"
#include "node_draw_state.h"
#include "player_status.h"
#include "sound_beat.h"
#include "system.h"
#include "screen.h"
#include "color.h"

typedef struct SecObj {
    char pad_00[6];
    short cnt;              /* 0x06 */
    char pad_08[8];
    int flags;              /* 0x10 */
} SecObj;

extern void console_clear(ConsoleCom *con);
extern ConsoleFontInfo console_fonts[];
extern void console_cell_age(ConsoleCell *cell);
extern void font_set(int font);
extern void *console_draw_cell(ConsoleCell *cell, void *buf, float x, float y, float sx);
extern float screen_bottom[];
extern void console_com_init(ConsoleCom *con, int cols, int rows, int mode);
/* Target of a NULL console argument. */
extern ConsoleCom *console_cur;

extern void console_free(ConsoleCom *con);

/* ConsoleCom task work: the two console lists. */
extern ConsoleSys *console_sys;

extern ConsoleCom *console_create(ConsoleCom **head, int cols, int rows, int mode);
extern void console_destroy(ConsoleCom **head, ConsoleCom *con);
extern void console_set_cursor(ConsoleCom *con, int x, int y);

extern void console_clear_line(ConsoleCom *con);

extern void console_scroll(ConsoleCom *con);

extern ConsoleCom *console_create_fixed(int cols, int rows);
extern void console_set_pos(ConsoleCom *con, float x, float y);
extern void console_set_font_mode(ConsoleCom *con, int mode);
extern void console_set_panel(ConsoleCom *con, int on);
extern void console_events_register(void);
extern float screen_layout[];
extern void console_events_unregister(void);
extern void console_destroy_fixed(ConsoleCom *con);

extern void console_update_cells(ConsoleCom *con);

extern void console_follow(ConsoleCom *parent, ConsoleCom *con);
extern void console_draw(ConsoleCom *con, float sx);
extern struct TaskDesc console_com_desc;

extern void console_set_mode(ConsoleCom *con, int mode);

extern char *console_node_names[4];
extern char *console_level_msgs[2];
extern char *console_form_names[8];
extern char *console_fire_names[9];
extern int game_flags;
extern ConEvent console_events[8];
extern int console_event_handles[8];
extern void console_cell_clear(short *cell);
extern void console_cell_set(short *cell, short ch, short age);
extern void console_newline();
extern int console_puts(ConsoleCom *con, const char *str, unsigned flag);

/* 0x0010cb30 */
void console_cell_clear(short *cell)
{
    cell[0] = 0;
    cell[1] = 0;
}

/* 0x0010cb40 */
void console_cell_set(short *cell, short ch, short age)
{
    cell[0] = ch;
    cell[1] = age;
}

/* Settles at age 10. */
/* 0x0010cb50 */
void console_cell_age(ConsoleCell *cell)
{
    if (cell->ch != 0 && cell->age < 10)
        cell->age++;
}

/* First frame: block glyph (127); then taller, normal by age 10. Returns the next font buffer. */
/* 0x0010cb80 */
void *console_draw_cell(ConsoleCell *cell, void *buf, float x, float y, float sx)
{
    float sy;
    int ch;

    if (cell->ch != 0 && cell->age > 0) {
        if (cell->age < 2) {
            ch = 127;
            sy = 1.0f;
        } else {
            ch = cell->ch;
            sy = (float)(10 - cell->age) * 0.5f / 10.0f + 1.0f;
        }
        buf = font_put_char_scaled(buf, x, y, sx, ch, sy);
    }
    return buf;
}

/* Row font modes reset to the console's. */
/* 0x0010cc18 */
void console_clear(ConsoleCom *con)
{
    int n;
    ConsoleCell *p;
    int *row;

    n = con->cols * con->nrows;
    if (n > 0) {
        p = con->cells;
        do {
            console_cell_clear(p);
            n--;
            p++;
        } while (n != 0);
    }
    if (con->nrows > 0) {
        n = con->nrows;
        row = con->rows;
        do {
            *row = con->mode;
            n--;
            row++;
        } while (n != 0);
    }
}

/* Cursor to end of row. */
/* 0x0010cca8 */
void console_clear_line(ConsoleCom *con)
{
    int n;
    ConsoleCell *p;

    n = con->cols - con->cur_x;
    if (n > 0) {
        p = &con->cells[con->cur_y * con->cols + con->cur_x];
        do {
            console_cell_clear(p);
            n--;
            p++;
        } while (n != 0);
    }
}

/* Character advance from console_fonts. */
/* 0x0010cd10 */
void console_com_init(ConsoleCom *con, int cols, int rows, int mode)
{
    int n = cols * rows;

    if (cols <= 0 || rows <= 0)
        con->cells = 0;
    else
        con->cells = heap_block_tag(game_malloc(n * 4), "console_com.c", 337);
    if (rows <= 0)
        con->rows = 0;
    else
        con->rows = heap_block_tag(game_malloc(rows * 4), "console_com.c", 343);
    con->mode = mode;
    con->cols = cols;
    con->size_x = cols;
    con->nrows = rows;
    con->size_y = rows;
    con->cur_x = 0;
    con->cur_y = 0;
    con->pos_x = 0;
    con->pos_y = 0;
    con->col = COL_WHITE;
    con->panel_col = console_get_pcolp();
    con->panel_col2 = RGBA(0, 0, 0, 0x5a);
    con->panel = 1;
    con->next = 0;
    console_clear(con);
    {
        float adv_x = console_fonts[mode].adv_x;
        float adv_y = console_fonts[mode].adv_y;

        con->adv_x = adv_x;
        con->adv_y = adv_y;
    }
}

/* 0x0010ce38 */
void console_free(ConsoleCom *con)
{
    heap_free(con->cells, 0);
    heap_free(con->rows, 0);
}

/* 0x0010ce70 */
void console_scroll(ConsoleCom *con)
{
    ConsoleCell *p;
    int *row;
    int i;
    int j;
    int n;

    p = 0;
    i = con->nrows - 1;
    if (i > 0) {
        p = con->cells;
        do {
            j = con->cols;
            do {
                *p = p[con->cols];
                j--;
                p++;
            } while (j != 0);
            i--;
        } while (i != 0);
    }
    j = con->cols;
    do {
        console_cell_clear(p);
        j--;
        p++;
    } while (j != 0);
    if (con->cur_y > 0)
        con->cur_y--;
    n = con->nrows - 1;
    if (n > 0) {
        row = con->rows;
        do {
            *row = row[1];
            n--;
            row++;
        } while (n != 0);
    }
}

/* 0x0010cf58 */
void console_update_cells(ConsoleCom *con)
{
    int n;
    ConsoleCell *p;

    n = con->cols * con->nrows;
    p = con->cells;
    if (n > 0) {
        do {
            console_cell_age(p);
            n--;
            p++;
        } while (n != 0);
    }
}

/* 1 px per frame: first console to the screen bottom, others 16 px right of and above the last. */
/* 0x0010cfa8 */
void console_follow(ConsoleCom *parent, ConsoleCom *con)
{
    float tx;
    float ty;
    float v;

    if (parent == 0) {
        tx = screen_cx - floorf(con->adv_x * 10.0f);
        ty = (*(float *)&screen_bottom) - (con->adv_y * (float)con->nrows + 10.0f); /* steer(cast) */
    } else {
        tx = parent->pos_x + 16.0f;
        ty = parent->pos_y - 16.0f;
    }
    v = con->pos_x;
    if (v < tx)
        con->pos_x = v + 1.0f;
    else if (tx < v)
        con->pos_x = v - 1.0f;
    v = con->pos_y;
    if (v < ty)
        con->pos_y = v + 1.0f;
    else if (ty < v)
        con->pos_y = v - 1.0f;
}

/* Panel if any, then each visible row in its font. */
/* steer(cast) */
/* 0x0010d0b0 */
void console_draw(ConsoleCom *con, float sx)
{
    ConsoleCell *cell;
    int *rowp;
    int rows;
    int n;
    int drawn;
    float x;
    float y;
    float adv;
    void *buf;

    draw_set_blend(4, 5);
    if (con->panel != 0)
        rect_draw_outline(con->pos_x, con->pos_y, sx, con->adv_x * (float)con->size_x, con->adv_y * (float)con->size_y,
                          con->panel_col, con->panel_col2);
    sx = float_add_bits16(1, sx);
    rows = con->size_y;
    rowp = con->rows;
    cell = con->cells;
    if (rows > 0) {
        y = con->pos_y;
        do {
            font_set(console_fonts[*rowp].font);
            buf = font_begin_current(con->col);
            n = con->size_x;
            adv = console_fonts[*rowp].adv_x;
            drawn = n;
            if (n > 0) {
                x = con->pos_x;
                do {
                    buf = console_draw_cell(cell, buf, x, y, sx);
                    cell++;
                    x += adv;
                    n--;
                } while (n != 0);
                drawn = con->size_x;
            }
            n = con->cols - drawn;
            if (n > 0) {
                do {
                    cell++;
                    n--;
                } while (n != 0);
            }
            font_end_current(buf);
            y += con->adv_y;
            rowp++;
        } while (--rows != 0);
    }
}

/* First console starts at the screen bottom, later ones beside the last. */
/* 0x0010d278 */
ConsoleCom *console_create(ConsoleCom **head, int cols, int rows, int mode)
{
    ConsoleCom *con;
    ConsoleCom *p;
    float tx;
    float ty;

    con = heap_block_tag(game_malloc(72), "console_com.c", 593);
    console_com_init(con, cols, rows, mode);
    if (*head == 0) {
        *head = con;
        tx = screen_cx - floorf(con->adv_x * 10.0f);
        con->pos_x = tx;
        ty = screen_bottom[0] - (con->adv_y * (float)con->nrows + 10.0f);
        con->pos_y = ty;
    } else {
        p = *head;
        while (p->next != 0)
            p = p->next;
        p->next = con;
        con->pos_x = p->pos_x + 16.0f;
        con->pos_y = p->pos_y - 16.0f;
    }
    return con;
}

/* 0x0010d3b0 */
void console_destroy(ConsoleCom **head, ConsoleCom *con)
{
    ConsoleCom **pp;

    if (con == 0)
        con = console_cur;
    for (pp = head; *pp != 0; pp = &(*pp)->next) {
        if (*pp == con) {
            *pp = con->next;
            console_free(con);
            heap_free(con, 0);
            break;
        }
    }
}

/* Font mode 1. NULL while the ConsoleCom task is not running. */
/* 0x0010d410 */
ConsoleCom *console_create_follow(int cols, int rows)
{
    if (console_sys == 0)
        return 0;
    return console_create(&console_sys->list_b, cols, rows, 1);
}

/* Font mode 1. NULL while the ConsoleCom task is not running. */
/* 0x0010d450 */
ConsoleCom *console_create_fixed(int cols, int rows)
{
    if (console_sys == 0)
        return 0;
    return console_create(&console_sys->list_a, cols, rows, 1);
}

/* 0x0010d490 */
void console_destroy_follow(ConsoleCom *con)
{
    if (console_sys != 0)
        console_destroy(&console_sys->list_b, con);
}

/* 0x0010d4b8 */
void console_destroy_fixed(ConsoleCom *con)
{
    if (console_sys != 0)
        console_destroy(&console_sys->list_a, con);
}

/* Also takes that font's spacing. */
/* 0x0010d4e0 */
void console_set_font_mode(ConsoleCom *con, int mode)
{
    float adv_x;
    float adv_y;

    if (con == 0)
        con = console_cur;
    con->mode = mode;
    adv_x = console_fonts[mode].adv_x;
    adv_y = console_fonts[mode].adv_y;
    con->adv_x = adv_x;
    con->adv_y = adv_y;
}

/* For new text; spacing unchanged. */
/* 0x0010d520 */
void console_set_mode(ConsoleCom *con, int mode)
{
    if (con == 0)
        con = console_cur;
    con->mode = mode;
}

/* 0x0010d538 */
void console_set_pos(ConsoleCom *con, float x, float y)
{
    if (con == 0)
        con = console_cur;
    con->pos_x = x;
    con->pos_y = y;
}

/* 0x0010d558 */
void console_get_pos(ConsoleCom *con, float *px, float *py)
{
    if (con == 0)
        con = console_cur;
    *px = con->pos_x;
    *py = con->pos_y;
}

static inline void console_window_clamp(ConsoleCom *con, int width, int height)
{
    if (con == 0)
        con = console_cur;
    if (width < 0)
        width = 0;
    if (height < 0)
        height = 0;
    if (width >= con->cols)
        width = con->cols;
    if (height >= con->nrows)
        height = con->nrows;
    con->size_x = width;
    con->size_y = height;
}

/* Clamped to the cells. */
/* 0x0010d580 */
void console_set_window(ConsoleCom *con, int width, int height)
{
    console_window_clamp(con, width, height);
}

/* Cells. */
/* 0x0010d5d8 */
void console_get_size(ConsoleCom *con, int *px, int *py)
{
    if (con == 0)
        con = console_cur;
    *px = con->size_x;
    *py = con->size_y;
}

/* Pixels. */
/* 0x0010d600 */
void console_get_pixel_size(ConsoleCom *con, int *pw, int *ph)
{
    if (con == 0)
        con = console_cur;
    *pw = (int)((float)con->size_x * con->adv_x);
    *ph = (int)((float)con->size_y * con->adv_y);
}

/* 0x0010d640 */
void console_set_panel(ConsoleCom *con, int on)
{
    if (con == 0)
        con = console_cur;
    con->panel = on;
}

/* 0x0010d658 */
void console_clear_home(ConsoleCom *con)
{
    ConsoleCom *cur = con;

    if (cur == 0)
        cur = console_cur;
    console_clear(cur);
    console_set_cursor(con, 0, 0);
}

/* 0x0010d698 */
void console_clear_eol(ConsoleCom *con)
{
    if (con == 0)
        con = console_cur;
    console_clear_line(con);
}

/* 0x0010d6b0 */
void console_set_cursor(ConsoleCom *con, int x, int y)
{
    if (con == 0)
        con = console_cur;
    con->cur_x = x;
    con->cur_y = y;
}

/* 0x0010d6d0 */
void console_get_cursor(ConsoleCom *con, int *px, int *py)
{
    if (con == 0)
        con = console_cur;
    *px = con->cur_x;
    *py = con->cur_y;
}

/* Scrolls while below the last row. */
/* 0x0010d6f8 */
void console_newline(ConsoleCom *con)
{
    if (con == 0)
        con = console_cur;
    con->cur_x = 0;
    con->cur_y++;
    while (con->cur_y >= con->nrows)
        console_scroll(con);
}

/* 0x0010d760 */
void console_set_color(ConsoleCom *con, int col)
{
    if (con == 0)
        con = console_cur;
    con->col = col;
}

/* 0x0010d778 */
void console_set_panel_colors(ConsoleCom *con, int col_a, int col_b)
{
    if (con == 0)
        con = console_cur;
    con->panel_col = col_a;
    con->panel_col2 = col_b;
}

/* On the current console (NULL) also restarts the beat countdown to auto-scroll. */
/* 0x0010d798 */
void console_reset_cursor_x(ConsoleCom *con)
{
    if (con == 0) {
        con = console_cur;
        console_sys->mode = 8;
    }
    con->cur_x = 0;
}

/* Flags 1, 2: pop in (age 0); else settled (age 10). Returns the pop-in frames. */
/* 0x0010d7c0 */
int console_putc(ConsoleCom *con, int ch, unsigned flag)
{
    ConsoleCom *cur = con;
    ConsoleCell *cell;
    int *row;
    int age;

    if (cur == 0) {
        cur = console_cur;
        console_sys->mode = 8;
    }
    switch (flag) {
    case 0:
        age = 10;
        break;
    case 1:
    case 2:
        age = 0;
        break;
    default:
        age = 10;
        break;
    }
    if (cur->cur_y < cur->nrows && cur->cur_x < cur->cols) {
        cell = &cur->cells[cur->cur_y * cur->cols + cur->cur_x];
        row = &cur->rows[cur->cur_y];
        if (ch == 10) {
            console_newline(con);
        } else {
            ((void (*)(ConsoleCell *, int, int))console_cell_set)(cell, ch, age); /* steer(cast) */
            *row = cur->mode;
        }
        cur->cur_x++;
    }
    return 10 - age;
}

/*
 * Flag 1: all pop in at once; 2: typed out, 2 frames apart; else settled. Returns frames until the
 * last character settles.
 */
/* 0x0010d8b0 */
int console_puts(ConsoleCom *con, const char *str, unsigned flag)
{
    ConsoleCom *cur;
    ConsoleCell *cell;
    int *row;
    int age;
    int step;
    char ch;
    int x;

    switch (flag) {
    case 0:
    default:
        age = 10;
        step = 0;
        break;
    case 1:
        age = 0;
        step = 0;
        break;
    case 2:
        age = 0;
        step = -2;
        break;
    }
    if (con == 0)
        console_sys->mode = 8;
    cur = con;
    if (cur == 0)
        cur = console_cur;
    if (cur->cur_y < cur->nrows) {
        cell = &cur->cells[cur->cur_y * cur->cols + cur->cur_x];
        row = &cur->rows[cur->cur_y];
        ch = *str++;
        while (ch != 0) {
            if (ch == 10) {
                console_newline(con);
                cell = &cur->cells[cur->cur_y * cur->cols + cur->cur_x];
                row = &cur->rows[cur->cur_y];
            } else if (cur->cur_x < cur->cols) {
                ((void (*)(ConsoleCell *, int, int))console_cell_set)(cell, ch, age); /* steer(cast) */
                cell++;
                x = cur->cur_x;
                *row = cur->mode;
                cur->cur_x = x + 1;
            }
            ch = *str++;
            age += step;
        }
    }
    return 10 - age;
}

/* Main console 20 x 9, font mode 2, no panel; event handlers; display list 13. */
/* 0x0010da40 */
void console_com_task_init(ConsoleSys *work)
{
    DEBUG_PRINTF("ConsoleCom Init\n");
    console_sys = work;
    console_cur = console_create_fixed(20, 9);
    console_set_pos(0, screen_layout[6] + 0.0f, screen_layout[8] + 0.0f);
    console_set_font_mode(0, 2);
    console_set_panel(0, 0);
    console_events_register();
    task_set_dlist(work, 13);
}

/* 0x0010dad8 */
void console_com_task_release(ConsoleSys *work)
{
    console_sys = 0;
    DEBUG_PRINTF("ConsoleCom Exit\n");
    console_events_unregister();
    console_destroy_fixed(console_cur);
    console_cur = 0;
    while (work->list_b != 0)
        console_destroy(&work->list_b, work->list_b);
    while (work->list_a != 0)
        console_destroy(&work->list_a, work->list_a);
}

/* Each beat counts work->mode down; at 0 the main console scrolls and the count restarts at 2. */
/* 0x0010db70 */
void console_com_task_exec(ConsoleSys *work)
{
    ConsoleCom *con;
    ConsoleCom *prev;

    prev = 0;
    con = work->list_b;
    if (con != 0) {
        do {
            console_update_cells(con);
            console_follow(prev, con);
            prev = con;
            con = con->next;
        } while (con != 0);
    }
    con = work->list_a;
    if (con != 0) {
        do {
            console_update_cells(con);
            con = con->next;
        } while (con != 0);
    }
    if (beat_grid_hit(0)) {
        if (--work->mode <= 0) {
            work->mode = 2;
            console_newline(0);
        }
    }
}

/* Following, then fixed consoles, each further back. Main console hidden by status 0x80000000. */
/* steer(cast) */
/* 0x0010dc38 */
void console_com_task_disp(ConsoleSys *work)
{
    ConsoleCom *con;
    float s;

    draw_set_blend(4, 5);
    s = 2.2222223f;
    draw_set_fc_mode(8192);
    con = work->list_b;
    if (con != 0) {
        do {
            console_draw(con, s);
            s = float_add_bits16(5, s);
            con = con->next;
        } while (con != 0);
    }
    con = work->list_a;
    while (con != 0) {
        if (con == console_cur && ((int (*)(unsigned))player_status_test_flags)(0x80000000)) {
            con = con->next;
        } else {
            console_draw(con, s);
            s = float_add_bits16(5, s);
            con = con->next;
        }
    }
}

/* 0x0010dd18 */
void *console_com_task_create(void *parent)
{
    return task_create(parent, &console_com_desc, 12);
}

/* "Unknown" when nameless; own line, font mode 0. */
/* 0x0010dd40 */
void console_msg_name(int **pp)
{
    int *obj = *pp;
    const char *name;

    if (obj != 0) {
        name = (const char *)obj[6];
        if (name == 0)
            name = "Unknown";
        console_set_mode(0, 0);
        console_puts(0, name, 2);
        console_newline(0);
    }
}

/* "System / Overload Engaged". */
/* 0x0010dda8 */
void console_msg_overload(void)
{
    console_set_mode(0, 1);
    console_puts(0, "System\nOverload Engaged\n", 2);
}

/* "Cracking... / Security Attack" when cnt > 0 and kind (flags bits 3..5) is 3. */
/* 0x0010ddd8 */
void console_msg_security_attack(SecObj **pp)
{
    SecObj *obj = *pp;

    if (obj->cnt > 0) {
        switch ((obj->flags & 0x38) >> 3) {
        case 1:
            break;
        case 3:
            console_set_mode(0, 1);
            console_puts(0, "Cracking...\nSecurity Attack\n", 2);
            break;
        }
    }
}

/* "Security breached / Analyzing Data nnn%". */
/* 0x0010de40 */
void console_msg_analyzing(void)
{
    char buf[112];
    int pct;

    pct = (int)ConfigStageAnalyzeRate();
    sprintf(buf, "Security breached\nAnalyzing Data %3d%%\n", pct);
    console_set_mode(0, 2);
    console_puts(0, buf, 2);
}

/* Name for node types 0..3, then "Downloading... / Updating Schematics". */
/* 0x0010de90 */
void console_msg_node(int *node)
{
    char *name;

    switch (*node) {
    case 0:
        name = console_node_names[0];
        break;
    case 1:
        name = console_node_names[1];
        break;
    case 2:
        name = console_node_names[2];
        break;
    case 3:
        name = console_node_names[3];
        break;
    default:
        name = "";
        break;
    }
    if (name != 0) {
        console_set_mode(0, 1);
        console_puts(0, name, 2);
        console_puts(0, "Downloading...\nUpdating Schematics\n", 2);
    }
}

/* "Level Up"/"Level Down" (p[0] old form, p[1] new), then the new form's name. */
/* 0x0010df58 */
void console_msg_level(int *p)
{
    char *msg;
    char **names;

    if (p[0] <= p[1])
        msg = console_level_msgs[0];
    else
        msg = console_level_msgs[1];
    console_set_mode(0, 2);
    console_puts(0, msg, 2);
    names = console_form_names;
    console_puts(0, names[p[1]], 2);
}

/* Fire level p[6] (0..8), font mode 0. */
/* 0x0010dfe0 */
void console_msg_fire(int *p)
{
    unsigned lv = p[6];
    char *name;

    if (lv < 9) {
        name = console_fire_names[lv];
        console_set_mode(0, 0);
        console_puts(0, name, 2);
    }
}

/*
 * Only with game_flags 0x40: optional title p[0], "100% Annihilation / Receiving Bonus... / n
 * points" with bonus p[1].
 */
/* 0x0010e048 */
void console_msg_bonus(int *p)
{
    char buf[64];

    if (game_flags & 0x40) {
        console_set_mode(0, 1);
        if (p[0] != 0) {
            console_puts(0, (const char *)p[0], 2);
            console_newline(0);
        }
        sprintf(buf, "100%% Annihilation\nReceiving Bonus...\n%d points\n", p[1]);
        console_puts(0, buf, 2);
    }
}

/* The eight handlers of console_events. */
/* 0x0010e0c8 */
void console_events_register(void)
{
    unsigned i;

    for (i = 0; i < 8; i++)
        console_event_handles[i] = listener_add(console_events[i].id, console_events[i].fn);
}

/* 0x0010e130 */
void console_events_unregister(void)
{
    unsigned i;

    for (i = 0; i < 8; i++) {
        listener_remove(console_events[i].id, console_event_handles[i]);
        console_event_handles[i] = 0;
    }
}
