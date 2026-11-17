/*
 * Debug and utility: view-cone visibility, screen projection, string/number parsing, debug HUD,
 * debug text and message windows, the "Debug" flag menu, debug commands, reset messages.
 */

#include "types_game.h"
#include "color.h"
#include <stdarg.h>
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "debugdraw.h"
#include "debugmenu.h"
#include "heap.h"
#include "math_util.h"
#include "render.h"
#include "main.h"
#include "ul_math.h"
#include "gw_system.h"
#include "ul_console.h"
#include "gw_console.h"
#include "system.h"

#define VEC_GET(p, vx, vy, vz) \
    do { \
        vx = (p)[0]; \
        vy = (p)[1]; \
        vz = (p)[2]; \
    } while (0)

extern float view_eye[3];
extern float view_dir[3];
extern float ulVec3Dot(float *p, float *v);
extern ScreenCfg screen_cfg;
extern void ulMatrixApplyPoint(int dev, float *in, float *out);
extern void njGetMatrix(void *dst);
extern void njSetMatrix(void *dst, void *src);
extern void gw_win_printf(int win, int n, int y, const char *fmt, ...);
extern char debug_bits_buf[];
extern void soft_reset(const char *name);
extern int debug_margin_left;
extern int debug_margin_right;
extern int debug_margin_top;
extern int debug_margin_bottom;
extern int debug_x;
extern int debug_y;
extern int *gw_window_get_slot(int a, int b);
extern void gw_window_close(int a);
extern void gw_window_get_pos(int a, int *x, int *y);
extern void gw_window_set_draw_color(int a, int color);
extern DebugFlag debug_flag_table[5];
extern int gw_item_get_value(int item);
extern int gw_item_set_value(int item, int v);
extern int debug_flags;
extern void gw_command_add(int *);
extern void gw_command_remove(int *);

extern float view_local_eye[3];
extern float view_local_dir[3];
extern ScreenRect screen_layout;
extern SysState sys_state;
extern unsigned int debug_disp_level;
extern volatile int fps_measured;
extern void debug_set_color(int color);
extern int gw_window_open(const char *name, int a, int x, int y, int w, int h, int parent);
extern void gw_window_set_handlers(int win, void (*a)(), void (*b)(), void (*c)());
extern void gw_window_set_color(int win, int v);
extern void gw_window_set_slot(int win, int a, int *data);

/* Cone half-angle from the field of view n. */
/* 0x0010f408 */
void set_view_params(float *eye, float *dir, int n)
{
    view_eye[0] = eye[0];
    view_eye[1] = eye[1];
    view_cone_ang = (int)((float)(n >> 1) * 1.25f);
    view_eye[2] = eye[2];
    view_dir[0] = dir[0];
    view_dir[1] = dir[1];
    view_dir[2] = dir[2];
}

/*
 * Apex pulled back along dir by radius / sin(angle), so the test is an angle check of the centre.
 */
/* 0x0010f470 */
int ray_hits(float *eye, float *dir, int angle, float *center, float radius)
{
    float to_center[3];
    float sc[2];
    float inv;

    ulSinCos(angle, &sc[0], &sc[1]);
    if (fabsf(sc[0]) < FLT_EPSILON)
        return 0;
    inv = radius / sc[0];
    vec3_normalize_into(to_center,
                 center[0] - (eye[0] - dir[0] * inv),
                 center[1] - (eye[1] - dir[1] * inv),
                 center[2] - (eye[2] - dir[2] * inv));
    if (sc[1] < ulVec3Dot(dir, to_center))
        return 1;
    return 0;
}

/* Cone from set_view_params. */
/* 0x0010f568 */
int view_sphere_visible(Vec3 *pos, float r)
{
    return ray_hits(view_eye, view_dir, view_cone_ang, &pos->x, r);
}

/* Against view_local_eye / view_local_dir. */
/* 0x0010f5a0 */
int view_sphere_visible_local(Vec3 *pos, float r)
{
    return ray_hits(view_local_eye, view_local_dir, view_cone_ang, &pos->x, r);
}

/* out = screen x, y, depth -z. Returns 1/-z. */
/* 0x0010f5d8 */
float project_point(Vec3 *p, Vec3 *out)
{
    float inv = -1.0f / p->z;
    float x;
    float y;

    x = p->x * inv * screen_cfg.sx + screen_cfg.cx;
    y = p->y * inv * screen_cfg.sy + screen_cfg.cy;
    out->z = -p->z;
    out->x = x;
    out->y = y;
    return inv;
}

/* Returns 1/-z. */
/* 0x0010f640 */
float project_xyz(float x, float y, float z, float *out)
{
    float inv = -1.0f / z;
    float rx;
    float ry;

    rx = x * inv * screen_cfg.sx + screen_cfg.cx;
    ry = y * inv * screen_cfg.sy + screen_cfg.cy;
    out[2] = -z;
    out[0] = rx;
    out[1] = ry;
    return inv;
}

/* Extent: project_point without the screen centre, x and y made positive. Returns 1/-z. */
/* 0x0010f6a0 */
float project_point_abs(float *p, float *out)
{
    float inv = -1.0f / p[2];
    float x;
    float y;

    x = fabsf(p[0] * inv * screen_cfg.sx);
    y = fabsf(p[1] * inv * screen_cfg.sy);
    out[0] = x;
    out[1] = y;
    out[2] = -p[2];
    return inv;
}

/* Returns 1/-z. */
/* 0x0010f708 */
float project_xyz_abs(float x, float y, float z, float *out)
{
    float inv = -1.0f / z;
    float rx;
    float ry;

    rx = fabsf(x * inv * screen_cfg.sx);
    ry = fabsf(y * inv * screen_cfg.sy);
    out[2] = -z;
    out[0] = rx;
    out[1] = ry;
    return inv;
}

/* Also scales *sx, *sy (either may be NULL) to screen size. Returns 1/-z. */
/* 0x0010f760 */
float project_point_size(float *p, float *out, float *sx, float *sy)
{
    float inv = -1.0f / p[2];

    out[0] = p[0] * inv * screen_cfg.sx + screen_cfg.cx;
    out[1] = p[1] * inv * screen_cfg.sy + screen_cfg.cy;
    out[2] = -p[2];
    if (sx != 0)
        *sx = fabsf(*sx * inv * screen_cfg.sx);
    if (sy != 0)
        *sy = fabsf(*sy * inv * screen_cfg.sy);
    return inv;
}

/* Returns 1/-z. */
/* 0x0010f808 */
float project_xyz_size(float x, float y, float z, float *out, float *sx, float *sy)
{
    float inv = -1.0f / z;

    out[0] = x * inv * screen_cfg.sx + screen_cfg.cx;
    out[1] = y * inv * screen_cfg.sy + screen_cfg.cy;
    out[2] = -z;
    if (sx != 0)
        *sx = fabsf(*sx * inv * screen_cfg.sx);
    if (sy != 0)
        *sy = fabsf(*sy * inv * screen_cfg.sy);
    return inv;
}

/* 0 when dist is about 0. */
/* 0x0010f8a0 */
float ratio_guarded(float dist, float size)
{
    if (fabsf(dist) < FLT_EPSILON)
        return 0.0f;
    return size * (*(float *)&screen_cfg) / dist; /* steer(cast) */
}

/* World size covering size pixels at pos (current matrix). */
/* 0x0010f8e8 */
float view_unproject_size(float *pos, float size)
{
    float view_pos[3];
    float inv_sx = -1.0f;

    ulMatrixApplyPoint(ulMatrixCur, pos, view_pos);
    inv_sx /= screen_cfg.sx;
    return size * view_pos[2] * inv_sx;
}

/* Pixels; pos in the current matrix's space. */
/* 0x0010f948 */
float view_project_size(float size, float *pos)
{
    float view_pos[3];

    ulMatrixApplyPoint(ulMatrixCur, pos, view_pos);
    return size * (-1.0f / view_pos[2]) * screen_cfg.sx;
}

/* 1 for power about 0, 0 for base about 0. */
/* 0x0010f9a8 */
float safe_powf(float base, float power)
{
    if (fabsf(power) < FLT_EPSILON)
        return 1.0f;
    if (fabsf(base) < FLT_EPSILON)
        return 0.0f;
    return powf(base, power);
}

/* mtx NULL: copies the current matrix. */
/* 0x0010fa00 */
void matrix_get_or_set(void *mtx, void *dst)
{
    if (mtx == 0)
        njGetMatrix(dst);
    else
        njSetMatrix(dst, mtx);
}

/* 4x4, unit axis. Returns mtx. */
/* 0x0010fa20 */
float *matrix_from_axis_angle(float *mtx, float *axis, int angle)
{
    float sc[2];
    float x, y, z;
    float xx, yy, zz, xy, yz, zx;
    float s, c, t;
    float xs, ys, zs;

    VEC_GET(axis, x, y, z);
    xx = x * x;
    yy = y * y;
    ulSinCos(angle, &sc[0], &sc[1]);
    c = sc[1];
    s = sc[0];
    zz = z * z;
    xy = x * y;
    yz = y * z;
    zx = z * x;
    t = 1.0f - c;
    xs = x * s;
    ys = y * s;
    zs = z * s;
    mtx[0] = t * xx + c;
    mtx[1] = t * xy + zs;
    mtx[2] = t * zx - ys;
    mtx[4] = t * xy - zs;
    mtx[5] = t * yy + c;
    mtx[6] = t * yz + xs;
    mtx[8] = t * zx + ys;
    mtx[9] = t * yz - xs;
    mtx[10] = t * zz + c;
    mtx[3] = 0;
    mtx[7] = 0;
    mtx[11] = 0;
    mtx[12] = 0;
    mtx[13] = 0;
    mtx[14] = 0;
    mtx[15] = 1.0f;
    return mtx;
}

/* 0x0010fb40 */
void matrix_window_draw(int win)
{
    int cell[2];
    float *mtx;
    int y;

    gw_get_cell_size(&cell[0], &cell[1]);
    mtx = *gw_window_get_slot(win, 0);
    if (mtx != 0) {
        gw_win_printf(win, 4, 0, "% 8.3f,% 8.3f,% 8.3f,% 8.3f",
                     (double)mtx[0], (double)mtx[1], (double)mtx[2], (double)mtx[3]);
        y = cell[1];
        gw_win_printf(win, 4, y, "% 8.3f,% 8.3f,% 8.3f,% 8.3f",
                     (double)mtx[4], (double)mtx[5], (double)mtx[6], (double)mtx[7]);
        y += cell[1];
        gw_win_printf(win, 4, y, "% 8.3f,% 8.3f,% 8.3f,% 8.3f",
                     (double)mtx[8], (double)mtx[9], (double)mtx[10], (double)mtx[11]);
        y += cell[1];
        gw_win_printf(win, 4, y, "% 8.3f,% 8.3f,% 8.3f,% 8.3f",
                     (double)mtx[12], (double)mtx[13], (double)mtx[14], (double)mtx[15]);
    }
}

/* 40 x 4 cells, live. Returns the window or 0. */
/* 0x0010fd10 */
int matrix_window_create(int title, int x, int y, int mtx)
{
    int cell[2];
    int obj;

    gw_get_cell_size(&cell[0], &cell[1]);
    obj = gw_window_open(title, 95, x, y, cell[0] * 40, cell[1] * 4, 0);
    if (obj != 0) {
        gw_window_set_handlers(obj, 0, matrix_window_draw, 0);
        gw_window_set_slot(obj, 0, mtx);
    }
    return obj;
}

/* Up to the first newline or NUL. */
/* 0x0010fe78 */
int line_length(const char *str)
{
    int n = 0;

    while (*str != 0 && *str != 10) {
        n++;
        str++;
    }
    return n;
}

/* Returns the byte after the terminator. */
/* 0x0010feb8 */
char *copy_line(char *dst, const char *src, int n)
{
    while (*src != 0 && *src != 10 && n-- > 0) {
        *dst = *src;
        src++;
        dst++;
    }
    *dst = 0;
    return dst + 1;
}

/* Word ends at NUL or space. 0 equal, 1 str first, -1 str after. */
/* 0x0010ff18 */
int token_compare(const char *str, const char *line)
{
    while (*str != 0 && *line != 0 && *line != ' ') {
        if (*str != *line) {
            if (*str > *line)
                return -1;
            return 1;
        }
        str++;
        line++;
    }
    if (*str == 0) {
        if (*line == 0 || *line == ' ')
            return 0;
        return 1;
    }
    if (*line == 0 || *line == ' ')
        return -1;
    return 0;
}

/* Skips spaces and 0x/0X; the digit loop is missing, so it always returns 0. */
/* 0x00110000 */
int parse_hex_prefix(const char *str)
{
    int val = 0;

    while (*str == ' ')
        str++;
    if (*str == '0' && (str[1] == 'x' || str[1] == 'X'))
        str += 2;
    if (*str == 0) {
        val <<= 4;
        val >>= 4;
    }
    return val;
}

/* Spaces, optional '-', optional fraction. 0x number = raw float bits. */
/* 0x00110078 */
float str_to_float(const char *str)
{
    char ch;
    int seen_dot;
    int digit;
    float sign;
    float v;
    float scale;
    union {
        int i;
        float f;
    } u;

    while (*str == ' ')
        str++;
    if (*str == '0') {
        if (str[1] == 'x' || str[1] == 'X') {
            u.i = parse_hex_prefix(str);
            return u.f;
        }
    }
    scale = 1.0f;
    seen_dot = 0;
    v = 0.0f;
    sign = scale;
    if (*str == '-') {
        str++;
        sign = -1.0f;
    }
    while ((ch = *str++) != 0) {
        digit = ch - 48;
        if ((unsigned)digit < 10) {
            if (seen_dot) {
                scale = scale * 0.1f;
                v = v + (float)digit * scale;
            } else {
                v *= 10.0f;
                v += (float)digit;
            }
        } else if (ch == '.') {
            seen_dot = 1;
        } else {
            break;
        }
    }
    return v * sign;
}

/* Spaces, optional '-'; non-digits skipped. 0x -> parse_hex_prefix. */
/* 0x001101b0 */
int str_to_int(const char *str)
{
    char ch;
    int val;
    float sign;

    while (*str == ' ')
        str++;
    if (*str == '0') {
        if (str[1] == 'x' || str[1] == 'X')
            return parse_hex_prefix(str);
    }
    val = 0;
    sign = 1.0f;
    if (*str == '-') {
        str++;
        sign = -1.0f;
    }
    while ((ch = *str++) != 0) {
        if ((unsigned)(ch - 48) < 10) {
            val = val * 10;
            val += (ch - 48);
        }
    }
    return (int)((float)val * sign);
}

/* Highest first, into debug_bits_buf. Bit i uses on[i]/off[i]; NULL gives '1'/'0'. */
/* 0x001102a8 */
char *bits_to_string(unsigned bits, int n, const unsigned char *on, const unsigned char *off)
{
    char *p;
    int ch;

    if (on == 0)
        on = (const unsigned char *)"11111111111111111111111111111111";
    if (off == 0)
        off = (const unsigned char *)"00000000000000000000000000000000";
    bits <<= -n;
    p = debug_bits_buf;
    n--;
    while (n != -1) {
        if ((int)bits < 0)
            ch = *on;
        else
            ch = *off;
        *p = ch;
        do { p++; } while (0);
        on++;
        off++;
        n--;
        bits <<= 1;
    }
    *p = 0;
    return debug_bits_buf;
}

/* 19-character field, right-aligned, space padded, commas every three digits. */
/* 0x00110320 */
void format_number_commas(char *dst, int val)
{
    char buf[32];
    char *p;
    char *src;
    int n;
    char ch;

    sprintf(buf, "%15d", val);
    dst[19] = 0;
    n = 14;
    p = dst + 18;
    src = buf + 14;
    for (; n != 0; n--, src--) {
        ch = *src;
        if ((unsigned)(ch - 48) < 10) {
            if (n == 14) {
                *p = ch;
                p--;
            } else if (n % 3 == 2) {
                *p = ',';
                p--;
                *p = ch;
                p--;
            } else {
                *p = ch;
                p--;
            }
        } else {
            *p = ' ';
        }
    }
    while (p >= dst) {
        *p = ' ';
        p--;
    }
}

/* HUD hidden (level 2), debug flags and text origin cleared. */
/* 0x00110400 */
void debug_init(void)
{
    hard_reset("Debug");
    debug_disp_level = 2;
    debug_flags = 0;
    debug_x = 0;
    debug_y = 0;
    ulConsoleSetScale(0.2f);
}

/* Text margins columns 1..39, rows 1..29; Debug and heap menus. */
/* 0x00110450 */
void debug_soft_init(void)
{
    soft_reset("Debug");
    debug_margin_left = 1;
    debug_margin_right = 39;
    debug_margin_top = 1;
    debug_margin_bottom = 29;
    info_default();
    debug_menu_init();
    heap_menu_register();
}

/*
 * While debug_disp_level < 2: two frame load gauges (%), highlighted at >= 100. Level 0 adds packet
 * and render load, heap bars, perf count, frame rates, random call counts, stabilizer rate.
 */
/* 0x001104a8 */
void debug_hud(void)
{
    float x, y, time_rate, draw_rate, z, one, dy, gw, gh;
    int col;
    SysState *st;

    if (debug_disp_level < 2) {
        st = &sys_state;
        debug_set_origin(0, 0);
        one = 1.0f;
        time_rate = st->dt3;
        z = 5.0f;
        x = screen_layout.left;
        y = screen_layout.top;
        draw_rate = render_get_frame_scale();
        gw = 50.0f;
        gh = 8.0f;
        time_rate = time_rate * 0.01f;
        draw_rate = draw_rate * 0.01f;
        col = RGBA(0xff, 0xff, 0xff, 0x80);
        if (one <= time_rate)
            col = RGBA(0xff, 0, 0, 0x80);
        debug_draw_gauge(z, x, y, gw, gh, time_rate, RGBA(0, 0xff, 0, 0x80), col);
        dy = 9.0f;
        y = y + dy;
        col = RGBA(0xff, 0xff, 0xff, 0x80);
        if (one <= draw_rate)
            col = RGBA(0, 0, 0xff, 0x80);
        debug_draw_gauge(z, x, y, gw, gh, draw_rate, RGBA(0xff, 0, 0, 0x80), col);
        y = y + dy;
        if (debug_disp_level == 0) {
            debug_draw_gauge(z, x, y, gw, gh, render_get_packet_load(), RGBA(0xff, 0x80, 0, 0x80), RGBA(0xff, 0xff, 0xff, 0x80));
            y = render_debug_gauges(x, y + dy, z, gw, gh);
            if (debug_disp_level == 0) {
                heap_draw_bars(x, y + z, z);
                debug_set_origin(8, 0);
                debug_printf("T%7d", perf_record_cnt);
                debug_newline();
                if (fps_measured >= frame_rate)
                    debug_set_color(RGBA(0x80, 0, 0, 0x80));
                else
                    debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
                debug_printf("F(%2d) %2d", fps_measured, frame_rate);
                debug_newline();
                debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
                debug_printf("R1%6d", rand_lcg_count_a);
                debug_newline();
                debug_printf("R2%6d", rand_lcg_count_b);
                debug_newline();
                if (stabilizer_rate != frame_rate) {
                    debug_set_color(RGBA(0x80, 0, 0, 0x80));
                    debug_printf("AUTO STABILIZER %1d", stabilizer_rate);
                    debug_newline();
                }
                debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
            }
        }
    }
}

/* Cells from the left/top margin; negative from right/bottom. */
/* 0x001107a8 */
void debug_set_origin(int x, int y)
{
    if (x >= 0)
        debug_x = x + debug_margin_left;
    else
        debug_x = x + debug_margin_right;
    if (y >= 0)
        debug_y = y + debug_margin_top;
    else
        debug_y = y + debug_margin_bottom;
}

/* 0x00110800 */
void debug_get_origin(int *px, int *py)
{
    *px = debug_x;
    *py = debug_y;
}

/* 0x00110820 */
void debug_move_x(int n)
{
    debug_x += n;
}

/* 0x00110838 */
void debug_move_y(int n)
{
    debug_y += n;
}

/* 0x00110850 */
void debug_newline(void)
{
    debug_y++;
}

/* Up to 1023 characters. */
/* 0x00110868 */
void debug_printf(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);

    vsprintf(buf, fmt, ap);
    ulConsoleSetCursor(debug_x, debug_y);
    ulConsolePuts(buf);
}

/* 0x001108c8 */
void debug_puts(char *str)
{
    ulConsoleSetCursor(debug_x, debug_y);
    ulConsolePuts(str);
}

/* 0x00110908 */
void debug_set_color(int color)
{
}

/* Closes when time runs out; 0 keeps it open. */
/* 0x00110910 */
void debug_message_window_tick(int win)
{
    int *timer = gw_window_get_slot(win, 0);

    if (*timer > 0 && --*timer == 0)
        gw_window_close(win);
}

/* Sets the colour only; no text drawing. */
/* 0x00110968 */
void debug_message_window_draw(int win)
{
    int pos[2];

    gw_window_get_slot(win, 0);
    gw_window_get_pos(win, &pos[0], &pos[1]);
    gw_window_set_draw_color(win, ARGB(0x80, 0x80, 0x80, 0x80));
}

/* Frees the text and data block. */
/* 0x001109b0 */
void debug_message_window_closed(int win)
{
    int *p = gw_window_get_slot(win, 0);

    heap_free((void *)p[1], 0);
    heap_free(p, 0);
}

/* 202 x 24; timer frames (0 = until closed). ax, ay: 0 left/top, 1 centre, 2 right/bottom. */
/* 0x001109f0 */
void debug_message_window(int ax, int ay, int timer, int col, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    char *text;
    int x;
    int y;
    int win;
    int width;
    int height;
    int *data;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    text = heap_block_tag(game_malloc(strlen(buf) + 1), "debug.c", 537);
    strcpy(text, buf);
    width = 202;
    height = 24;
    switch ((unsigned)ax) {
    case 0:
        x = (int)(screen_layout.left + 1.0f);
        break;
    case 1:
        x = (int)(screen_cfg.cx - 101.0f);
        break;
    case 2:
        x = (int)(screen_layout.right - (float)width - 1.0f);
        break;
    default:
        x = 0;
        break;
    }
    switch ((unsigned)ay) {
    case 0:
        y = (int)(screen_layout.top + 1.0f);
        break;
    case 1:
        y = (int)(screen_cfg.cy - (float)(height >> 1));
        break;
    case 2:
        y = (int)(screen_layout.bottom - (float)height - 1.0f);
        break;
    default:
        y = 0;
        break;
    }
    win = gw_window_open("Message", 76, x, y, width, height, 0);
    if (win != 0) {
        gw_window_set_handlers(win, debug_message_window_tick, debug_message_window_draw, debug_message_window_closed);
        gw_window_set_color(win, col);
        data = heap_block_tag(game_malloc(8), "debug.c", 597);
        data[0] = timer;
        data[1] = (int)text;
        gw_window_set_slot(win, 0, data);
    }
}

/* Prints and hangs. */
/* 0x00110c08 */
void debug_error(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);

    vsprintf(buf, fmt, ap);
    printf(buf);
    for (;;)
        ;
}

/* Flag of entry idx and its tick. */
/* 0x00110c68 */
void debug_flag_toggle(int item, int val, int idx)
{
    if (gw_item_get_value(item)) {
        gw_item_set_value(item, 0);
        debug_flags_clear(debug_flag_table[idx].mask);
    } else {
        gw_item_set_value(item, 1);
        debug_flags_set(debug_flag_table[idx].mask);
    }
}

/* Toggles MESO, TMIT, KODE, YUKI, NODA, ticked from debug_flags. */
/* 0x00110cf0 */
void debug_menu_init(void)
{
    unsigned i;

    menu_item_add(0, "Debug", 0, 0, 1);
    for (i = 0; i < 5; i++) {
        menu_item_add("Debug", debug_flag_table[i].name, debug_flag_toggle, i, 0);
        menu_item_set_value("Debug", debug_flag_table[i].name, debug_flags & debug_flag_table[i].mask);
    }
}

/* 0x00110da8 */
void debug_menu_refresh(void)
{
    unsigned i;

    for (i = 0; i < 5; i++)
        menu_item_set_value("Debug", debug_flag_table[i].name, debug_flags & debug_flag_table[i].mask);
}

/* Also ticks the menu items. */
/* 0x00110e20 */
void debug_flags_set(unsigned mask)
{
    unsigned i;

    for (i = 0; i < 5; i++) {
        if (debug_flag_table[i].mask & mask)
            menu_item_set_value("Debug", debug_flag_table[i].name, 1);
    }
    debug_flags |= mask;
}

/* Also unticks the menu items. */
/* 0x00110eb0 */
void debug_flags_clear(unsigned mask)
{
    unsigned i;

    for (i = 0; i < 5; i++) {
        if (debug_flag_table[i].mask & mask)
            menu_item_set_value("Debug", debug_flag_table[i].name, 0);
    }
    debug_flags &= ~mask;
}

/* 0x00110f50 */
int debug_flags_test(int mask)
{
    return debug_flags & mask;
}

/* 0x00110f60 */
void debug_command_add(int name, int help, int run)
{
    int cmd[3];

    cmd[0] = name;
    cmd[1] = help;
    cmd[2] = run;
    gw_command_add(cmd);
}

/* 0x00110f88 */
void debug_command_remove(int name)
{
    int cmd[3];

    cmd[0] = name;
    cmd[1] = 0;
    cmd[2] = 0;
    gw_command_remove(cmd);
}

/* 0 if any failed. */
/* 0x00110fb0 */
int debug_command_add_table(DebugCommand *cmd, int n)
{
    int ok = 1;

    if (n > 0) {
        do {
            if (((int (*)(int, int, int))debug_command_add)(cmd->name, cmd->help, cmd->run) == 0) /* steer(cast) */
                ok = 0;
            cmd++;
            n--;
        } while (n != 0);
    }
    return ok;
}

/* 0x00111018 */
void debug_command_remove_table(DebugCommand *cmd, int n)
{
    if (n > 0) {
        do {
            debug_command_remove(cmd->name);
            cmd++;
            n--;
        } while (n != 0);
    }
}

/* Empty, like the two below. */
/* 0x00111060 */
void debug_trace(const char *str)
{
}

/* 0x00111068 */
void debug_tracef(const char *fmt, ...)
{
}

/* 0x00111090 */
void debug_error_tracef(const char *fmt, ...)
{
}

/* "HardReset <name>", reset message colour. */
/* 0x001110b8 */
void hard_reset(const char *name)
{
    int old;

    old = debug_color_set(RGBA(0x50, 0x50, 0, 0x80));
    DEBUG_PRINTF("HardReset %s\n", name);
    debug_color_set(old);
}

/* "SoftReset <name>", reset message colour. */
/* 0x00111100 */
void soft_reset(const char *name)
{
    int old;

    old = debug_color_set(RGBA(0x50, 0x50, 0, 0x80));
    DEBUG_PRINTF("SoftReset %s\n", name);
    debug_color_set(old);
}

/* Returns the previous attribute. */
/* 0x00111148 */
int debug_color_set(int attr)
{
    return gw_console_set_attr(attr);
}
