/*
 * Boss rush score screen between bosses: counts up the boss's score (faster while the button is
 * held), shows the result; after the last boss lists all five and the total, then ends the game.
 */

#include "stg21.h"
#include "billboard.h"
#include "color.h"
#include "config.h"
#include "console.h"
#include "console_com.h"
#include "debug.h"
#include "fade.h"
#include "game.h"
#include "libc.h"
#include "lockon.h"
#include "node_draw_state.h"
#include "player_status.h"
#include "result_message.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"
#include "br_score.h"

#define RESULT_TEXT_COL RGBA(0x72, 0x3d, 0x21, 0x80)

extern GameState game_state;
extern void font_set(int id);
extern struct TaskDesc br_score_bg_desc;

typedef struct BrRect {         /* 20 bytes */
    int tex;
    float x, y, width, h;
} BrRect;

extern float digit_uvs[][4];
extern char *area_names[];
extern char *total_labels[];
extern int total_rows[];
extern float digit_sep_uv[];
extern float bar_uv[];
extern struct TaskDesc br_score_desc;
extern float br_score_bg_uvs[][4];
extern BrRect br_score_bg_rects[][5];
extern void *system_ptm;
extern unsigned int *ConfigGetRankRecord(int t, int i);

/* 0x00af7298 */
void br_score_draw_number(int num, float x, float y, float z)
{
    char buf[32];
    int i;
    int ch;
    float dx = 20.0f;

    sprintf(buf, "%15d", num);
    for (i = 14; i != 0; i--) {
        ch = (unsigned char)buf[i];
        if ((unsigned)(ch - 48) < 10) {
            if (i != 14) {
                if (i % 3 == 2) {
                    x -= 6.0f;
                    tex_rect_draw_uv(7, RESULT_TEXT_COL, digit_sep_uv, x, y, z, 6.0f, 26.0f);
                }
            }
            x -= dx;
            tex_rect_draw_uv(7, RESULT_TEXT_COL, digit_uvs[ch], x, y, z, 20.0f, 26.0f);
        }
    }
}

/* 0x00af7400 */
void br_score_count_area(BrScore *w)
{
    w->shown = (int)((float)(w->times[w->area] * w->timer) / FPS);
    if (((BrPad *)pad_main)->on & 4)
        w->timer += 4;
    else
        w->timer += 1;
    if (60.0f < (float)w->timer) {
#ifdef VERSION_US
        w->shown = w->times[w->area];
        w->timer = 0;
        w->state = 5;
#else
        w->timer = 0;
        w->state = 5;
#endif
    }
}

/* 0x00af74a8 */
void br_score_count_total(BrScore *w)
{
    int i, sum;

    w->shown = (int)((float)(w->times[w->count] * w->timer) / FPS);
    if (((BrPad *)pad_main)->on & 4)
        w->timer += 4;
    else
        w->timer += 1;
    if (60.0f < (float)w->timer) {
        w->timer = 0;
        w->shown = 0;
        if (++w->count == 5)
            w->state = 5;
    }
    sum = 0;
    for (i = 0; i < w->count; i++)
        sum += w->times[i];
    if (w->count != 5)
        sum += w->shown;
    w->total = sum;
}

/* 0x00af75a0 */
void br_score_exec(BrScore *w)
{
    unsigned int *rec;

    switch (w->state) {
    case 0:
        fade_start(0, 1, 60);
        w->disp_level = console_set_disp_level(0);
        w->state = 1;
        break;
    case 1:
        if (fade_is_idle() != 1)
            break;
        ConfigSetSelect(8, 4);
        lockon_set_enable(0);
        console_clear_home(0);
        w->msg = result_message_disp_create(w, 0);
#ifdef VERSION_US
        if (w->area == 4 && (ConfigGetSelect(5) || ConfigGetSelect(7)))
            result_message_create_child(w);
#else
        if (w->area == 4 && !ConfigGetSelect(5) && !ConfigGetSelect(7)) {
            rec = ConfigGetRankRecord(6, 9);
            if (rec[1] >= player_status_get_boss_score())
                result_message_create_child(w);
        }
#endif
        w->total = 0;
        w->state = 2;
        w->shown = 0;
        break;
    case 2:
        fade_start_out(30);
        if (w->bg)
            TASK_OF(w->bg)->flags &= ~TASK_F_SUSPENDED;
        w->state = 3;
        break;
    case 3:
        if (fade_is_idle() != 1)
            break;
        w->state = 4;
        break;
    case 4:
        if (w->area == 4)
            br_score_count_total(w);
        else
            br_score_count_area(w);
        break;
    case 5:
        if (!result_message_is_done(w->msg))
            break;
        if (!(((BrPad *)pad_main)->trig & 4)) {
            if (!(300.0f < (float)++w->timer))
                break;
        }
        w->timer = 0;
        w->state = 6;
        break;
    case 6:
        fade_start(0, 1, 60);
        w->state = 7;
        break;
    case 7:
        if (fade_is_idle() != 1)
            break;
        FINISH_IF(w->msg);
        if (w->bg)
            TASK_OF(w->bg)->flags |= TASK_F_SUSPENDED;
        console_set_disp_level(w->disp_level);
        ConfigSetSelect(8, w->cfg);
        lockon_set_enable(1);
        if (w->area != 4) {
            w->state = 8;
            break;
        }
        stage_exit_flags |= 2;
        stage_exit_print();
        GameExit(1);
        w->state = 10;
        break;
    case 8:
        fade_start_out(30);
        w->state = 9;
        break;
    case 9:
        if (fade_is_idle() != 1)
            break;
        w->state = 10;
        break;
    }
}

/* 0x00af7848 */
void br_score_draw_total(BrScore *w)
{
    char buf[32];
    unsigned int i;
    int j;
    int x;
    float bx;
    char **aname;

    font_set(3);
    for (i = 0, aname = area_names; i < 5; i++)
        font_print(84.0f, total_rows[i] - 42, 2.5f, RESULT_TEXT_COL, *aname++);
    font_print(84.0f, 302.0f, 2.5f, RESULT_TEXT_COL, total_labels[game_state.lang]);
    if (w->state >= 4) {
        format_number_commas(buf, w->total);
        font_print(-193.0f, 355.0f, 2.5f, RESULT_TEXT_COL, buf);
    }
    bx = 511.0f;
    ulGxSetTextureList((void *)stage_textures);
    for (i = 0; i < 5; i++) {
        bx = 511.0f;
        tex_rect_draw_uv(0, RESULT_TEXT_COL, bar_uv, bx, (int)(total_rows[i] - 31.0f), 2.5f, 54.0f, 31.0f);
    }
    tex_rect_draw_uv(0, RESULT_TEXT_COL, bar_uv, bx, 366.0f, 2.5f, 54.0f, 31.0f);
    x = 491;
    ulGxSetTextureList(system_ptm);
    for (j = 0; j < w->count; j++)
        br_score_draw_number(w->times[j], x, (int)(total_rows[j] - 26.0f), 2.5f);
    if (w->state >= 4 && w->count != 5)
        br_score_draw_number(w->shown, x, total_rows[w->count] - 26.0f, 2.5f);
}

/* 0x00af7b30 */
void br_score_disp(BrScore *w)
{
    if (w->state == 10)
        return;
    if (w->state < 2)
        return;
    if (w->state >= 8)
        return;
    if (w->area == 4) {
        br_score_draw_total(w);
        return;
    }
    font_set(3);
    font_print(84.0f, 182.0f, 2.5f, RESULT_TEXT_COL, area_names[w->area]);
    ulGxSetTextureList((void *)stage_textures);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, bar_uv, 511.0f, 193.0f, 2.5f, 54.0f, 31.0f);
    if (w->state >= 4) {
        ulGxSetTextureList(system_ptm);
        br_score_draw_number(w->shown, 491.0f, 198.0f, 2.5f);
    }
}

/* 0x00af7c68 */
void br_score_init(BrScore *w)
{
    BrScoreBg *bg;

    w->state = 10;
    stage_init_print();
    bg = br_score_bg_create(w);
    w->bg = bg;
    do {
        if (bg)
            TASK_OF(bg)->flags |= TASK_F_SUSPENDED;
    } while (0);
    w->cfg = ConfigGetSelect(8);
}

/* 0x00af7cc8 */
void br_score_release(BrScore *w)
{
    ConfigSetSelect(8, w->cfg);
}

/* 0x00af7cd8 */
BrScore *br_score_create(void *parent)
{
    return task_create(parent, &br_score_desc, 64);
}

/* 0x00af7d00 */
void br_score_open(BrScore *w, int area)
{
    int cur_score;

    w->area = area;
    w->state = 0;
    cur_score = player_status_get_score();
    w->times[area] = cur_score - w->last;
    w->last = cur_score;
    br_score_bg_set_last(w->bg, area);
}

/* 0x00af7d58 */
int br_score_is_idle(BrScore *w)
{
    return w->state == 10;
}

/* 0x00af7d68 */
void br_score_bg_draw_rects(int idx, unsigned int color)
{
    BrRect *rect = br_score_bg_rects[idx];
    float (*uv)[4] = br_score_bg_uvs;
    unsigned int i;

    for (i = 0; i < 5; i++) {
        tex_rect_draw_uv(rect->tex, color, *uv, rect->x, rect->y, 2.2222223f, rect->width, rect->h);
        uv++;
        rect++;
    }
}

/* 0x00af7e08 */
void br_score_bg_exec(BrScoreBg *w)
{
    float a = w->alpha + 4.25f;

    w->alpha = a < 255.0f ? (int)a : 255;
}

/* 0x00af7e48 */
void br_score_bg_disp(BrScoreBg *w)
{
    ulGxSetTextureList((void *)stage_textures);
    draw_set_blend(4, 5);
    br_score_bg_draw_rects(w->last, (w->alpha + 1) >> 1 << 24 | 0x808080);
}

/* 0x00af7ea0 */
void *br_score_bg_create(void *parent)
{
    return task_create(parent, &br_score_bg_desc, 8);
}

/* 0x00af7ec8 */
void br_score_bg_set_last(BrScoreBg *w, int area)
{
    w->last = area == 4;
}
