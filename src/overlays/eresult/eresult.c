/*
 * End result screen: rates for a normal game, a counted-up score for score attack and the extra
 * modes, then a result message. The decide button skips.
 */

#include "eresult_shared.h"
#include "task.h"
#include "types_common.h"
#include "billboard.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "fade.h"
#include "game.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad.h"
#include "player_status.h"
#include "result_message.h"
#include "system.h"
#include "ul_gx.h"

#define RESULT_TEXT_COL RGBA(0x72, 0x3d, 0x21, 0x80)

extern Pad *pad_main;

extern unsigned int pad_decide_mask;

extern struct TaskDesc er_score_desc;

void *er_score_create(void *parent);

/* 16 bytes */
typedef struct EndResultWork {
    int state;
    void *result;               /* 0x04: the "er_result" task (rates) */
    void *msg;                  /* 0x08: result message task */
    void *score;
} EndResultWork;

/* 20 bytes */
typedef struct ErResultWork {
    float rate[3];              /* analysis, shot down, item */
    float t;                    /* 0x0c: frames shown */
    int done;                   /* 0x10 */
} ErResultWork;

typedef struct ErScoreWork {
    int mode;                   /* 0: "area5" caption, 1: "total" */
    int shown;
    int score;                  /* 0x08 */
    int count;                  /* 0x0c: frames counted */
    int frames;                 /* 0x10: frames of the count-up */
    int done;                   /* 0x14 */
} ErScoreWork;

/* 20 bytes; five per row in er_bg_quads */
typedef struct ErQuad {
    int tex;
    float x, y, width, h;
} ErQuad;

extern int ConfigRankingFind(int t, int *rec);
extern void font_set(int id);

extern void *system_ptm;

/* eresult.ptm and its load request */
extern void *end_result_textures;

extern char model_load_req[];

extern struct TaskDesc end_result_desc;

extern struct TaskDesc er_bg_desc;

extern struct TaskDesc er_result_desc;

extern ErQuad er_bg_quads[][5];

/* texture coordinates */
extern float er_bg_quad_uvs[][4];

/* [11]: decimal point */
extern float er_result_digit_uvs[][4];

extern float er_result_caption_uvs[][4];

/* by character */
extern float er_score_digit_uvs[][4];

extern float er_score_comma_uv[4];

extern float er_score_frame_uv[4];

/* by language */
extern char *er_score_total_texts[];

void *end_result_create(void *parent);

int end_result_rank_check(void);
void *er_bg_create(void *parent, int kind);
void *er_result_create(void *parent);
int er_result_is_done(ErResultWork *w);
void er_bg_draw_rects(int row, int color);
void er_result_draw_rate(int y, float pct);
void er_score_draw_number(int n, float x, float y, float z);
int er_score_is_done(ErScoreWork *w);

/* 0x00af6700 */
void *module_entry(void *parent)
{
    return end_result_create(parent);
}

/* 1 when the record makes the top ten (or there is no table). */
int end_result_rank_check(void)
{
    int rec[6];
    int tbl;

    switch (game_type_get()) {
    case 4:
        tbl = 4;
        break;
    case 5:
        switch ((unsigned int)ConfigGetExtraType()) {
        case 0:
            tbl = 5;
            break;
        case 1:
            tbl = 7;
            break;
        case 3:
            tbl = 6;
            break;
        default:
            return 1;
        }
        break;
    default:
        return 1;
    }
    ConfigMakeGameRecord(rec);
    return ConfigRankingFind(tbl, rec) < 10;
}

/* steer(cast) */
/* 0x00af6720 */
void end_result_init(EndResultWork *w)
{
    end_result_textures = model_load_timed(model_load_req, 0xe000);
    pad_input_enable(0);
    switch (((unsigned int (*)(void))game_type_get)()) {
    case 3:
        w->result = er_result_create(w);
        er_bg_create(w, 0);
        break;
    case 5:
        if (ConfigGetExtraType())
            goto end;
    case 4:
        w->score = er_score_create(w);
        do { er_bg_create(w, 1); } while (0);
        break;
    default:
    end:
        FINISH_IF(w);
        return;
    }
    w->msg = result_message_disp_create(w, 0);
#ifdef VERSION_US
    if (game_type_get() == 5 && (ConfigGetSelect(5) || ConfigGetSelect(7)))
        result_message_create_child(w->msg);
#else
    if (game_type_get() != 3 && !end_result_rank_check())
        result_message_create_child(w->msg);
#endif
}

/* 0x00af6858 */
void end_result_release(void)
{
    model_unload_timed(end_result_textures);
}

/* 0x00af6868 */
void end_result_exec(EndResultWork *w)
{
    switch (w->state) {
    case 0:
        fade_start_out(30);
        w->state++;
        break;
    case 1:
        if (fade_is_idle())
            w->state++;
        break;
    case 2:
        if (er_result_is_done(w->result) && er_score_is_done(w->score) && result_message_is_done(w->msg))
            w->state = 100;
        break;
    case 100:
        fade_start(0, 1, 120);
        w->state++;
        break;
    case 101:
        if (fade_is_idle())
            do { FINISH(w); } while (0);
        break;
    }
}

/* 0x00af6970 */
void end_result_disp(void)
{
}

/* 0x00af6978 */
void *end_result_create(void *parent)
{
    return task_create(parent, &end_result_desc, 16);
}

/* 0x00af69a0 */
void er_bg_draw_rects(int row, int color)
{
    ErQuad *rect = er_bg_quads[row];
    float (*uv)[4] = er_bg_quad_uvs;
    unsigned int i;

    for (i = 0; i < 5; i++) {
        tex_rect_draw_uv(rect->tex, color, *uv, rect->x, rect->y, 2.2222223f, rect->width, rect->h);
        rect++;
        uv++;
    }
}

/* 0x00af6a40 */
void er_bg_disp(int *w)
{
    unsigned int type;
    unsigned int col;

    ulGxSetTextureList(end_result_textures);
    draw_set_blend(4, 5);
    type = game_type_get();
    if (type < 3)
        col = COL_WHITE;
    else if (type < 5)
        col = COL_WHITE;
    else if (type == 5) {
        ConfigGetExtraType();
        col = COL_WHITE;
    } else
        col = COL_WHITE;
    er_bg_draw_rects(w[0], col);
}

/* 0x00af6ac0 */
void *er_bg_create(void *parent, int kind)
{
    int *w = task_create(parent, &er_bg_desc, 4);

    w[0] = kind;
    return w;
}

/* 0x00af6af8 */
void er_result_exec(ErResultWork *w)
{
    if (!(pad_main->raw.trig & pad_decide_mask)) {
        w->t += 1.0f;
        if (!(480.0f < w->t))
            return;
    }
    w->done = 1;
}

/* steer(cast) */
/* 0x00af6b50 */
void er_result_draw_rate(int y, float pct)
{
    char buf[20];
    float z, digit_h, fy, step;
    int digit;

    format_number_commas(buf, (int)pct);
    z = 2.5f;
    digit_h = 26.0f;
    font_print(-206.0f, (float)(y - 42), z, RESULT_TEXT_COL, buf);
    step = 20.0f;
    fy = (float)y - digit_h;
    tex_rect_draw_uv(7, RESULT_TEXT_COL, er_result_digit_uvs[11], 478.0f, fy, z, 6.0f, digit_h);
    digit = (int)(((float (*)(float))float_frac)(pct) * 10.0f);
    tex_rect_draw_uv(7, RESULT_TEXT_COL, er_result_digit_uvs[digit], 484.0f, fy, z, step, digit_h);
    digit = (int)(((float (*)(float))float_frac)(pct) * 100.0f) - digit * 10;
    tex_rect_draw_uv(7, RESULT_TEXT_COL, er_result_digit_uvs[digit], 504.0f, fy, z, step, digit_h);
}

/* 0x00af6ce0 */
void er_result_disp(ErResultWork *w)
{
    float z, cap_h, x, wd, pct_x, pct_h;

    draw_set_blend(4, 5);
    z = 2.5f;
    cap_h = 41.0f;
    x = 84.0f;
    ulGxSetTextureList(end_result_textures);
    wd = 256.0f;
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[0], x, 106.0f, z, wd, cap_h);
    pct_x = 524.0f;
    pct_h = 36.0f;
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[3], pct_x, 112.0f, z, cap_h, pct_h);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[1], x, 177.0f, z, wd, cap_h);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[3], pct_x, 183.0f, z, cap_h, pct_h);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[2], x, 249.0f, z, wd, cap_h);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_result_caption_uvs[3], pct_x, 255.0f, z, cap_h, pct_h);
    ulGxSetTextureList(system_ptm);
    font_set(3);
    er_result_draw_rate(148, w->rate[0]);
    er_result_draw_rate(219, w->rate[1]);
    er_result_draw_rate(291, w->rate[2]);
}

/* 0x00af6ed0 */
void er_result_init(ErResultWork *w)
{
    w->rate[0] = ConfigGameAnalyzeRate();
    w->rate[1] = ConfigGameShotdownRate();
    w->rate[2] = ConfigGameItemRate();
}

/* 0x00af6f08 */
void *er_result_create(void *parent)
{
    return task_create(parent, &er_result_desc, 20);
}

/* 0x00af6f30 */
int er_result_is_done(ErResultWork *w)
{
    int ret = 1;

    if (w)
        ret = w->done;
    return ret;
}

/* 0x00af6f48 */
void er_score_draw_number(int num, float x, float y, float z)
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
                    tex_rect_draw_uv(7, RESULT_TEXT_COL, er_score_comma_uv, x, y, z, 6.0f, 26.0f);
                }
            }
            x -= dx;
            tex_rect_draw_uv(7, RESULT_TEXT_COL, er_score_digit_uvs[ch], x, y, z, 20.0f, 26.0f);
        }
    }
}

/* 0x00af70b0 */
void er_score_exec(ErScoreWork *w)
{
    Pad *pad = pad_main;

    if (pad->raw.on & pad_decide_mask)
        w->count += 3;
    w->count++;
    if (w->frames < w->count) {
        w->shown = w->score;
        if ((pad->raw.trig & pad_decide_mask) || (float)w->frames + 480.0f < (float)w->count)
            w->done = 1;
    } else {
        w->shown = w->score * w->count / w->frames;
    }
}

/* 0x00af7168 */
void er_score_disp(ErScoreWork *w)
{
    float z;

    font_set(3);
    z = 2.5f;
    ulGxSetTextureList(end_result_textures);
    tex_rect_draw_uv(0, RESULT_TEXT_COL, er_score_frame_uv, 511.0f, 193.0f, z, 54.0f, 31.0f);
    if (w->mode == 0)
        font_print(84.0f, 182.0f, z, RGBA(0x72, 0x3d, 0x21, 0x80), "area5");
    else
        font_print(84.0f, 182.0f, z, RESULT_TEXT_COL, er_score_total_texts[game_lang]);
    ulGxSetTextureList(system_ptm);
    er_score_draw_number(w->shown, 491.0f, 198.0f, 2.5f);
}

/* 0x00af7290 */
void er_score_init(ErScoreWork *w)
{
    w->score = player_status_get_score();
    w->frames = 120;
    if (game_type_get() == 4)
        w->mode = 0;
    else
        w->mode = 1;
}

/* 0x00af72e0 */
void *er_score_create(void *parent)
{
    return task_create(parent, &er_score_desc, 24);
}
