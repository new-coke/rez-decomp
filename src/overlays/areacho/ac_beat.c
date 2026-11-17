/*
 * Area choose screen: beat-driven marks beside each area. Their nodes appear as the area's best
 * analysis reaches 50%, 90% and 100%.
 */

#include "areacho.h"
#include "config.h"
#include "model.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ac_beat.h"

/* 776 bytes */
typedef struct AcBeat {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart parts[8];            /* 0x08: two per area, shown by the area's record */
} AcBeat;

extern struct TaskDesc ac_beat_desc;

/* Hides nodes for analysis rates the area has not reached. */
/* 0x00ca0700 */
int ac_beat_show_cb(unsigned int *flags, int node_idx, int area)
{
    float *rec;
    unsigned int fl;
    int show = 1;

    rec = ConfigGetStageRecord(area);
    fl = *flags;

    if (fl & 0x2000000 && rec[0] < 50.0f)
        show = 0;
    if (fl & 0x4000000 && rec[0] < 90.0f)
        show = 0;
    if (fl & 0x8000000 && rec[0] < 100.0f)
        show = 0;
    if (fl & 0x10000000 && rec[0] < 100.0f)
        show = 0;
    return show;
}

/* steer(cast) */
/* 0x00ca07f0 */
void ac_beat_init(AcBeat *w)
{
    ModelPart *p;
    int n, root_idx, clip_idx, light, root, clip, clip_a, clip_b;
    int area_no;

    p = w->parts;
    area_no = 0;
    root_idx = 25;
    n = 8;
    clip_idx = 26;
    do {
        light = model_pnb_get_wire(w->models, root_idx);
        root = *((int *)pnb_get_entry(w->models, root_idx));
        root_idx += 4;
        clip = *((int *)pnb_get_entry(w->models, clip_idx));
        clip_a = *((int *)pnb_get_entry(w->models, clip_idx + 1));
        clip_b = *((int *)pnb_get_entry(w->models, clip_idx + 2));
        clip_idx += 4;
        dc_object_init_cursor(p, root, clip, clip_a, clip_b, 4096);
        dc_object_set_callback(p, (int (*)())ac_beat_show_cb, (void *)area_no);
        if (n-- & 1) {
            dc_object_set_wire(p, light);
            area_no++;
        }
        p++;
    } while (n);
}

/* 0x00ca0928 */
void ac_beat_release(AcBeat *w)
{
    ModelPart *p = w->parts;
    int n;

    for (n = 8; n != 0; n--) {
        dc_object_release(p);
        p++;
    }
}

/* 0x00ca0968 */
void ac_beat_exec(AcBeat *w)
{
    ModelPart *p;
    int n;
    float bar = beat_clock_get_bar();

    p = w->parts;
    for (n = 8; n != 0; n--) {
        p->frame = p->frames * bar;
        p++;
    }
}

/* 0x00ca09b8 */
void ac_beat_disp(AcBeat *w)
{
    ModelPart *p = w->parts;
    int n;
    float prev_lw = line_width_set(2.0f);

    for (n = 8; n != 0; n--) {
        dc_object_draw(p);
        p++;
    }
    line_width_set(prev_lw);
}

/* 0x00ca0a18 */
void *ac_beat_create(void *parent)
{
    return task_create_inherit(parent, &ac_beat_desc, 776);
}
