/*
 * Enemy status: 256 slots, one per live enemy, from creation to shot down or escaped. Game and
 * stage totals, current and fake counts, formation counters, escape damage, action time -> ticks
 * per frame.
 */

#include "types_game.h"
#include "debug.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "enemy_status.h"
#include "callback.h"
#include "debugmenu.h"
#include "enemy_formation.h"
#include "game_flow_editor.h"
#include "game_flow_enemy.h"
#include "player_status.h"
#include "sound_midise.h"
#include "task.h"

typedef struct FxMsg {
    float pos[3];
    int arg;
    char pad_10[16];
} FxMsg;

extern struct GwVarNode *enemy_status_vars_group;
extern struct GwVarNode *gw_varedit_root;
extern GwVarDef enemy_status_vardef[];
extern GwVarDef enemy_status_vardefs[];
extern GwVarDef enemy_status_flag_vardefs[];
extern int enemy_status_cur;
extern int enemy_status_miss_disp;
extern float enemy_status_miss_damage;
extern int scr_root;
extern float enemy_status_action_time;
extern int enemy_status_ticks;
extern int gm_appear;
extern int gm_die;
extern int gm_away;
extern int stg_appear;
extern int stg_die;
extern int stg_away;
extern int enemy_status_last_appear;
extern int enemy_status_last_die;
extern int enemy_status_last_away;
extern EnemyStatus enemy_status;
extern char str_enemy_status_init_game[];
extern char str_enemy_status_init_stage[];
extern struct TaskDesc enemy_status_man_desc;
extern int enemy_status_life_disp;
extern int enemy_status_life_time;
extern struct TaskDesc enemy_status_man2_desc;
/* EnemyStatus window and its "List" child. */
extern int enemy_status_window;
extern int enemy_status_list;
extern GwDesktop screen_layout;

/* Info debug menu. */
/* "EnemyStatus" */
extern char str_enemy_status[];
/* "List" */
extern char str_enemy_status_list[];
extern char str_enemy_status_current[];
extern char *enemy_status_state_names[];
/* "id|for status reg" */
extern char str_enemy_status_list_head[];
/* "%2d|% 3d %s %2d:" */
extern char str_enemy_status_list_row[];
extern char str_enemy_status_name[];

/*
 * The SomeNumber debug window and its texts: "Game", " Item", "  Appear%4d", "  Get   %4d",
 * " Enemy", "  Die   %4d", "  Away  %4d", "Stage", "Cur     %4d" and the fake count.
 */
extern int some_number_window;
extern char str_some_number[];
extern char str_some_number_game[];
extern char str_some_number_item[];
extern char str_some_number_appear[];
extern char str_some_number_get[];
extern char str_some_number_enemy[];
extern char str_some_number_die[];
extern char str_some_number_away[];
extern char str_some_number_stage[];
extern char str_some_number_cur[];
extern char str_some_number_fake[];

static void enemy_status_ent_init(FxEntry *ent, int owner, int id, int handle);
static int EnemyStatusFindFree(EnemyStatus *es);
static int enemy_status_ent_is_fake(int *p);
static void enemy_status_life_draw(void);
static int enemy_status_miss_draw(void);
static void enemy_status_window_closed(void);
static void enemy_status_window_resize(void);
static void enemy_status_window_draw(void *w);
static void enemy_status_list_draw(void *w);
static void enemy_status_window_toggle(void);
static void some_number_window_toggle(void);

/* Info menu windows plus variable editor entries. */
/* 0x00112998 */
void enemy_status_vars_register(void)
{
    menu_item_add(info_menu_name, "EnemyStatus", enemy_status_window_toggle, 0, 0);
    menu_item_add(info_menu_name, "SomeNumber", some_number_window_toggle, 0, 0);
    enemy_status_vars_group = gw_varedit_add(&gw_varedit_root, 0, enemy_status_vardef);
    gw_varedit_add_table(&gw_varedit_root, enemy_status_vars_group, enemy_status_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, enemy_status_vars_group, enemy_status_vardef), enemy_status_flag_vardefs);
}

/* Bit 0 stops the simulation ticks. Returns the previous flags. */
/* 0x00112a68 */
int enemy_status_set_flags(int mask)
{
    int old = (*(int *)&enemy_status); /* steer(cast) */

    (*(int *)&enemy_status) = old | mask; /* steer(cast) */
    return old;
}

/* Returns the previous flags. */
/* 0x00112a80 */
int enemy_status_clear_flags(int mask)
{
    int old = (*(int *)&enemy_status); /* steer(cast) */

    (*(int *)&enemy_status) = old & ~mask; /* steer(cast) */
    return old;
}

/* 0x00112a98 */
static int enemy_status_get_flags(void)
{
    return (*(int *)&enemy_status); /* steer(cast) */
}

/* 0 out of range. */
/* 0x00112aa8 */
int enemy_status_ent_get_id(unsigned idx)
{
    char *slot;

    if (idx >= 256)
        return 0;
    slot = (char *)&enemy_status + idx * 36;
    return *(int *)(slot + 80);
}

/* 0x00112ad8 */
int enemy_status_get_cur(void)
{
    return enemy_status_cur;
}

/* Types 32..45 play their bullet rhythm sound. */
/* 0x00112ae8 */
static void enemy_status_ent_appear(FxEntry *ent)
{
    ent->state = 2;
    if (ent->handle >= 0)
        enemy_formation_appear(ent->handle, 1);
    if (ent->id >= 32 && ent->id < 46)
        SoundRhythmSe_Bullet(ent->id, 0);
}

/* Event 4 with position and slot index. */
/* 0x00112b48 */
static void enemy_status_ent_die(FxEntry *ent, int idx)
{
    FxMsg msg;
    float *p;

    ent->state = 3;
    if (ent->handle >= 0)
        enemy_formation_die(ent->handle, 1, ent->pos);
    p = ent->pos;
    msg.arg = idx;
    msg.pos[0] = ent->pos[0];
    msg.pos[1] = p[1];
    msg.pos[2] = p[2];
    listener_dispatch(4, &msg);
}

/* 0x00112bd0 */
static void enemy_status_ent_away(int *p)
{
    p[4] = 3;
    if (p[3] >= 0)
        enemy_formation_away(p[3], 1);
}

/*
 * op 2 appeared, 3 and 5 shot down, 4 escaped (may damage the player by enemy_status_miss_damage).
 * Frame totals count only enemy classes 0 and 1.
 */
/* 0x00112bf8 */
void enemy_status_event(unsigned idx, unsigned op, float *pos)
{
    FxTable *tbl;
    FxEntry *ent;
    float *ent_pos;
    int appear, die, away, cur, fake;
    int *obj;

    if (idx >= 256)
        return;
    tbl = &enemy_status;
    ent = &tbl->slot[idx];
    if (pos != 0) {
        ent->pos[0] = pos[0];
        ent_pos = ent->pos;
        ent_pos[1] = pos[1];
        ent_pos[2] = pos[2];
    }
    appear = 0;
    die = 0;
    away = 0;
    cur = 0;
    fake = 0;
    switch (op) {
    case 0:
    case 1:
        break;
    case 2:
        appear = 1;
        if (enemy_status_ent_is_fake(ent))
            fake = 1;
        else
            cur = 1;
        enemy_status_ent_appear(ent);
        break;
    case 3:
    case 5:
        if (ent->state >= 2) {
            die = 1;
            if (enemy_status_ent_is_fake(ent))
                fake = -1;
            else
                cur = -1;
        }
        enemy_status_ent_die(ent, idx);
        break;
    case 4:
        if (ent->state >= 2) {
            away = 1;
            if (enemy_status_ent_is_fake(ent))
                fake = -1;
            else
                cur = -1;
        }
        if (enemy_status_miss_disp != 0)
            ((void (*)(float *))enemy_status_miss_draw)(ent->pos); /* steer(cast) */
        if (0.0f < enemy_status_miss_damage)
            player_status_add_damage(enemy_status_miss_damage);
        enemy_status_ent_away(ent);
        break;
    }
    obj = flow_enemy_tbl_get(game_flow_get_data(), ent->id);
    switch (obj[11]) {
    case 0:
    case 1:
        tbl->add_appear += appear;
        tbl->add_die += die;
        tbl->add_away += away;
        tbl->fake += fake;
        tbl->cur += cur;
        break;
    case 2:
    case 3:
        tbl->fake += fake;
        tbl->cur += cur;
        break;
    case 4:
        break;
    }
}

/* handle -1: no formation. -1 when all slots are used. */
/* 0x00112e28 */
int enemy_status_ent_alloc(int id, int handle, int owner)
{
    FxTable *tbl = &enemy_status;
    FxEntry *ent;
    int idx;

    idx = EnemyStatusFindFree(tbl);
    if (idx >= 0) {
        ent = &tbl->slot[idx];
        enemy_status_ent_init(ent, owner, id, handle);
        ent->state = 1;
        if (ent->handle >= 0)
            enemy_formation_assign(ent->handle, 1);
    }
    return idx;
}

/* p: placement data (type id, formation). */
/* 0x00112ed0 */
int enemy_status_ent_create(unsigned char *p, int owner)
{
    return enemy_status_ent_alloc(p[0], *(short *)(p + 2), owner);
}

/* Nothing when shots is 0. */
/* 0x00112ef8 */
void enemy_status_score_popup(int slot, Vec3 *pos, int score, int shots)
{
    int popup[3];

    if (shots > 0) {
        popup[0] = (int)pos;    /* position pointer */
        popup[1] = score;
        popup[2] = shots;
        player_status_disp_score_create(scr_root, popup);
    }
}

/* val: frames left before the enemy leaves. */
/* 0x00112f30 */
void enemy_status_ent_set_pos(unsigned idx, float *pos, int val)
{
    if (idx < 256) {
        FxTable *tbl = &enemy_status;
        FxEntry *ent = &tbl->slot[idx];
        float *ent_pos;

        ent->pos[0] = pos[0];
        ent_pos = ent->pos;
        ent->life = val;
        ent_pos[1] = pos[1];
        ent_pos[2] = pos[2];
    }
}

/* Simulation ticks per frame; 1.0 normal. */
/* 0x00112f78 */
void enemy_status_set_action_time(float v)
{
    enemy_status_action_time = v;
}

/* 0x00112f88 */
float enemy_status_get_action_time(void)
{
    return enemy_status_action_time;
}

/* 0x00112f98 */
int enemy_status_get_ticks(void)
{
    return enemy_status_ticks;
}

/* Game and stage totals and this frame's counts. */
/* 0x00112fa8 */
int enemy_status_get_gm_appear(void)
{
    return gm_appear;
}

/* 0x00112fb8 */
int enemy_status_get_gm_die(void)
{
    return gm_die;
}

/* 0x00112fc8 */
static int enemy_status_get_gm_away(void)
{
    return gm_away;
}

/* 0x00112fd8 */
int enemy_status_get_stg_appear(void)
{
    return stg_appear;
}

/* 0x00112fe8 */
int enemy_status_get_stg_die(void)
{
    return stg_die;
}

/* 0x00112ff8 */
static int enemy_status_get_stg_away(void)
{
    return stg_away;
}

/* 0x00113008 */
static int enemy_status_get_last_appear(void)
{
    return enemy_status_last_appear;
}

/* 0x00113018 */
static int enemy_status_get_last_die(void)
{
    return enemy_status_last_die;
}

/* 0x00113028 */
static int enemy_status_get_last_away(void)
{
    return enemy_status_last_away;
}

/* 0x00113038 */
static void enemy_status_ent_clear(FxEntry *ent)
{
    float *pos = ent->pos;

    ent->unk_00 = 0;
    ent->owner = 0;
    ent->id = 0;
    ent->handle = -1;
    ent->state = 0;
    pos[0] = 0;
    pos[1] = 0;
    pos[2] = 0;
    ent->life = -1;
}

/* 0x00113068 */
static void enemy_status_ent_init(FxEntry *ent, int owner, int id, int handle)
{
    float *pos = ent->pos;

    pos[0] = 0;
    ent->owner = owner;
    ent->life = -1;
    ent->id = id;
    ent->handle = handle;
    ent->state = 0;
    pos[1] = 0;
    pos[2] = 0;
    ent->unk_00 = 0;
}

/* -1 if none. */
/* 0x00113098 */
static int EnemyStatusFindFree(EnemyStatus *es)
{
    EnemyEnt *ent = es->ent;
    int i;

    for (i = 0; i < 256; i++, ent++) {
        if (ent->state == 0)
            return i;
    }
    return -1;
}

/* Formation 9. */
/* 0x001130c8 */
static int enemy_status_ent_is_fake(int *p)
{
    return p[3] == 9;
}

/* 0x001130d8 */
void EnemyStatusInitGame(void)
{
    EnemyStatus *es = &enemy_status;

    DEBUG_PRINTF(str_enemy_status_init_game);
    enemy_status.flags = 0;
    es->action_time = 1.0f;
    es->phase = 0;
    es->ticks = 0;
    es->game[0] = 0;
    es->game[1] = 0;
    es->game[2] = 0;
    es->stage[0] = 0;
    es->stage[1] = 0;
    es->stage[2] = 0;
    es->add[0] = 0;
    es->add[1] = 0;
    es->add[2] = 0;
    es->last[0] = 0;
    es->last[1] = 0;
    es->last[2] = 0;
}

/* 0x00113158 */
void EnemyStatusInitStage(void)
{
    EnemyStatus *es = &enemy_status;
    EnemyEnt *ent;
    int i;

    DEBUG_PRINTF(str_enemy_status_init_stage);
    es->cur = 0;
    es->fake = 0;
    es->stage[0] = 0;
    es->stage[1] = 0;
    es->stage[2] = 0;
    es->add[0] = 0;
    es->add[1] = 0;
    es->add[2] = 0;
    es->last[0] = 0;
    es->last[1] = 0;
    es->last[2] = 0;
    ent = es->ent;
    for (i = 255; i >= 0; i--) {
        enemy_status_ent_clear(ent);
        ent++;
    }
}

/* Whole ticks from action time, fraction carried (flag bit 0: none); frame counts -> totals. */
/* 0x001131e8 */
static void EnemyStatusUpdate(EnemyStatus *es)
{
    float phase;
    int n;
    int appear;
    int die;
    int away;

    if (es->flags & 1) {
        es->ticks = 0;
    } else {
        phase = es->phase;
        phase += es->action_time;
        n = (int)floorf(phase);
        es->ticks = n;
        phase -= (float)n;
        es->phase = phase;
    }
    appear = es->add[0];
    die = es->add[1];
    away = es->add[2];
    es->game[0] += appear;
    es->game[1] += die;
    es->game[2] += away;
    es->stage[0] += appear;
    es->stage[1] += die;
    es->stage[2] += away;
    es->last[0] = appear;
    es->last[1] = die;
    es->last[2] = away;
    es->add[0] = 0;
    es->add[1] = 0;
    es->add[2] = 0;
}

/* 0x001132c0 */
static void enemy_status_man_init(void)
{
}

/* 0x001132c8 */
static void enemy_status_man_release(void)
{
}

/* A gone slot stays reserved one frame before it is freed. */
/* 0x001132d0 */
static void enemy_status_man_exec(void)
{
    EnemyEnt *ent;
    int i;

    EnemyStatusUpdate(&enemy_status);
    ent = enemy_status.ent;
    for (i = 255; i >= 0; i--) {
        switch ((unsigned int)ent->state) {
        case 0:
        case 1:
        case 2:
            break;
        case 3:
            ent->state = 4;
            break;
        case 4:
            ent->state = 0;
            break;
        }
        ent++;
    }
}

/* 0x00113348 */
static void enemy_status_man_disp(void)
{
}

/* 0x00113350 */
int enemy_status_man_create(void *parent)
{
    return task_create(parent, &enemy_status_man_desc, 4);
}

/* 0x00113378 */
static void enemy_status_man2_init(void)
{
}

/* One more update. */
/* 0x00113380 */
static void enemy_status_man2_release(void)
{
    EnemyStatusUpdate(&enemy_status);
}

/* 0x00113390 */
static void enemy_status_man2_exec(void)
{
}

/* With enemy_status_life_disp, calls the (empty) life display per on-screen enemy. */
/* 0x00113398 */
static void enemy_status_man2_disp(void)
{
    EnemyStatus *es = &enemy_status;
    EnemyEnt *ent = es->ent;
    int i;

    for (i = 0; i < 256; i++, ent++) {
        switch ((unsigned int)ent->state) {
        case 0:
        case 1:
            break;
        case 2:
            if (enemy_status_life_disp != 0 && enemy_status_life_time > 0)
                ((void (*)(void *, int))enemy_status_life_draw)(&ent->pos[0], ent->pos[3]); /* steer(cast) */
            break;
        case 3:
        case 4:
            break;
        }
    }
}

/* 0x00113448 */
int enemy_status_man2_create(void *parent)
{
    return task_create(parent, &enemy_status_man2_desc, 4);
}

/* 0x00113470 */
static void enemy_status_life_draw(void)
{
}

/* 0x00113478 */
static int enemy_status_miss_draw(void)
{
    return 0;
}

/* 30 x 18 cells, lower left, scrolling slot list. */
/* 0x00113480 */
static void enemy_status_window_open(void)
{
    int cw;
    int ch;
    int win;
    float rect[5];

    gw_get_cell_size(&cw, &ch);
    win = gw_window_open(str_enemy_status, 95, (int)(screen_layout.left + 4.0f),
                     (int)(screen_layout.bottom - (float)(ch * 18) - 4.0f), cw * 30, ch * 18, 0);
    enemy_status_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, enemy_status_window_resize, enemy_status_window_draw, enemy_status_window_closed);
        win = gw_window_open(str_enemy_status_list, 0x4012c, 2, 2, 0, 0, enemy_status_window);
        enemy_status_list = win;
        gw_window_set_handlers(win, 0, enemy_status_list_draw, 0);
        rect[0] = 0;
        rect[1] = 0;
        rect[2] = (float)(ch * 10);
        rect[3] = (float)ch;
        rect[4] = (float)ch * 5.0f;
        gw_window_add_scrollbar(enemy_status_list, gw_scrollbar_create(1, rect, 0, 0, 0, 0, enemy_status_list));
        menu_item_set_value(info_menu_name, str_enemy_status, 1);
    }
}

/* 0x00113618 */
static void enemy_status_window_closed(void)
{
    enemy_status_window = 0;
    menu_item_set_value(info_menu_name, str_enemy_status, 0);
}

/* List below the counts line. */
/* 0x00113638 */
static void enemy_status_window_resize(void)
{
    int cw;
    int ch;

    gw_get_cell_size(&cw, &ch);
    gw_window_set_pos(enemy_status_list, 2, ch + 4);
    gw_window_size_to_window(enemy_status_list, enemy_status_window, 2, 2);
}

/* Current and fake counts. */
/* 0x00113690 */
static void enemy_status_window_draw(void *w)
{
    gw_win_printf(w, 2, 2, str_enemy_status_current, enemy_status.cur, enemy_status.fake);
}

/* Per used slot: formation, state, type and its name from the game flow data. */
/* 0x001136b8 */
static void enemy_status_list_draw(void *w)
{
    EnemyStatus *es = &enemy_status;
    EnemyEnt *ent;
    int *gf;
    int **type;
    int aw;
    int ah;
    int cw;
    int ch;
    int y;
    int n;
    int i;
    int bar;
    int len;
    int total;

    gw_window_get_size(w, &aw, &ah);
    gw_get_cell_size(&cw, &ch);
    gw_win_printf(w, 2, 2, str_enemy_status_list_head);
    y = ch + 2;
    gf = game_flow_get_data();
    n = 1;
    if (gf != 0) {
        ent = es->ent;
        for (i = 0; i < 256; i++, ent++) {
            if (ent->state != 0) {
                gw_win_printf(w, 2, y, str_enemy_status_list_row, i, ent->ids[3], enemy_status_state_names[ent->state], ent->ids[2]);
                type = flow_enemy_tbl_get(gf, ent->ids[2]);
                if (type != 0 && *type != 0)
                    gw_win_printf(w, cw * 17 + 2, y, str_enemy_status_name, *type);
                y += ch;
                n++;
            }
        }
    }
    bar = gw_window_get_vscrollbar(w);
    total = gw_scrollbar_get_track(bar);
    len = (int)((float)ah / (float)(n * ch) * (float)total);
    if (len > total)
        len = total;
    if (len < 4)
        len = 4;
    gw_scrollbar_set_pos(bar, len);
    gw_scrollbar_set_max(bar, (float)((n - 1) * ch));
}

/* 0x00113898 */
static void enemy_status_window_toggle(void)
{
    if (enemy_status_window != 0)
        gw_window_close(enemy_status_window);
    else
        enemy_status_window_open();
}

/* 0x001138b8 */
static void some_number_window_closed(void)
{
    some_number_window = 0;
    menu_item_set_value(info_menu_name, str_some_number, 0);
}

/* 0x001138d8 */
static void some_number_window_resize(void)
{
}

/* Game and stage: items appeared/collected, enemies appeared/shot down/escaped; current, fake. */
/* 0x001138e0 */
static void some_number_window_draw(void *w)
{
    EnemyStatus *es = &enemy_status;
    int cell[2];
    int y;

    gw_get_cell_size(&cell[0], &cell[1]);
    gw_win_text(w, 2, 2, str_some_number_game);
    y = cell[1] + 2;
    gw_win_text(w, 2, y, str_some_number_item);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_appear, player_status_get_game_item_total());
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_get, player_status_get_game_items_got());
    y += cell[1];
    gw_win_text(w, 2, y, str_some_number_enemy);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_appear, es->game[0]);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_die, es->game[1]);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_away, es->game[2]);
    y += cell[1];
    gw_win_text(w, 2, y, str_some_number_stage);
    y += cell[1];
    gw_win_text(w, 2, y, str_some_number_item);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_appear, player_status_get_stage_item_total());
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_get, player_status_get_stage_items_got());
    y += cell[1];
    gw_win_text(w, 2, y, str_some_number_enemy);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_appear, es->stage[0]);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_die, es->stage[1]);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_away, es->stage[2]);
    y += cell[1];
    gw_win_printf(w, 2, y, str_some_number_cur, es->cur);
    gw_win_printf(w, 2, y + cell[1], str_some_number_fake, es->fake);
}

/* 13 x 19 cells, lower left. */
/* 0x00113bc0 */
static void some_number_window_open(void)
{
    int cw;
    int ch;
    int win;

    gw_get_cell_size(&cw, &ch);
    win = gw_window_open(str_some_number, 95, (int)(screen_layout.left + 4.0f),
                     (int)(screen_layout.bottom - (float)(ch * 19) - 4.0f), cw * 13, ch * 19, 0);
    some_number_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, some_number_window_resize, some_number_window_draw, some_number_window_closed);
        menu_item_set_value(info_menu_name, str_some_number, 1);
    }
}

/* 0x00113c98 */
static void some_number_window_toggle(void)
{
    if (some_number_window != 0)
        gw_window_close(some_number_window);
    else
        some_number_window_open();
}
