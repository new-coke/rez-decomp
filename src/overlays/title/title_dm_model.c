/*
 * Title screen: attract demo of up to three model screens joined by wipes; the "BG" task then
 * fades in the title picture (title sound after START) and ends the demo.
 */

#include "title_shared.h"
#include "camera.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "heap.h"
#include "mathdef.h"
#include "model.h"
#include "node_draw_state.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_pcmstream.h"
#include "task.h"
#include "ul_gx.h"
#include "wipefade.h"
#include "title_dm_model.h"
#include "title.h"

#include "color.h"

/* 24 bytes; tables in demo_screens */
typedef struct ScrObj {
    int light;                  /* non-zero: lit */
    int color;
    int model, clip, motion, motion2;   /* model-file indices; < 0 none */
} ScrObj;

typedef struct ScrDef {         /* 20 bytes */
    int fog_color;
    float fog_near, fog_far;
    ScrObj *objs;
    int count;
} ScrDef;

/* one demo screen, 32 bytes */
typedef struct DemoModel {
    void *models;               /* 0x00: inherited */
    void *textures;             /* 0x04: inherited */
    Part *parts;                /* 0x08 */
    int count;                  /* 0x0c */
    float remain;               /* 0x10: longest remaining motion */
    CameraWork *cam;                   /* 0x14 */
    int screen;                 /* 0x18 */
    int fog_frames;             /* 0x1c */
} DemoModel;

/* 28 bytes */
typedef struct TitleDemo {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int state;                  /* 0x08 */
    int loading;                /* 0x0c: file requests still pending */
    int count;                  /* 0x10: screens shown */
    void *child;                /* 0x14: the current screen's "DemoModel" task */
    void *wipe;                 /* 0x18: the screen-change wipe task */
} TitleDemo;

extern struct TaskDesc bg_desc;
extern struct TaskDesc title_demo_desc;

/* title_dm.pnb, title_dm.ptm */
extern int title_demo_files[];

extern char pcm_s_title1[];

extern char pcm_s_title2[];

/* model indices per screen */
extern int demo_camera_motions[][2];

extern char demo_camera_hooks[];

extern struct TaskDesc demo_camera_desc;
extern struct TaskDesc demo_model_desc;
extern ScrDef demo_screens[];

/* 0x00af6700 */
void bg_init(BgWork *w, int sound)
{
    TitleTasks *tt = title_get_tasks();

    if (tt)
        tt->bg = w;
    if (sound)
        pcm_play(pcm_s_title2, 1);
    w->alpha = 0;
    task_set_dlist(w, 5);
}

/* 0x00af6758 */
void bg_release(void)
{
    TitleTasks *tt = title_get_tasks();

    if (tt)
        tt->bg = 0;
}

/* 0x00af6780 */
void bg_exec(BgWork *w)
{
    int a = w->alpha + 4;

    w->alpha = a < 256 ? a : 255;
}

/* 0x00af67a0 */
void bg_disp(BgWork *w)
{
    ulGxSetTextureList(w->textures);
    draw_set_blend(4, 5);
    bg_draw_image(RGBA(0x80, 0x80, 0x80, (w->alpha + 1) >> 1));
}

/* 0x00af67f0 */
void *bg_create(int sound)
{
    void *parent = title_get_task();
    void *bg;

    bg = title_get_tasks()->bg;
    if (!bg)
        return task_create_inherit_arg(parent, &bg_desc, 12, sound);
    return bg;
}

/* 0x00af6848 */
void bg_draw_image(int color)
{
    screen_image_draw(0, color, 2.2222223f);
}

/* 0x00af6860 */
int bg_is_opaque(BgWork *w)
{
    return w->alpha >= 255;
}

/* 0x00af6870 */
void title_demo_init(TitleDemo *w)
{
    w->loading = 0;
}

/* 0x00af6878 */
void title_demo_release(TitleDemo *w)
{
    if (w->loading) {
        file_request_cancel(&title_demo_files[0]);
        file_request_cancel(&title_demo_files[2]);
    } else {
        model_unload_timed(w->textures);
        model_pnb_unload(w->models);
        w->textures = 0;
        w->models = 0;
    }
    if (w->wipe)
        FINISH(w->wipe);
}

/* steer(cast) */
/* 0x00af68f8 */
void title_demo_exec_play(TitleDemo *w)
{
    if (bg_is_done()) {
        FINISH_IF(w);
        return;
    }
    switch (w->state) {
    case 0:
        w->count = 0;
        title_demo_preload_lights(w);
        w->child = demo_model_create(w, w->count);
        w->state++;
        break;
    case 1:
        pcm_play(pcm_s_title1, 1);
        fade_start_out(120);
        w->state = 100;
        break;
    case 100:
        if (demo_model_get_remaining(w->child) <= 0) {
            if (!bg_exists() && ++w->count < 3) {
                w->wipe = wipe_start(0, 0, 0);
                w->state++;
            } else {
                w->state = 200;
            }
        }
        break;
    case 101:
        if (wipe_is_covered(w->wipe)) {
            ((void (*)(void *, int, int))wipe_finish)(w->wipe, 90, 255);
            if (w->child)
                FINISH(w->child);
            w->child = demo_model_create(w, w->count);
            w->state++;
        }
        break;
    case 102:
        if (wipe_is_done(w->wipe)) {
            w->state = 100;
            w->wipe = 0;
        }
        break;
    case 200:
        if (demo_model_get_remaining(w->child) <= 0) {
            w->state++;
            bg_create(1);
        }
        break;
    }
}

/* 0x00af6ac8 */
void title_demo_exec(TitleDemo *w)
{
    void *models;
    void *textures;

    switch (w->state) {
    case 0:
        file_list_request(title_demo_files);
        w->loading = 1;
        w->state++;
        break;
    case 1:
        if (file_list_is_loaded(title_demo_files)) {
            w->loading = 0;
            do {
                models = model_pnb_load(title_demo_files);
            } while (0);
            textures = model_load_timed(&title_demo_files[2], 0x5100);
            w->models = models;
            w->textures = textures;
            if (bg_exists()) {
                TASK_OF(w)->exec = 0;
            } else {
                w->state = 0;
                TASK_OF(w)->exec = (void (*)(void *))title_demo_exec_play;
            }
        }
        break;
    }
}

/* 0x00af6b98 */
void title_demo_disp(void)
{
}

/* 0x00af6ba0 */
void *title_demo_create(void *parent)
{
    return task_create(parent, &title_demo_desc, 28);
}

/* 0x00af6bc8 */
void demo_camera_node_init(void)
{
}

/* 0x00af6bd0 */
void demo_camera_node_update(Camera *out, CameraWork *w)
{
    *out = w->cam;
}

/* 0x00af6c28 */
void demo_camera_node_release(void)
{
}

/* 0x00af6c30 */
void demo_camera_init(CameraWork *w, int screen)
{
    int *ids = demo_camera_motions[screen];
    Camera *cur = camera_get_current();

    w->motion = pnb_get_entry(w->models, ids[0])->src;
    if (ids[1] >= 0) {
        w->target = pnb_get_entry(w->models, ids[1])->src;
        w->end = (float)*((unsigned int *)w->target + 1) - 1.0f;
    } else {
        w->target = 0;
        w->end = 0;
    }
    w->node = camera_node_add(2, demo_camera_hooks, w);
    w->cam = *cur;
    w->t = 0;
    w->dt = FPS / game_state.fps;
}

/* 0x00af6d80 */
void demo_camera_release(CameraWork *w)
{
    camera_node_remove(w->node);
}

/* 0x00af6d88 */
void demo_camera_exec(CameraWork *w)
{
    camera_eval_motion(&w->cam, w->motion, w->target, w->t);
    w->t += w->dt;
    if (w->end <= w->t)
        w->t = w->end;
}

/* 0x00af6de0 */
void demo_camera_disp(void)
{
}

/* 0x00af6de8 */
void *demo_camera_create(void *parent, int screen)
{
    return task_create_inherit_arg(parent, &demo_camera_desc, 72, screen);
}

/* 0x00af6e10 */
float demo_camera_get_remaining(CameraWork *w)
{
    return w->end - w->t;
}

/* 0x00af6e20 */
void title_demo_preload_lights(TitleDemo *w)
{
    unsigned int i;
    int j;
    ScrDef *scr;
    ScrObj *obj;

    for (i = 0; i < 3; i++) {
        scr = &demo_screens[i];
        obj = scr->objs;
        for (j = 0; j < scr->count; j++, obj++)
            if (obj->light)
                model_pnb_get_wire(w->models, obj->model);
    }
}

/* 0x00af6ee8 */
void demo_model_init(DemoModel *w, int screen)
{
    ScrDef *scr = &demo_screens[screen];
    ScrObj *obj;
    Part *p;
    int n;
    void *root;
    void *clip;
    void *mot;

    w->screen = screen;
    {
        Part *buf = heap_block_tag(game_malloc((w->count = scr->count) * 96), "title_dm_model.c", 226);
        obj = scr->objs;
        w->parts = buf;
        p = buf;
    }
    n = w->count;
    do {
        root = pnb_get_entry(w->models, obj->model)->src;
        clip = obj->clip >= 0 ? pnb_get_entry(w->models, obj->clip)->src : 0;
        if (obj->motion >= 0) {
            void *mot2;

            mot = pnb_get_entry(w->models, obj->motion)->src;
            mot2 = pnb_get_entry(w->models, obj->motion2)->src;
            dc_object_init_cursor(p, root, clip, mot, mot2, 0);
        } else {
            dc_object_init(p, root, clip, 0);
        }
        if (obj->light)
            dc_object_set_wire(p, model_pnb_get_wire(w->models, obj->model));
        else
            dc_object_rebuild(p);
        p->color = obj->color;
        p->rate = FPS / game_state.fps;
        obj++;
        p++;
    } while (--n);
    w->cam = demo_camera_create(w, screen);
    w->fog_frames = 3;
    do {
        fog_set(scr->fog_color, 4, scr->fog_near, scr->fog_far);
    } while (0);
    fog_set_enable(1);
    BackColReq(scr->fog_color);
}

/* steer(cast) */
/* 0x00af70d0 */
void demo_model_release(DemoModel *w)
{
    Part *p;
    int n;

    ((void (*)(unsigned int))fog_req_color)(COL_BLACK);
    BackColReq(COL_BLACK);
    p = w->parts;
    n = w->count;
    do {
        dc_object_release(p);
        p++;
    } while (--n);
    heap_free(w->parts, 0);
}

/* 0x00af7140 */
void demo_model_exec(DemoModel *w)
{
    Part *p;
    int n;
    float rem, t;
    ScrDef *scr;

    w->remain = 0;
    p = w->parts;
    n = w->count;
    do {
        dc_object_advance_time(p);
        t = p->t;
        rem = p->end - t;
        if (rem <= w->remain)
            rem = w->remain;
        w->remain = rem;
        p++;
    } while (--n);
    if (w->fog_frames > 0) {
        w->fog_frames--;
        scr = &demo_screens[w->screen];
        do {
            fog_set(scr->fog_color, 4, scr->fog_near, scr->fog_far);
        } while (0);
        fog_set_enable(1);
        BackColReq(scr->fog_color);
    }
}

/* 0x00af7220 */
void demo_model_disp(DemoModel *w)
{
    Part *p;
    int n;

    ulGxSetTextureList(w->textures);
    p = w->parts;
    n = w->count;
    do {
        draw_set_color(p->color);
        dc_object_draw(p);
        p++;
    } while (--n);
}

/* 0x00af7278 */
void *demo_model_create(void *parent, int screen)
{
    return task_create_inherit_arg(parent, &demo_model_desc, 32, screen);
}

/* 0x00af72a0 */
float demo_model_get_remaining(DemoModel *w)
{
    float rem = demo_camera_get_remaining(w->cam);

    return rem <= w->remain ? w->remain : rem;
}
