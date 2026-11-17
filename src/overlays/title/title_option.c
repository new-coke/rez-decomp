/*
 * Option menu from the title: 17 items, each with a setting box. Also opens the score ranking and
 * the config load; leaving saves through the auto save module.
 */

#include "title_shared.h"
#include "color.h"
#include "debug.h"
#include "config.h"
#include "fade.h"
#include "fog.h"
#include "libulmc.h"
#include "module.h"
#include "node_draw_state.h"
#include "pad.h"
#include "render.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "system.h"
#include "task.h"
#include "warning.h"
#include "title_option.h"
#include "title_option_bg.h"
#include "title_option_camera.h"
#include "backup.h"

/* 20 bytes, table option_box_kinds: 1 BoxONOFF, 2 BoxVolume, 3 BoxSelect, 4 plain + hook */
typedef struct BoxKind {
    int (*create)(void *parent, int item); /* 0x00: creates the box task */
    void (*close)(void *box, void *item);  /* 0x04: starts closing it */
    int take_ready;                 /* 0x08: the box's take-ready hook */
    int icon12;                 /* 0x0c: hint icon 12 shown by the options Bg */
    int icon14;                 /* 0x10: hint icon 14 shown by the options Bg */
} BoxKind;

typedef struct OptItem {        /* 40 bytes, table option_items */
    unsigned char *name[6];     /* per language */
    int kind;                   /* 0x18: box kind; 0 = no item */
    char pad_1c[4];
    void (*select)(void *w);    /* 0x20 */
    int arg;                    /* 0x24 */
} OptItem;

extern BoxKind option_box_kinds[];
extern OptItem option_items[];
extern OptionWork *option_menu;
extern struct TaskDesc option_desc;

/* 0x00af72e8 */
int option_box_create(void *menu, int item)
{
    OptItem *rec = &option_items[item];
    int (*create)(void *, int) = option_box_kinds[rec->kind].create;

    if (create)
        return create(menu, rec->arg);
    return 0;
}

/* 0x00af7348 */
void option_box_close(void *box, int item)
{
    OptItem *rec = &option_items[item];
    void (*close)(void *, void *) = (option_box_kinds + rec->kind)->close;

    if (close)
        close(box, rec);
}

/* steer(cast) */
/* 0x00af73a0 */
void option_init(OptionWork *w, void *caller)
{
    OptItem *rec;
    BoxKind *bkind;

    w->caller = caller;
    w->stereo = ConfigGetStereo();
    w->fog_mode = fog_get_current(&w->fog_color, &w->fog_back, &w->fog_near, &w->fog_far);
    fog_set(COL_BLACK, 4, 417.0f, 1000.0f);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    w->bg = option_bg_create(w);
    w->camera = option_camera_create(w);
    rec = &option_items[w->sel];
    bkind = &option_box_kinds[rec->kind];
    w->box = (void *)option_box_create(w, w->sel);
    option_bg_set_icons(w->bg, bkind->icon12, bkind->icon14);
    music_change(1);
    beat_clock_request_tempo(120.0f);
    midiseq_mute_request(6);
    midiseq_play();
    w->active = 1;
    option_menu = w;
}

/* 0x00af74b8 */
void option_release(OptionWork *w)
{
    midiseq_mute_request(0);
    midiseq_stop();
    do {
        if (w->caller)
            TASK_OF(w->caller)->flags &= ~TASK_F_SUSPENDED;
    } while (0);
    ((void (*)(int, float, float, int))fog_set)(w->fog_color, w->fog_near, w->fog_far, w->fog_mode); /* steer(cast): mode last */
    BackColReq(w->fog_back);
    option_menu = 0;
}

/* 0x00af7530 */
void option_open(OptionWork *w)
{
    switch (w->state) {
    case 0:
        w->state = 1;
        break;
    case 1:
        fade_start_out(20);
        w->mode = 1;
        w->state = 0;
        break;
    }
}

/* 0x00af7590 */
void option_navigate(OptionWork *w)
{
    int cur = w->sel;
    int prev = cur;
    OptItem *rec;
    BoxKind *bkind;
    void (*select)(void *);

    if (pad_system.pad[0]->menu.trig & PAD_UP) {
        do {
            cur--;
            if (!(cur > -1))
                cur = 16;
        } while (option_items[cur].kind == 0);
    }
    if (pad_system.pad[0]->menu.trig & PAD_DOWN) {
        do {
            cur++;
            if (!((unsigned)cur < 17))
                cur = 0;
        } while (option_items[cur].kind == 0);
    }
    if (prev != cur) {
        if (w->box) {
            option_box_close(w->box, prev);
            w->box = 0;
        }
        w->sel = cur;
        SoundRhythmSe_Common(0);
        rec = &option_items[cur];
        bkind = &option_box_kinds[rec->kind];
        w->box = (void *)option_box_create(w, cur);
        option_bg_set_icons(w->bg, bkind->icon12, bkind->icon14);
    }
    if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
        select = (option_items + w->sel)->select;
        if (select)
            select(w);
    }
    if (pad_system.pad[0]->menu.trig & pad_cancel_mask)
        option_select_return(w);
}

/* 0x00af7778 */
void option_autosave(OptionWork *w)
{
    switch (w->state) {
    case 0:
        fade_start(0, 1, 12);
        ConfigSetStereo(w->stereo);
        module_entry_start_load(14);
        DEBUG_PRINTF("AutoSave Request\n");
        w->state++;
        break;
    case 1:
        if (module_file_status(14)) {
            DEBUG_PRINTF("AutoSave Start\n");
            w->module = module_start(14, w);
            w->state++;
        }
        break;
    case 2:
        if (!w->module || (TASK_OF(w->module)->flags & TASK_F_END))
            w->state = 100;
        break;
    case 100:
        if (fade_is_idle() == 1 && w)
            FINISH(w);
        break;
    }
}

/* 0x00af78a0 */
void option_ranking_data(OptionWork *w)
{
    switch (w->state) {
    case 0:
        fade_start(0, 1, 12);
        module_entry_start_load(9);
        w->state++;
        break;
    case 1:
        if (module_file_status(9) && fade_is_idle() == 1) {
            void *mod = module_start(9, w);
            void *t_ = w->bg;
            w->module = mod;
            w->active = 0;
            do {
                if (t_ && !(TASK_OF(t_)->flags & TASK_F_END))
                    TASK_OF(t_)->flags |= TASK_F_SUSPENDED;
                t_ = w->box;
                if (t_ && !(TASK_OF(t_)->flags & TASK_F_END))
                    TASK_OF(t_)->flags |= TASK_F_SUSPENDED;
                w->state++;
            } while (0);
        }
        break;
    case 2:
        if (w->module && !(TASK_OF(w->module)->flags & TASK_F_END))
            break;
        /* steer(barrier) */
        {
            void *t_ = w->bg;
            w->active = 1;
            if (t_)
                do { if (!(TASK_OF(t_)->flags & TASK_F_END)) TASK_OF(t_)->flags &= ~2; } while (0);
        }
        {
            void *t_ = w->box;
            if (t_)
                do { if (!(TASK_OF(t_)->flags & TASK_F_END)) TASK_OF(t_)->flags &= ~2; } while (0);
        }
        fade_start_out(20);
        w->state = 0;
        w->mode = 1;
        break;
    }
}

#ifdef VERSION_US

extern int mc_drive_slots[];
extern char config_file_name[];

/* Mode 4: checks the card (warnings 31, 17, 18), then runs the auto load module (13). */
/* 0x00af7a38 */
void option_load_data(OptionWork *w)
{
    int done = 0;
    int drv;

    switch (w->state) {
    case 0:
        drv = auto_load_drive;
        if (mc_slots[mc_drive_slots[drv]].flags & 1)
            w->state = 1;
        else
            w->state = 2000;
        break;
    case 1:
        if (!ulMcFindFile(auto_load_drive, config_file_name))
            w->state = 100;
        else
            w->state = 1000;
        break;
    case 100:
        if (warning_request(31, 0)) {
            fade_start_alpha(0.5f, 0, 12);
            w->state++;
        }
        break;
    case 101:
        drv = auto_load_drive;
        if (!mc_is_connected(ulMcCardToPort(drv), ulMcCardToSlot(drv))) {
            warning_cancel();
            w->state = 0;
            break;
        }
        if (!warning_is_done())
            break;
        if (warning_get_result())
            w->state = 200;
        else
            done = 1;
        break;
    case 200:
        module_entry_start_load(13);
        module_set_clear_flag(7, 0);
        DEBUG_PRINTF("AutoLoad Request\n");
        w->state++;
        break;
    case 201:
        if (module_file_status(13)) {
            DEBUG_PRINTF("AutoLoad Start\n");
            w->module = module_start(13, w);
            w->state++;
        }
        break;
    case 202:
        if (!w->module || (TASK_OF(w->module)->flags & TASK_F_END)) {
            if (module_get_clear_flag(7)) {
                DEBUG_PRINTF("AutoLoad End But Error\n");
                w->state = 0;
            } else {
                done = 1;
                DEBUG_PRINTF("AutoLoad End\n");
            }
        }
        break;
    case 1000:
        if (warning_request(17, 1)) {
            fade_start_alpha(0.5f, 0, 12);
            w->state++;
        }
        break;
    case 1001:
        drv = auto_load_drive;
        if (!mc_is_connected(ulMcCardToPort(drv), ulMcCardToSlot(drv))) {
            warning_cancel();
            w->state = 0;
            break;
        }
        done = warning_is_done() != 0;
        break;
    case 2000:
        if (warning_request(18, 1)) {
            fade_start_alpha(0.5f, 0, 12);
            w->state++;
        }
        break;
    case 2001:
        if (game_state.card_changed > 0) {
            w->card = game_state.card_changed;
            game_state.card_changed = 0;
            DEBUG_PRINTF("New MemoryCard Insert\n");
            warning_cancel();
            w->state = 0;
            break;
        }
        done = warning_is_done() != 0;
        break;
    }
    if (done) {
        game_state.card_changed = w->card;
        fade_start_out(12);
        w->state = 0;
        w->mode = 1;
    }
}
#endif

/* 0x00af7d80 */
void option_exec(OptionWork *w)
{
    switch ((unsigned int)w->mode) {
    case 0:
        option_open(w);
        break;
    case 1:
        option_navigate(w);
        break;
    case 2:
        option_autosave(w);
        break;
    case 3:
        option_ranking_data(w);
        break;
#ifdef VERSION_US
    case 4:
        option_load_data(w);
        break;
#endif
    }
}

/* 0x00af7de0 */
void option_disp(OptionWork *w)
{
    Vec3 pos;
    Vec3 cur;
    OptItem *rec;
    unsigned int i;
    int lang;
    int color;
    float cur_ofs;
    float step;

    if (w->active) {
        cur_ofs = 20.0f;
        lang = game_state.lang;
        i = 0;
        color = game_state.menu_color;
        draw_set_fc_mode(0);
        draw_set_blend(4, 5);
        step = 19.0f;
        option_bg_get_anchor(w->bg, &pos);
        pos.x += 30.0f;
        pos.y -= 9.5f;
        RenderSetCurrentDisplayList(6);
        rec = option_items;
        do {
            if (i == w->sel) {
                cur.y = pos.y;
                cur.z = pos.z;
                cur.x = pos.x - cur_ofs;
                sprite_draw3d(&cur, 1.0f, color, 20, 1, 1);
            }
            font_set(2);
            if (rec->name[lang])
                font_print3d(rec->name[lang], &pos, 0, 0, 0, color);
            i++;
            rec++;
            pos.y -= step;
        } while (i < 17);
    }
}

/* 0x00af7f48 */
void *option_create(void *caller)
{
#ifdef VERSION_US
    /* 76 bytes: the work ends with the card field option_load_data uses */
    return task_create_sibling_inherit_arg(caller, &option_desc, 76, (int)caller);
#else
    return task_create_sibling_inherit_arg(caller, option_desc, 72, (int)caller);
#endif
}

/* 0x00af7f70 */
void option_select_ranking_data(OptionWork *w)
{
    if (!score_attack_unlocked) {
        SoundRhythmSe_Common(6);
        return;
    }
    w->state = 0;
    w->mode = 3;
    SoundRhythmSe_Common(1);
}

#ifdef VERSION_US
/* 0x00af7f98 */
void option_select_load_data(OptionWork *w)
{
    SoundRhythmSe_Common(1);
    w->state = 0;
    w->mode = 4;
}
#endif

void option_select_default(void)
{
    ConfigSetVolSe(4);
    ConfigSetVolSeq(4);
    ConfigSetUpdown(0);
    ConfigSetShotButton(4);
    ConfigSetOdButton(2);
    ConfigSetVibration(0);
    ConfigSetDispConsole(1);
    ConfigSetDispScore(0);
    SoundRhythmSe_Common(1);
}

/* 0x00af7fd0 */
void option_select_return(OptionWork *w)
{
    w->mode = 2;
    w->state = 0;
    SoundRhythmSe_Common(6);
}

/* 0x00af7fe8 */
int option_get_stereo(void)
{
    return option_menu->stereo;
}

/* 0x00af7ff8 */
void option_set_stereo(int v)
{
    option_menu->stereo = v;
}
