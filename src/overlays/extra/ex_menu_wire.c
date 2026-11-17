/* Beyond menu: page frame lines; grow as the page opens, shrink twice as fast as it closes. */

#include "extra_shared.h"
#include "mathdef.h"
#include "color.h"
#include "float_bits.h"
#include "line_batch.h"
#include "ex_menu_wire.h"

/* (x0, y0, x1, y1) each */
typedef struct ExMenuWireLines {
    float (*pts)[4];
    int count;
} ExMenuWireLines;

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))
extern struct TaskDesc ex_menu_wire_desc;
extern ExMenuWireLines ex_menu_wire_lines[];

/* 0x00ca25b0 */
void ex_menu_wire_init(ExMenuWire *w, int id)
{
    w->id = id;
    w->state = 0;
}

/* 0x00ca25c0 */
void ex_menu_wire_release(void)
{
}

/* 0x00ca25c8 */
void ex_menu_wire_exec(ExMenuWire *w)
{
    switch (w->state) {
    case 0:
        w->t += 0.05f;
        break;
    case 1:
        w->t -= 0.1f;
        break;
    }
    w->t = MIN(MAX_GE(w->t, 0.0f), 1.0f);
}

/* 0x00ca2660 */
void ex_menu_wire_disp(ExMenuWire *w)
{
    float (*p)[4];
    void *lh;
    int n;
    float z;

    p = ex_menu_wire_lines[w->id].pts;
    n = ex_menu_wire_lines[w->id].count;
    lh = line_set_color(RGBA(0x48, 0x2b, 0x12, 0x80));
    z = float_add_bits16(1, 2.2222223f);
    do {
        lh = line_add_seg(lh, (*p)[0], (*p)[1], z, (*p)[0] + ((*p)[2] - (*p)[0]) * w->t,
                          (*p)[1] + ((*p)[3] - (*p)[1]) * w->t, z);
        p++;
    } while (--n);
    line_flush(lh);
}

/* 0x00ca2740 */
void *ex_menu_wire_create(void *parent, int id)
{
    ExMenuWire *w;

    w = task_create(parent, &ex_menu_wire_desc, 12);
    if (w)
        ex_menu_wire_init(w, id);
    return w;
}

/* 0x00ca2790 */
void ex_menu_wire_close(ExMenuWire *w)
{
    w->state = 1;
}
