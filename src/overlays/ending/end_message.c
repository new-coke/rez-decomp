/* Ending: closing message after the staff roll, typed out line by line; ends 20 s later. */

#include "ending_shared.h"
#include "color.h"
#include "console_com.h"
#include "fog.h"
#include "libc.h"
#include "task.h"
#include "end_message.h"
#include "ending.h"

extern ScreenCfg screen_cfg;

/* 16 bytes */
typedef struct EndMessage {
    void *con;
    int line;                   /* 0x04 */
    int delay;                  /* 0x08 */
    char **lines;               /* 0x0c: ended by 0 */
} EndMessage;

extern struct TaskDesc end_message_desc;
extern char *end_message_lines_us[6][4];
extern char *end_message_lines_ge[6][4];
extern char *end_message_lines_fr[6][4];
extern char *end_message_lines_es[6][4];
extern char *end_message_lines_it[6][4];

/* 0x00af8030 */
void end_message_center_console(EndMessage *w)
{
    int sz[2];

    console_get_pixel_size(w->con, &sz[0], &sz[1]);
    console_set_pos(w->con, screen_cfg.cx - sz[0] * 0.5f, screen_cfg.cy - sz[1] * 0.5f + 0.0f);
}

/* steer(cast) */
/* 0x00af80b0 */
void end_message_init(EndMessage *w)
{
    task_set_dlist(w, 4);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    fog_req_color(COL_BLACK);
    w->con = console_create_fixed(52, 4);
    console_set_font_mode(w->con, 2);
    console_set_panel(w->con, 0);
    end_message_center_console(w);
    switch (game_lang) {
    case 2:
        w->lines = end_message_lines_ge[ending_type_get()];
        break;
    case 3:
        w->lines = end_message_lines_fr[ending_type_get()];
        break;
    case 4:
        w->lines = end_message_lines_es[ending_type_get()];
        break;
    case 5:
        w->lines = end_message_lines_it[ending_type_get()];
        break;
    case 0:
    default:
        w->lines = end_message_lines_us[ending_type_get()];
        break;
    }
    w->line = 0;
    w->delay = 0;
}

/* 0x00af81d0 */
void end_message_release(EndMessage *w)
{
    console_destroy_fixed(w->con);
}

/* 0x00af81d8 */
void end_message_exec_end(EndMessage *w)
{
    if (time_scale * 20.0f * FPS < w->delay++) {
        ending_set_end();
        TASK_OF(w)->exec = 0;
    }
}

/* 0x00af8248 */
void end_message_exec(EndMessage *w)
{
    char *str;
    int len;
    int cur[2];

    if (w->delay-- <= 0) {
        str = w->lines[w->line++];
        if (!str) {
            TASK_OF(w)->exec = (void (*)(void *))end_message_exec_end;
        } else {
            len = strlen(str);
            console_get_cursor(w->con, &cur[0], &cur[1]);
            cur[0] = (52 - len) / 2;
            console_set_cursor(w->con, cur[0], cur[1]);
            w->delay = console_puts(w->con, str, 2);
        }
    }
}

/* 0x00af8318 */
void end_message_disp(void)
{
}

/* 0x00af8320 */
void *end_message_create(void *parent)
{
    return task_create(parent, &end_message_desc, 16);
}
