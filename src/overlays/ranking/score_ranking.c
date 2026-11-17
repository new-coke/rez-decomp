/*
 * Score ranking screen: from the options menu the player pages through open tables; after a game
 * it shows that mode's table for ten seconds. Any button fades out.
 */

#include "ranking.h"
#include "task.h"
#include "color.h"
#include "config.h"
#include "fade.h"
#include "fog.h"
#include "game.h"
#include "model.h"
#include "pad.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"
#include "ranking_bar.h"
#include "ranking_box.h"

/* 40 bytes */
typedef struct ScoreRanking {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int timer;                  /* 0x08: frames left when not paged by the player */
    int step;                   /* 0x0c */
    Box *box;                   /* 0x10 */
    Box *next;                  /* 0x14 */
    PageCam *cam;                     /* 0x18 */
    int paged;                  /* 0x1c: 1 = the player pages through the tables */
    int page;                   /* 0x20: also the first table shown */
    int table;                  /* 0x24 */
} ScoreRanking;

extern GameState game_state;
extern PadSystem pad_system;
extern unsigned int pad_decide_mask;

extern int system_ptm;
extern float page_arrow_y;
extern float screen_layout[];

extern char score_ranking_textures[];

extern char score_ranking_models[];

/* game_state.setting.by.area_table entry that opens each table */
extern int table_area_index[8];

extern struct TaskDesc score_ranking_desc;

int table_is_available(unsigned int table);
void *score_ranking_create(void *parent);

/* 0x00ca15b8 */
void *module_entry(void *parent)
{
    return score_ranking_create(parent);
}

/* Open once its area or mode is unlocked. */
/* 0x00ca15d8 */
int table_is_available(unsigned int table)
{
    switch (table) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
        return game_state.setting.by.area_table[table_area_index[table]];
    case 5:
        return ConfigGetSetting(0) > 0;
    }
    return 0;
}

/* steer(cast) */
/* 0x00ca1650 */
void score_ranking_init(ScoreRanking *w)
{
    int game_type;
    int stage_num;
    int page;
    int paged;
    int extra;

    w->textures = model_create_timed(score_ranking_textures, 0xc000);
    w->models = model_pnb_register(score_ranking_models);
    game_type = game_type_get();
    stage_num = stage_get_num();
    if (stage_num == 5)
        stage_num = 4;
    switch (game_type) {
    case 4:
        page = stage_num;
        paged = 0;
        break;
    case 5:
        extra = ConfigGetExtraType();
        switch ((unsigned int)extra) {
        case 1: page = 7; break;
        case 0: page = 5; break;
        case 3: page = 6; break;
        default: page = 5; break;
        }
        paged = 0;
        break;
    default:
        page = 0;
        paged = 1;
        break;
    }
    w->page = page;
    w->paged = paged;
    w->timer = 600;
    system_set_frame_rate(1);
    fog_set_enable(1);
    fog_set(COL_BLACK, 4, 417.0f, 750.0f);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    pad_input_enable(0);
    w->cam = demo_camera_create(w);
    w->box = box_create(w, 0, w->page);
    fade_start(0, 1, 0);
}

/* 0x00ca1790 */
void score_ranking_release(ScoreRanking *w)
{
    model_list_remove(w->textures);
    model_free_all_buffers_checked(w->models);
}

/* 0x00ca17c0 */
void score_ranking_exec(ScoreRanking *w)
{
    int page;
    int table;

    switch (w->step) {
    case 0:
        w->step = 1;
        break;
    case 1:
        fade_start_out(30);
        w->step = 100;
        break;
    case 100:
        if (w->paged) {
            page = w->page;
            table = w->table;
            if (pad_system.pad[0]->menu.trig & PAD_RIGHT) {
                do { page++; } while (0);
            up:
                table++;
                if (table >= 8)
                    table = 0;
                if (!table_is_available(table))
                    goto up;
            }
            if (pad_system.pad[0]->menu.trig & PAD_LEFT) {
                do { page--; } while (0);
            down:
                table--;
                if (table < 0)
                    table = 7;
                if (!table_is_available(table))
                    goto down;
            }
            if (w->page != page) {
                do { demo_camera_request_page(w->cam, w->page < page);
                w->page = page;
                w->table = table;
                w->next = box_create(w, page, table);
                w->step = 101; } while (0);
            }
        } else if (w->timer-- <= 0) {
            w->step = 102;
        }
        break;
    case 101:
        if (w->page == demo_camera_get_page(w->cam)) {
            FINISH_IF(w->box);
            w->box = w->next;
            w->step = 100;
        }
        break;
    case 102:
        fade_start(0, 1, 30);
        w->step = 103;
        break;
    case 103:
        if (fade_is_idle())
            FINISH_IF(w);
        break;
    }
    do {
        if (pad_system.pad[0]->menu.trig & (pad_cancel_mask | pad_decide_mask | 8)) {
            if (w->step < 102)
                w->step = 102;
        }
    } while (0);
}

/* 0x00ca1a70 */
void score_ranking_disp(ScoreRanking *w)
{
    int col;
    float y;
    float depth;

    if (w->paged) {
        col = game_menu_color;
        ulGxSetTextureList((void *)system_ptm);
        depth = 1.0526316f;
        y = page_arrow_y;
        sprite_draw(screen_layout[6], y, depth, col, 19, 0, 1);
        sprite_draw(screen_layout[7], y, depth, col, 20, 2, 1);
    }
}

/* 0x00ca1b38 */
void *score_ranking_create(void *parent)
{
    return task_create(parent, &score_ranking_desc, 40);
}
