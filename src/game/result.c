/*
 * Stage result: "Result" types the clear message and clears the background, then shows the
 * result screen (world, player and camera motion, score, analysis rates, stage shells, unlocks;
 * in play-through the change to the next stage, and the final form before stage 5).
 */

#include "system.h"
#include "types_game.h"
#include "debug.h"
#include "libc.h"
#include "nj_draw.h"
#include "nj_strip.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "result.h"
#include "anim.h"
#include "approach.h"
#include "billboard.h"
#include "boss_start.h"
#include "camera.h"
#include "color.h"
#include "config.h"
#include "console.h"
#include "console_com.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "game_flow_editor.h"
#include "game_flow_enemy.h"
#include "heap.h"
#include "light.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pad_vib.h"
#include "pause.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "result_message.h"
#include "rez_wire_compile.h"
#include "screen.h"
#include "sequence.h"
#include "sound_beat.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"

typedef struct { float x, y, z, u, v; int col; } GVtx;

#ifdef VERSION_US

#define player_model_set_rate player_model_set_rate

#define player_model_update player_model_update
#define camera_eval_motion camera_eval_motion

#endif /* VERSION_US */

typedef struct { float x, y, z; } ResultMotionVec;

typedef struct { char pad_00[8]; unsigned long *reg; } GsInner;
typedef struct { GsInner *tex; } GsObj;

extern ResultWork *result_work;
extern unsigned stage_exit_flags;
extern unsigned system_flags;
extern unsigned game_flags;
extern unsigned pad_decide_mask;
extern int *pad_main;
extern int scr_root;
extern void (*overlay_result_hook)(void);
extern int result_stage_tables[];
extern struct TaskDesc result_desc;
extern char result_digit_uvs[][16];
extern char result_sep_uv[];
extern ScreenRect screen_layout;
extern int system_ptm;
extern float result_score_label_pos[][4];
extern float result_score_label_uvs[][4];

extern struct TaskDesc result_score_desc;
extern float result_digit_w;
extern float result_digit_h;
extern float result_decimal_w;
extern float result_decimal_h;
extern struct TaskDesc result_number_desc;
extern char result_motion_cam_hooks[];
extern void ulMatrixPush(void *);
extern struct TaskDesc result_motion_desc;
extern struct TaskDesc world_draw_desc;
extern int result_shell_models[];
extern struct TaskDesc result_shell_desc;
extern Vtx screen_quad[4];
extern struct TaskDesc background_delete_desc;

/* Analysis rates, stage/boss/total scores, previous bests; game type 3 registers a new record. */
/* 0x0015d1e0 */
void result_analyze(ResultWork *w)
{
    float buf[8];
    int stage;
    unsigned i;
    float v;

    stage = stage_get_num();
    ConfigMakeStageRecord(buf);
    w->stage = stage;
    w->analyze = buf[2];
    w->shotdown = buf[3];
    w->item = buf[4];
    w->stage_score = player_status_get_stage_score();
    w->boss_score = player_status_get_boss_score();
    w->total_score = w->stage_score + w->boss_score;
    for (i = 0; i < 4; i++) {
        v = *(float *)ConfigGetStageRecord(i);
        w->best_analyze[i] = v;
        DEBUG_PRINTF("Result AnalyzeOld % 8.3f %d\n", v, i);
    }
    w->new_analyze = buf[2];
    DEBUG_PRINTF("Result AnalyzeNew % 8.3f\n", buf[2]);
    if (game_type_get() == 3) {
        ConfigMakeGameRecord(buf);
        ConfigStageDataRegist(stage, buf);
    }
}

/*
 * No pausing; records the clear per game type. Play-through: next stage follows (5 always after
 * 4); after stage 5 straight to the next sequence; extra type 3 only locks input.
 */
/* steer(cast) */
/* 0x0015d2f8 */
void result_init(ResultWork *w, ResultDef *res)
{
    int three = 3;
    int stage;
    unsigned mode;
    unsigned extra_type;
    int r;
    int g;
    int b;
    int a;

    system_pause_disable();
    w->def = res;
    result_work = w;
    w->col = console_get_pcol();
    r = color_get_a(w->col);
    g = ((int (*)(int))color_get_r)(w->col);
    b = color_get_g(w->col);
    a = color_get_b(w->col);
    w->rgba[0] = (float)r / 255.0f;
    w->rgba[1] = (float)g / 255.0f;
    *(int *)&w->time = 0; /* steer(type) */
    *(int *)&w->step = 0; /* steer(type) */
    w->timing = 0;
    w->rgba[2] = (float)b / 255.0f;
    w->rgba[3] = (float)a / 255.0f;
    stage = stage_get_num();
    w->go_next = 0;
    mode = game_type_get();
    switch (mode) {
    case 2:
        break;
    case 3:
        ConfigSetClearFlag(stage, 1);
        break;
    case 4:
        ConfigAddScoreAttackClear(stage, 1);
        break;
    case 5:
        extra_type = ConfigGetExtraType();
        switch (extra_type) {
        case 0:
            if (stage == 5)
                ConfigAddGameClear(1);
            else if ((unsigned)stage < 4) {
                w->next_stage = stage + 1;
                w->go_next = 1;
            }
            break;
        case 1:
        case 2:
            ConfigSetClearFlag(stage, 1);
            break;
        case 3:
            ConfigSetClearFlag(stage, 1);
            TASK_HDR(w)->exec = (void (*)(void *))result_lock_input;
            break;
        }
        break;
    }
    if (stage == 5) {
        TASK_HDR(w)->exec = (void (*)(void *))result_analyze_set_next;
    } else if (stage == 4) {
        w->next_stage = 5;
        w->go_next = 1;
    }
    peri_act_select_reset(2);
}

/* 0x0015d510 */
void result_release(ResultWork *w)
{
    peri_act_select_reset(0);
    TASK_END(w->log_task);
    result_work = 0;
    do { model_unload_timed(w->models); } while (0);
    model_pnb_unload(w->pnb);
    w->models = 0;
    w->pnb = 0;
}

/* Without a result screen: once, input off and leave the stage (exit flag 0x10). */
/* 0x0015d578 */
void result_lock_input(ResultWork *w)
{
    if (!w->phase) {
        pad_input_disable(0);
        stage_exit_flags |= 0x10;
        w->phase++;
    }
}

/* After stage 5: results, then sequence 14. */
/* 0x0015d5c8 */
void result_analyze_set_next(ResultWork *w)
{
    result_analyze(w);
    sound_sequence_set_next(14);
}

/*
 * Leaves the stage, loads models, builds the screen, fades in, next stage setup, unlocks near the
 * motion's end, fades out. Result time: 1 per frame, 4 with decide held.
 */
/* 0x0015d5e8 */
void result_show_exec(ResultWork *w)
{
    ResultDef *res = w->def;
    float step;
    int pnb_hdl, model_hdl;

    switch (w->phase) {
    case 0:
        pad_input_disable(0);
        stage_exit_flags |= 0x10;
        system_flags &= ~SYS_STABILIZER;
        w->phase++;
        break;
    case 1:
        pnb_hdl = model_pnb_load(res->files);
        model_hdl = model_load_timed(res->files + 8, 0x930000);
        w->pnb = pnb_hdl;
        w->models = model_hdl;
        goto next;
    case 2:
        if (stage_is_not_ready() == 0)
            break;
        result_analyze(w);
        world_draw_create(w);
        if ((game_flags & 0x40) && w->stage != 4)
            w->score_task = result_score_create(w);
        w->number_task = result_number_create(w);
        if ((unsigned)w->stage < 4)
            w->shell_task = result_shell_create(w->new_analyze, w, w->best_analyze);
        w->motion_task = result_motion_create(w);
        system_flags |= SYS_STABILIZER;
        system_set_frame_rate(1);
        w->phase++;
        break;
    case 3:
        w->timing = 1;
        fade_start_out(60);
        w->phase++;
        break;
    case 4:
        if (w->go_next) {
            stage_next_set(w->next_stage);
            module_stage_setup(w);
        }
        w->phase++;
        break;
    case 5:
        if (result_motion_get_remaining(w->motion_task) <= 480.0f) {
            if (!w->go_next)
                w->message_task = result_message_disp_create(w, 1);
            w->phase++;
        }
        break;
    case 6:
        if (result_motion_get_remaining(w->motion_task) <= 60.0f)
            w->phase = 500;
        break;
    case 500:
        if (!w->go_next && !result_message_is_done(w->message_task))
            break;
        fade_start(0, 1, 60);
        w->phase++;
        break;
    case 501:
        if (fade_is_idle() != 1)
            break;
        if (!w->go_next) {
            if (game_type_get() == 6)
                GameExit(0);
            else
                GameExit(1);
        }
        TASK_END(w);
    next:
        w->phase++;
        break;
    }
    if (w->timing) {
        step = 1.0f;
        if (pad_main[26] & pad_decide_mask)
            step = 4.0f;
        w->step = step;
        w->time += step;
    }
}

/*
 * Stage clear: reads both result files; on a bar's first beat types the clear message (LogIO) and
 * starts the background clear (and overlay hook); when done fades out -> result_show_exec.
 */
/* 0x0015d928 */
void result_exec(ResultWork *w)
{
    ResultDef *res = w->def;
    int buf[5];
    int *log_task;
    int *bg_task;

    switch ((unsigned)w->phase) {
    case 0:
        file_request_simple((void *)res->files, 1);
        file_request_simple((char *)res->files + 8, 1);
        w->phase++;
        break;
    case 1:
        if (file_request_all_done())
            w->phase++;
        break;
    case 2:
        if (beat_grid_hit(0) == 0)
            break;
        if (beat_clock_get_haku() & 3)
            break;
        console_set_disp_level(0);
        if (res->msg) {
            buf[0] = res->msg;
            buf[1] = game_menu_color;
            buf[2] = 15;
            buf[3] = 120;
            buf[4] = 15;
            w->log_task = logio_create(scr_root, buf);
        }
        w->bg_task = background_delete_create(w, 150);
        texforce_disable();
        if (overlay_result_hook)
            overlay_result_hook();
        w->phase++;
        break;
    case 3:
        log_task = w->log_task;
        if (!log_task || (TASK_HDR(log_task)->flags & 1)) {
            w->log_task = 0;
            log_task = 0;
        }
        bg_task = w->bg_task;
        if (!bg_task || (TASK_HDR(bg_task)->flags & 1))
            w->bg_task = 0;
        if (log_task)
            break;
        if (w->bg_task)
            break;
        {
            int col = res->fade_col;

            w->log_task = 0;
            fade_start(0, col, 60);
        }
        w->phase++;
        break;
    case 4:
        if (fade_is_idle() != 1)
            break;
        console_clear_home(0);
        w->phase = 0;
        TASK_HDR(w)->exec = (void (*)(void *))result_show_exec;
        break;
    }
}

/* 0x0015db08 */
void result_disp(void)
{
}

/* Returns the running one if any; 0 for a stage without a result. */
/* 0x0015db10 */
void *result_create(void)
{
    void *mod = module_get_current();
    int n;
    int table;
    void *work = result_work;

    if (work == 0) {
        n = stage_get_num();
        table = result_stage_tables[n];
        if (table)
            return task_create_arg(mod, &result_desc, sizeof(ResultWork), table);
        return 0;
    }
    return work;
}

/* Right-aligned, digits 20 units apart, separator every three digits. */
/* 0x0015db88 */
void result_draw_number(int val, float x, float y, float z)
{
    char buf[32];
    int i;
    int ch;
    float dx = 20.0f;

    sprintf(buf, "%15d", val);
    for (i = 14; i != 0; i--) {
        ch = (unsigned char)buf[i];
        if ((unsigned)(ch - 48) < 10) {
            if (i != 14) {
                if (i % 3 == 2) {
                    x -= 6.0f;
                    tex_rect_draw_uv(7, COL_WHITE, result_sep_uv, x, y, z, 6.0f, 26.0f);
                }
            }
            x -= dx;
            tex_rect_draw_uv(7, COL_WHITE, result_digit_uvs[ch], x, y, z, 20.0f, 26.0f);
        }
    }
}

/* Stage, boss and total score; first wait 240 frames. */
/* 0x0015dcf0 */
void result_score_init(ResultScoreWork *w, ResultWork *res)
{
    w->res = res;
    w->score[0] = res->stage_score;
    w->score[1] = res->boss_score;
    w->score[2] = 0;
    w->score[3] = res->total_score;
    w->row_timer = 0;
    w->count_timer = 240;
}

/* 0x0015dd20 */
void result_score_release(void)
{
}

/*
 * From result time 730 one more row every 60 frames (up to three); then the total counts up from
 * 0 over 180 frames.
 */
/* 0x0015dd28 */
void result_score_exec(ResultScoreWork *w)
{
    ResultWork *res = w->res;
    int fade;
    float rate;
    float dt;

    dt = res->step;
    if (730.0f <= res->time) {
        w->row_timer -= (int)dt;
        if (w->row_timer <= 0) {
            w->row_timer = 60;
            w->rows++;
            if (w->rows >= 4)
                w->rows = 3;
        }
    }
    if (w->rows >= 3) {
        fade = w->count_timer;
        if (fade > 0) {
            fade--;
            w->count_timer = fade;
            rate = 1.0f - (float)fade / 180.0f;
            if (0.0f <= rate)
                w->score[2] = int_lerp_f(rate, w->score[3], 0);
        }
    }
}

/* Shown rows: label and score at the right edge. */
/* 0x0015de10 */
void result_score_disp(ResultScoreWork *w)
{
    int i;
    float *pos;
    float x;
    float y;
    float z = 2.2222223f;
    float *uv = result_score_label_uvs[0];

    draw_set_blend(4, 5);
    pos = result_score_label_pos[0];
    for (i = 0; i < w->rows; i++) {
        ulGxSetTextureList(w->models);
        y = pos[1];
        uv = result_score_label_uvs[i];
        tex_rect_draw_uv(0, COL_WHITE, uv, screen_layout.left + pos[0], y, z, pos[2], pos[3]);
        pos += 4;
        x = screen_layout.right - 64.0f - 54.0f;
        tex_rect_draw_uv(0, COL_WHITE, result_score_label_uvs[3], x, y, z, 54.0f, 31.0f);
        ulGxSetTextureList(system_ptm);
        result_draw_number(w->score[i], x, y, z);
    }
}

/* 0x0015df98 */
void *result_score_create(void *parent)
{
    void *task = task_create_inherit(parent, &result_score_desc, 40);

    result_score_init(task, parent);
    return task;
}

/* World position of the node selected by rec's mask (where the number goes). Returns 1. */
/* 0x0015dfe8 */
int result_hit_callback(int *node, int idx, ResultRate *rec)
{
    int buf[4];
    int mtx;

    if (node[0] & rec->node_mask) {
        mtx = camera_get_inv_mtx();
        ulMatrixGetTranslation(ulMatrixCur, buf);
        ulMatrixApplyPoint(mtx, buf, rec);
    }
    return 1;
}

/* uv = (u0, v0, u1, v1); 3D quad with its right edge at x. */
/* 0x0015e058 */
void result_draw_glyph(float x, float y, float z, float width, float height, int col, float *uv)
{
    GVtx vtx[4];

    vtx[3].z = z;
    vtx[2].z = z;
    vtx[0].z = vtx[1].z = z;
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = col;
    vtx[1].x = vtx[0].x = x - width;
    vtx[2].y = vtx[0].y = y + height;
    vtx[1].y = y;
    vtx[2].x = x;
    vtx[3].x = x;
    vtx[3].y = y;
    vtx[0].u = uv[0];
    vtx[1].u = uv[0];
    vtx[0].v = uv[1];
    vtx[2].v = uv[1];
    vtx[1].v = uv[3];
    vtx[2].u = uv[2];
    vtx[3].u = uv[2];
    vtx[3].v = uv[3];
    nj_clip_tex_strip_begin(1);
    nj_clip_tex_strip_vertices(vtx, 4);
    nj_clip_tex_strip_end();
}

/* "nnn.nn" right to left at rec's position: two small decimals, point, three large digits. */
/* 0x0015e108 */
void result_draw_float(ResultRate *rec, int res, float v)
{
    char buf[24];
    float x;
    float y;
    float z;
    float width;
    float height;
    int i;
    int ch;

    sprintf(buf, "%6.2f", v);
    x = rec->pos[0];
    y = rec->pos[1];
    z = rec->pos[2];
    width = result_decimal_w;
    height = result_decimal_h;
    ch = buf[5];
    if ((unsigned)(ch - 48) < 10)
        result_draw_glyph(x, y, z, width, height, res, result_digit_uvs[ch]);
    x -= width;
    if ((unsigned)(buf[4] - 48) < 10)
        result_draw_glyph(x, y, z, width, height, res, result_digit_uvs[buf[4]]);
    x -= width;
    width = 6.0f;
    result_draw_glyph(x, y, z, width, height, res, result_digit_uvs[58]);
    x -= width;
    width = result_digit_w;
    height = result_digit_h;
    for (i = 2; i >= 0; i--) {
        ch = buf[i];
        if ((unsigned)(ch - 48) < 10)
            result_draw_glyph(x, y, z, width, height, res, result_digit_uvs[ch]);
        x -= width;
    }
}

/* Per number table entry a model whose marked node places the rate. No table: ends at once. */
/* steer(cast) */
/* 0x0015e2d8 */
void result_number_init(ResultNumberWork *w, ResultWork *res)
{
    int *tbl;
    int *ent;
    char *p;
    ResultRate *rec;
    int i;
    unsigned kind;

    w->res = res;
    tbl = res->def->numbers;
    if (!tbl) {
        TASK_END(w);
        return;
    }
    ent = tbl;
    p = w->parts[0];
    rec = w->rates;
    for (i = 3; i; i--) {
        dc_object_init(p, *((int *(*)(void *, int))pnb_get_entry)(w->pnb, ent[0]), 0, 0);
        dc_object_set_callback(p, result_hit_callback, rec);
        kind = ent[2];
        rec->node_mask = ent[1];
        switch (kind) {
        case 0:
            rec->val = w->res->analyze;
            break;
        case 1:
            rec->val = w->res->shotdown;
            break;
        case 2:
            rec->val = w->res->item;
            break;
        default:
            break;
        }
        p += 92;
        ent += 3;
        rec++;
    }
}

/* Number models, then the three rates in the console colour. */
/* 0x0015e400 */
void result_number_disp(ResultNumberWork *w)
{
    char *rec = (char *)w->rates;
    char *part = w->parts[0];
    int col = w->res->col;
    int i = 3;

    do {
        dc_object_draw(part);
        part += 92;
    } while (--i);
    draw_set_blend(4, 5);
    part = rec;
    i = 3;
    ulGxSetTextureList(system_ptm);
    ulGxSelectTexture(ulGxGetTextureList(), 7);
    do {
        result_draw_float((ResultRate *)part, col, ((ResultRate *)part)->val); /* steer(cast) */
        part += sizeof(ResultRate);
    } while (--i);
}

/* 0x0015e4b8 */
void *result_number_create(void *parent)
{
    void *task = task_create_inherit(parent, &result_number_desc, sizeof(ResultNumberWork));

    result_number_init(task, parent);
    return task;
}

/* 0x0015e508 */
void result_motion_cam_init(void)
{
}

/* Camera follows the result's camera motion. */
/* 0x0015e510 */
void result_motion_cam_update(Camera *dst, ResultMotionWork *src)
{
    *dst = src->cam;
}

/* 0x0015e568 */
void result_motion_cam_release(void)
{
}

/* Player and camera motion, result camera hook, player model for the level; display list 6. */
/* steer(cast) */
/* 0x0015e570 */
void result_motion_init(ResultMotionWork *w, ResultWork *res)
{
    ResultDef *cfg = res->def;
    char *cam;
    int entry;
    unsigned n;

    w->res = res;
    w->node = (void *)system_pnb_entry_get(0);
    w->motion = (void *)*((int *(*)(void *, int))pnb_get_entry)(w->pnb, cfg->player_motion);
    w->scale = 1.0f;
    cam = camera_get_current();
    w->cam_motion = (float *)*((int *(*)(void *, int))pnb_get_entry)(w->pnb, cfg->cam_motion);
    w->cam_hdr = *((int *(*)(void *, int))pnb_get_entry)(w->pnb, cfg->cam_hdr);
    w->cam_node = camera_node_add(2, result_motion_cam_hooks, w);
    w->cam = *(Camera *)cam;
    player_model_init();
    entry = cfg->player_models[player_status_get_level()];
    if (entry >= 0)
        player_model_set(*((int *(*)(void *, int))pnb_get_entry)(w->pnb, entry));
    n = ((unsigned *)w->motion)[1];
    w->time_max = (float)n - 1.0f;
    task_set_dlist(w, 6);
}

/* steer(cast) */
/* 0x0015e6e8 */
void result_motion_release(ResultMotionWork *w)
{
    ((void (*)())player_model_release)(w);
    camera_node_remove(w->cam_node);
}

/*
 * Motions at result time. Stage 5 next: below level 6 the model shrinks over frames 365..380, then
 * the level 6 final form grows back by 0.08 a frame.
 */
/* steer(cast) */
/* 0x0015e710 */
void result_motion_exec(ResultMotionWork *w)
{
    float limit = w->time_max;
    float t = w->res->time;
    float step = w->res->step;
    float frame;
    float nf;
    float scale;
    float v;
    ResultDef *cfg;
    float buf[4];
    int *args[3];
    unsigned len;

    if (limit <= t)
        t = limit;
    w->time = t;
    if (w->res->next_stage == 5) {
        if ((unsigned)player_status_get_level() < 6) {
            frame = w->time;
            if (380.0f <= frame) {
                cfg = w->res->def;
                DEBUG_PRINTF("Player Change To Final Form\n");
                player_model_release();
                player_status_set_level(6);
                if (cfg->player_models[6] >= 0) {
                    player_model_init();
                    player_model_set(*((int *(*)(void *, int))pnb_get_entry)(w->pnb, cfg->player_models[6]));
                }
            } else if (365.0f <= frame) {
                w->scale = (380.0f - frame) / 15.0f;
            }
        } else {
            scale = w->scale;
            if (scale < 1.0f) {
                nf = scale + 0.08f;
                w->scale = nf;
                if (nf < 1.0f)
                    v = nf;
                else
                    v = 1.0f;
                w->scale = v;
            }
        }
    }
    args[0] = (int *)w->pos;
    args[1] = w->rot;
    args[2] = (int *)buf;
    anim_eval_hierarchy((int)w->node, (int)w->motion, (struct AnimCursorSetSrc *)args, t);
    player_model_set_rate(step);
    player_model_update();
    len = ((unsigned *)w->cam_hdr)[1];
    t = w->res->time;
    nf = (float)len - 1.0f;
    if (nf <= t)
        t = nf;
    camera_eval_motion(&w->cam, w->cam_motion, (void *)w->cam_hdr, t);
}

/* 0x0015e920 */
void result_motion_disp(ResultMotionWork *w)
{
    float *pos;
    float z;
    float y;

    ulMatrixPush(w);
    psGsSetZMask(0);
    pos = w->pos;
    z = pos[2];
    y = pos[1];
    do { vu0_matrix_translate(ulMatrixCur, pos[0], y, z); } while (0);
    result_rotate_yxz(w->rot);
    vu0_matrix_scale(ulMatrixCur, w->scale, w->scale, w->scale);
    player_model_draw();
    psGsSetZMask(1);
    ulMatrixPop();
}

/* 0x0015e9a0 */
void *result_motion_create(void *w)
{
    return task_create_inherit_arg(w, &result_motion_desc, sizeof(ResultMotionWork), w);
}

/* 0x0015e9c8 */
float result_motion_get_remaining(ResultMotionWork *w)
{
    return w->time_max - w->time;
}

/*
 * Result fog and background colour; per world table entry a solid or wire model with colour (or
 * console colour), highlight and hide-at-end flags.
 */
/* steer(cast) */
/* 0x0015e9d8 */
void world_draw_init(WorldDrawWork *w, ResultWork *res)
{
    int *ent;
    int root;
    ResultDef *cfg = res->def;
    WorldPart *part;
    int n;
    int clip;
    int cnt;

    w->res = res;
    cnt = cfg->world_cnt;
    w->cnt = cnt;
    w->parts = heap_block_tag(game_malloc(cnt * sizeof(WorldPart)), "result.c", 1810);
    fog_set(cfg->fog_color, cfg->fog_mode, cfg->fog_near, cfg->fog_far);
    BackColReq(cfg->fog_color);
    ent = cfg->world_ents;
    part = w->parts;
    n = w->cnt;
    do {
        root = *((int *(*)(int, int))pnb_get_entry)(w->pnb, ent[2]);
        if (ent[3] >= 0)
            clip = *((int *(*)(int, int))pnb_get_entry)(w->pnb, ent[3]);
        else
            clip = 0;
        dc_object_init(part, root, clip, 4096);
        switch (ent[0]) {
        case 0:
            dc_object_rebuild(part);
            break;
        case 1:
            dc_object_set_wire(part, model_pnb_get_wire(w->pnb, ent[2]));
            break;
        }
        if (ent[4])
            part->col = ent[4];
        else
            part->col = w->res->col;
        part->highlight = ent[1];
        part->hide_at_end = ent[5];
        ent += 6;
        part++;
    } while (--n);
}

/* 0x0015eb40 */
void world_draw_release(WorldDrawWork *w)
{
    WorldPart *part = w->parts;
    int n = w->cnt;

    do {
        dc_object_release(part);
        part++;
    } while (--n);
    heap_free(w->parts, 0);
}

/* Animations at result time. */
/* 0x0015eb98 */
void world_draw_exec(WorldDrawWork *w)
{
    float z = w->res->time;
    WorldPart *part = w->parts;
    int n = w->cnt;
    float lim;
    float v;

    do {
        lim = part->model.frames;
        v = z;
        if (lim <= v)
            v = lim - 1.0f;
        part->model.frame = v;
        part++;
    } while (--n);
}

/*
 * Fog and second light off; highlighted in the result colour; hide-at-end ones hidden once their
 * animation finishes.
 */
/* 0x0015ebe0 */
void world_draw_disp(WorldDrawWork *w)
{
    float lim = 1.1f;
    float intensity[2];
    WorldPart *part;
    int n;
    int old;
    int vis;

    ulGxSetTextureList(w->models);
    part = w->parts;
    n = w->cnt;
    old = fog_set_enable(1);
    light_get_intensity(intensity, intensity + 1);
    light_set_intensity(intensity[0], 0.0f);
    do {
        vis = 1;
        if (part->hide_at_end) {
            if (part->model.frames - lim <= part->model.frame)
                vis = 0;
        }
        if (vis) {
            if (part->highlight) {
                draw_set_override_flags(DRAW_COLOR_SET);
                draw_set_override_color(w->res->rgba);
            } else {
                draw_set_override_flags(0);
            }
            draw_set_color(part->col);
            dc_object_draw(part);
        }
        part++;
    } while (--n);
    {
        float y = intensity[1];
        float x = intensity[0];

        light_set_intensity(x, y);
    }
    fog_set_enable(old);
    draw_set_override_flags(0);
}

/* 0x0015ecf8 */
void *world_draw_create(void *parent)
{
    void *task = task_create_inherit(parent, &world_draw_desc, sizeof(WorldDrawWork));

    world_draw_init(task, parent);
    return task;
}

/*
 * Shell per stage with best analysis vals[i] below 100%; the current stage's breaks when this play
 * reached 100% (thr).
 */
/* steer(cast) */
/* 0x0015ed48 */
void result_shell_init(ResultShellWork *w, ResultWork *res, float *vals, float thr)
{
    ShellPart *part;
    int stage;
    int x;
    int y;
    unsigned i;
    int *mdl;

    w->res = res;
    stage = stage_get_num();
    for (i = 0; i < 4; i++) {
        mdl = result_shell_models + i * 6;
        part = &w->parts[i];
        x = *((int *(*)(int, int))pnb_get_entry)(w->pnb, mdl[2]);
        y = *((int *(*)(int, int))pnb_get_entry)(w->pnb, mdl[3]);
        dc_object_init(part, x, y, 0);
        if (i == stage && 100.0f <= thr)
            part->breaks = 1;
        else
            part->breaks = 0;
        if (vals[i] < 100.0f)
            part->shown = 1;
        else
            part->shown = 0;
    }
}

/* 0x0015ee60 */
void result_shell_release(ResultShellWork *w)
{
    int n = 4;
    ShellPart *part = w->parts;

    do {
        dc_object_release(part);
        part++;
    } while (--n);
}

/* Breaking shell's animation at result time; gone at its last frame. */
/* 0x0015eea0 */
void result_shell_exec(ResultShellWork *w)
{
    float z = w->res->time;
    int n = 4;
    ShellPart *part = w->parts;
    float lim;
    float v;

    do {
        n--;
        if (part->shown) {
            if (part->breaks) {
                lim = part->model.frames - 1.0f;
                if (lim <= z) {
                    v = lim;
                    part->shown = 0;
                } else
                    v = z;
                part->model.frame = v;
            }
        }
        part++;
    } while (n);
}

/* 0x0015ef08 */
void result_shell_disp(ResultShellWork *w)
{
    ShellPart *part = w->parts;
    int n = 4;

    ulGxSetTextureList(w->models);
    do {
        if (part->shown)
            dc_object_draw(part);
        part++;
    } while (--n);
}

/* 0x0015ef68 */
void *result_shell_create(float thr, ResultWork *parent, float *vals)
{
    void *task = task_create_inherit(parent, &result_shell_desc, sizeof(ResultShellWork));

    if (task)
        result_shell_init(task, parent, vals, thr);
    return task;
}

/* Screen via the capture texture, player in a second pass; fog settings kept to fade from. */
/* 0x0015efd8 */
void background_delete_init(BgDeleteWork *w)
{
    screen_swap_render(1);
    screen_swap_render_back(0);
    player_set_2nd_draw(1);
    w->capturing = 1;
    w->fog_near = fog_get_near();
    w->fog_col = fog_get_color();
    w->back_col = fog_get_back_color();
}

/* Once. */
/* 0x0015f030 */
void background_delete_restore(BgDeleteWork *w)
{
    if (w->capturing) {
        screen_swap_render(0);
        screen_swap_render_back(1);
        player_set_2nd_draw(0);
        w->capturing = 0;
    }
}

/* Also clears the game flow; display list 12. */
/* 0x0015f078 */
void background_delete_start(BgDeleteWork *w, void *owner, int frames)
{
    w->owner = owner;
    w->fade = 1.0f;
    w->fade_step = -1.0f / (float)frames;
    background_delete_init(w);
    GameFlowClear();
    task_set_dlist(w, 12);
}

/* 0x0015f0e0 */
void background_delete_release(BgDeleteWork *w)
{
    background_delete_restore(w);
}

/*
 * Fades fog and background to black over the frames; then removes enemies, the stage task and the
 * boss, and ends next frame. Ends at once with its owner.
 */
/* 0x0015f0e8 */
void background_delete_exec(BgDeleteWork *w)
{
    int color;
    int back;
    float near;
    float far;
    int mode;
    int fog_col;
    int back_col;
    float t;

    switch ((unsigned)w->phase) {
    case 0:
        t = w->fade + w->fade_step;
        w->fade = t;
        if (t <= 0.0f) {
            w->fade = 0.0f;
            flow_enemy_kill_all();
            stage_end_child();
            boss_start_kill();
            w->phase++;
        }
        break;
    case 1:
        background_delete_restore(w);
        TASK_END(w);
        break;
    }
    mode = fog_get_current(&color, &back, &near, &far);
    near = float_lerp(near, w->fog_near, 1.0f - w->fade);
    fog_set(color, mode, near, far);
    fog_col = color_blend(w->fog_col, COL_BLACK, 1.0f - w->fade);
    back_col = color_blend(w->back_col, COL_BLACK, 1.0f - w->fade);
    fog_req_color(fog_col);
    BackColReq(back_col);
    if (!w->owner || (TASK_HDR(w->owner)->flags & 1))
        do {
            TASK_HDR(w)->flags |= 1;
        } while (0);
}

/* Once capturing: background colour fill, then the captured frame, fading with the fade. */
/* steer(cast) */
/* 0x0015f260 */
void background_delete_disp(BgDeleteWork *w)
{
    Vtx vtx[4];
    BgVtx bg[4];
    float eps;
    int saved, fog, r, g, b;
    unsigned col, tint;
    GsObj *gs;

    if (screen_get_pass_cnt() >= 2) {
        vtx[0] = screen_quad[0];
        vtx[1] = screen_quad[1];
        vtx[2] = screen_quad[2];
        vtx[3] = screen_quad[3];
        draw_set_blend(4, 5);
        eps = 1.0e-6f;
        tint = ((((int)(w->fade * 255.0f) + 1) >> 1) << 24) | RGBA(0x80, 0x80, 0x80, 0);
        vtx[0].col = tint; vtx[1].col = tint; vtx[2].col = tint; vtx[3].col = tint;
        psGsSetDepthTest(1, 1);
        psGsSetZMask(1);
        saved = fog_set_enable(0);
        fog = fog_get_back_color();
        r = ((int (*)(int))color_get_r)(fog);
        g = color_get_g(fog);
        b = color_get_b(fog);
        col = (r & 0xff) | (((g & 0xff) << 8) | ((b & 0xff) << 16));
        bg[0].z = eps; bg[1].z = eps; bg[2].z = eps; bg[3].z = eps;
        bg[0].x = 0.0f; bg[0].y = 0.0f; bg[0].c = col;
        bg[1].x = 640.0f; bg[1].y = 0.0f; bg[1].c = col;
        bg[2].x = 0.0f; bg[2].y = 480.0f; bg[2].c = col;
        bg[3].x = 640.0f; bg[3].y = 480.0f; bg[3].c = col;
        ulGxBeginStrip(0);
        ulGxAddStrip(bg, 4);
        ulGxEndStrip();
        gs = screen_get_tex_list();
        ulGxSetTextureList(gs);
        ulGxSelectTexture(ulGxGetTextureList(), 0);
        psGsWriteReg(6, *gs->tex->reg & ~((unsigned long)1 << 34));
        vtx[0].z = eps; vtx[1].z = eps; vtx[2].z = eps; vtx[3].z = eps;
        nj_tex_strip_draw(vtx, 4, 1);
        fog_set_enable(saved);
        psGsSetZMask(0);
        psGsSetDepthTest(1, 2);
    }
}

/* Under the pause task. */
/* 0x0015f538 */
void *background_delete_create(void *owner, int frames)
{
    void *parent = pause2_get_task();
    void *task = task_create(parent, &background_delete_desc, sizeof(BgDeleteWork));

    background_delete_start(task, owner, frames);
    return task;
}

/* 0x0015f5a0 */
void result_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
