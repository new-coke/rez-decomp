/* Name entry screen: cursor arrows; glow when the cursor moves, dim once three letters are in. */

#include "name_e.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ne_arrow.h"

extern struct TaskDesc ne_arrow_desc;

/* 0x00af6700 */
void ne_arrow_init(NeArrow *w)
{
    void *mroot;

    mroot = pnb_get_entry(w->models, 6)->src;
    dc_object_init((struct MapPart *)&w->parts[0], mroot, 0, 0);
    mroot = pnb_get_entry(w->models, 7)->src;
    dc_object_init((struct MapPart *)&w->parts[1], mroot, 0, 0);
    mroot = pnb_get_entry(w->models, 4)->src;
    dc_object_init((struct MapPart *)&w->parts[2], mroot, 0, 0);
    mroot = pnb_get_entry(w->models, 5)->src;
    dc_object_init((struct MapPart *)&w->parts[3], mroot, 0, 0);
    w->lamp[1].on = 1;
    w->lamp[0].on = 1;
}

/* 0x00af67b0 */
void ne_arrow_release(NeArrow *w)
{
    dc_object_release((struct MapPart *)&w->parts[0]);
    dc_object_release((struct MapPart *)&w->parts[1]);
    dc_object_release((struct MapPart *)&w->parts[2]);
    dc_object_release((struct MapPart *)&w->parts[3]);
}

/* 0x00af67f0 */
void ne_arrow_exec(NeArrow *w)
{
    NeLamp *lp = w->lamp;
    int n;

    for (n = 2; n != 0; n--, lp++) {
        switch (lp->state) {
        case 0:
            break;
        case 1:
            lp->t += 0.5f;
            if (1.0f <= lp->t) {
                lp->t = 1.0f;
                lp->state = 2;
            }
            break;
        case 2:
            lp->t -= 0.05f;
            if (lp->t < 0.0f) {
                lp->t = 0;
                lp->state = 0;
            }
            break;
        }
    }
}

/* 0x00af68a0 */
void ne_arrow_disp(NeArrow *w)
{
    float rgba[4];
    ModelPart *p;
    NeLamp *lp, *glow;
    int n, blend;
    float dim;

    dim = 0.4f;
    ulGxSetTextureList(w->textures);
    blend = draw_set_override_flags(16);
    rgba[3] = rgba[2] = rgba[1] = 1.0f;
    p = &w->parts[0];
    lp = w->lamp;
    for (n = 2; n != 0; n--) {
        if (lp->on)
            rgba[0] = 1.0f;
        else
            rgba[0] = dim;
        draw_set_override_color(rgba);
        lp++;
        dc_object_draw((struct MapPart *)p);
        p++;
    }
    p = &w->parts[2];
    glow = w->lamp;
    for (n = 2; n != 0; n--) {
        rgba[0] = glow->t;
        glow++;
        draw_set_override_color(rgba);
        dc_object_draw((struct MapPart *)p);
        p++;
    }
    draw_set_override_flags(blend);
}

/* 0x00af69b8 */
void *ne_arrow_create(void *parent)
{
    return task_create_inherit(parent, &ne_arrow_desc, 408);
}

/* 0x00af69e0 */
void ne_arrow_set_enabled(NeArrow *w, int i, int on)
{
    w->lamp[i].on = on;
}

/* 0x00af69f0 */
void ne_arrow_light(NeArrow *w, int i)
{
    if (w->lamp[i].on)
        w->lamp[i].state = 1;
}
