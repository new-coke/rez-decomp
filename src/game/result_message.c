/*
 * Result unlock messages: one per newly opened play area, score attack area, "beyond" option and
 * play-time reward, shown in turn in a text panel (decide or timeout). Also the blinking "high
 * score entry not allowed" notice.
 */

#include "result_message.h"
#include "color.h"
#include "config.h"
#include "console_com.h"
#include "game.h"
#include "types_game.h"
#include "task.h"

#define RESULT_MSG_COL RGBA(0x72, 0x3d, 0x21, 0x80)

typedef struct { char pad_00[2968]; int play_area[5]; char pad_bac[24]; int score_attack_area[5]; } GameStateView;

#define FRAMES_SPLIT(frames, min, sec, s, cs) \
    do { \
        (min) = (frames) / 3600; \
        (sec) = (frames) / 60; \
        (s) = (sec) % 60; \
        (cs) = (frames) % 60 % 100; \
    } while (0)

extern char *result_message_play_area_texts[][5];
extern char *result_message_score_attack_area_texts[][5];
extern char *result_message_direct_assault_music_texts[][6];
extern char *result_message_beyond_lost_area_texts[];
extern char *result_message_beyond_trance_mission_texts[];
extern char *result_message_beyond_boss_rush_texts[];
extern char *result_message_player_select_texts[][8];
extern char *result_message_beyond_immortal_texts[];
extern char *result_message_beam_type_texts[][7];
extern char *result_message_beyond_overdrive_infinity_texts[];
extern char *result_message_camera_view_texts[][5];
extern char *result_message_no_entry_texts[];
extern void font_set(int);
extern void font_print(int, char *, float, float, float);
extern struct TaskDesc result_message_desc;
extern GameStateView game_state;
extern int result_message_state[];
extern int result_message_unlock_cnts[];
extern int result_message_hour_steps[8];
extern unsigned stage_exit_flags;
extern int *pad_main;
extern unsigned pad_decide_mask;
extern char *result_message_lang_texts[];
extern char *(*result_message_text_funcs[])(int);
extern struct TaskDesc result_message_timer_desc;
extern struct TaskDesc result_message_disp_desc;

static void result_message_dispatch_create(ResultMsgDispWork *parent, int type, int idx);

/* 0x0015f5f0 */
static char *result_message_text_play_area(int i)
{
    return result_message_play_area_texts[game_lang][i];
}

/* 0x0015f620 */
static char *result_message_text_score_attack_area(int i)
{
    return result_message_score_attack_area_texts[game_lang][i];
}

/* 0x0015f650 */
static char *result_message_text_direct_assault_music(int i)
{
    return result_message_direct_assault_music_texts[game_lang][i];
}

/* 0x0015f680 */
static char *result_message_text_beyond_lost_area(void)
{
    return result_message_beyond_lost_area_texts[game_lang];
}

/* 0x0015f6a0 */
static char *result_message_text_beyond_trance_mission(void)
{
    return result_message_beyond_trance_mission_texts[game_lang];
}

/* 0x0015f6c0 */
static char *result_message_text_beyond_boss_rush(void)
{
    return result_message_beyond_boss_rush_texts[game_lang];
}

/* 0x0015f6e0 */
static char *result_message_text_player_select(int i)
{
    return result_message_player_select_texts[game_lang][i];
}

/* 0x0015f708 */
static char *result_message_text_beyond_immortal(void)
{
    return result_message_beyond_immortal_texts[game_lang];
}

/* 0x0015f728 */
static char *result_message_text_beam_type(int i)
{
    return result_message_beam_type_texts[game_lang][i];
}

/* 0x0015f758 */
static char *result_message_text_beyond_overdrive_infinity(void)
{
    return result_message_beyond_overdrive_infinity_texts[game_lang];
}

/* 0x0015f778 */
static char *result_message_text_camera_view(int i)
{
    return result_message_camera_view_texts[game_lang][i];
}

/* Blinks "high score entry not allowed". */
/* 0x0015f7a8 */
static void result_message_disp(ResultMsgBlinkWork *w)
{
    int frame = w->frame + 1;

    w->frame = frame;
    if ((frame >> 4) & 1) {
        font_set(2);
        font_print(RESULT_MSG_COL, result_message_no_entry_texts[game_lang], 84.0f, 73.0f, 2.5f);
    }
}

/* 0x0015f828 */
static void *result_message_create(void *w)
{
    return task_create(w, &result_message_desc, sizeof(ResultMsgBlinkWork));
}

/*
 * Unlocks not shown yet: kind 10 per play area, 11 per score attack area, i per step config
 * setting i (0..8) gained.
 */
/* 0x0015f850 */
static void result_message_post_unlocks(ResultMsgDispWork *w)
{
    int n;

    {
        unsigned i;
    for (i = 0; i < 5; i++) {
            if (result_message_state[i] == 0) {
                if (game_state.play_area[i]) {
                    result_message_dispatch_create(w, 10, i);
                    result_message_state[i] = 1;
                }
            }
        }
    }
    {
        unsigned i;
    for (i = 0; i < 5; i++) {
            if (result_message_state[11 + i] == 0) {
                if (game_state.score_attack_area[i]) {
                    result_message_dispatch_create(w, 11, i);
                    result_message_state[11 + i] = 1;
                }
            }
        }
    }
    {
        unsigned i;
    for (i = 0; i < 9; i++) {
            n = ConfigGetSetting(i);
            while (result_message_unlock_cnts[i] < n) {
                result_message_dispatch_create(w, i, result_message_unlock_cnts[i]++);
            }
        }
    }
}

/* Kind 1 once total play time reaches 5 hours; kind 4 per result_message_hour_steps step passed. */
/* 0x0015f9e0 */
static void result_message_time_check(ResultMsgDispWork *w)
{
    int min;
    int hours;
    int steps;                     /* signed: the message count compares signed */
    int fr, sec, sec_rem, cs, min_rem;
    unsigned int i;

    fr = play_time;
    FRAMES_SPLIT(fr, min, sec, sec_rem, cs);
    hours = min / 60;
    min_rem = min % 60;
    if (result_message_state[23] <= 0 && hours >= 5) {
        result_message_dispatch_create(w, 1, 0);
        result_message_state[23] = 1;
    }
    for (i = 0; i < 8; i++)
        if (hours < result_message_hour_steps[i])
            break;
    steps = i;
    while (result_message_state[26] < steps) {
        result_message_dispatch_create(w, 4, result_message_state[26]++);
    }
}

/* 50x3 console at (60, 408), (60, 389) with alt_pos; no play-time rewards after exit flag 2. */
/* 0x0015fb10 */
static void result_message_disp_init(ResultMsgDispWork *w, int alt_pos)
{
    ConsoleCom *con;

    con = console_create_fixed(50, 3);
    w->alt_pos = alt_pos;
    w->con = con;
    console_set_font_mode(con, 2);
    console_set_color(con, RESULT_MSG_COL);
    console_set_panel_colors(con, RESULT_MSG_COL, RGBA(0, 0, 0, 0x32));
    console_set_panel(con, 0);
    if (alt_pos)
        console_set_pos(con, 60.0f, 389.0f);
    else
        console_set_pos(con, 60.0f, 408.0f);
    if (stage_exit_flags & 2) {
        result_message_post_unlocks(w);
        return;
    }
    result_message_post_unlocks(w);
    result_message_time_check(w);
}

/* 0x0015fc18 */
static void result_message_disp_release(ResultMsgDispWork *w)
{
    console_destroy_fixed(w->con);
}

/* 0x0015fc20 */
static ConsoleCom *result_message_disp_get_text(ResultMsgDispWork *w)
{
    return w->con;
}

/*
 * Prints into the parent panel, then the language's follow-up line; then up to 120 frames or
 * decide.
 */
/* 0x0015fc28 */
static void result_message_timer_exec(ResultMsgWork *w)
{
    Task *ph;
    ResultMsgDispWork *disp;
    ConsoleCom *con;

    switch (w->phase) {
    case 0:
        ph = TASK_OF(w)->parent;
        disp = ph ? (ResultMsgDispWork *)(ph + 1) : 0;
        con = result_message_disp_get_text(disp);
        console_clear_home(con);
        w->timer = console_puts(con, w->text, 2);
        w->phase = w->phase + 1;
        break;
    case 1:
        w->timer = w->timer - 1;
        if (w->timer < 0) {
            ph = TASK_OF(w)->parent;
            disp = ph ? (ResultMsgDispWork *)(ph + 1) : 0;
            con = result_message_disp_get_text(disp);
            w->timer = console_puts(con, result_message_lang_texts[game_lang], 2);
            w->phase = w->phase + 1;
        }
        break;
    case 2:
        w->timer = w->timer - 1;
        if (w->timer < 0) {
            w->timer = 120;
            w->phase = 3;
        }
        break;
    case 3:
        w->timer = w->timer - 1;
        if (w->timer >= 0 && !(pad_main[27] & pad_decide_mask))
            break;
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
        break;
    }
}

/* Wakes the next queued message (clears its sleep flag). */
/* 0x0015fd88 */
static void result_message_timer_release(ResultMsgWork *w)
{
    Task *ph = TASK_OF(w)->parent;
    Task *sh = TASK_OF(w)->next;
    ResultMsgDispWork *p = ph ? (ResultMsgDispWork *)(ph + 1) : 0;
    ResultMsgWork *next = sh ? (ResultMsgWork *)(sh + 1) : 0;

    p->queued--;
    do {
        if (next)
            TASK_OF(next)->flags &= ~TASK_F_SUSPENDED;
    } while (0);
}

/*
 * Text from result_message_text_funcs[type] (none: nothing); each after the first sleeps until the
 * one before ends.
 */
/* 0x0015fdd0 */
static ResultMsgWork *result_message_timer_create(ResultMsgDispWork *parent, int type, int idx)
{
    char *text;
    ResultMsgWork *w;
    int alt_pos;

    text = result_message_text_funcs[type](idx);
    if (!text)
        return 0;
    w = task_create(parent, &result_message_timer_desc, sizeof(ResultMsgWork));
    alt_pos = parent->alt_pos;
    w->text = text;
    if (alt_pos)
        console_set_panel(result_message_disp_get_text(parent), 1);
    if (parent->queued++ > 0) {
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_SUSPENDED;
        } while (0);
    }
    return w;
}

/* Kind 9: the blinking notice; others: panel message. */
/* 0x0015fe90 */
static void result_message_dispatch_create(ResultMsgDispWork *parent, int type, int idx)
{
    if (type == 9)
        result_message_create(parent);
    else
        result_message_timer_create(parent, type, idx);
}

/* alt_pos: the higher position. */
/* 0x0015feb0 */
void *result_message_disp_create(void *parent, int alt_pos)
{
    return task_create_arg(parent, &result_message_disp_desc, sizeof(ResultMsgDispWork), alt_pos);
}

/* 1 without a panel. */
/* 0x0015fed8 */
int result_message_is_done(ResultMsgDispWork *w)
{
    if (!w)
        return 1;
    return w->queued == 0;
}

/* 0x0015fef0 */
void *result_message_create_child(void *parent)
{
    return result_message_create(parent);
}

/* Play areas, score attack areas, config settings 0..8 before a play, so only new unlocks post. */
/* 0x0015ff10 */
void result_message_state_init(void)
{
    unsigned i;
    unsigned j;

    for (i = 0; i < 5; i++) {
        result_message_state[i] = game_state.play_area[i];
        result_message_state[i + 11] = game_state.score_attack_area[i];
    }
    for (j = 0; j < 9; j++)
        result_message_state[j + 22] = ConfigGetSetting(j);
}
