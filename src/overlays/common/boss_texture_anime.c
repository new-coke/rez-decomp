/*
 * Boss movie panel: streams a texture animation from disc a frame at a time into a texture shown
 * as a coloured quad, with play, visibility, colour, rectangle, frame limit, play-once and timed
 * fades. Also keeps a still 128x128 texture.
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "color.h"
#include "file.h"
#include "fog.h"
#include "heap.h"
#include "libc.h"
#include "nj_strip.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "ps_gs.h"
#include "render.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_texture.h"

#include "debug.h"
#include "boss_texture_anime.h"
#include "boss_common.h"

extern CmTexAnimeWork *tex_anime_work;

extern struct TaskDesc cm_tex_anime_desc;
extern char str_cm_tex[];
extern int tex_anime_on;
extern struct TaskDesc cm_tex_anime_color_desc;
extern void *tex_anime_surface;
extern void *tex_anime_frame;
extern void *tex_still_surface;
extern void *tex_still_buf;
extern char str_boss_texture_anime_c[];

/* 0x00cc41f8 b_earth, 0x00cd6738 b_eden, 0x00cc2da0 b_mars, 0x00cc74d0 b_uranus, 0x00cca208 b_venus */
void tex_still_create(void)
{
    int req[5];
    void **surf = &tex_still_surface;

    tex_still_buf = heap_block_tag(game_calloc(18432), str_boss_texture_anime_c, 83);
    req[0] = 128;
    req[1] = 128;
    req[2] = 0;
    req[3] = 19;
    req[4] = (int)tex_still_buf;
    *surf = ulTextureCreateSurface(req);
}

/* 0x00cc4268 b_earth, 0x00cd67a8 b_eden, 0x00cc2e10 b_mars, 0x00cc7540 b_uranus, 0x00cca278 b_venus */
void tex_still_free(void)
{
    ulTextureFree(tex_still_surface);
    heap_free(tex_still_buf, 0);
    tex_still_surface = 0;
    tex_still_buf = 0;
}

/* 0x00cc42b0 b_earth, 0x00cd67f0 b_eden, 0x00cc2e58 b_mars, 0x00cc7588 b_uranus, 0x00cca2c0 b_venus */
void tex_anime_keep_frame(void)
{
    if (tex_still_buf && tex_anime_frame)
        nj_copy8(tex_still_buf, tex_anime_frame, 18432);
}

/* 0x00cc42e0 b_earth, 0x00cd6820 b_eden, 0x00cc2e88 b_mars, 0x00cc75b8 b_uranus, 0x00cca2f0 b_venus */
void tex_anime_surface_create(CmTexAnimeWork *w)
{
    int req[5];
    void **surf = &tex_anime_surface;

    req[0] = 128;
    req[1] = 128;
    req[2] = 0;
    req[3] = 19;
    req[4] = (int)w->buf1;
    *surf = ulTextureCreateSurface(req);
}

/* 0x00cc4330 b_earth, 0x00cd6870 b_eden, 0x00cc2ed8 b_mars, 0x00cc7608 b_uranus, 0x00cca340 b_venus */
void tex_anime_upload(CmTexAnimeWork *w, void *data)
{
    tex_anime_frame = data;
    ulTextureLoadImage(tex_anime_surface, data);
    psGsWriteReg(63, 0);
}

/* 0x00cc4360 b_earth, 0x00cd68a0 b_eden, 0x00cc2f08 b_mars, 0x00cc7638 b_uranus, 0x00cca370 b_venus */
void tex_anime_surface_free(CmTexAnimeWork *w)
{
    ulTextureFree(tex_anime_surface);
}

/* 0x00cc4370 b_earth, 0x00cd68b0 b_eden, 0x00cc2f18 b_mars, 0x00cc7648 b_uranus, 0x00cca380 b_venus */
void tex_anime_select_frame(CmTexAnimeWork *w)
{
    ulTextureLoadImage(tex_anime_surface, tex_anime_frame);
    psGsWriteReg(63, 0);
    ulGxSetTexture(tex_anime_surface);
}

/* 0x00cc43b8 b_earth, 0x00cd68f8 b_eden, 0x00cc2f60 b_mars, 0x00cc7690 b_uranus, 0x00cca3c8 b_venus */
void tex_anime_select(void)
{
    if (tex_anime_on == 1) {
        tex_anime_select_frame(tex_anime_work);
    } else if (tex_still_surface) {
        ulTextureLoadImage(tex_still_surface, tex_still_buf);
        psGsWriteReg(63, 0);
        ulGxSetTexture(tex_still_surface);
    } else {
        ulGxSetTextureList((void *)boss_get_task()[1]);
        ulGxSelectTexture(ulGxGetTextureList(), 0);
    }
}

/* 0x00cc4458 b_earth, 0x00cd6998 b_eden, 0x00cc3000 b_mars, 0x00cc7730 b_uranus, 0x00cca468 b_venus */
void cm_tex_anime_draw(CmTexAnimeWork *w)
{
    TaVtx vtx[4];
    int old_list, old_fog;
    float inv_z, x, y, right, bottom;

    if (!color_get_a(w->color))
        return;
    old_list = RenderSetCurrentDisplayList(3);
    inv_z = -1.0f / (-fog_get_far() + 1.0f);
    x = w->x;
    y = w->y;
    bottom = y + w->h;
    vtx[0].q = vtx[1].q = vtx[2].q = vtx[3].q = inv_z;
    vtx[3].color = w->color;
    vtx[2].color = w->color;
    vtx[1].color = w->color;
    vtx[0].x = x;
    vtx[2].y = vtx[0].y = y;
    vtx[0].u = 0.0f;
    vtx[0].v = 0.0f;
    vtx[1].x = x;
    vtx[3].y = vtx[1].y = bottom;
    vtx[1].u = 0.0f;
    vtx[1].v = 1.0f;
    right = x + w->width;
    vtx[3].x = vtx[2].x = right;
    vtx[0].color = w->color;
    vtx[2].u = 1.0f;
    vtx[2].v = 0.0f;
    vtx[3].v = vtx[3].u = 1.0f;
    draw_set_fc_mode(0x2000);
    old_fog = (int)fog_set_enable(0);
    draw_set_blend(4, 5);
    tex_anime_select_frame(w);
    nj_tex_strip_begin(1);
    nj_tex_strip_vertices(vtx, 4);
    nj_tex_strip_end();
    fog_set_enable((void *)old_fog);
    ulGxSetTextureList((void *)w->tex_list);
    RenderSetCurrentDisplayList(old_list);
}

/* 0x00cc45a8 b_earth, 0x00cd6ae8 b_eden, 0x00cc3150 b_mars, 0x00cc7880 b_uranus, 0x00cca5b8 b_venus */
void cm_tex_anime_exec_end(CmTexAnimeWork *w)
{
    if (w->state == 2 && file_handle_is_idle(w->handle) != 1) {
        if (++w->close_wait <= 120.0f)
            return;
    }
    tex_anime_keep_frame();
    TASK_OF(w)->disp = 0;
    tex_anime_on = 0;
    FINISH_IF(w);
}

/* 0x00cc4638 b_earth, 0x00cd6b78 b_eden, 0x00cc31e0 b_mars, 0x00cc7910 b_uranus, 0x00cca648 b_venus */
void cm_tex_anime_exec(CmTexAnimeWork *w)
{
    int cur_buf, n;

    switch (w->state) {
    case 1:
        w->handle = file_handle_open(&w->vol);
        if (!w->handle) {
            w->err |= 0x10000;
            break;
        }
        if (w->sector && !file_handle_seek(w->handle, w->sector, 0)) {
            w->err |= 0x20000;
            file_handle_close(w->handle);
            w->handle = 0;
            break;
        }
        if (file_handle_read(w->handle, w->fsec, ((TexAnimBufs *)w)->buf[w->buf_idx]) <= 0) {
            w->err |= 0x40000;
            file_handle_close(w->handle);
            w->handle = 0;
            break;
        }
        w->state = 2;
    case 2:
        if (file_handle_is_idle(w->handle) != 1)
            break;
        w->state = 3;
        file_handle_close(w->handle);
        w->handle = 0;
        break;
    }
    switch (w->playing) {
    case 0:
        if (!w->play)
            return;
        if (w->state == 3) {
            cur_buf = w->buf_idx;
            w->playing = 1;
            w->play_pos[0] = cur_buf;
            w->buf_idx = cur_buf ^ 1;
            w->sector += w->fsec;
            if (w->sector >= w->sectors) {
                w->nbuf = w->nbuf_full;
                w->fsec = w->fsec2;
                w->at_end = 1;
                w->sector = 0;
                w->frame_no = 0;
            } else if (w->sector + w->fsec > w->sectors) {
                w->fsec = w->sectors - w->sector;
                w->nbuf = (w->fsec << 11) / w->fsize;
                w->at_end = 1;
            } else {
                w->nbuf = w->nbuf_full;
                w->fsec = w->fsec2;
            }
            w->state = 1;
        } else {
            w->err++;
            if (--w->wait > 0)
                return;
            w->wait = w->interval;
            if (++w->play_pos[1] >= w->nbuf)
                w->play_pos[1] = w->nbuf - 1;
            return;
        }
    case 1:
        for (n = 1; w->play; ) {
            if (--w->wait <= 0) {
                w->wait = w->interval;
                tex_anime_upload(w, ((TexAnimBufs *)w)->buf[w->play_pos[0]] + w->fsize * w->play_pos[1]);
                w->frame_no++;
                w->have_frame = 1;
                w->pvr++;
                if (w->limit_on && w->limit < w->pvr) {
                    TASK_OF(w)->exec = (void (*)(void *))cm_tex_anime_exec_end;
                    return;
                }
                if (++w->play_pos[1] >= w->nbuf) {
                    w->play_pos[1] = 0;
                    w->playing = 0;
                    if (w->at_end && w->once)
                        TASK_OF(w)->exec = (void (*)(void *))cm_tex_anime_exec_end;
                    return;
                }
            }
            if (--n == 0)
                return;
        }
        break;
    }
}

/* 0x00cc4970 b_earth, 0x00cd6eb0 b_eden, 0x00cc3518 b_mars, 0x00cc7c48 b_uranus, 0x00cca980 b_venus */
void cm_tex_anime_disp(CmTexAnimeWork *w)
{
    if (w->have_frame & w->visible)
        cm_tex_anime_draw(w);
}

/* 0x00cc4998 b_earth, 0x00cd6ed8 b_eden, 0x00cc3540 b_mars, 0x00cc7c70 b_uranus, 0x00cca9a8 b_venus */
void cm_tex_anime_release(CmTexAnimeWork *w)
{
    if (w->state == 2 && w->handle)
        file_handle_close(w->handle);
    if (w->buf1)
        heap_free(w->buf1, 0);
    if (w->buf2)
        heap_free(w->buf2, 0);
    tex_anime_running_set(0);
    tex_anime_surface_free(w);
}

/* 0x00cc4a10 b_earth, 0x00cd6f50 b_eden, 0x00cc35b8 b_mars, 0x00cc7ce8 b_uranus, 0x00ccaa20 b_venus */
void cm_tex_anime_exec_open(CmTexAnimeWork *w)
{
    void *fh = file_handle_open(&w->vol);
    int size;

    if (fh) {
        size = file_handle_size(fh);
        w->nframes = size / w->fsize;
        w->sectors = size / 2048;
        file_handle_close(fh);
        TASK_OF(w)->exec = (void (*)(void *))cm_tex_anime_exec;
    }
}

/* 0x00cc4a98 b_earth, 0x00cd6fd8 b_eden, 0x00cc3640 b_mars, 0x00cc7d70 b_uranus, 0x00ccaaa8 b_venus */
CmTexAnimeWork *cm_tex_anime_create(void *parent, int vol, char *name, int arg, int fsize, int nbuf, int interval)
{
    CmTexAnimeWork *w = task_create_inherit(parent, &cm_tex_anime_desc, 192);
    char *name_buf;
    int size;
    void *fh;

    if (tex_anime_running_get())
        return 0;
    name_buf = w->name;
    tex_anime_running_set(1);
    tex_anime_work = w;
    w->nbuf = nbuf;
    w->nbuf_full = nbuf;
    w->fsize = fsize;
    w->buf1 = heap_block_tag(file_heap_alloc(fsize * nbuf), str_cm_tex, -1);
    w->buf2 = heap_block_tag(file_heap_alloc(fsize * nbuf), str_cm_tex, -1);
    memset(w->buf1, 1, fsize * nbuf);
    memset(w->buf2, 1, fsize * nbuf);
    w->buf_idx = 0;
    w->state = 1;
    sprintf(name_buf, name);
    w->vol = vol;
    w->namep = name_buf;
    w->arg = arg;
    w->interval = interval;
    w->wait = interval;
    w->play = 0;
    w->x = 0.0f;
    w->y = 0.0f;
    w->fsec2 = w->fsec = w->nbuf * w->fsize / 2048;
    w->color = COL_WHITE;
    w->width = 640.0f;
    w->h = 480.0f;
    tex_anime_surface_create(w);
    tex_anime_on = 1;
    fh = file_handle_open(&w->vol);
    if (fh) {
        size = file_handle_size(fh);
        w->nframes = size / fsize;
        w->sectors = size / 2048;
        file_handle_close(fh);
    } else {
        TASK_OF(w)->exec = (void (*)(void *))cm_tex_anime_exec_open;
    }
    return w;
}

/* 0x00cc4cb0 b_earth, 0x00cd71f0 b_eden, 0x00cc3858 b_mars, 0x00cc7f88 b_uranus, 0x00ccacc0 b_venus */
void tex_anime_set_play(int on)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get())
        w->play = on;
}

/* 0x00cc4cf0 b_earth, 0x00cd7230 b_eden, 0x00cc3898 b_mars, 0x00cc7fc8 b_uranus, 0x00ccad00 b_venus */
void tex_anime_set_visible(int on)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get())
        w->visible = on;
}

/* 0x00cc4d30 b_earth, 0x00cd7270 b_eden, 0x00cc38d8 b_mars, 0x00cc8008 b_uranus, 0x00ccad40 b_venus */
void tex_anime_set_color(unsigned int color)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get())
        w->color = color;
}

/* 0x00cc4d70 b_earth, 0x00cd72b0 b_eden, 0x00cc3918 b_mars, 0x00cc8048 b_uranus, 0x00ccad80 b_venus */
void tex_anime_set_rect(float x, float y, float width, float height)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get()) {
        w->x = x;
        w->y = y;
        w->width = width;
        w->h = height;
    }
}

/* 0x00cc4de0 b_earth, 0x00cd7320 b_eden, 0x00cc3988 b_mars, 0x00cc80b8 b_uranus, 0x00ccadf0 b_venus */
void tex_anime_stop(void)
{
    if (tex_anime_running_get()) {
        tex_anime_keep_frame();
        FINISH_IF(tex_anime_work);
        tex_anime_on = 0;
    }
}

/* 0x00cc4e30 b_earth, 0x00cd7370 b_eden, 0x00cc39d8 b_mars, 0x00cc8108 b_uranus, 0x00ccae40 b_venus */
void tex_anime_finish(void)
{
    if (tex_anime_running_get())
        TASK_OF(tex_anime_work)->exec = (void (*)(void *))cm_tex_anime_exec_end;
}

/* 0x00cc4e68 b_earth, 0x00cd73a8 b_eden, 0x00cc3a10 b_mars, 0x00cc8140 b_uranus, 0x00ccae78 b_venus */
void tex_anime_set_limit(int limit)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get()) {
        w->limit = w->nframes;
        w->limit_arg = limit;
        w->limit_on = 1;
    }
}

/* 0x00cc4eb8 b_earth, 0x00cd73f8 b_eden, 0x00cc3a60 b_mars, 0x00cc8190 b_uranus, 0x00ccaec8 b_venus */
void tex_anime_set_once(void)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get())
        w->once = 1;
}

/* 0x00cc4ef0 b_earth, 0x00cd7430 b_eden, 0x00cc3a98 b_mars, 0x00cc81c8 b_uranus, 0x00ccaf00 b_venus */
void cm_tex_anime_color_exec(CmTexAnimeColorWork *w)
{
    if (w->delay) {
        w->delay--;
        return;
    }
    w->count--;
    *w->dst = boss_color_blend(w->from, w->to, (float)w->count / w->total);
    if (w->count <= 0)
        FINISH(w);
}

/* 0x00cc4f78 b_earth, 0x00cd74b8 b_eden, 0x00cc3b20 b_mars, 0x00cc8250 b_uranus, 0x00ccaf88 b_venus */
CmTexAnimeColorWork *cm_tex_anime_color_create(CmTexAnimeWork *w, unsigned int from, unsigned int to, int delay, int frames)
{
    CmTexAnimeColorWork *fade;

    if (!w)
        w = tex_anime_work;
    fade = task_create(w, &cm_tex_anime_color_desc, 24);
    fade->dst = &w->color;
    fade->from = from;
    fade->to = to;
    fade->delay = delay;
    fade->total = frames;
    fade->count = frames;
    return fade;
}

/* 0x00cc5010 b_earth, 0x00cd7550 b_eden, 0x00cc3bb8 b_mars, 0x00cc82e8 b_uranus, 0x00ccb020 b_venus */
CmTexAnimeWork *tex_anime_start(void *parent, int *file, int arg, int limit)
{
    CmTexAnimeWork *w;

    if (!file || !parent) {
        tex_anime_work = 0;
        return 0;
    }
    w = cm_tex_anime_create(parent, file[0], (char *)file[1], arg, 18432, 15, 4);
    tex_anime_set_limit(limit);
    tex_anime_set_color(0);
    tex_anime_set_visible(0);
    tex_anime_set_play(0);
    return w;
}

/* 0x00cc5098 b_earth, 0x00cd75d8 b_eden, 0x00cc3c40 b_mars, 0x00cc8370 b_uranus, 0x00ccb0a8 b_venus */
void tex_anime_fade_in_out(void)
{
    CmTexAnimeWork *w = tex_anime_work;

    if (tex_anime_running_get()) {
        tex_anime_set_play(1);
        tex_anime_set_visible(1);
        cm_tex_anime_color_create(0, 0, COL_WHITE, 0, 16);
        cm_tex_anime_color_create(0, COL_WHITE, 0, w->nframes * w->interval - 16, 16);
    }
}
