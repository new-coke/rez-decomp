/*
 * Name entry screen, after a top-ten score attack or Beyond game: three letters picked with
 * left/right against a beat countdown; the record then goes into the ranking table.
 */

#include "name_e.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "fade.h"
#include "fog.h"
#include "game.h"
#include "libc.h"
#include "model.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pad_vib.h"
#include "pnb.h"
#include "result_message.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ne_arrow.h"
#include "ne_camera.h"
#include "ne_char.h"

extern PadSystem pad_system;
extern unsigned int pad_decide_mask;

extern Pad *pad_main;

extern void font_set(int id);

/* for nj_clip_tex_strip_draw */
typedef struct NeTVtx {
    float x, y, z;
    float u, v;
    unsigned int color;
} NeTVtx;

/* 20 bytes, table at name_entry_objs */
typedef struct NeMdl {
    int light;                  /* 1: lit with the model's light */
    int model;                  /* 0x04 */
    int clip;                   /* 0x08: < 0 = none */
    int (*node_cb)(void *n, void *a, void *w);  /* 0x0c: reads marker positions */
    unsigned int color;         /* 0x10 */
} NeMdl;

/* 520 bytes */
typedef struct NameEntry {
    void *models;
    void *textures;             /* 0x04 */
    int ready;                  /* 0x08 */
    ModelPart parts[3];            /* 0x0c */
    int mode;                   /* 0x120: 0 intro, 1 entry, 2 store */
    int state;                  /* 0x124 */
    int timer;                  /* 0x128 */
    int count;                  /* 0x12c: the countdown, half-seconds */
    Vec3 pts[10];               /* 0x130: marker positions read by the model hooks */
    void *cam;                  /* 0x1a8 */
    NeArrow *arrow;             /* 0x1ac */
    int rank;                   /* 0x1b0 */
    RankRec rec;                  /* 0x1b4 */
    int stage;                  /* 0x1d4 */
    int pos;                    /* 0x1d8: the letter being entered */
    int blink;                  /* 0x1dc */
    int sel;                    /* 0x1e0: the selected character */
    NeChar *chars[3];           /* 0x1e4 */
    char score[20];             /* 0x1f0 */
    void *msg;                  /* 0x204 */
} NameEntry;

extern unsigned char end_mark_chars[];
extern char name_entry_textures[];
extern char name_entry_models[];
extern unsigned int name_entry_node_masks_0[];
extern unsigned int name_entry_node_masks_1[];
extern NeMdl name_entry_objs[3];
extern char *rank_labels[][10];
extern unsigned char char_table[];
extern struct TaskDesc name_entry_desc;
extern int label_textures[];
extern int ConfigRankingFind(int t, RankRec *rec);
void *name_entry_create(void *parent);
int name_entry_node_cb_0(void *flags, void *node_idx, NameEntry *w);
int name_entry_node_cb_1(void *flags, void *node_idx, NameEntry *w);
void name_entry_draw_cursor(NameEntry *w);
void name_entry_draw_label(Vec3 *p, int idx);

/* 0x00af73c8 */
void *module_entry(void *parent)
{
    return name_entry_create(parent);
}

/* steer(cast) */
/* 0x00af73e8 */
void name_entry_init(NameEntry *w)
{
    NeMdl *mdl;
    ModelPart *p;
    void *root, *clip;
    int i, game_type, stage, stage_num;

    stage = 0;
    w->textures = model_create_timed(name_entry_textures, 0xb000);
    w->models = model_pnb_register(name_entry_models);
    w->arrow = ne_arrow_create(w);
    system_set_frame_rate(1);
    fog_set_enable(1);
    fog_set(COL_BLACK, 4, 417.0f, 2500.0f);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    pad_input_enable(0);
    fade_start(0, 1, 0);
    fade_start_out(30);
    music_change(11);
    beat_clock_request_tempo(110.0f);
    midiseq_mute_request(0);
    midiseq_play();
    game_type = game_type_get();
    stage_num = stage_get_num();
    if (stage_num == 5)
        stage_num = 4;
    switch (game_type) {
    case 4:
        stage = stage_num;
        break;
    case 5:
        switch ((unsigned int)ConfigGetExtraType()) {
        case 2:
            FINISH(w);
            return;
        case 3:
            stage = 6;
            break;
        case 1:
            stage = 7;
            break;
        case 0:
            stage = 5;
            break;
        }
        break;
    default:
        FINISH(w);
        return;
    }
    w->stage = stage;
    ConfigMakeGameRecord(&w->rec);
    w->rank = ConfigRankingFind(w->stage, &w->rec);
    if (w->rank >= 10) {
        do { FINISH(w); } while (0);
        return;
    }
    format_number_commas(w->score, w->rec.score);
    w->count = 100;
    p = w->parts;
    mdl = name_entry_objs;
    for (i = 3; i != 0; i--) {
        root = pnb_get_entry(w->models, mdl->model)->src;
        if (mdl->clip < 0)
            clip = 0;
        else
            clip = pnb_get_entry(w->models, mdl->clip)->src;
        dc_object_init((struct MapPart *)p, root, clip, 0);
        if (mdl->light == 1)
            dc_object_set_wire((struct MapPart *)p, model_pnb_get_wire(w->models, mdl->model));
        if (mdl->node_cb)
            dc_object_set_callback(p, mdl->node_cb, w);
        mdl++;
        p++;
    }
    w->ready = 1;
    peri_act_select_reset(1);
}

/* 0x00af7688 */
void name_entry_release(NameEntry *w)
{
    ModelPart *p;
    int n;

    peri_act_select_reset(0);
    midiseq_stop();
    if (w->ready) {
        p = w->parts;
        for (n = 3; n != 0; n--) {
            dc_object_release((struct MapPart *)p);
            p++;
        }
    }
    model_list_remove(w->textures);
    model_free_all_buffers_checked(w->models);
}

/* 0x00af76f8 */
void name_entry_step_intro(NameEntry *w)
{
    switch (w->state) {
    case 0:
        if (w->cam && !(TASK_OF(w->cam)->flags & TASK_F_END)) FINISH_IF(w->cam);
        do { w->cam = demo_camera_create(w, 0); w->state++; } while (0);
        break;
    case 1:
        w->mode = 1;
        w->state = 0;
        break;
    }
}

/* 0x00af7788 */
void name_entry_step_entry(NameEntry *w)
{
    NeCharArg prm;
    int sel, done;
    unsigned char ch;

    done = 0;
    sel = w->sel;
    if (pad_system.pad[0]->menu.rep & PAD_RIGHT) {
        ne_arrow_light(w->arrow, 1);
        sel++;
    }
    if (pad_system.pad[0]->menu.rep & PAD_LEFT) {
        sel--;
        ne_arrow_light(w->arrow, 0);
    }
    if (w->pos == 3)
        sel = 47;
    if (w->sel != sel) {
        if (!(sel > -1))
            sel = 47;
        if (!((unsigned int)sel < 48))
            sel = 0;
        w->sel = sel;
        SoundRhythmSe_Common(0);
    }
    if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
        ch = char_table[w->sel];
        if (ch == 197) {
            done = 1;
            SoundRhythmSe_Common(7);
            midiseq_stop();
        } else {
            SoundRhythmSe_Common(1);
            prm.life = 30;
            prm.scale = 100.0f;
            prm.color = RGBA(0xff, 0xff, 0xff, 0x80);
            { Vec3 *p = &w->pts[6 + w->pos]; prm.pos.x = p->x; prm.pos.y = p->y; prm.pos.z = p->z; }
            prm.ch = ch;
            if (w->chars[w->pos] && !(TASK_OF(w->chars[w->pos])->flags & TASK_F_END)) {
                ne_char_dismiss(w->chars[w->pos]);
                w->chars[w->pos] = 0;
            }
            w->chars[w->pos] = ne_char_create(w, &prm);
            w->rec.name[w->pos] = ch;
            w->pos = w->pos + 1 < 4 ? w->pos + 1 : 3;
            if (w->pos == 3) {
                ne_arrow_set_enabled(w->arrow, 0, 0);
                ne_arrow_set_enabled(w->arrow, 1, 0);
                w->sel = 47;
            }
        }
    }
    if (pad_system.pad[0]->menu.trig & pad_cancel_mask) {
        w->pos = w->pos - 1 > -1 ? w->pos - 1 : 0;
        SoundRhythmSe_Common(6);
        if (w->chars[w->pos] && !(TASK_OF(w->chars[w->pos])->flags & TASK_F_END)) {
            ne_char_dismiss(w->chars[w->pos]);
            w->chars[w->pos] = 0;
            w->rec.name[w->pos] = ' ';
        }
        ne_arrow_set_enabled(w->arrow, 0, 1);
        ne_arrow_set_enabled(w->arrow, 1, 1);
    }
    switch (w->state) {
    case 0:
        w->state = 1;
        break;
    case 1:
        if (!(pad_system.pad[0]->menu.on & PAD_L1) && beat_grid_hit(0) && --w->count < 0) {
            done = 1;
            w->count = 0;
        }
        if (pad_system.pad[0]->menu.trig & PAD_START)
            done = 1;
        break;
    }
    if (done) {
        w->state = 0;
        w->mode = 2;
    }
}

/* 0x00af7ae0 */
void name_entry_step_store(NameEntry *w)
{
    switch (w->state) {
    case 0:
        ConfigRankingInsert(w->stage, &w->rec);
        if (w->cam && !(TASK_OF(w->cam)->flags & TASK_F_END)) FINISH_IF(w->cam);
        do { w->cam = demo_camera_create(w, 1); w->timer = 180; w->state++; } while (0);
        break;
    case 1:
        if (--w->timer <= 0) {
            w->msg = result_message_disp_create(w, 1);
            w->state++;
        }
        if (pad_main->menu.trig & (pad_cancel_mask | pad_decide_mask | 8))
            w->timer = 0;
        break;
    case 2:
        if (result_message_is_done(w->msg)) {
            fade_start(0, 1, 30);
            w->state++;
        }
        break;
    case 3:
        if (fade_is_idle())
            FINISH_IF(w);
        break;
    }
}

/* 0x00af7c50 */
void name_entry_exec(NameEntry *w)
{
    ModelPart *p;
    int n;

    if (w->ready) {
        p = w->parts;
        for (n = 3; n != 0; n--) {
            dc_object_advance_time((struct MapPart *)p);
            p++;
        }
    }
    switch ((unsigned int)w->mode) {
    case 0:
        name_entry_step_intro(w);
        break;
    case 1:
        name_entry_step_entry(w);
        break;
    case 2:
        name_entry_step_store(w);
        break;
    }
}

/* 0x00af7d10 */
void name_entry_disp(NameEntry *w)
{
    Vec3 pos, step;
    char buf[16];
    NeMdl *mdl;
    ModelPart *p;
    int n, lang = 0; /* steer(dead) */
    unsigned char ch = 0;
    float up = 0;

    ulGxSetTextureList(w->textures);
    if (!w->ready)
        return;
    lang = game_lang;
    mdl = name_entry_objs;
    p = w->parts;
    for (n = 3; n != 0; n--) {
        draw_set_color(mdl->color);
        mdl++;
        dc_object_draw((struct MapPart *)p);
        p++;
    }
    up = 21.0f;
    name_entry_draw_cursor(w);
    name_entry_draw_label(&w->pts[0], 0);
    name_entry_draw_label(&w->pts[1], 1);
    name_entry_draw_label(&w->pts[2], 2);
    font_set(3);
    step.x = 36.0f;
    step.y = 0.0f;
    step.z = 0.0f;
    sprintf(buf, "%2d", w->count >> 1);
    { Vec3 *q = &w->pts[3]; float x = q->x, y = q->y, z = q->z; pos.x = x - 54.0f; pos.y = y + up; pos.z = z; }
    font_print3d((unsigned char *)buf, &pos, &step, 0, 0, COL_WHITE);
    { Vec3 *q = &w->pts[4]; float x = q->x, y = q->y, z = q->z; pos.z = z; pos.x = x - 126.0f; pos.y = y + up; }
    font_print3d((unsigned char *)rank_labels[lang][w->rank], &pos, &step, 0, 0, COL_WHITE);
    { Vec3 *q = &w->pts[9]; float x = q->x, y = q->y, z = q->z; pos.x = x; pos.y = y + up; pos.z = z; }
    font_print3d_rev(w->score, &pos, &step, 0, 0, COL_WHITE);
    ch = char_table[w->sel];
    if (ch == 197)
        ch = end_mark_chars[lang];
    { Vec3 *q = &w->pts[5]; float x = q->x, y = q->y, z = q->z; pos.x = x; pos.y = y; pos.z = z; }
    font_draw_char3d(ch, &pos, 0, 0, COL_WHITE);
}

/* 0x00af7fd8 */
void *name_entry_create(void *parent)
{
    return task_create(parent, &name_entry_desc, 520);
}

/* Stores the flagged marker nodes' camera-space positions in w->pts. */
/* 0x00af8000 */
void name_entry_node_read(unsigned int *node_flags, void *node_idx, NameEntry *w, int which)
{
    Vec3 *out;
    unsigned int *mask;
    unsigned int flags;
    float *mtx;
    int i;

    if (!which) {
        out = &w->pts[0];
        mask = name_entry_node_masks_0;
    } else {
        out = &w->pts[3];
        mask = name_entry_node_masks_1;
    }
    flags = *node_flags;
    for (i = 0; i < 7; i++) {
        mtx = camera_get_inv_mtx();
        if (flags & mask[i]) {
            ulMatrixGetTranslation(ulMatrixCur, &out[i]);
            ulMatrixApplyPoint(mtx, &out[i], &out[i]);
        }
    }
}

/* 0x00af80c0 */
int name_entry_node_cb_0(void *flags, void *node_idx, NameEntry *w)
{
    name_entry_node_read(flags, node_idx, w, 0);
    return 1;
}

/* 0x00af80e0 */
int name_entry_node_cb_1(void *flags, void *node_idx, NameEntry *w)
{
    name_entry_node_read(flags, node_idx, w, 1);
    return 1;
}

/* 0x00af8100 */
void name_entry_draw_cursor(NameEntry *w)
{
    TrailVtx vtx[4];
    Vec3 *p;
    unsigned int col;
    float x, y, z, x_min, x_max, y_min, y_max;

    if (w->pos < 3) {
        do { w->blink += 10; draw_set_blend(4, 5); col = (w->blink % 40 + 51) >> 1 << 24 | 0xc8c8c8; } while (0);
        do { p = &w->pts[6 + w->pos]; x = p->x; y = p->y; z = p->z; } while (0);
        z = z - 1.0f;
        x_min = x - 50.0f;
        vtx[0].x = x_min;
        vtx[3].c = col;
        vtx[2].c = col;
        y_max = y + 50.0f;
        y_min = y - 50.0f;
        vtx[0].y = y_max;
        vtx[0].q = z;
        vtx[1].y = y_min;
        vtx[1].q = z;
        x_max = x + 50.0f;
        vtx[2].x = x_max;
        vtx[3].x = x_max;
        vtx[3].y = y_min;
        vtx[2].q = z;
        vtx[3].q = z;
        vtx[1].x = x_min;
        vtx[2].y = y_max;
        vtx[1].c = col;
        vtx[0].c = col;
        nj_clip_color_strip_begin(1);
        nj_clip_color_strip_vertices(vtx, 4);
        nj_clip_color_strip_end();
    }
}

/* 0x00af8218 */
void name_entry_draw_label(Vec3 *p, int idx)
{
    float rects[3][4] = {{0.0f, 0.0f, 256.0f, 34.0f}, {0.0f, 34.0f, 256.0f, 28.0f}, {0.0f, 238.0f, 256.0f, 18.0f}};
    NeTVtx vtx[4];
    float tu, tv, width, height;

    tu = rects[idx][0];
    tv = rects[idx][1];
    width = rects[idx][2];
    height = rects[idx][3];
    vtx[1].x = vtx[0].x = p->x - width * 0.5f;
    vtx[3].x = vtx[2].x = p->x + width * 0.5f;
    vtx[2].y = vtx[0].y = p->y + height * 0.5f;
    vtx[3].y = vtx[1].y = p->y - height * 0.5f;
    vtx[3].z = vtx[2].z = vtx[1].z = vtx[0].z = p->z;
    vtx[1].u = vtx[0].u = (tu + 0.5f) * 0.00390625f;
    vtx[3].u = vtx[2].u = (tu + width + 0.5f) * 0.00390625f;
    vtx[2].v = vtx[0].v = (tv + 0.5f) * 0.00390625f;
    vtx[3].v = vtx[1].v = (tv + height + 0.5f) * 0.00390625f;
    vtx[3].color = vtx[2].color = vtx[1].color = vtx[0].color = COL_WHITE;
    ulGxSelectTexture(ulGxGetTextureList(), label_textures[game_lang]);
    nj_clip_tex_strip_draw(vtx, 4, 1);
}
