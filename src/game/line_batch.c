/*
 * Batched lines, up to 100 points per batch. Vector segments go to the VU1 batch, float and 2D
 * ones to the VU0 batch; switching flushes the pending batch.
 */

#include "types_game.h"
#include "line_batch.h"
#include "rez_wire_compile.h"

extern int line_translucent;
extern int line_count;
extern int line_color;
extern LineMode line_mode;
extern LinePoint line_buffer[];

/* 0x0017cf08 */
void line_flush_vu1(void)
{
    line_begin_vu1(line_translucent);
    line_draw_vu1(line_buffer, line_count);
    line_end_vu1();
    line_count = 0;
    line_mode = 0;
}

/* 0x0017cf58 */
void line_flush_vu0(void)
{
    line_begin_vu0(line_translucent);
    line_draw_vu0(line_buffer, line_count);
    line_end_vu0();
    line_count = 0;
    line_mode = 0;
}

/* Top bit clear turns translucency on, set turns it off. Returns 0. */
/* 0x0017cfa8 */
int line_set_color(int color)
{
    line_color = color;
    if (color >= 0)
        line_translucent = 1;
    else
        line_translucent = 0;
    return 0;
}

/* ret (the line_add chain value) unused. */
/* 0x0017cfd8 */
void line_flush(void *ret)
{
    switch (line_mode) {
    case 1:
        line_flush_vu0();
        break;
    case 2:
        line_flush_vu1();
        break;
    }
}

/* Flushes a pending VU0 batch or a full buffer first. Returns ret for chaining. */
/* 0x0017d010 */
void *line_add_v(void *ret, float *start, float *end)
{
    LinePoint *pt;

    if (line_mode == 2) {
    } else if (line_mode == 1) {
        line_flush_vu0();
    }
    if (line_count >= 100)
        line_flush_vu1();
    pt = &line_buffer[line_count];
    pt->x = start[0];
    pt->y = start[1];
    pt->z = start[2];
    pt->w = line_color;
    line_mode = 2;
    pt++;
    pt->x = end[0];
    pt->y = end[1];
    pt->z = end[2];
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Flushes a pending VU1 batch or a full buffer first. Returns ret. */
/* 0x0017d108 */
static void *line_add_f(void *ret, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->w = line_color;
    pt->y = y0;
    pt->z = z0;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Same as line_add_f. */
/* 0x0017d218 */
void *line_add_seg(void *ret, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->w = line_color;
    pt->y = y0;
    pt->z = z0;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Same as line_add_v. */
/* 0x0017d328 */
void *line_add_v_w(void *ret, float *start, float *end)
{
    LinePoint *pt;

    if (line_mode == 2) {
    } else if (line_mode == 1) {
        line_flush_vu0();
    }
    if (line_count >= 100)
        line_flush_vu1();
    pt = &line_buffer[line_count];
    pt->x = start[0];
    pt->y = start[1];
    pt->z = start[2];
    pt->w = line_color;
    line_mode = 2;
    pt++;
    pt->x = end[0];
    pt->y = end[1];
    pt->z = end[2];
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Same as line_add_f. */
/* 0x0017d420 */
void *line_add_f_w(void *ret, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->w = line_color;
    pt->y = y0;
    pt->z = z0;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Same as line_add_f. */
/* 0x0017d530 */
void *line_add_seg_w(void *ret, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->w = line_color;
    pt->y = y0;
    pt->z = z0;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = line_color;
    line_count += 2;
    return ret;
}

/* Returns 0. */
/* 0x0017d640 */
int line_set_translucent(int on)
{
    line_translucent = on;
    return 0;
}

/* ret unused. */
/* 0x0017d650 */
void line_end(void *ret)
{
    switch (line_mode) {
    case 1:
        line_flush_vu0();
        break;
    case 2:
        line_flush_vu1();
        break;
    }
}

/* Gradient: own colour at each end. */
/* 0x0017d688 */
static void *line_add_f_rgb(void *ret, int start_col, int end_col, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->y = y0;
    pt->z = z0;
    pt->w = start_col;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = end_col;
    line_count += 2;
    return ret;
}

/* Same as line_add_f_rgb. */
/* 0x0017d7a8 */
void *line_add_seg_rgb(void *ret, int start_col, int end_col, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->y = y0;
    pt->z = z0;
    pt->w = start_col;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = end_col;
    line_count += 2;
    return ret;
}

/* Gradient: own colour at each end. */
/* 0x0017d8c8 */
void *line_add_v_rgb(void *ret, int start_col, int end_col, float *start, float *end)
{
    LinePoint *pt;

    if (line_mode == 2) {
    } else if (line_mode == 1) {
        line_flush_vu0();
    }
    if (line_count >= 100)
        line_flush_vu1();
    pt = &line_buffer[line_count];
    pt->x = start[0];
    pt->y = start[1];
    pt->z = start[2];
    pt->w = start_col;
    line_mode = 2;
    pt++;
    pt->x = end[0];
    pt->y = end[1];
    pt->z = end[2];
    pt->w = end_col;
    line_count += 2;
    return ret;
}

/* Same as line_add_f_rgb. */
/* 0x0017d9d0 */
static void *line_add_grad(void *ret, int start_col, int end_col, float x0, float y0, float z0, float x1, float y1, float z1)
{
    LinePoint *pt;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->y = y0;
    pt->z = z0;
    pt->w = start_col;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z1;
    pt->w = end_col;
    line_count += 2;
    return ret;
}

/* Depth stored as 1 / depth; VU0 batch. Returns ret. */
/* 0x0017daf0 */
static void *line_add_2d(void *ret, float depth, float x0, float y0, float x1, float y1)
{
    LinePoint *pt;
    float z;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    z = 1.0f / depth;
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->y = y0;
    pt->z = z;
    pt->w = line_color;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z;
    pt->w = line_color;
    line_mode = 1;
    line_count += 2;
    return ret;
}

/* Gradient: own colour at each end. */
/* 0x0017dc08 */
static void *line_add_2d_rgb(void *ret, int start_col, int end_col, float depth, float x0, float y0, float x1, float y1)
{
    LinePoint *pt;
    float z;

    if (line_mode == 1) {
    } else if (line_mode == 2) {
        line_flush_vu1();
    }
    if (line_count >= 100)
        line_flush_vu0();
    z = 1.0f / depth;
    pt = &line_buffer[line_count];
    pt->x = x0;
    pt->y = y0;
    pt->z = z;
    pt->w = start_col;
    line_count += 2;
    line_mode = 1;
    pt++;
    pt->x = x1;
    pt->y = y1;
    pt->z = z;
    pt->w = end_col;
    return ret;
}
