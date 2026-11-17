/*
 * Model draw state: blend, texture clamp/filter/flip, shading, colour and strip overrides.
 * Setters record what they apply, so callers can save and restore.
 */

#include "types_game.h"
#include "nj_view.h"
#include "ul_gx.h"
#include "node_draw_state.h"
#include "color.h"

extern int draw_strip_mask;
extern int draw_blend_src;
extern int draw_blend_dst;
extern int draw_nj_state0;
extern int draw_nj_state1;
extern float draw_nj_state3;
extern int draw_nj_state4;
extern int draw_nj_state5;
extern int draw_nj_state7;
extern int draw_nj_state2;
extern int draw_test_mode;
extern int draw_fc_mode;
extern int draw_ztest_mode;
extern int draw_tex_function;
extern int draw_nj_state6;
extern int draw_nj_color0;
extern int draw_nj_color1;
extern int draw_override_flags;
extern int draw_strip_flags;
extern int draw_strip_flags_saved;
extern int draw_strip_mask_saved;
extern DrawColor4 draw_override_color;
extern DrawColor4 draw_override_color_saved;
extern const int draw_blend_src_table[];
extern const int draw_blend_dst_table[];

extern int draw_blend_src_saved;
extern int draw_blend_dst_saved;
extern void nj_state_set0(int);
extern void nj_state_set1(int);
extern void nj_state_set3(float);
extern void nj_state_set4(int);
extern void nj_state_set5(int);
extern void nj_state_set7(int);
extern void nj_state_set2(int);
extern void nj_texture_flip_mode(int);
extern void njTextureShadingMode(int);
extern void nj_state_set6(void);
extern void nj_state_set_colors(int, int);

/* Recorded state only; hardware untouched. */
/* 0x0013ef20 */
void model_render_state_init(void)
{
    (*(unsigned int *)&draw_strip_flags) = 0xffffffff; /* steer(cast) */
    draw_strip_mask = 0;
    draw_blend_src = 4;
    draw_blend_dst = 5;
    draw_nj_state0 = 0;
    draw_nj_state1 = 4;
    draw_nj_state3 = 0.01f;
    draw_nj_state4 = 0x8000000;
    draw_nj_state5 = -1;
    draw_nj_state7 = 0;
    draw_nj_state2 = 0;
    draw_test_mode = 0;
    draw_fc_mode = 0;
    draw_ztest_mode = 0;
    draw_tex_function = 192;
    draw_nj_state6 = 0;
    draw_nj_color0 = RGBA(0x60, 0x60, 0x60, 0x80);
    draw_nj_color1 = COL_BLACK;
}

/* Returns the previous flags. */
/* 0x0013f038 */
int draw_set_override_flags(int flags)
{
    int old = draw_override_flags;

    draw_override_flags = flags;
    nj_cnk_set_override_flags(flags);
    return old;
}

/* mask: strip flags the override replaces. */
/* 0x0013f070 */
void draw_set_strip_flags(int flags, int mask)
{
    draw_strip_flags = flags;
    draw_strip_mask = mask;
    nj_cnk_set_override_strip_flags(flags, mask);
}

/* One level. */
/* 0x0013f088 */
void draw_push_strip_flags(void)
{
    draw_strip_flags_saved = draw_strip_flags;
    draw_strip_mask_saved = draw_strip_mask;
}

/* Not re-applied. */
/* 0x0013f0b0 */
void draw_pop_strip_flags(void)
{
    draw_strip_flags = draw_strip_flags_saved;
    draw_strip_mask = draw_strip_mask_saved;
}

/* 0x0013f0d8 */
void draw_set_override_color(float *rgba)
{
    draw_override_color = *(DrawColor4 *)rgba;
    nj_cnk_set_override_color(rgba);
}

/* One level. */
/* 0x0013f108 */
void draw_push_override_color(void)
{
    draw_override_color_saved = draw_override_color;
}

/* Not re-applied. */
/* 0x0013f140 */
void draw_pop_override_color(void)
{
    draw_override_color = draw_override_color_saved;
}

/* src, dst: indices into the blend tables. */
/* 0x0013f178 */
void draw_set_blend(int src, int dst)
{
    int x = draw_blend_src_table[src];
    int y = draw_blend_dst_table[dst];

    draw_blend_src = src;
    draw_blend_dst = dst;
    ulGxSetAlphaBlend(x, y);
}

/* One level. */
/* 0x0013f1c0 */
void draw_push_blend(void)
{
    draw_blend_src_saved = draw_blend_src;
    draw_blend_dst_saved = draw_blend_dst;
}

/* Re-applies the saved modes. */
/* 0x0013f1e8 */
void draw_pop_blend(void)
{
    draw_set_blend(draw_blend_src_saved, draw_blend_dst_saved);
}

/* Ninja state setters below: record, apply, return the previous value. */
/* 0x0013f200 */
static int draw_set_nj_state0(int val)
{
    int old = draw_nj_state0;

    draw_nj_state0 = val;
    nj_state_set0(val);
    return old;
}

/* 0x0013f238 */
static int draw_set_nj_state1(int val)
{
    int old = draw_nj_state1;

    draw_nj_state1 = val;
    nj_state_set1(val);
    return old;
}

/* 0x0013f270 */
static float draw_set_nj_state3(float val)
{
    float old = draw_nj_state3;

    draw_nj_state3 = val;
    nj_state_set3(val);
    return old;
}

/* 0x0013f2a8 */
static int draw_set_nj_state4(int val)
{
    int old = draw_nj_state4;

    draw_nj_state4 = val;
    nj_state_set4(val);
    return old;
}

/* 0x0013f2e0 */
static int draw_set_nj_state5(int val)
{
    int old = draw_nj_state5;

    draw_nj_state5 = val;
    nj_state_set5(val);
    return old;
}

/* 0x0013f318 */
static int draw_set_nj_state7(int val)
{
    int old = draw_nj_state7;

    draw_nj_state7 = val;
    nj_state_set7(val);
    return old;
}

/* 0x0013f350 */
static int draw_set_nj_state2(int val)
{
    int old = draw_nj_state2;

    draw_nj_state2 = val;
    nj_state_set2(val);
    return old;
}

/* Ninja bits: 0x10000 clamp U, 0x8000 clamp V, 0x18000 both, else repeat. Returns previous. */
/* 0x0013f388 */
int draw_set_test_mode(int mode)
{
    int old = draw_test_mode;

    draw_test_mode = mode;
    switch (mode) {
    case 0x10000:
        ulGxSetTexClamp(1, 0);
        break;
    case 0x8000:
        ulGxSetTexClamp(0, 1);
        break;
    case 0x18000:
        ulGxSetTexClamp(1, 1);
        break;
    case 0:
    default:
        ulGxSetTexClamp(0, 0);
        break;
    }
    return old;
}

/* Ninja bits: 0x2000 filter 1, 0x4000/0x6000 filter 2, else 0. Returns previous. */
/* 0x0013f420 */
int draw_set_fc_mode(int mode)
{
    int old = draw_fc_mode;

    draw_fc_mode = mode;
    switch (mode) {
    case 0x2000:
        ulGxSetTexFilter(1);
        break;
    case 0x4000:
    case 0x6000:
        ulGxSetTexFilter(2);
        break;
    case 0:
    default:
        ulGxSetTexFilter(0);
        break;
    }
    return old;
}

/* Texture flip mode; returns the previous one. */
/* 0x0013f4a0 */
int draw_set_ztest_mode(int mode)
{
    int old = draw_ztest_mode;

    draw_ztest_mode = mode;
    nj_texture_flip_mode(mode);
    return old;
}

/* Texture shading mode; returns the previous one. */
/* 0x0013f4d8 */
static int draw_set_tex_function(int mode)
{
    int old = draw_tex_function;

    draw_tex_function = mode;
    njTextureShadingMode(mode);
    return old;
}

/* Returns val, unlike the other setters. */
/* 0x0013f510 */
static int draw_set_nj_state6(int val)
{
    draw_nj_state6 = val;
    nj_state_set6();
    return val;
}

/* 0x0013f540 */
static void draw_set_nj_colors(int col0, int col1)
{
    draw_nj_color0 = col0;
    draw_nj_color1 = col1;
    nj_state_set_colors(col0, col1);
}
