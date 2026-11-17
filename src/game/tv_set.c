/*
 * TV set-up screen: a three-entry menu run from main's TV-mode loop; one entry confirms with a
 * five-second countdown back to the menu, the other two finish with their choice.
 */

#include "system.h"
#include "color.h"
#include "ul_gx.h"
#include "tv_set.h"
#include "billboard.h"
#include "file.h"
#include "heap.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pnb.h"
#include "render.h"
#include "game.h"

typedef struct Slot {
    void *key;
    char pad_04[8];
} Slot;

#include "tvset.h"

extern Slot tvset_slots[];
extern int tvset_file[];
extern TvRect tvset_item_rects[];
extern SpriteUV tvset_item_uvs[];
extern int tvset_item_tex_ids[];

extern SpriteUV tvset_digit_uvs[];
extern Pad *pad_main;
extern int pad_decide_mask;
extern TvSet *tvset_work;

/* Key -1. */
/* 0x00172a30 */
void tvset_slots_clear(void)
{
    Slot *slot = tvset_slots;
    int n;

    for (n = 16; n != 0; n--) {
        slot->key = (void *)0xffffffff;
        slot++;
    }
}

/* 0x00172a70 */
static void tvset_nop(void)
{
}

/* 0x00172a78 */
static int tvset_return_one(void)
{
    return 1;
}

/* Choice, loaded file, texture object. */
/* 0x00172a80 */
static void tvset_reset(int *w)
{
    w[6] = 0;
    w[4] = 0;
    w[5] = 0;
    w[1] = 0;
}

/* 0x00172a98 */
static void tvset_load(TvSet *w)
{
    w->data = file_load_sync(tvset_file, 0, 0);
}

/* 0x00172ad0 */
static void tvset_unload(TvSet *w)
{
    heap_free(w->data, 0);
    w->data = 0;
}

/* Sprite tvset_item_uvs[i] in tvset_item_rects[i], current language's texture. */
/* 0x00172b00 */
static void tvset_draw_item(TvSet *w, int i)
{
    float x;
    float y;
    float rw;
    float rh;
    int k;
    SpriteUV *uv;

    if (w->tex) {
        k = game_lang;
        x = tvset_item_rects[i].x;
        uv = &tvset_item_uvs[i];
        y = tvset_item_rects[i].y;
        rw = tvset_item_rects[i].w;
        rh = tvset_item_rects[i].h;
        draw_set_blend(4, 5);
        ulGxSetTextureList(w->tex);
        tex_rect_draw_uv(tvset_item_tex_ids[k], COL_WHITE, uv, x, y, 2.0f, rw, rh);
    }
}

/* Clamped at 0; 85x85 at (491, 264). */
/* 0x00172c10 */
static void tvset_draw_digit(TvSet *w, int n)
{
    SpriteUV *uv;
    float size;

    if (w->tex) {
        if (n < 0)
            n = 0;
        size = 85.0f;
        uv = &tvset_digit_uvs[n];
        draw_set_blend(4, 5);
        ulGxSetTextureList(w->tex);
        tex_rect_draw_uv(10, COL_WHITE, uv, 491.0f, 264.0f, 2.0f, size, size);
    }
}

/* Image 5 for the confirmation page (flag), else 0. */
/* 0x00172cd8 */
static void tvset_draw_bg(TvSet *w, int flag)
{
    if (w->tex) {
        ulGxSetTextureList(w->tex);
        screen_image_draw(flag ? 5 : 0, COL_WHITE, 1.0f);
    }
}

/* Returns whether confirmed (pad_cancel_mask button), accepted only after the first second. */
/* 0x00172d30 */
static int tvset_countdown_draw(TvSet *w)
{
    tvset_draw_bg(w, 1);
    tvset_draw_item(w, 0);
    tvset_draw_digit(w, w->timer / 60);
    ((int (*)(TvSet *, int))tvset_draw_item)(w, 7); /* steer(cast) */
    return w->timer <= 240 && (pad_main->menu.trig & pad_cancel_mask) != 0;
}

/*
 * Cursor 0..2; decide stores the entry's choice (3, 4 or 2). Pad read through a volatile pointer,
 * so trigger bits are reread after each store.
 */
/* 0x00172dc0 */
static void tvset_menu(TvSet *w)
{
    volatile Pad *pad = pad_main;
    int cursor;

    if (pad->menu.trig & PAD_UP)
        w->cursor--;
    if (pad->menu.trig & PAD_DOWN)
        w->cursor++;
    w->cursor = (w->cursor < 0) ? 0 : w->cursor;
    cursor = w->cursor;
    w->cursor = cursor = (cursor > 2) ? 2 : cursor;
    if (pad->menu.trig & pad_decide_mask) {
        switch (cursor) {
        case 0:
            w->choice = 3;
            break;
        case 1:
            w->choice = 4;
            break;
        case 2:
            w->choice = cursor;
            break;
        }
    }
    tvset_draw_bg(w, 0);
    tvset_draw_item(w, 0);
    tvset_draw_item(w, 1);
    tvset_draw_item(w, 2);
    tvset_draw_item(w, 3);
    tvset_draw_item(w, w->cursor + 8);
    tvset_draw_item(w, w->cursor + 4);
}

/* At VRAM 0x290000, unless it exists. */
/* 0x00172f00 */
static void tvset_tex_create(TvSet *w)
{
    if (!w->tex)
        w->tex = model_create_timed(w->data, 0x290000);
}

/* 0x00172f40 */
static void tvset_tex_release(TvSet *w)
{
    if (w->tex) {
        model_list_remove(w->tex);
        w->tex = 0;
    }
}

/*
 * Called each frame until non-zero. Choice below 3: 300-frame confirmation (2 at start, 1 on
 * timeout). Final choice 3/4: unloads, frees, returns it a few frames after unloading.
 */
/* 0x00172f78 */
unsigned int tv_set_select(void)
{
    TvSet *w;
    unsigned int choice;

    if (tvset_work == 0) {
        tvset_work = heap_block_tag(game_malloc(28), "tv_set.c", 438);
        tvset_work->state = 0;
    }
    w = tvset_work;
    RenderSetCurrentDisplayList(3);
    choice = 0;
    switch (w->state) {
    case 0:
        pad_input_enable(0);
        tvset_reset(w);
        tvset_load(w);
        w->state++;
        break;
    case 1:
        tvset_tex_create(w);
        tvset_menu(w);
        choice = w->choice;
        if (choice) {
            if (choice >= 3) {
                w->state = 1000;
                choice = 0;
            } else {
                w->state = 500;
                choice = 0;
            }
        }
        break;
    case 500:
        w->timer = 300;
        w->state = 501;
        choice = 2;
        break;
    case 501:
        if (--w->timer <= 0) {
            w->choice = 0;
            w->state = 1;
            choice = 1;
        } else {
            tvset_tex_create(w);
            if (tvset_countdown_draw(w))
                w->timer = 0;
        }
        break;
#ifdef VERSION_US
    case 1000:
        tvset_tex_release(w);
        tvset_unload(w);
        pad_input_disable(0);
        w->state++;
        break;
    case 1001:
    case 1002:
    case 1003:
        w->state++;
        break;
    case 1004:
        choice = w->choice;
        break;
#else
    case 1000:
        choice = w->choice;
        tvset_unload(w);
        pad_input_disable(0);
        break;
#endif
    }
    if (choice)
        tvset_tex_release(w);
    if (choice >= 3) {
        heap_free(tvset_work, 0);
        tvset_work = 0;
    }
    return choice;
}
