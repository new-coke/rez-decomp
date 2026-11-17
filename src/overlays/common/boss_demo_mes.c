/*
 * Boss briefing text: a console window that types out the briefing lines (boss name and level in
 * its tag), then shrinks shut; and console lines printed in turn, held a few beats, taken down.
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "console.h"
#include "console_com.h"
#include "libc.h"
#include "sound_beat.h"
#include "task.h"
#include "boss_demo_mes.h"
#include "boss_common.h"

extern ScreenCfg screen_cfg;

extern char *boss_demo_mes_tail[];
extern char *boss_demo_mes2_lines0[];
extern char *boss_demo_mes2_lines1[];
extern char *boss_demo_mes2_lines2[];
extern char *boss_demo_mes2_lines3[];
extern char *str_boss_name_ptr;
extern char *boss_level_tags[];
extern char *boss_demo_mes_lines[];
extern char str_newline[];
extern char str_s_s[];
extern struct TaskDesc boss_demo_mes2_desc;
extern struct TaskDesc boss_demo_mes_desc;
extern char boss_name[];
extern char boss_name_line[];
extern int boss_demo_mes_color;

/* 0x00cc20d8 b_earth, 0x00cc0c80 b_mars, 0x00cc53b0 b_uranus, 0x00cc80e8 b_venus */
char *boss_name_subst(char *src, int level)
{
    char *tag;
    char *rest;
    char *dst;
    int len;

    tag = strstr(src, str_boss_name_ptr);
    if (!tag)
        return src;
    dst = boss_name_line;
    len = tag - src;
    rest = tag + strlen(str_boss_name_ptr);
    memcpy(dst, src, len);
    dst += len;
    sprintf(dst, str_s_s, boss_name, boss_level_tags[level]);
    strcat(dst, rest);
    return boss_name_line;
}

/* 0x00cc21a8 b_earth, 0x00cc0d50 b_mars, 0x00cc5480 b_uranus, 0x00cc81b8 b_venus */
void boss_demo_mes2_exec(BossDemoMes2Work *w)
{
    switch (w->state) {
    case 0:
        if (--w->delay > 0)
            break;
        if (w->idx >= w->n) {
            if (w->quit) {
                FINISH(w);
                break;
            }
            do {
                w->state = 1;
                w->delay = 8;
            } while (0);
            break;
        }
        {
            char *mes = boss_name_subst(w->lines[w->idx], w->level);

            console_set_mode(0, 1);
            w->delay = console_puts(0, mes, 2);
            w->idx++;
        }
        break;
    case 1:
        if (beat_grid_hit(0)) {
            if (--w->delay <= 0)
                w->state++;
        }
        break;
    case 2:
        if (--w->delay > 0)
            break;
        console_set_mode(0, 1);
        w->delay = console_puts(0, str_newline, 2);
        if (--w->idx <= 0)
            FINISH(w);
        break;
    }
}

/* 0x00cc2318 b_earth, 0x00cc0ec0 b_mars, 0x00cc55f0 b_uranus, 0x00cc8328 b_venus */
BossDemoMes2Work *boss_demo_mes2_create(void *parent, char **lines, int n, int delay)
{
    BossDemoMes2Work *w = task_create(parent, &boss_demo_mes2_desc, 28);

    w->level = boss_get_level();
    w->delay = delay;
    w->lines = lines;
    w->n = n;
    return w;
}

/* 0x00cc2388 b_earth, 0x00cc0f30 b_mars, 0x00cc5660 b_uranus, 0x00cc8398 b_venus */
void boss_demo_mes2_set_end(BossDemoMes2Work *w, int quit)
{
    w->quit = quit;
}

/* 0x00cc2390 b_earth, 0x00cc0f38 b_mars, 0x00cc5668 b_uranus, 0x00cc83a0 b_venus */
void boss_demo_mes_center(BossDemoMesWork *w)
{
    int sz[2];

    console_get_pixel_size(w->con, &sz[0], &sz[1]);
    console_set_pos(w->con, screen_cfg.cx - sz[0] * 0.5f, screen_cfg.cy - sz[1] * 0.5f - 60.0f);
}

/* 0x00cc2410 b_earth, 0x00cc0fb8 b_mars, 0x00cc56e8 b_uranus, 0x00cc8420 b_venus */
void boss_demo_mes_init(BossDemoMesWork *w)
{
    int i;

    w->con = console_create_fixed(28, 6);
    console_set_font_mode(w->con, 2);
    console_set_panel_colors(w->con, console_get_pcol(), boss_demo_mes_color);
    console_set_window(w->con, w->width, w->h);
    w->level = boss_get_level();
    for (i = 4; i >= 0; i--) {
        w->delay = console_puts(w->con, boss_name_subst(boss_demo_mes_lines[w->line], w->level), 0);
        w->line++;
    }
}

/* 0x00cc24e0 b_earth, 0x00cc1088 b_mars, 0x00cc57b8 b_uranus, 0x00cc84f0 b_venus */
void boss_demo_mes_release(BossDemoMesWork *w)
{
    console_destroy_fixed(w->con);
}

/* 0x00cc24e8 b_earth, 0x00cc1090 b_mars, 0x00cc57c0 b_uranus, 0x00cc84f8 b_venus */
void boss_demo_mes_exec_close(BossDemoMesWork *w)
{
    if (w->delay > 0) {
        w->delay--;
        return;
    }
    if (w->h > 0)
        w->h--;
    else if (w->width > 0)
        w->width--;
    else
        FINISH_IF(w);
    console_set_window(w->con, w->width, w->h);
    boss_demo_mes_center(w);
}

/* 0x00cc2578 b_earth, 0x00cc1120 b_mars, 0x00cc5850 b_uranus, 0x00cc8588 b_venus */
void boss_demo_mes_exec_type(BossDemoMesWork *w)
{
    boss_demo_mes_center(w);
    if (w->delay-- > 0)
        return;
    w->delay = console_puts(w->con, boss_name_subst(boss_demo_mes_lines[w->line], w->level), 2) * 0.5f;
    if ((unsigned int)++w->line < 29)
        return;
    TASK_OF(w)->exec = (void (*)(void *))boss_demo_mes_exec_close;
    w->delay = 60;
    boss_demo_mes2_set_end(boss_demo_mes2_create(w->parent, boss_demo_mes_tail, 10, 60), 1);
}

/* 0x00cc2658 b_earth, 0x00cc1200 b_mars, 0x00cc5930 b_uranus, 0x00cc8668 b_venus */
void boss_demo_mes_exec(BossDemoMesWork *w)
{
    if (w->width < 28) {
        w->width++;
    } else if (w->h < 6) {
        w->h++;
    } else {
        w->delay = 12;
        TASK_OF(w)->exec = (void (*)(void *))boss_demo_mes_exec_type;
    }
    console_set_window(w->con, w->width, w->h);
    boss_demo_mes_center(w);
}

/* 0x00cc26d0 b_earth, 0x00cc1278 b_mars, 0x00cc59a8 b_uranus, 0x00cc86e0 b_venus */
void boss_demo_mes_disp(void)
{
}

/* 0x00cc26d8 b_earth, 0x00cc1280 b_mars, 0x00cc59b0 b_uranus, 0x00cc86e8 b_venus */
BossDemoMesWork *boss_demo_mes_create(void *parent, char *name)
{
    BossDemoMesWork *w;

    sprintf(boss_name, name);
    w = task_create(parent, &boss_demo_mes_desc, 28);
    w->parent = parent;
    return w;
}

/* 0x00cc2720 b_earth, 0x00cc12c8 b_mars, 0x00cc59f8 b_uranus, 0x00cc8730 b_venus */
BossDemoMes2Work *boss_demo_mes2_start(void *parent, char *name, int kind)
{
    sprintf(boss_name, name);
    switch ((unsigned int)kind) {
    case 0:
        return boss_demo_mes2_create(parent, boss_demo_mes2_lines0, 3, 0);
    case 1:
        return boss_demo_mes2_create(parent, boss_demo_mes2_lines1, 7, 0);
    case 2:
        return boss_demo_mes2_create(parent, boss_demo_mes2_lines2, 3, 0);
    case 3:
        return boss_demo_mes2_create(parent, boss_demo_mes2_lines3, 5, 0);
    }
    return 0;
}
