/*
 * Debug GUI keyboard: state record and the one-line editor for console and text fields. USB HID
 * key codes. No keyboard is read in this build, so the record stays empty.
 */

#include "types_gw.h"
#include "gw_keyboard.h"

extern GwKbd gw_keyboard_state;
extern int gw_kbd_repeat_start;
extern int gw_kbd_repeat_interval;

/* 0x001969b0 */
static void gw_keyboard_init(void)
{
}

/* 0x001969b8 */
static void gw_keyboard_exit(void)
{
}

/* Up to six keys held. */
/* 0x001969c0 */
int gw_keyboard_is_down(int key)
{
    unsigned char *down;
    int n;

    down = gw_keyboard_state.down;
    n = 6;
    do {
        if (*down++ == key)
            return 1;
    } while (--n);
    return 0;
}

/* Repeat after 10 frames, no interval. */
/* 0x001969f8 */
static void gw_keyboard_reset_repeat(void)
{
    gw_kbd_repeat_start = 10;
    gw_kbd_repeat_interval = 0;
}

/* 0x00196a08 */
static void gw_keyboard_clear(GwKbd *k)
{
    int i;

    for (i = 5; i >= 0; i--)
        k->down[i] = 0;
    k->unk_0c = 0;
    k->key_press = 0;
    k->key = 0;
    k->unk_0f = 0;
    k->mods = 0;
    k->unk_11 = 0;
    k->unk_12 = 0;
    k->ch = 0;
}

/* Always 0 in this build. */
/* 0x00196a50 */
static int gw_keyboard_is_present(void)
{
    return 0;
}

/* 0x00196a58 */
static void gw_keyboard_read(void)
{
}

/* 0x00196a60 */
static int gw_keyboard_return_zero(void)
{
    return 0;
}

/* 0x00196a68 */
static void gw_keyboard_update(void)
{
}

/* 0x00196a70 */
void gw_keyboard_poll(void)
{
    if (gw_keyboard_is_present()) {
        gw_keyboard_read();
        gw_keyboard_update();
    }
}

/* 127-character line; moves the rest right. */
/* 0x00196aa8 */
static void gw_line_insert_gap(char *buf, int pos)
{
    int i;

    for (i = 126; i != pos; i--)
        buf[i] = buf[i - 1];
}

/* Nothing at the end of the text. */
/* 0x00196ae0 */
static void gw_line_delete(char *buf, int pos)
{
    int i;

    if (buf[pos] && pos < 127) {
        i = pos;
        do {
            buf[i] = buf[i + 1];
            i++;
        } while (buf[i] && i < 127);
    }
}

/* 0x00196b28 */
void gw_line_clear(GwLine *line)
{
    line->text[0] = 0;
    line->text[126] = 0;
    line->cursor = 0;
}

/*
 * Character inserted at the cursor: 1; Enter: 2; Backspace, Delete, Left, Right edit. 0 if nothing.
 */
/* 0x00196b38 */
int gw_line_edit_key(GwLine *line)
{
    unsigned char code;
    unsigned char ch;

    code = gw_keyboard_state.key;
    ch = gw_keyboard_state.ch;
    line->status = 0;
    if (ch) {
        gw_line_insert_gap(line, line->cursor);
        line->text[line->cursor] = ch;
        if (line->cursor < 126)
            line->cursor++;
        line->status = 1;
    } else {
        switch (code) {
        case 80:
            if (line->cursor > 0)
                line->cursor--;
            break;
        case 79:
            if (line->text[line->cursor] && line->cursor < 126)
                line->cursor++;
            break;
        case 42:
            if (line->cursor > 0) {
                line->cursor--;
                gw_line_delete(line, line->cursor);
            }
            break;
        case 76:
            gw_line_delete(line, line->cursor);
            break;
        case 40:
            line->status = 2;
            break;
        }
    }
    return line->status;
}

/* Returns the old setting. */
/* 0x00196c50 */
static int gw_keyboard_set_serial_in(int enable)
{
    int old;

    old = gw_keyboard_state.serial_in;
    gw_keyboard_state.serial_in = enable;
    return old;
}

/* 0x00196c68 */
int gw_keyboard_get_inject_ch(void)
{
    return gw_keyboard_state.inject_ch;
}

/* 0x00196c78 */
int gw_keyboard_get_inject_key(void)
{
    return gw_keyboard_state.inject_key;
}
