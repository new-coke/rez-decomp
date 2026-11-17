/*
 * Title screen: attract demo (and a demo replay after 40 s) until START, then the title menu:
 * play, travelling, score attack, options, how to play, Beyond, replays; each once unlocked.
 */

#include "title_shared.h"
#include "billboard.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "heap.h"
#include "libc.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "pad.h"
#include "replay.h"
#include "sequence.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "stage.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "title.h"
#include "title_dm_model.h"
#include "title_option.h"

#define MENU_TEXT_COL RGBA(0x46, 0x2d, 0, 0x80)

extern TitleWork *title_task;

/* title.pnb, title.ptm */
extern int title_files[];

extern float screen_layout[];

extern float title_banner_uv[];

/* "press START" text lines per language */
extern unsigned char *press_start_texts[];

typedef struct TopItem {        /* 32 bytes */
    unsigned char *name[6];
    int next;                   /* 0x18: the step it starts */
    int (*enabled)(void);       /* 0x1c */
} TopItem;

extern TopItem title_menu_items[9];
extern unsigned char title_demo_brackets[];
extern unsigned char *title_demo_names[];

/* tutorial.rep, demo01.rep .. demo04.rep */
extern int title_replay_files[5][2];

extern struct TaskDesc title_desc;

extern unsigned int system_flags;

/* steer(cast) */
/* 0x00af9770 */
void title_init(TitleWork *w)
{
    Vec3 eye;
    Vec3 at;
    void *models;
    int rflag;
    int resume;
    int sel;

    title_task = w;
    system_flags |= SYS_STABILIZER;
    models = model_pnb_load(title_files);
    do { w->textures = model_load_timed(&title_files[2], 0x5000); w->models = models; } while (0);
    filer_cancel_all();
    pad_input_enable(0);
    fade_start(1, 1, 0);
    w->step = 0;
    w->next_step = 0;
    w->phase = 0;
    w->item = 2;
    w->demo_no = module_get_clear_flag(6);
    module_set_clear_flag(6, w->demo_no);
    rflag = module_get_clear_flag(0);
    module_set_clear_flag(0, 0);
    switch (rflag) {
    case 1:
        resume = 1;
        sel = 1;
        break;
    case 2:
        resume = 1;
        sel = 2;
        break;
    case 3:
        resume = 1;
        sel = 3;
        break;
    case 4:
        resume = 1;
        sel = 6;
        break;
    default:
        resume = 0;
        sel = 2;
        break;
    }
    if (!resume) {
        system_flags |= SYS_TITLE_LOOP;
        w->demo = title_demo_create(w);
    } else {
        w->step = resume;
        w->next_step = resume;
        w->item = sel;
        bg_create(0);
        fade_start_out(60);
    }
    w->unk_30 = 0;
    w->idle = 0;
    w->next_seq = 4;
    music_change(1);
    midiseq_stop();
    beat_clock_request_tempo(55.0f);
    do {
        fog_set(COL_BLACK, 0, 0.0f, 0.0f);
    } while (0);
    fog_set_enable(1);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    system_set_frame_rate(1);
    eye.x = 0.0f;
    eye.y = 0.0f;
    eye.z = 0.0f;
    {
        Vec3 *look = &at;
        at.x = 0.0f;
        at.y = 0.0f;
        look->z = -1.0f;
        camera_move_eye(&eye);
        camera_look_offset(look);
    }
    camera_set_far(10000.0f);
    if (system_flags & SYS_DEBUG)
        title_debug_update(w);
    task_set_dlist(w, 6);
}

/* 0x00af99d0 */
void title_release(TitleWork *w)
{
    model_unload_timed(w->textures);
    model_pnb_unload(w->models);
    w->textures = 0;
    w->models = 0;
    do { title_task = 0; } while (0);
}

/* 0x00af9a10 */
void title_draw_final(int color)
{
    int cw = 10;
    float x;
    float y;

    x = screen_layout[7] - (float)(unsigned int)(line_length("FINAL") * cw);
    y = screen_layout[8];
    font_set(1);
    font_print(x, y, 3.3333333f, color, (unsigned char *)"FINAL");
}

/* 0x00af9ac8 */
void title_print_line(TitleWork *w, int line, unsigned char *str, int color)
{
    float y = floorf((line - 0.5f) * 19.0f) + 256.0f;

    if (!str) {
        ulGxSetTextureList(w->textures);
        tex_rect_draw_uv(5, color, title_banner_uv, 78.0f, y + 3.0f, 3.3333333f, 128.0f, 16.0f);
    } else {
        font_set(2);
        font_print(78.0f, y, 3.3333333f, color, str);
    }
}

/* 0x00af9bc8 */
void title_step_press_start(TitleWork *w)
{
    unsigned int sflags;

    sflags = system_flags;
    if (sflags & 4)
        return;
    switch (w->phase) {
    case 0:
        if (seqload_idle())
            w->phase++;
        break;
    case 1:
        system_flags = sflags | SYS_TITLE_LOOP;
        if (pad_main->menu.trig & PAD_START) {
            system_flags = (sflags | SYS_TITLE_LOOP) & ~SYS_TITLE_LOOP;
            SoundRhythmSe_Common(4);
            bg_create(1);
            w->next_step = 1;
            w->item = 2;
            w->idle = 0;
        }
        if ((system_flags & SYS_DEBUG) && !pad_keyboard_key_down(17))
            break;
        if (2400.0f <= (float)w->idle++)
            w->next_step = 10;
        break;
    }
}

/* 0x00af9ce8 */
void title_draw_press_start(TitleWork *w, int lang)
{
    if (w->phase > 0 && !((w->blink >> 5) & 1))
        title_print_line(w, 0, press_start_texts[lang], game_menu_color);
}

/* 0x00af9d38 */
void title_step_menu(TitleWork *w)
{
    unsigned int sflags = system_flags;
    int sel = w->item;
    int demo_sel = w->demo_no;
    unsigned int btn;
    int ok;

    if (sflags & 4) {
        system_flags = sflags & ~4;
        w->next_step = 0;
        return;
    }
    if (pad_system.pad[0]->menu.rep & PAD_DOWN) {
        do {
            sel++;
            if (!((unsigned)sel < 9))
                sel = 0;
            ok = title_menu_items[sel].enabled ? title_menu_items[sel].enabled() : 1;
        } while (!ok);
        w->blink = 4;
    }
    if (pad_system.pad[0]->menu.rep & PAD_UP) {
        do {
            sel--;
            if (!(sel > -1))
                sel = 8;
            ok = title_menu_items[sel].enabled ? title_menu_items[sel].enabled() : 1;
        } while (!ok);
        w->blink = 4;
    }
    btn = pad_system.pad[0]->menu.rep;
    if (btn & 0x80) {
        demo_sel++;
        if (!((unsigned)demo_sel < 5))
            demo_sel = 0;
    }
    if (btn & 0x40) {
        demo_sel--;
        if (!(demo_sel > -1))
            demo_sel = 4;
    }
    if (w->item != sel) {
        SoundRhythmSe_Common(0);
        w->item = sel;
    }
    if (w->demo_no != demo_sel) {
        w->demo_no = demo_sel;
        module_set_clear_flag(6, demo_sel);
    }
    if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
        SoundRhythmSe_Common(1);
        w->next_step = title_menu_items[w->item].next;
    }
    if (pad_system.pad[0]->menu.trig & pad_cancel_mask) {
        SoundRhythmSe_Common(6);
        w->next_step = 0;
    }
}

/* 0x00af9f48 */
void title_draw_menu(TitleWork *w, int lang)
{
    TopItem *rec = title_menu_items;
    unsigned int i = 0;
    int line = 0;
    int color;
    int ok;

    do {
        if (i == w->item)
            color = (w->blink & 4) ? RGBA(0, 0x80, 0x0f, 0x80) : RGBA(0, 0x32, 0, 0x80);
        else
            color = MENU_TEXT_COL;
        ok = rec->enabled ? rec->enabled() : 1;
        if (ok) {
            title_print_line(w, line, rec->name[lang], color);
            line++;
        }
        i++;
        rec++;
    } while (i < 9);
    if (title_menu_items[8].enabled()) {
        title_print_line(w, i, title_demo_brackets, MENU_TEXT_COL);
        title_print_line(w, i, title_demo_names[w->demo_no], MENU_TEXT_COL);
    }
}

/* 0x00afa0b0 */
void title_step_play_unused(TitleWork *w)
{
    stage_next_set(0);
    replay_set_mode(1);
    w->next_seq = 10;
    w->next_step = 11;
}

/* 0x00afa0f0 */
void title_step_travelling(TitleWork *w)
{
    replay_set_mode(1);
    module_set_clear_flag(3, 0);
    w->next_seq = 3;
    w->next_step = 11;
}

/* 0x00afa138 */
void title_step_play(TitleWork *w)
{
    replay_set_mode(1);
    module_set_clear_flag(4, 0);
    w->next_seq = 4;
    w->next_step = 11;
}

/* 0x00afa180 */
void title_step_score_attack(TitleWork *w)
{
    replay_set_mode(1);
    module_set_clear_flag(5, 0);
    w->next_seq = 6;
    w->next_step = 11;
}

/* 0x00afa1c8 */
void title_step_beyond(TitleWork *w)
{
    replay_set_mode(1);
    module_set_clear_flag(2, 0);
    w->next_seq = 7;
    w->next_step = 11;
}

/* 0x00afa210 */
void title_step_replay(TitleWork *w)
{
    replay_set_mode(2);
    w->next_seq = 8;
    w->next_step = 11;
}

/* 0x00afa248 */
void title_step_options(TitleWork *w)
{
    switch (w->phase) {
    case 0:
        fade_start_out(0);
        w->phase++;
        break;
    case 1:
        fade_start(0, 1, 12);
        pcm_stop();
        w->phase++;
        break;
    case 2:
        if (fade_is_idle() == 1) {
            game_type_set(1);
            if (w)
                TASK_OF(w)->flags |= TASK_F_SUSPENDED;
            option_create(w);
            w->phase++;
        }
        break;
    case 3:
        game_type_set(0);
        fade_start_out(12);
        beat_clock_request_tempo(55.0f);
        w->next_step = 1;
        break;
    }
}

/* 0x00afa350 */
void title_step_how_to_play(TitleWork *w)
{
    void *data;

    switch (w->phase) {
    case 0:
        pcm_stop();
        w->phase++;
        break;
    case 1:
        replay_set_mode(2);
        data = file_load_sync(title_replay_files[0], 0, 0);
        ReplayLoadData(data);
        heap_free(data, 0);
        w->next_seq = 8;
        w->next_step = 11;
        break;
    }
}

/* 0x00afa3f0 */
void title_step_demo(TitleWork *w)
{
    void *data;
    int n;

    switch (w->phase) {
    case 0:
        pcm_stop();
        w->phase++;
        break;
    case 1:
        replay_set_mode(2);
        n = w->demo_no;
        data = file_load_sync(title_replay_files[n++], 0, 0);
        if ((unsigned int)n >= 5)
            n = 0;
        module_set_clear_flag(6, n);
        ReplayLoadData(data);
        heap_free(data, 0);
        w->next_seq = 8;
        w->next_step = 11;
        break;
    }
}

/* 0x00afa4c0 */
void title_step_next_sequence(TitleWork *w)
{
    switch (w->phase) {
    case 0:
        fade_start_out(0);
        w->phase++;
        break;
    case 1:
        fade_start(0, 1, 30);
        w->phase++;
        break;
    case 2:
        if (fade_is_idle() == 1) {
            pcm_stop();
            sound_sequence_set_next(w->next_seq);
        }
        break;
    }
}

/* 0x00afa580 */
void title_exec(TitleWork *w)
{
    switch (w->step) {
    case 0:
        title_step_press_start(w);
        break;
    case 1:
        title_step_menu(w);
        break;
    case 2:
        title_step_play_unused(w);
        break;
    case 3:
        title_step_travelling(w);
        break;
    case 4:
        title_step_play(w);
        break;
    case 5:
        title_step_score_attack(w);
        break;
    case 6:
        title_step_options(w);
        break;
    case 7:
        title_step_how_to_play(w);
        break;
    case 8:
        title_step_beyond(w);
        break;
    case 9:
        title_step_replay(w);
        break;
    case 10:
        title_step_demo(w);
        break;
    case 11:
        title_step_next_sequence(w);
        break;
    }
    if (w->step != w->next_step) {
        w->step = w->next_step;
        w->phase = 0;
    }
    w->blink++;
}

/* 0x00afa6a8 */
void title_disp(TitleWork *w)
{
    int lang = game_lang;

    draw_set_blend(4, 5);
    switch (w->step) {
    case 0:
        title_draw_press_start(w, lang);
        break;
    case 1:
        title_draw_menu(w, lang);
        break;
    case 2:
        break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        goto done;
    }
done:
    if (system_flags & SYS_DEBUG)
        title_draw_final(MENU_TEXT_COL);
}

/* 0x00afa758 */
void *title_create(void *parent)
{
    return task_create(parent, &title_desc, 52);
}

/* 0x00afa780 */
int title_debug_update(TitleWork *w)
{
    return 0;
}

/* 0x00afa788 */
TitleWork *title_get_task(void)
{
    return title_task;
}

/* 0x00afa798 */
TitleTasks *title_get_tasks(void)
{
    TitleWork *tw = title_task;

    if (tw)
        return (TitleTasks *)&tw->demo;
    return 0;
}

/* 0x00afa7b8 */
int bg_exists(void)
{
    TitleTasks *tt = title_get_tasks();

    if (tt && tt->bg)
        return 1;
    return 0;
}

/* 0x00afa7e8 */
int bg_is_done(void)
{
    TitleTasks *tt = title_get_tasks();

    if (tt && tt->bg && bg_is_opaque(tt->bg))
        return 1;
    return 0;
}

/* 0x00afa830 */
int title_play_unused_enabled(void)
{
#ifdef VERSION_US
    /* debug mode only, while the debug button is held */
    if ((pad_system.pad[0]->raw.on & PAD_R1) && (system_flags & SYS_DEBUG))
        return 1;
    return 0;
#else
    return 0;
#endif
}

/* 0x00afa868 */
int title_item_enabled(void)
{
    return 1;
}

/* 0x00afa870 */
int title_score_attack_enabled(void)
{
    if (pad_keyboard_key_down(44) && (system_flags & SYS_DEBUG))
        return 1;
    return score_attack_unlocked != 0;
}

/* 0x00afa8b0 */
int title_options_enabled(void)
{
    return 1;
}

/* 0x00afa8b8 */
int title_how_to_play_enabled(void)
{
    return 1;
}

/* 0x00afa8c0 */
int title_beyond_enabled(void)
{
    int n;

    n = ConfigGetSetting(0);
    n += ConfigGetSetting(1);
    n += ConfigGetSetting(2);
    n += ConfigGetSetting(3);
    n += ConfigGetSetting(4);
    n += ConfigGetSetting(5);
    n += ConfigGetSetting(6);
    n += ConfigGetSetting(7);
    n += ConfigGetSetting(8);
    if ((pad_main->raw.on & PAD_R1) != 0 && (system_flags & SYS_DEBUG) != 0)
        return 1;
    if (n > 0)
        return 1;
    return 0;
}

/* 0x00afa978 */
int title_replay_enabled(void)
{
    if ((pad_main->raw.on & PAD_R1) && (system_flags & SYS_DEBUG))
        return 1;
    return 0;
}
