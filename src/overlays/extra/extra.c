/*
 * Beyond menu module (the title's extra modes): runs the menu pages; a chosen mode resets the
 * game status and starts its stage. Leaving the top page returns to the title.
 */

#include "extra_shared.h"
#include "task.h"
#include "color.h"
#include "console.h"
#include "enemy_status.h"
#include "fade.h"
#include "fog.h"
#include "game.h"
#include "model.h"
#include "module.h"
#include "player_status.h"
#include "sequence.h"
#include "stage.h"
#include "system.h"
#include "ex_menu.h"


extern struct TaskDesc ex_desc;

void *ex_create(void *parent);

/* 28 bytes */
typedef struct ExMain {
    void *models;
    void *textures;             /* 0x04 */
    ExMenu *menu;               /* 0x08 */
    void *logio;                /* 0x0c */
    int res;                    /* 0x10: the menu's result */
    int state;                  /* 0x14 */
    int saved;                  /* 0x18: console_set_disp_level */
} ExMain;

extern char ex_textures[];
extern char ex_models[];
extern unsigned char *direct_assault_names[];
extern unsigned char *stage_login_names[][11];

/* 0x00ca27a0 */
void *module_entry(void *parent)
{
    return ex_create(parent);
}

/* 0x00ca27c0 */
void ex_load(ExMain *w)
{
    w->textures = model_create_timed(ex_textures, 0xf000);
    w->models = model_pnb_register(ex_models);
}

/* 0x00ca2808 */
void ex_unload(ExMain *w)
{
    model_list_remove(w->textures);
    model_free_all_buffers_checked(w->models);
}

/* steer(cast) */
/* 0x00ca2838 */
void ex_init(ExMain *w)
{
    int clear;

    clear = module_get_clear_flag(2);
    do {
        fog_set(COL_BLACK, 4, 1000.0f, 4000.0f);
    } while (0);
    fog_set_enable(1);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    w->saved = console_set_disp_level(0);
    ex_load(w);
    w->menu = ex_menu_create(w, clear >> 4, clear & 15);
    system_set_frame_rate(1);
    fade_start_out(120);
}

/* 0x00ca28d0 */
void ex_release(ExMain *w)
{
    ex_unload(w);
    console_set_disp_level(w->saved);
}

/* 0x00ca28f8 */
void ex_exec(ExMain *w)
{
    int prm[5];
    int lang;

    lang = game_state.lang;
    switch (w->state) {
    case 0:
        switch ((unsigned int)ex_menu_get_result(w->menu, &w->res)) {
        case 1:
            w->state = 400;
            break;
        case 0:
            break;
        case 2:
            w->state = 500;
            break;
        }
        break;
    case 400:
        player_status_init_game();
        EnemyStatusInitGame();
        w->saved = console_set_disp_level(0);
        game_task_request(module_get_current());
        stage_next_set(w->res);
        module_stage_setup(w);
        w->state++;
        break;
    case 401:
    case 600:
        fade_start(0, 1, 120);
        w->state++;
        break;
    case 402:
        if (fade_is_idle())
            w->state++;
        break;
    case 403:
        prm[0] = (int)(w->res == 0 ? direct_assault_names[lang] : stage_login_names[lang][w->res]);
        prm[1] = game_state.menu_color;
        prm[3] = 120;
        prm[2] = 15;
        prm[4] = 15;
        w->logio = logio_create(w, prm);
        w->state++;
        break;
    case 404:
        if (!w->logio || TASK_OF(w->logio)->flags & TASK_F_END)
            FINISH_IF(w);
        break;
    case 500:
        if (w->res < 0)
            w->state = 600;
        fade_start(0, 1, 15);
        w->state++;
        break;
    case 501:
        if (fade_is_idle()) {
            FINISH_IF(w->menu);
            w->menu = ex_menu_create(w, w->res, 0);
            fade_start_out(15);
            w->state = 0;
        }
        break;
    case 601:
        if (fade_is_idle()) {
            module_set_clear_flag(0, 4);
            sound_sequence_set_next(2);
            FINISH_IF(w);
            w->state++;
        }
        break;
    }
}

/* 0x00ca2be0 */
void ex_disp(void)
{
}

/* 0x00ca2be8 */
void *ex_create(void *parent)
{
    return task_create(parent, &ex_desc, 28);
}
