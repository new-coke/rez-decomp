/* Debug GUI memory dump: 'Memory Dump Window', bytes/halfwords/words/floats plus ASCII ('dump'). */

#include "types_gw.h"
#include "ul_fs.h"
#include "libc.h"
#include "gw_dump.h"
#include "gw_button.h"
#include "gw_choicebox.h"
#include "gw_draw.h"
#include "gw_heap.h"
#include "gw_system.h"
#include "gw_textfield.h"
#include "gw_window.h"

static GwWin *gw_dump_window_create(unsigned addr, unsigned end, int x, int y, int width, int height, GwWin *parent);

extern unsigned char gw_keyboard_state[];

/* 'dump [hex address [hex length]]'; length parsed but unused. 0 ok, -1 failed. */
/* 0x0019db20 */
static int gw_dump_command(int argc, char **argv)
{
    unsigned addr = 0x8c000000;
    GwWin *win;
    int ret = -1;

    if (argc == 2) {
        addr = gw_parse_hex(argv[1]);
    } else if (argc >= 3) {
        addr = gw_parse_hex(argv[1]);
        gw_parse_hex(argv[2]);
    }
    win = gw_dump_window_create(addr, 0, -1, -1, 0, 0, 0);
    if (win)
        ret = 0;
    return ret;
}

/* 'Dump Type' choice box. */
/* 0x0019dbb0 */
static void gw_dump_set_type(GwWin *win, int type)
{
    GwDump *dump = gw_window_get_user(gw_window_get_parent(win));

    dump->type = type;
}

/* '> <' toggle: 8 bytes per row while on, else 16. */
/* 0x0019dbe0 */
static void gw_dump_width_button(GwBtn *btn)
{
    GwDump *dump = gw_window_get_user(gw_window_get_parent((GwWin *)btn));

    if (gw_toggle_button_get(btn))
        dump->stride = 8;
    else
        dump->stride = 16;
}

/* Non-hex characters count as 0 digits; null gives 0. */
/* 0x0019dc30 */
unsigned gw_parse_hex(char *s)
{
    unsigned sum = 0;
    unsigned place = 1;
    unsigned i;
    char *cp;
    char c;
    int digit, val, lower;

    if (s == 0)
        return 0;
    i = 0;
    cp = s + strlen(s) - 1;
    while (i < strlen(s)) {
        c = *cp;
        if ((_ctype_ + 1)[c] & 1)
            lower = c + 32;
        else
            lower = c;
        lower = (char)lower;
        digit = lower - '0';
        if ((unsigned char)digit < 10)
            val = digit;
        else
            val = ((unsigned char)(lower - 'a') > 5) ? 0 : lower - 'a' + 10;
        sum += val * place;
        cp--;
        place <<= 4;
        i++;
    }
    return sum;
}

/*
 * Only addresses in the Dreamcast main memory range are accepted (the 'dump' default is there too).
 */
/* 0x0019dd30 */
static void gw_dump_addr_entered(GwWin *win)
{
    GwDump *dump;
    unsigned addr;

    if (gwTextFieldIsDone(win)) {
        dump = gw_window_get_user(win->parent);
        addr = gw_parse_hex(gwTextFieldGetText(win));
        if (((addr & 0x0fffffff) + 0xf4000000) <= 0xffffff)
            dump->addr = addr;
    }
}

/* Up to 16; unprintable as '.'. */
/* 0x0019ddb0 */
static void gw_dump_draw_ascii(GwWin *win, int x, int y, char *data, int n)
{
    char buf[17];
    char *dst;
    int i;
    int cnt;
    int fits = n < 17;

    cnt = 16;
    if (fits)
        cnt = n;
    dst = buf;
    for (i = 0; i < cnt; i++) {
        char c = *data++;

        if (((signed char *)(_ctype_ + 1))[c] & 0x97)
            *dst = c;
        else
            *dst = '.';
        dst++;
    }
    *dst = 0;
    gw_win_text(win, x, y, buf);
}

/* Row: address, n bytes in hex, ASCII. */
/* 0x0019de30 */
static void gw_dump_draw_bytes(GwWin *win, int x, int y, unsigned char *data, int n)
{
    int cw, ch, rows, i, j, cur_x;
    unsigned char *row;
    int yy;
    int cs;                     /* cell width */

    gw_get_cell_size(&cw, &ch);
    rows = win->h / ch;
    yy = y;
    for (i = 0; i < rows; i++) {
        cur_x = x;
        row = data;
        gw_win_printf(win, cur_x, yy, "%08X", row);
        cs = cw;
        cur_x += cs * 9;
        for (j = 0; j < n; j++) {
            gw_win_printf(win, cur_x, yy, "%02X", *data++);
            cs = cw;
            cur_x += cs * 3;
        }
        ((void (*)(GwWin *, int, int, char *, int, int))gw_dump_draw_ascii)(win, cur_x, yy, (char *)row, n, cs); /* steer(cast) */
        yy += ch;
    }
}

/* Row: address, n / 2 halfwords in hex, ASCII. */
/* 0x0019df88 */
static void gw_dump_draw_words(GwWin *win, int x, int y, unsigned short *data, int n)
{
    int cw, ch, rows, i, j, cur_x;
    unsigned short *row;
    int yy;
    int cs;                     /* cell width */

    gw_get_cell_size(&cw, &ch);
    rows = win->h / ch;
    yy = y;
    for (i = 0; i < rows; i++) {
        cur_x = x;
        row = data;
        gw_win_printf(win, cur_x, yy, "%08X", row);
        cs = cw;
        cur_x += cs * 9;
        for (j = 0; j < (unsigned)n >> 1; j++) {
            gw_win_printf(win, cur_x, yy, "%04X", *data++);
            cs = cw;
            cur_x += cs * 5;
        }
        ((void (*)(GwWin *, int, int, char *, int, int))gw_dump_draw_ascii)(win, cur_x, yy, (char *)row, n, cs); /* steer(cast) */
        yy += ch;
    }
}

/* Row: address, n / 4 words in hex, ASCII. */
/* 0x0019e0e8 */
static void gw_dump_draw_longs(GwWin *win, int x, int y, unsigned *data, int n)
{
    int cw, ch, rows, i, j, cur_x;
    unsigned *row;
    int yy;
    int cs;                     /* cell width */

    gw_get_cell_size(&cw, &ch);
    rows = win->h / ch;
    yy = y;
    for (i = 0; i < rows; i++) {
        cur_x = x;
        row = data;
        gw_win_printf(win, cur_x, yy, "%08X", row);
        cs = cw;
        cur_x += cs * 9;
        for (j = 0; j < (unsigned)n >> 2; j++) {
            gw_win_printf(win, cur_x, yy, "%08X", *data++);
            cs = cw;
            cur_x += cs * 9;
        }
        ((void (*)(GwWin *, int, int, char *, int, int))gw_dump_draw_ascii)(win, cur_x, yy, (char *)row, n, cs); /* steer(cast) */
        yy += ch;
    }
}

/* Row: address, n / 4 floats (exponent form when too wide), ASCII. */
/* 0x0019e248 */
static void gw_dump_draw_floats(GwWin *win, int x, int y, float *data, int n)
{
    int cw, ch, rows, i, j, cur_x;
    float *row;
    int yy;
    char buf[128];
    int cs;                     /* cell width */

    gw_get_cell_size(&cw, &ch);
    rows = win->h / ch;
    yy = y;
    for (i = 0; i < rows; i++) {
        cur_x = x;
        row = data;
        gw_win_printf(win, cur_x, yy, "%08X", row);
        cs = cw;
        cur_x += cs * 9;
        for (j = 0; j < (unsigned)n >> 2; j++) {
            sprintf(buf, "% 9f", *data);
            if (strlen(buf) > 10)
                sprintf(buf, "% 9.3e", *data);
            gw_win_text(win, cur_x, yy, buf);
            data++;
            cs = cw;
            cur_x += cs * 11;
        }
        ((void (*)(GwWin *, int, int, char *, int, int))gw_dump_draw_ascii)(win, cur_x, yy, (char *)row, n, cs); /* steer(cast) */
        yy += ch;
    }
}

/* Address aligned to the current type. */
/* 0x0019e3f8 */
static void gw_dump_frame_draw(GwWin *win)
{
    GwDump *dump = gw_window_get_user(win->parent);

    switch (dump->type) {
    case 0:
        gw_dump_draw_bytes(win, 0, 0, dump->addr, dump->stride);
        break;
    case 1:
        gw_dump_draw_words(win, 0, 0, dump->addr & ~1u, dump->stride);
        break;
    case 2:
        gw_dump_draw_longs(win, 0, 0, dump->addr & ~3u, dump->stride);
        break;
    case 3:
        gw_dump_draw_floats(win, 0, 0, dump->addr & ~3u, dump->stride);
        break;
    }
}

/* Keeps it filling the dump window. */
/* 0x0019e4f8 */
static void gw_dump_frame_update(void *win)
{
    gw_window_size_to_parent(win, 2, 2);
}

/* Up/Down 16 bytes, Page Up/Down 256; Ctrl+B/W/L/F select BYTE/WORD/LONG/FLOAT. */
/* 0x0019e508 */
static void gw_dump_window_update(GwWin *win)
{
    GwDump *dump = gw_window_get_user(win);
    unsigned char key;

    if (gw_window_has_active(win)) {
        key = gw_keyboard_state[14];
        if (key == 82)
            dump->addr = dump->addr - 16;
        else if (key == 81)
            dump->addr = dump->addr + 16;
        else if (key == 75)
            dump->addr = dump->addr - 256;
        else if (key == 78)
            dump->addr = dump->addr + 256;
        if (gw_keyboard_state[16] & 0x11) {
            switch (gw_keyboard_state[13]) {
            case 5:
                dump->type = 0;
                break;
            case 26:
                dump->type = 1;
                break;
            case 15:
                dump->type = 2;
                break;
            case 9:
                dump->type = 3;
                break;
            }
        }
    }
}

/* 0x0019e618 */
static void gw_dump_window_close(GwWin *win)
{
    GwDump *dump = gw_window_get_user(win);

    if (dump) {
        gw_dc_free(dump, "gw_dump.c", 537);
        gw_window_set_user(win, 0);
    }
}

/*
 * View, address field, 'Dump Type' box, '> <' toggle. Zero size gives 56 x 10 chars. 0 on failure.
 */
/* 0x0019e670 */
static GwWin *gw_dump_window_create(unsigned addr, unsigned end, int x, int y, int width, int height, GwWin *parent)
{
    char buf[32];
    int coords[6];
    GwWin *win;
    GwDump *dump;
    GwWin *choice;

    gw_get_cell_size(&coords[0], &coords[1]);
    if (width <= 0)
        width = coords[0] * 56 + 4;
    if (height <= 0)
        height = coords[1] * 10;
    win = gw_window_open("Memory Dump Window", 0x1005f, x, y, width, height, parent);
    if (win == 0)
        return 0;
    gw_window_set_owner(win, 0);
    dump = gw_dc_calloc(1, 48, "gw_dump.c", 575);
    gw_window_set_user(win, dump);
    dump->addr = addr;
    dump->type = 0;
    if (end < addr)
        end = 0xffffffff;
    dump->end = end;
    dump->stride = 16;
    gw_window_set_handlers(win, gw_dump_window_update, 0, gw_dump_window_close);
    dump->frame = gw_window_open("dump frame", 0x4012c, 2, coords[1] * 2, width - 4, height - coords[1] * 2 - 4, win);
    gw_window_size_to_parent(dump->frame, 2, 2);
    gw_window_set_handlers(dump->frame, gw_dump_frame_update, gw_dump_frame_draw, 0);
    sprintf(buf, "%08X", dump->addr);
    dump->addr_edit = gwTextFieldCreate(buf, gw_dump_addr_entered, coords[0], coords[1] / 2, coords[0] * 10, coords[1], win);
    gw_window_get_pos(dump->addr_edit, &coords[2], &coords[3]);
    gw_window_get_size(dump->addr_edit, &coords[4], &coords[5]);
    coords[2] = coords[2] + (coords[4] + coords[0] * 2);
    choice = gw_choicebox_create("Dump Type", coords[2], coords[3], coords[0] * 8, coords[1], win);
    dump->choice = choice;
    gw_choicebox_add_item(choice, "BYTE");
    gw_choicebox_add_item(dump->choice, "WORD");
    gw_choicebox_add_item(dump->choice, "LONG");
    gw_choicebox_add_item(dump->choice, "FLOAT");
    gw_choicebox_set_sel(dump->choice, 0);
    gw_choicebox_set_callback(dump->choice, gw_dump_set_type);
    gw_window_get_pos(choice, &coords[2], &coords[3]);
    gw_window_get_size(choice, &coords[4], &coords[5]);
    coords[2] = coords[2] + (coords[4] + coords[0] * 2);
    dump->toggle = gw_toggle_button_create(gw_dump_width_button, "> <", coords[2], coords[3], 0, 0, win);
    return win;
}
