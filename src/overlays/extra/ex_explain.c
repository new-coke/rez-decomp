/* Beyond menu: hint bar with button marks; images in English, text in other languages. */

#include "extra_shared.h"
#include "billboard.h"
#include "color.h"
#include "float_bits.h"
#include "node_draw_state.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ex_explain.h"

extern struct TaskDesc ex_explain_desc;
extern unsigned char **explain_texts[];
extern int explain_strips[][2];

/* 0x00ca0700 */
void ex_explain_draw_strip(int strip, float x, float y, float z)
{
    float uv[4];

    uv[0] = 0.0f;
    uv[1] = strip * 28.0f * 0.001953125f;
    uv[2] = 1.0f;
    uv[3] = (strip + 1) * 28.0f * 0.001953125f;
    tex_rect_draw_uv(0, COL_WHITE, uv, x, y, z, 512.0f, 28.0f);
}

/* 0x00ca0780 */
void ex_explain_init(void)
{
}

/* 0x00ca0788 */
void ex_explain_release(void)
{
}

/* 0x00ca0790 */
void ex_explain_exec(void)
{
}

/* 0x00ca0798 */
void ex_explain_disp(ExExplain *w)
{
    GameState *gs;
    unsigned char *s;
    int lang, col, i;
    int *strip;
    float z, pos, step;

    s = 0;
    if (w->hint < 0)
        return;
    gs = &game_state;
    lang = gs->lang;
    draw_set_blend(4, 5);
    z = 2.2222223f;
    col = gs->menu_color;
    pos = 412.0f;
    sprite_draw(82.0f, pos, z, col, 11, 0, 0);
    if (w->adjust)
        sprite_draw(197.0f, pos, z, col, 12, 0, 0);
    if (w->accept)
        sprite_draw(344.0f, pos, z, col, 14, 0, 0);
    pos = 488.0f;  /* last mark's x; reused as the text's y */
    sprite_draw(pos, 412.0f, z, col, 13, 0, 0);
    if (lang != 0) {
        s = explain_texts[lang][w->hint];
        if (!s)
            return;
    }
    rect_draw_outline(25.0f, 324.0f, z, 586.0f, 83.0f, gs->line_color, RGBA(0, 0, 0, 0x64));
    pos = 338.0f;
    z = float_add_bits16(2, z);
    if (lang == 0) {
        step = 28.0f;
        ulGxSetTextureList(w->textures);
        strip = explain_strips[w->hint];
        for (i = 2; i != 0; i--) {
            if (*strip >= 0)
                ex_explain_draw_strip(*strip, 38.0f, pos, z);
            pos += step;
            strip++;
        }
    } else {
        col = gs->menu_color;
        font_set(2);
        font_print(38.0f, pos, z, col, s);
    }
}

/* 0x00ca09d8 */
void *ex_explain_create(void *parent)
{
    return task_create_inherit(parent, &ex_explain_desc, 20);
}

/* 0x00ca0a00 */
void ex_explain_set(ExExplain *w, int hint, int adjust, int accept)
{
    w->hint = hint;
    w->adjust = adjust;
    w->accept = accept;
}
