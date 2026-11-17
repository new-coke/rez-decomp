/* Shape transition draw hook. */

#include "types_game.h"
#include "fade_shape_fade_shape_state_draw.h"

extern int fade_shape_object_offsets[];
extern int fade_shape_motion_offsets[];
extern int fade_shape_draw_offsets[];
extern int fade_shape_texlist_offsets[];
extern void fade_shape_draw(float f, int a, int b, int c, int d, int alpha);

/* Object, motion, draw data and textures via four offset tables relative to w->base. */
/* 0x001164c8 */
static void fade_shape_state_draw(FadeShapeWork *w)
{
    int k = w->kind;
    int base = w->base;

    fade_shape_draw(w->mot_frm, base + fade_shape_object_offsets[k], base + fade_shape_motion_offsets[k], base + fade_shape_draw_offsets[k], base + fade_shape_texlist_offsets[k], w->alpha);
}
