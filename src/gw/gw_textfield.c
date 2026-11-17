/*
 * Debug GUI text fields: one line (up to 127 chars, optionally as '*'), blinking cursor, callbacks.
 */

#include "types_gw.h"
#include "libc.h"
#include "gw_textfield.h"
#include "gw_console.h"
#include "gw_draw.h"
#include "gw_keyboard.h"
#include "gw_mouse.h"
#include "gw_rect.h"
#include "gw_system.h"
#include "gw_window.h"

typedef struct GwTextFieldView {
    char pad_00[316];
    void (*cb)(void *);
} GwTextFieldView;

/* Called on a typed character or Enter. */
/* 0x001a1250 */
void gwTextFieldSetEnterCallback(GwTextFieldView *tf, void (*cb)(void *))
{
    tf->cb = cb;
}

/* Last key was Enter. */
/* 0x001a1258 */
int gwTextFieldIsDone(GwTextField *tf)
{
    return tf->state == 2;
}

/* At most 127 characters (longer refused with a console error); cursor at the end. */
/* steer(cast) */
/* 0x001a1268 */
void gwTextFieldSetText(GwTextField *tf, char *s)
{
    if (((int (*)(char *))strlen)(s) >= 128) {
        gw_console_printf("gwTextFieldSetText ERROR: %s is too long\n", s);
        return;
    }
    strncpy(tf->text, s, 127);
    tf->cursor = strlen(s);
}

/* 0x001a12e0 */
char *gwTextFieldGetText(GwTextField *tf)
{
    return tf->text;
}

/*
 * Text (one '*' per char in mode 1) centred vertically; cursor blinks every 16 frames while active.
 */
/* 0x001a12e8 */
static void gwTextFieldDraw(GwTextField *tf, float scale)
{
    int sz[2];
    int x;
    int y;
    unsigned i;

    if (gw_window_inside_owner(tf) == 0)
        return;
    gw_get_cell_size(&sz[0], &sz[1]);
    gw_draw_window_body(tf, scale);
    x = tf->xoff;
    y = (tf->h - sz[1]) / 2;
    switch (tf->mode) {
    case 0:
        gw_win_text(tf, x, y, tf->text);
        break;
    case 1:
        for (i = 0; i < strlen(tf->text); i++) {
            gw_win_text(tf, x, y, "*");
            x += sz[0];
        }
        break;
    }
    if (gw_window_is_active(tf) && (tf->blink & 0x10)) {
        x = tf->xoff + sz[0] * tf->cursor;
        if (x + sz[0] < tf->w) {
            gw_draw_rect_fill(tf, x, y, x + sz[0] - 1, y + sz[1] - 1, 0x80c87878, 1.0f);
        }
    }
}

/*
 * One cell high plus border; active: edits with this frame's key, calls cb; a click moves the
 * cursor.
 */
/* 0x001a1458 */
static void gwTextFieldUpdate(GwTextField *tf)
{
    int cw;
    int ch;
    int mx;
    int my;
    int lx;
    int ly;
    int n;

    gw_get_cell_size(&cw, &ch);
    tf->h = ch + 2;
    if (gw_window_is_active(tf)) {
        if ((unsigned)(gw_line_edit_key(tf->text) - 1) < 2) {
            if (tf->on_enter)
                tf->on_enter(tf);
        }
        tf->blink++;
    }
    if (gw_mouse_get_press() & 1) {
        gw_mouse_get_pos(&mx, &my);
        if (gw_window_hit_local(tf, mx, my, &lx, &ly)) {
            tf->cursor = (lx - tf->xoff) / cw;
            n = strlen(tf->text);
            if (n < tf->cursor)
                tf->cursor = n;
        }
    }
}

/* 0x001a1550 */
static void gwTextFieldNop(void)
{
}

/* 'Text Field', cursor at the end. 0 unless parent is a frame window. */
/* 0x001a1558 */
GwTextField *gwTextFieldCreate(char *text, GwTextFieldFn on_enter, int x, int y, int width, int height, struct GwWin *parent)
{
    GwTextField *tf;
    int cw;
    int ch;

    if (parent == 0)
        return 0;
    if (gw_window_is_frame(parent) == 0)
        return 0;
    gw_get_cell_size(&cw, &ch);
    tf = gw_window_new(7, "Text Field", 0x4012c, x, y, width, ch + 2, parent);
    if (tf == 0)
        return 0;
    tf->on_enter = on_enter;
    tf->xoff = 0;
    tf->update = gwTextFieldUpdate;
    tf->draw = gwTextFieldDraw;
    tf->destroy = gwTextFieldNop;
    gw_line_clear(tf->text);
    if (text) {
        strcpy(tf->text, text);
        tf->cursor = strlen(text);
    }
    return tf;
}
