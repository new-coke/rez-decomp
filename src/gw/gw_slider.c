/*
 * Debug GUI sliders: track and draggable thumb over a float range; track clicks step towards the
 * pointer.
 */

#include "types_gw.h"
#include "gw_slider.h"
#include "gw_draw.h"
#include "gw_mouse.h"
#include "gw_rect.h"
#include "gw_scrollbar.h"
#include "gw_window.h"

/* Called on every value change. */
/* 0x001a0a90 */
void gw_slider_set_callback(GwSlider *slider, GwSliderFn cb)
{
    slider->cb = cb;
}

/* Step of a track click. */
/* 0x001a0a98 */
static void gw_slider_set_step(GwSlider *slider, float val)
{
    slider->vals.step = val;
}

/* 0x001a0aa0 */
static float gw_slider_get_step(GwSlider *slider)
{
    return slider->vals.step;
}

/* 0x001a0aa8 */
static void gw_slider_set_min(GwSlider *slider, float val)
{
    slider->vals.min = val;
}

/* 0x001a0ab0 */
float gw_slider_get_min(GwSlider *slider)
{
    return slider->vals.min;
}

/* 0x001a0ab8 */
static void gw_slider_set_max(GwSlider *slider, float val)
{
    slider->vals.max = val;
}

/* 0x001a0ac0 */
float gw_slider_get_max(GwSlider *slider)
{
    return slider->vals.max;
}

/* Clamped to [min, max]. */
/* 0x001a0ac8 */
void gw_slider_set_value(GwSlider *slider, float val)
{
    GwSliderVals *vals = &slider->vals;

    if (vals->max < val)
        val = vals->max;
    if (val < vals->min)
        val = vals->min;
    slider->vals.val = val;
}

/* 0x001a0b00 */
float gw_slider_get_value(GwSlider *slider)
{
    return slider->vals.val;
}

/* Only when slider and thumb are inside their owner. */
/* 0x001a0b08 */
static void gw_slider_thumb_draw(GwWin *thumb, float z)
{
    if (gw_window_inside_owner(thumb->parent)) {
        if (gw_window_inside_owner(thumb))
            gw_draw_window_body(thumb, z);
    }
}

/*
 * Dragged with left: movement along the track moves the value by the same share, calls cb.
 * Bug: placement uses val / (max - min), right only for ranges starting at 0.
 */
/* 0x001a0b68 */
static void gw_slider_thumb_update(GwWin *thumb)
{
    GwSlider *slider = (GwSlider *)thumb->parent;
    GwSliderVals *vals = &slider->vals;
    int move[2];
    float ratio;
    float delta;

    switch (slider->vertical) {
    case 0:
        thumb->w = 8;
        thumb->h = ((GwWin *)slider)->h;
        break;
    case 1:
        thumb->w = ((GwWin *)slider)->w;
        thumb->h = 8;
        break;
    }
    if (gw_window_is_active(thumb)) {
        if (gw_mouse_get_on() & 1) {
            if (gw_mouse_moved()) {
                gw_mouse_get_move(&move[0], &move[1]);
                switch (slider->vertical) {
                case 0:
                    delta = ((float)move[0] / (float)slider->len_x) * (vals->max - vals->min);
                    vals->val += delta;
                    break;
                case 1:
                    delta = ((float)move[1] / (float)slider->len_y) * (vals->max - vals->min);
                    vals->val += delta;
                    break;
                }
                if (vals->val < vals->min)
                    vals->val = vals->min;
                else if (vals->max < vals->val)
                    vals->val = vals->max;
                gw_range_set(vals, vals->val);
                if (slider->cb)
                    slider->cb((GwSlider *)thumb->parent);
            }
        }
    }
    ratio = vals->val / (vals->max - vals->min);
    switch (slider->vertical) {
    case 0:
        thumb->x = slider->x0 - thumb->w / 2 + (int)(ratio * (float)slider->len_x);
        break;
    case 1:
        thumb->y = slider->y0 - thumb->h / 2 + (int)(ratio * (float)slider->len_y);
        break;
    }
}

/* 'Slider Control'; 0 when slider is not a slider. */
/* 0x001a0da0 */
static GwWin *gw_slider_thumb_create(GwSlider *slider)
{
    GwWin *thumb;

    if (slider == 0)
        return 0;
    if (!gw_window_is_slider((GwWin *)slider))
        return 0;
    thumb = gw_window_new(0, "Slider Control", 364, 0, 0, 0, 0, (GwWin *)slider);
    thumb->update = gw_slider_thumb_update;
    thumb->draw = (GwFnF)gw_slider_thumb_draw;
    switch (slider->vertical) {
    case 0:
        thumb->w = 4;
        break;
    case 1:
        thumb->h = 4;
        break;
    }
    return thumb;
}

/* Dark track, then the window body. */
/* 0x001a0e50 */
static void gw_slider_draw(GwSlider *slider, float z)
{
    if (gw_window_inside_owner((GwWin *)slider)) {
        gw_draw_rect_fill((GwWin *)slider, slider->x0, slider->y0, slider->x0 + slider->len_x - 1, slider->y0 + slider->len_y - 1, 0x80202020, 1.0f);
        gw_draw_window_body((GwWin *)slider, z);
    }
}

/* Active: each left repeat steps towards the pointer and calls cb. */
/* 0x001a0ee0 */
static void gw_slider_update(GwSlider *slider)
{
    GwSliderVals *vals;
    GwWin *thumb;
    int pos[2];

    thumb = slider->thumb;
    switch (slider->vertical) {
    case 0:
        slider->len_y = 2;
        slider->len_x = ((GwWin *)slider)->w - 8;
        slider->x0 = 4;
        slider->y0 = ((GwWin *)slider)->h / 2 - 1;
        break;
    case 1:
        slider->len_x = 2;
        slider->len_y = ((GwWin *)slider)->h - 8;
        slider->y0 = 4;
        slider->x0 = ((GwWin *)slider)->w / 2 - 1;
        break;
    }
    vals = &slider->vals;
    if (gw_window_is_active((GwWin *)slider)) {
        if (gw_mouse_get_repeat() & 1) {
            gw_window_hit_mouse((GwWin *)slider, &pos[0], &pos[1]);
            switch (slider->vertical) {
            case 0:
                if (pos[0] < thumb->x) {
                    slider->vals.val = slider->vals.val - vals->step;
                    if (slider->vals.val < vals->min)
                        slider->vals.val = vals->min;
                } else if (thumb->x < pos[0]) {
                    slider->vals.val = slider->vals.val + vals->step;
                    if (vals->max < slider->vals.val)
                        slider->vals.val = vals->max;
                }
                gw_range_set(vals, vals->val);
                break;
            case 1:
                if (pos[1] < thumb->y) {
                    slider->vals.val = slider->vals.val - vals->step;
                    if (slider->vals.val < vals->min)
                        slider->vals.val = vals->min;
                } else if (thumb->y < pos[1]) {
                    slider->vals.val = slider->vals.val + vals->step;
                    if (vals->max < slider->vals.val)
                        slider->vals.val = vals->max;
                }
                gw_range_set(vals, vals->val);
                break;
            }
            if (slider->cb)
                slider->cb(slider);
        }
    }
}

/* 0x001a10e0 */
static void gw_slider_destroy(void)
{
}

/* 'Slider' from args (if given), 8 pixels thick by default. 0 unless parent is a frame window. */
/* 0x001a10e8 */
GwSlider *gw_slider_create(int vertical, GwSliderVals *args, int x, int y, int width, int height, GwWin *parent)
{
    GwSlider *slider;
    GwWin *thumb;
    int thickness;

    if (parent == 0 || !gw_window_is_frame(parent))
        return 0;
    slider = (GwSlider *)gw_window_new(6, "Slider", 0x4012c, x, y, width, height, parent);
    if (slider == 0)
        return 0;
    ((GwWin *)slider)->update = gw_slider_update;
    ((GwWin *)slider)->draw = (GwFnF)gw_slider_draw;
    ((GwWin *)slider)->destroy = gw_slider_destroy;
    slider->vertical = vertical;
    if (args)
        slider->vals = *args;
    thickness = 8;
    switch (vertical) {
    case 0:
        if (height <= 0)
            ((GwWin *)slider)->h = thickness;
        break;
    case 1:
        if (width <= 0)
            ((GwWin *)slider)->w = thickness;
        break;
    }
    thumb = gw_slider_thumb_create(slider);
    thumb->owner = parent;
    slider->thumb = thumb;
    return slider;
}

/* 0x001a1240 */
static void gw_slider_set_vertical(GwSlider *slider, int vertical)
{
    slider->vertical = vertical;
}

/* 0x001a1248 */
static int gw_slider_get_vertical(GwSlider *slider)
{
    return slider->vertical;
}
