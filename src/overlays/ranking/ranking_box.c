/* Score ranking screen: a board with a header and ten rows, started one after another. */

#include "ranking.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "model.h"
#include "pnb.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ranking_box.h"
#include "ranking_bar.h"

/* per language */
extern unsigned char *table_titles[][6];

extern struct TaskDesc box_desc;

/* Stores the title node's camera-space position in w->title. */
/* 0x00ca0700 */
int box_node_cb(unsigned int *flags, int node_idx, Box *w)
{
    Vec3 v;
    float *mtx;

    if (*flags & 0x40000000) {
        mtx = camera_get_inv_mtx();
        ulMatrixGetTranslation(ulMatrixCur, &v);
        ulMatrixApplyPoint(mtx, &v, &w->title);
    }
    return 1;
}

/* 0x00ca0768 */
void box_init(Box *w, int page, int table)
{
    int i;

    w->table = table;
    w->pos.y = 0;
    w->pos.z = 0;
    w->pos.x = (float)page * 1200.0f;
    dc_object_init(&w->part, pnb_get_entry(w->models, 10)->src, 0, 0);
    dc_object_set_callback(&w->part, box_node_cb, w);
    w->shown = 0;
    w->rows[0] = bar_create(w, -1, table, &w->pos, 2);
    for (i = 0; i < 10; i++)
        w->rows[i + 1] = bar_create(w, i, table, &w->pos, 0);
}

/* 0x00ca0868 */
void box_release(Box *w)
{
    dc_object_release(&w->part);
}

/* 0x00ca0870 */
void box_exec(Box *w)
{
    int n;
    int i;

    if (w->timer > 0) {
        w->timer--;
    } else {
        w->timer = 5;
        n = ++w->shown;
        i = 11 - n;
        do {
            bar_play(w->rows[i], 1);
            i++;
        } while (--n);
        if (w->shown >= 11)
            TASK_OF(w)->exec = 0;
    }
}

/* 0x00ca0910 */
void box_disp(Box *w)
{
    int lang = game_lang;
    unsigned char *s;
    int n;
    Vec3 pos;
    Vec3 adv;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    dc_object_draw(&w->part);
    ulMatrixPop();
    s = table_titles[w->table][lang];
    n = line_length(s) - 1;
    font_set(3);
    v3_copy(&pos, &w->title);
    pos.x -= (float)n * 30.0f * 0.5f;
    pos.y -= 42.0f;
    adv.x = 30.0f;
    adv.y = 0;
    adv.z = 0;
    font_print3d(s, &pos, &adv, 0, 0, COL_WHITE);
}

/* 0x00ca0a38 */
Box *box_create(void *parent, int page, int table)
{
    Box *w = task_create_inherit(parent, &box_desc, 180);

    box_init(w, page, table);
    return w;
}
