/*
 * Debug GUI scroll bars: horizontal/vertical, free or fitted to a window edge; two arrows and a
 * draggable thumb over a float range (min, max, value, line and page step).
 */

#include "types_gw.h"
#include "gw_scrollbar.h"
#include "gw_button.h"
#include "gw_draw.h"
#include "gw_mouse.h"
#include "gw_system.h"
#include "gw_window.h"

extern int gw_scrollbar_arrow_icons[];
extern char *gw_scrollbar_arrow_titles[];

/* 0 horizontal, 1 vertical, 2 along the parent's bottom, 3 along its right. */
/* 0x001935d0 */
int gw_scrollbar_get_orient(GwSb *sb)
{
    return sb->orient;
}

/* 0x001935d8 */
static void gw_scrollbar_set_user(GwSb *sb, int user)
{
    sb->user = user;
}

/* Between the arrows; -1 for null. */
/* 0x001935e0 */
int gw_scrollbar_get_track(GwSb *sb)
{
    if (sb == 0)
        return -1;
    switch (sb->orient) {
    case 0:
    case 2:
        return sb->sx;
    case 1:
    case 3:
        return sb->sy;
    default:
        return 0;
    }
}

/* Arrows active, thumb shown. */
/* 0x00193630 */
void gw_scrollbar_set_enabled(GwSb *sb, int on)
{
    if (on) {
        sb->sflags &= ~2;
        gw_button_set_enabled(sb->btn0, 1);
        gw_button_set_enabled(sb->btn1, 1);
        gw_window_show(sb->ctl);
    } else {
        sb->sflags |= 2;
        gw_button_set_enabled(sb->btn0, 0);
        gw_button_set_enabled(sb->btn1, 0);
        gw_window_hide(sb->ctl);
    }
}

/* 0x001936b8 */
static float gw_range_get(float *rng)
{
    return *rng;
}

/* 0x001936c0 */
void gw_range_set(float *rng, float val)
{
    *rng = val;
}

/* Clamped to the maximum; returns the value. */
/* 0x001936c8 */
static float gw_range_step_up(GwRange *rng)
{
    rng->val = rng->val + rng->step;
    if (rng->max < rng->val)
        rng->val = rng->max;
    gw_range_set(&rng->val, rng->val);
    return rng->val;
}

/* Clamped to the minimum; returns the value. */
/* 0x00193720 */
static float gw_range_step_down(GwRange *rng)
{
    rng->val = rng->val - rng->step;
    if (rng->val < rng->min)
        rng->val = rng->min;
    gw_range_set(&rng->val, rng->val);
    return rng->val;
}

/* Clamped to the maximum; returns the value. */
/* 0x00193778 */
static float gw_range_page_up(GwRange *rng)
{
    rng->val = rng->val + rng->page;
    if (rng->max < rng->val)
        rng->val = rng->max;
    gw_range_set(&rng->val, rng->val);
    return rng->val;
}

/* Clamped to the minimum; returns the value. */
/* 0x001937d0 */
static float gw_range_page_down(GwRange *rng)
{
    rng->val = rng->val - rng->page;
    if (rng->val < rng->min)
        rng->val = rng->min;
    gw_range_set(&rng->val, rng->val);
    return rng->val;
}

/* 0x00193828 */
static float gw_scrollbar_page_up(GwSb *sb)
{
    return gw_range_page_up(&sb->r);
}

/* 0x00193848 */
static float gw_scrollbar_page_down(GwSb *sb)
{
    return gw_range_page_down(&sb->r);
}

/* 0x00193868 */
float gw_scrollbar_step_up(GwSb *sb)
{
    return gw_range_step_up(&sb->r);
}

/* 0x00193888 */
float gw_scrollbar_step_down(GwSb *sb)
{
    return gw_range_step_down(&sb->r);
}

/* sb->pos long, arrow-thick, along the track by value share, kept off the track end. */
/* 0x001938a8 */
static void gw_scrollbar_ctl_place(GwWin *ctl, GwSb *sb)
{
    GwRange *rng = &sb->r;
    GwBtn *arrow[2];
    GwBtn *arrow_copy[2];
    int ox, oy, sx, sy;
    unsigned orient;
    float range;

    arrow_copy[0] = arrow[0] = sb->btn0;
    arrow_copy[1] = arrow[1] = sb->btn1;
    ox = sb->ox;
    oy = sb->oy;
    sx = sb->sx;
    sy = sb->sy;
    orient = sb->orient;
    range = rng->max - rng->min;
    switch (orient) {
    case 0:
    case 2:
        ctl->w = sb->pos;
        ctl->h = arrow[0]->h;
        break;
    case 1:
    case 3:
        ctl->w = arrow[0]->w;
        ctl->h = sb->pos;
        break;
    }
    if (ctl->w < 0)
        ctl->w = 0;
    if (ctl->h < 0)
        ctl->h = 0;
    switch (sb->orient) {
    case 0:
    case 2: {
        int pos = ox + (int)((rng->val - rng->min) / range * (float)sx);
        int end = ox + sx;

        ctl->x = pos;
        if (end < pos + ctl->w)
            ctl->x = pos - (pos + ctl->w - end);
        ctl->y = arrow[0]->y;
        break;
    }
    case 1:
    case 3: {
        int pos;
        int end = oy + sy;

        ctl->x = arrow[0]->x;
        pos = oy + (int)((rng->val - rng->min) / range * (float)sy);
        ctl->y = pos;
        if (end < pos + ctl->h)
            ctl->y = pos - (pos + ctl->h - end);
        break;
    }
    }
}

/* When shown and overlapping its owner. */
/* 0x00193a48 */
static void gw_scrollbar_ctl_draw(GwWin *ctl, float scale)
{
    int rect[4];
    int owner_rect[4];

    if (gw_window_is_visible(ctl)) {
        if (ctl->owner != 0) {
            gw_window_get_abs_rect(ctl, rect);
            gw_window_get_abs_rect(ctl->owner, owner_rect);
            if (!gw_rect_contains_rect(rect, owner_rect))
                return;
        }
        gw_draw_window_body(ctl, scale);
    }
}

/* Enabled and dragged with left: movement along the track moves the value by the same share. */
/* 0x00193ad0 */
static void gw_scrollbar_ctl_update(GwWin *ctl)
{
    GwSb *sb = (GwSb *)ctl->parent;
    GwRange *rng;
    int move[2];

    if (sb->sflags & 2)
        return;
    if (gw_window_is_active(ctl)) {
        if (gw_mouse_get_on() & 1) {
            if (gw_mouse_moved()) {
                gw_mouse_get_move(&move[0], &move[1]);
                rng = &sb->r;
                switch (sb->orient) {
                case 0:
                case 2:
                    rng->val = rng->val + (float)move[0] / (float)sb->sx * (rng->max - rng->min);
                    break;
                case 1:
                case 3:
                    rng->val = rng->val + (float)move[1] / (float)sb->sy * (rng->max - rng->min);
                    break;
                }
                if (rng->val < rng->min)
                    rng->val = rng->min;
                else if (rng->max < rng->val)
                    rng->val = rng->max;
                gw_range_set(&rng->val, rng->val);
            }
        }
    }
    gw_scrollbar_ctl_place(ctl, (GwSb *)ctl->parent);
}

/* 'Scrollbar Controller'; 0 when sb is not a scroll bar. */
/* 0x00193c10 */
static GwWin *gw_scrollbar_ctl_create(GwSb *sb)
{
    GwWin *ctl;

    if (sb == 0)
        return 0;
    if (!gw_window_is_scrollbar(sb))
        return 0;
    ctl = gw_window_new(0, "Scrollbar Controller", 364, 0, 0, 0, 0, (GwWin *)sb);
    ctl->update = gw_scrollbar_ctl_update;
    ctl->draw = gw_scrollbar_ctl_draw;
    return ctl;
}

/* Up/down (0, 1) 10 high at top/bottom, left/right (2, 3) 10 wide at the sides, a pixel inside. */
/* 0x00193c88 */
static void gw_scrollbar_place_arrow(GwBtn *arrow, GwSb *sb)
{
    unsigned int dir;
    int margin;

    switch (dir = arrow->idx) {
    case 0:
    case 1:
        arrow->w = sb->w - 2;
        arrow->h = 10;
        break;
    case 2:
    case 3:
        arrow->w = 10;
        arrow->h = sb->h - 2;
        break;
    }
    margin = 1;
    switch (dir) {
    case 0:
        arrow->x = margin;
        arrow->y = margin;
        break;
    case 2:
        arrow->x = margin;
        arrow->y = margin;
        break;
    case 1:
        arrow->x = margin;
        arrow->y = sb->h - margin - arrow->h;
        break;
    case 3:
        arrow->x = sb->w - margin - arrow->w;
        arrow->y = margin;
        break;
    }
}

/* Per left repeat: one line step down (up/left arrows) or up (down/right arrows). */
/* 0x00193d30 */
static void gw_scrollbar_arrow_repeat(GwBtn *arrow)
{
    GwRange *rng = &((GwSb *)arrow->parent)->r;

    if (gw_mouse_get_repeat() & 1) {
        switch ((int)arrow->idx) {
        case 0:
        case 2:
            rng->val = rng->val - rng->step;
            if (rng->val < rng->min)
                rng->val = rng->min;
            break;
        case 1:
        case 3:
            rng->val = rng->val + rng->step;
            if (rng->max < rng->val)
                rng->val = rng->max;
            break;
        }
        gw_range_set(&rng->val, rng->val);
    }
}

/* Places it, then the button update. */
/* 0x00193e08 */
static void gw_scrollbar_arrow_update(GwBtn *arrow)
{
    gw_scrollbar_place_arrow(arrow, (GwSb *)arrow->parent);
    gw_icon_button_update(arrow);
}

/* 'Scroll Up', 'Scroll Down', 'Scroll Left', 'Scroll Right'. 0 without a scroll bar. */
/* 0x00193e38 */
static GwBtn *gw_scrollbar_arrow_create(int idx, GwSb *sb)
{
    GwBtn *arrow;

    if (sb == 0)
        return 0;
    arrow = gw_icon_button_create(0, gw_scrollbar_arrow_icons[idx], 1.0f, 0, 0, 0, 0, (GwWin *)sb);
    if (arrow == 0)
        return 0;
    gw_window_set_title((GwWin *)arrow, gw_scrollbar_arrow_titles[idx]);
    arrow->idx = idx;
    gw_scrollbar_place_arrow(arrow, sb);
    arrow->update = gw_scrollbar_arrow_update;
    arrow->cb = gw_scrollbar_arrow_repeat;
    return arrow;
}

/* Track start and length between the arrows. */
/* 0x00193f08 */
static void gw_scrollbar_layout(GwSb *sb)
{
    GwBtn *arrow[2];
    int tw;
    int th;
    int bw;
    int bh;

    arrow[0] = sb->btn0;
    arrow[1] = sb->btn1;
    sb->ox = arrow[0]->w + 3;
    sb->oy = arrow[0]->h + 3;
    tw = sb->w - 3;
    th = sb->h - 3;
    bw = arrow[1]->w + 3;
    sb->sx = tw - (arrow[0]->w + bw);
    bh = arrow[1]->h + 3;
    sb->sy = th - (arrow[0]->h + bh);
}

/* 0x00193f80 */
static float gw_scrollbar_get_min(GwSb *sb)
{
    return sb->r.min;
}

/* 0x00193f88 */
static float gw_scrollbar_set_min(GwSb *sb, float val)
{
    float old = sb->r.min;

    sb->r.min = val;
    return old;
}

/* 0x00193f98 */
static float gw_scrollbar_get_max(GwSb *sb)
{
    return sb->r.max;
}

/* 0x00193fa0 */
float gw_scrollbar_set_max(GwSb *sb, float val)
{
    float old = sb->r.max;

    sb->r.max = val;
    return old;
}

/* 0x00193fb0 */
static float gw_scrollbar_get_step(GwSb *sb)
{
    return sb->r.step;
}

/* 0x00193fb8 */
static float gw_scrollbar_set_step(GwSb *sb, float val)
{
    float old = sb->r.step;

    sb->r.step = val;
    return old;
}

/* 0x00193fc8 */
static float gw_scrollbar_get_page(GwSb *sb)
{
    return sb->r.page;
}

/* 0x00193fd0 */
static float gw_scrollbar_set_page(GwSb *sb, float val)
{
    float old = sb->r.page;

    sb->r.page = val;
    return old;
}

/* 0x00193fe0 */
float gw_scrollbar_get_val(GwSb *sb)
{
    return sb->r.val;
}

/* Returns the previous one. */
/* 0x00193fe8 */
float gw_scrollbar_set_val(GwSb *sb, float val)
{
    float old = sb->r.val;

    sb->r.val = val;
    gw_range_set(&sb->r.val, val);
    return old;
}

/* Thumb length; -1 for other windows. */
/* 0x00194020 */
static int gw_scrollbar_get_pos(GwSb *sb)
{
    if (sb->kind != 3)
        return -1;
    return sb->pos;
}

/* Thumb length; returns the previous one; -1 for other windows. */
/* 0x00194040 */
int gw_scrollbar_set_pos(GwSb *sb, int val)
{
    int old;

    if (sb->kind != 3)
        return -1;
    old = sb->pos;
    sb->pos = val;
    return old;
}

/* Default size: win wide x 12 when horizontal, 12 x win high when vertical. */
/* 0x00194060 */
static void gw_scrollbar_thumb_size(GwSb *sb, GwWin *win, int *pw, int *ph)
{
    int cw, ch;

    gw_get_cell_size(&cw, &ch);
    switch (sb->orient) {
    case 0:
    case 2:
        *pw = win->w;
        *ph = 12;
        break;
    case 1:
    case 3:
        *pw = 12;
        *ph = win->h;
        break;
    }
}

/* 0x00194100 */
static void gw_scrollbar_draw(GwWin *win, float scale)
{
    gw_draw_window_body(win, scale);
}

/* Orientation 2: 12-pixel strip along the bottom, 3: along the right; 0 and 1 untouched. */
/* 0x00194108 */
static void gw_scrollbar_fit(GwSb *sb, GwWin *win)
{
    int cw, ch;

    gw_get_cell_size(&cw, &ch);
    switch (sb->orient) {
    case 2:
        sb->w = win->w - 2;
        sb->h = 12;
        sb->x = 1;
        sb->y = win->h - 13;
        break;
    case 3:
        sb->w = 12;
        sb->h = win->h - 2;
        sb->x = win->w - 13;
        sb->y = 1;
        break;
    case 0:
    case 1:
        break;
    }
    if (sb->w < 0)
        sb->w = 0;
    if (sb->h < 0)
        sb->h = 0;
}

/* Vertical bars; one line step per notch. */
/* 0x001941d8 */
void gw_scrollbar_wheel(GwSb *sb)
{
    GwRange *rng = &sb->r;
    int n;
    int i;

    if (sb->orient == 1 || sb->orient == 3) {
        n = gw_mouse_get_wheel();
        if (n >= 0) {
            for (i = 0; i < n; i++)
                gw_range_step_up(rng);
        } else {
            n = -n;
            for (i = 0; i < n; i++)
                gw_range_step_down(rng);
        }
    }
}

/* Vertical bars; gw_input_keys step a line or a page. */
/* 0x00194270 */
void gw_scrollbar_keys(GwSb *sb)
{
    int *keys;

    if (sb->orient == 1 || sb->orient == 3) {
        keys = gw_input_keys;
        if (keys[2] & 1)
            gw_scrollbar_step_down(sb);
        if (keys[2] & 2)
            gw_scrollbar_step_up(sb);
        if (keys[2] & 0x10)
            gw_scrollbar_page_down(sb);
        if (keys[2] & 0x20)
            gw_scrollbar_page_up(sb);
    }
}

/*
 * Fits and lays out; enabled and active, each left repeat on the track pages towards the pointer.
 */
/* 0x00194320 */
static void gw_scrollbar_update(GwSb *sb)
{
    GwWin *ctl = sb->ctl;
    GwRange *rng = &sb->r;
    int lx, ly;

    gw_scrollbar_fit(sb, sb->parent);
    gw_scrollbar_layout(sb);
    if (sb->sflags & 2)
        return;
    if (!gw_window_is_active((GwWin *)sb))
        return;
    if (gw_mouse_get_repeat() & 1) {
        gw_window_hit_mouse((GwWin *)sb, &lx, &ly);
        switch (sb->orient) {
        case 0:
        case 2:
            if (lx < ctl->x) {
                rng->val = rng->val - rng->page;
                if (rng->val < rng->min)
                    rng->val = rng->min;
            } else if (ctl->x < lx) {
                rng->val = rng->val + rng->page;
                if (rng->max < rng->val)
                    rng->val = rng->max;
            }
            gw_range_set(&rng->val, rng->val);
            break;
        case 1:
        case 3:
            if (ly < ctl->y) {
                rng->val = rng->val - rng->page;
                if (rng->val < rng->min)
                    rng->val = rng->min;
            } else if (ctl->y < ly) {
                rng->val = rng->val + rng->page;
                if (rng->max < rng->val)
                    rng->val = rng->max;
            }
            gw_range_set(&rng->val, rng->val);
            break;
        }
    }
}

/*
 * 'Scroll Bar' from init (if given), 8-pixel thumb, default size for 0. 0 unless parent is a frame.
 */
/* 0x001944a8 */
GwSb *gw_scrollbar_create(int orient, GwRange *init, int x, int y, int width, int height, GwWin *parent)
{
    GwSb *sb;

    if (parent == 0 || !gw_window_is_frame(parent))
        return 0;
    sb = gw_window_new(3, "Scroll Bar", 0x4012c, x, y, width, height, parent);
    if (sb == 0)
        return 0;
    sb->update = gw_scrollbar_update;
    sb->draw = gw_scrollbar_draw;
    sb->orient = orient;
    if (init != 0)
        sb->r = *init;
    sb->pos = 8;
    switch (orient) {
    case 0:
    case 1:
        if (width <= 0 || height <= 0)
            gw_scrollbar_thumb_size(sb, parent, &sb->w, &sb->h);
        break;
    case 2:
    case 3:
        if (width <= 0 || height <= 0)
            gw_scrollbar_thumb_size(sb, parent, &sb->w, &sb->h);
        break;
    }
    switch (orient) {
    case 1:
    case 3:
        sb->btn0 = gw_icon_button_create(0, 16, 1.0f, 1, 1, sb->w - 2, 8, (GwWin *)sb);
        sb->btn0->update = gw_scrollbar_arrow_update;
        sb->btn0->idx = 0;
        gw_button_set_callback(sb->btn0, gw_scrollbar_arrow_repeat);
        gw_scrollbar_place_arrow(sb->btn0, sb);
        sb->btn1 = gw_icon_button_create(0, 17, 1.0f, 1, sb->h - 9, sb->w - 2, 8, (GwWin *)sb);
        sb->btn1->update = gw_scrollbar_arrow_update;
        sb->btn1->idx = 1;
        gw_button_set_callback(sb->btn1, gw_scrollbar_arrow_repeat);
        gw_scrollbar_place_arrow(sb->btn1, sb);
        break;
    case 0:
    case 2:
        sb->btn0 = gw_icon_button_create(0, 18, 1.0f, 1, 1, sb->w - 2, 8, (GwWin *)sb);
        sb->btn0->update = gw_scrollbar_arrow_update;
        sb->btn0->idx = 2;
        gw_button_set_callback(sb->btn0, gw_scrollbar_arrow_repeat);
        gw_scrollbar_place_arrow(sb->btn0, sb);
        sb->btn1 = gw_icon_button_create(0, 19, 1.0f, 1, sb->h - 9, sb->w - 2, 8, (GwWin *)sb);
        sb->btn1->update = gw_scrollbar_arrow_update;
        sb->btn1->idx = 3;
        gw_button_set_callback(sb->btn1, gw_scrollbar_arrow_repeat);
        gw_scrollbar_place_arrow(sb->btn1, sb);
        break;
    }
    sb->ctl = gw_scrollbar_ctl_create(sb);
    return sb;
}
