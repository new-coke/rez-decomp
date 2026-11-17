/*
 * Continue screen: "Continue?" typed out, countdown from 20, a number every two seconds. Digits
 * are Num tasks of Char tasks whose wire pieces fly together. Accept continues; running out ends
 * the game.
 */

#include "se_low.h"
#include "types_game.h"
#include "task.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "pad.h"
#include "system.h"

extern void glyph_lookup(int a, int *x, int *y);
extern void *game_malloc(int size);
extern void *heap_block_tag(void *p, const char *file, int line);
extern int rand_lcg_next_a(void);
extern void heap_free(void *p, int flag);
extern void vec3_approach(float *a, float *b, float k);
extern void ang3_approach(float *a, float *b, float k);
extern struct { float f0, f4, cx, cy; } screen_cfg;
extern char char_quad_corners[];
extern char char_normal[];
extern void ulMatrixPush(void);
extern void matrix_unit_set(int a);
extern void continue_rotate_yxz(int *p);
extern int color_get_a(int c);
extern int color_blend(int a, int b, float t);
extern int line_set_color(int a);
extern int line_add_seg_w(int h, float x0, float y0, float z0, float x1, float y1, float z1, float w);
extern void line_flush(int h);
extern struct TaskDesc char_desc;

extern void num_layout(NumWork *w);
extern struct TaskDesc num_desc;
extern void char_set_rot(CharWork *w, int a, int b, int c);
extern void char_set_col(CharWork *w, int a);
extern PadState *pad_main;
extern int pad_decide_mask;
extern int console_get_pcol(void);
extern void fade_start_alpha(float a, int b, int c);
extern int continue_work;
extern unsigned system_flags;
extern unsigned stage_exit_flags;
extern int fade_is_idle(void);
extern void font_set(int font);
extern int scr_root;
extern struct TaskDesc continue_desc;
extern void char_set_pos(CharWork *w, float x, float y);
extern void char_set_scl(CharWork *w, float v);
extern void *char_task_create(void *parent, int prm);
extern void continue_count_exec(ContWork *w);
extern void continue_task_exec_end(int *w);
extern void num_set_state_min(NumWork *w, unsigned state);
extern void *num_task_create(void *parent, int prm);

/* Pieces start at the origin, random rotation up to 720 degrees per axis. No pieces: task ends. */
/* 0x0010e198 */
void char_task_init(CharWork *w, CharParam *prm)
{
    int *pos;
    int *ang;
    int n;

    w->x = prm->x;
    SND_TASK(w)->flags |= TASK_F_RUN_PAUSED;
    w->y = prm->y;
    w->time = prm->time;
    w->persp = prm->persp;
    w->col = prm->col;
    w->z = prm->z;
    w->scl = prm->scl;
    glyph_lookup(prm->ch, &w->verts, &w->cnt);
    if (w->cnt <= 0) {
        do {
            SND_TASK(w)->flags |= 1;
        } while (0);
        return;
    }
    w->pos = heap_block_tag(game_malloc(w->cnt * 12), "continue.c", 180);
    w->ang = heap_block_tag(game_malloc(w->cnt * 12), "continue.c", 181);
    ang = w->ang;
    pos = w->pos;
    n = w->cnt;
    do {
        pos[0] = 0;
        n--;
        pos[1] = 0;
        pos[2] = 0;
        pos += 3;
        ang[0] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 720.0f);
        ang[1] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 720.0f);
        ang[2] = (int)DEG_TO_ANG(RAND_FLOAT_A() * 720.0f);
        ang += 3;
    } while (n != 0);
}

/* 0x0010e368 */
void char_task_release(CharWork *w)
{
    heap_free(w->pos, 0);
    heap_free(w->ang, 0);
}

/* Assembled when the time left reaches 0. */
/* 0x0010e3a0 */
void char_task_exec(CharWork *w)
{
    float zero[3];
    float rate;
    int n;
    float *p;
    float *pos;
    float *ang;

    if (w->time > 0) {
        n = w->cnt;
        rate = 1.0f / (float)w->time;
    } else {
        n = w->cnt;
        rate = 1.0f;
    }
    if (n > 0) {
        zero[0] = 0;
        zero[1] = 0;
        zero[2] = 0;
        p = (float *)w->verts;
        pos = (float *)w->pos;
        ang = (float *)w->ang;
        do {
            vec3_approach(p, pos, rate);
            p += 6;
            pos += 3;
            ang3_approach(zero, ang, rate);
            ang += 3;
            n--;
        } while (n);
    }
    if (w->time > 0)
        w->time--;
}

/* Each piece a projected square outline, 2 px wide, darker facing away. */
/* 0x0010e488 */
void char_task_disp(CharWork *w)
{
    float pts[16];
    float corner[3];
    char nrm[16];
    float cx;
    float cy;
    float *pos;
    int *rec;
    float x;
    float y;
    float z;
    float scale;
    int n;
    int i;
    int batch;
    int blended;
    char *p;
    float *out;
    float persp;
    float *vp;

    n = w->cnt;
    if (n > 0) {
        x = w->x;
        y = w->y;
        z = w->z;
        scale = w->scl;
        pos = (float *)w->pos;
        rec = w->ang;
        cx = screen_cfg.cx;
        cy = screen_cfg.cy;
        ulMatrixPush();
        matrix_unit_set(0);
        continue_rotate_yxz(w->rot);
        do {
            ulMatrixPush();
            vu0_matrix_translate(ulMatrixCur, x + pos[0] * scale, y - pos[1] * scale, 0);
            continue_rotate_yxz(rec);
            ulMatrixApplyVector(ulMatrixCur, char_normal, nrm);
            blended = color_blend(color_get_a(w->col) + 1 >> 1 << 24 | 0x404040, w->col, (*(float *)(nrm + 8) + 1.0f) * 0.5f);
            vu0_matrix_scale(ulMatrixCur, w->scl, w->scl, w->scl);
            p = char_quad_corners;
            out = pts;
            vp = corner;
            for (i = 4; i != 0; i--) {
                ulMatrixApplyPoint(ulMatrixCur, p, vp);
                {
                    float vx, vy, vz;

                    vx = corner[0];
                    vy = vp[1];
                    do {
                        do {
                            vz = vp[2];
                        } while (0);
                    } while (0);
                    persp = w->persp / (vz + w->persp);
                    p += 12;
                    out[0] = vx * persp + cx;
                    out[1] = vy * persp + cy;
                }
                out += 4;
            }
            ulMatrixPop();
            batch = line_set_color(blended);
            {
                float start_y = pts[1];

                batch = line_add_seg_w(batch, pts[0], start_y, z, pts[4], pts[5], z, 2.0f);
            }
            {
                float start_y = pts[1];

                batch = line_add_seg_w(batch, pts[0], start_y, z, pts[8], pts[9], z, 2.0f);
            }
            batch = line_add_seg_w(batch, pts[8], pts[9], z, pts[12], pts[13], z, 2.0f);
            batch = line_add_seg_w(batch, pts[4], pts[5], z, pts[12], pts[13], z, 2.0f);
            line_flush(batch);
            pos += 3;
            rec += 3;
        } while (--n);
        ulMatrixPop();
    }
}

/* 0x0010e768 */
void *char_task_create(void *parent, int prm)
{
    return task_create_arg(parent, &char_desc, sizeof(CharWork), prm);
}

/* 0x0010e790 */
void char_set_pos(CharWork *w, float x, float y)
{
    w->x = x;
    w->y = y;
}

/* Whole-character rotation. */
/* 0x0010e7a0 */
void char_set_rot(CharWork *w, int x, int y, int z)
{
    int *rot = w->rot;

    rot[0] = x;
    rot[1] = y;
    rot[2] = z;
}

/* 0x0010e7b8 */
void char_set_col(CharWork *w, int col)
{
    w->col = col;
}

/* 0x0010e7c0 */
void char_set_scl(CharWork *w, float v)
{
    w->scl = v;
}

/* One or two digits, one Char task each, centred; start transparent, assemble over 15 frames. */
/* 0x0010e7c8 */
void num_task_init(NumWork *w, NumParam *prm)
{
    CharParam cp;
    int n;
    int i;
    float x;
    float y;

    SND_TASK(w)->flags |= TASK_F_RUN_PAUSED;
    if (prm->value >= 10) {
        n = 2;
        x = -0.5f;
    } else {
        n = 1;
        x = 0.0f;
    }
    w->digit_cnt = n;
    x += prm->x;
    y = 0.0f;
    y += prm->y;
    sprintf(w->digits, "%d", prm->value);
    w->color = prm->color;
    w->scale = prm->scl;
    w->color_cur = prm->color & 0xffffff;
    w->color_to = prm->color;
    w->rot_x = 0;
    w->rot_y = 0;
    w->rot_z = 0;
    for (i = 0; i < n; i++) {
        cp.ch = w->digits[i];
        cp.time = 15;
        cp.x = x * w->scale * 0.8f;
        cp.y = y * w->scale;
        cp.persp = prm->persp;
        cp.z = 3.3333333f;
        cp.scl = w->scale;
        cp.col = w->color_cur;
        w->rec[i].x = cp.x;
        w->rec[i].y = cp.y;
        w->rec[i].obj = char_task_create(w, &cp);
        x += 1.0f;
    }
}

/* 0x0010e980 */
void num_task_release(void)
{
}

/* States 0-1: fade in, slow growth. 2: over 30 frames fades out, turns, grows faster, ends. */
/* 0x0010e988 */
void num_task_exec(NumWork *w)
{
    float rate;
    int col;

    switch (w->state) {
    case 0:
        w->state = 1;
    case 1:
        w->color_cur = color_blend(w->color_cur, w->color_to, 0.1f);
        w->scale = w->scale * 1.001f;
        break;
    case 2:
        w->timer = 30;
        w->state = 3;
    case 3:
        rate = 1.0f - (float)w->timer / 30.0f;
        col = w->color & 0xffffff;
        w->color_to = col;
        w->rot_y = (int)(rate * ANG_QUARTER);
        w->color_cur = color_blend(w->color, col, rate);
        w->scale = w->scale * 1.001f * 1.001f;
        if (w->timer-- <= 0) {
            do {
                if (w)
                    SND_TASK(w)->flags |= 1;
            } while (0);
        }
        break;
    }
    num_layout(w);
}

/* 0x0010ead0 */
void num_task_disp(void)
{
}

/* 0x0010ead8 */
void *num_task_create(void *parent, int prm)
{
    return task_create_arg(parent, &num_desc, sizeof(NumWork), prm);
}

/* Centred; rotation, colour and scale passed to each Char task. */
/* 0x0010eb00 */
void num_layout(NumWork *w)
{
    NumRec *rec;
    float t;
    int i;

    t = -0.5f;
    if (w->digit_cnt == 1)
        t = 0.0f;
    rec = w->rec;
    for (i = 0; i < w->digit_cnt; i++) {
        rec->x = t * w->scale * 0.8f;
        t += 1.0f;
        char_set_pos(rec->obj, rec->x, rec->y);
        char_set_rot(rec->obj, w->rot_x, w->rot_y, w->rot_z);
        char_set_col(rec->obj, w->color_cur);
        char_set_scl(rec->obj, w->scale);
        rec++;
    }
}

/* 2 starts the exit; never moves back. */
/* 0x0010ebe8 */
void num_set_state_min(NumWork *w, unsigned state)
{
    if (state >= w->state)
        w->state = state;
}

/* 120 frames per number; past 0 -> sub 1 (game over). Decide/cancel skips; mask 8 -> sub 0. */
/* 0x0010ec00 */
void continue_count_exec(ContWork *w)
{
    NumInit n;
    volatile PadState *pad;

    if (w->timer > 0) {
        if (w->anim >= 36)
            w->timer = w->timer - 1;
        else
            w->anim = w->anim + 1;
    } else {
        w->timer = 120;
        w->lives = w->lives - 1;
        if (w->lives < 0) {
            w->sub = 1;
            w->state = 2;
        } else {
            if (w->num != 0)
                num_set_state_min(w->num, 2);
            n.x = 0;
            n.y = 0;
            n.persp = 500.0f;
            n.value = w->lives;
            n.scl = 160.0f;
            n.col = console_get_pcol();
            w->num = num_task_create(w, &n);
        }
    }
    pad = pad_main;
    if (pad->buttons & (pad_decide_mask | pad_cancel_mask))
        w->timer = 0;
    if (pad->buttons & PAD_START) {
        w->sub = 0;
        w->state = 2;
    }
}

/* Dims the game, countdown from 20, pause menu off. */
/* 0x0010ed10 */
void continue_task_init(int *w)
{
    w[1] = 1;
    w[0] = 0;
    SND_TASK(w)->flags |= TASK_F_RUN_PAUSED;
    continue_work = w;
    fade_start_alpha(0.3f, 0, 60);
    w[3] = 1;
    w[5] = 0;
    w[2] = 21;
    w[0] = 2;
    w[6] = 1;
    system_pause_disable();
}

/* Pause menu back on unless game over; clears system_flags bit 0. */
/* 0x0010ed90 */
void continue_task_release(void **p)
{
    continue_work = 0;
    if (p[6])
        system_pause_enable();
    system_flags &= ~SYS_PAUSED;
}

/*
 * Sub 0 (continue): clears stage_exit_flags bit 2, fades back in, ends after the fade.
 * Sub 1 (game over): sets stage_exit_flags bit 0, ends at once.
 */
/* 0x0010edd8 */
void continue_task_exec_end(int *w)
{
    switch ((unsigned)w[0]) {
    case 0:
        switch (w[1]) {
        case 0:
            w[0] = 100;
            break;
        case 1:
            w[0] = 200;
            break;
        }
        break;
    case 100:
        stage_exit_flags &= ~4;
        fade_start_alpha(0.3f, 0, -24);
        w[0]++;
        break;
    case 101:
        if (fade_is_idle() == 1) {
            do { SND_TASK(w)->flags |= 1; } while (0);
            w[0]++;
        }
        break;
    case 200:
        stage_exit_flags |= 1;
        w[6] = 0;
        do { SND_TASK(w)->flags |= 1; } while (0);
        w[0] = 201;
        break;
    }
}

/* First frame sets system_flags bit 0. */
/* 0x0010ef08 */
void continue_task_exec(int *w)
{
    switch ((unsigned)w[0]) {
    case 0: {
        unsigned flags = system_flags;

        w[0] = 1;
        system_flags = flags | SYS_PAUSED;
        return;
    }
    case 1:
        continue_count_exec(w);
        return;
    case 2:
        w[0] = 0;
        TASK_OF(w)->exec = continue_task_exec_end;
        return;
    }
}

/* Top left, font 3, one character per four frames. */
/* 0x0010ef60 */
void continue_task_disp(int *w)
{
    const unsigned char *str;
    float x;
    float y;
    int n;
    void *buf;

    str = (const unsigned char *)"Continue?";
    n = w[5] >> 2;
    if (n > 0) {
        x = 40.0f;
        font_set(3);
        y = x;
        buf = font_begin_current(console_get_pcol());
        do {
            buf = font_put_char_scaled(buf, x, y, 3.3333333f, *str, 1.0f);
            x += 36.0f;
            str++;
            n--;
        } while (n != 0);
        font_end_current(buf);
    }
}

/* NULL if already up. */
/* 0x0010f040 */
void *continue_task_create(void)
{
    if (continue_work == 0)
        return task_create(scr_root, &continue_desc, 28);
    return 0;
}

/* p = (x, y, z), applied Y, X, Z. */
/* 0x0010f088 */
void continue_rotate_yxz(int *p)
{
    vu0_matrix_rotate_y(ulMatrixCur, p[1]);
    vu0_matrix_rotate_x(ulMatrixCur, p[0]);
    vu0_matrix_rotate_z(ulMatrixCur, p[2]);
}
