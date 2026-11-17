/*
 * Frame renderer: two packet buffers of 14 display lists each, opened at frame start, closed and
 * queued at the flip, chained and kicked to the GS from vsync. Also the GS interrupt, frame-buffer
 * copies, background fill and display-list usage gauges.
 */

#include "types_game.h"
#include "sdk_compat.h"
#include "debug.h"
#include "libc.h"
#include "libkernl_intc.h"
#include "ps_image.h"
#include "ps_pkt.h"
#include "ul_gx_prim.h"
#include "render.h"
#include "callback.h"
#include "color.h"
#include "debugdraw.h"
#include "fog.h"
#include "heap.h"
#include "light.h"
#include "model.h"
#include "node_draw_state.h"
#include "perform.h"
#include "screen.h"
#include "system.h"
#include "texforce.h"
#include "main.h"

typedef struct { unsigned long csr; long idx; } GsLog;

#ifdef VERSION_US

#endif /* VERSION_US */

#define LOG(c) \
    do { \
        if (render_event_log_idx < 256) \
            render_event_log[render_event_log_idx++] = (c); \
    } while (0)

extern int render_draw_slot;
extern int render_flip_req;
extern int render_pkt_bufs[];
extern int render_dl_sizes[];
extern int render_event_log[];
extern char render_pkt_mem0[];
extern char render_pkt_mem1[];
extern int psDisplayListInit(int, int, int);
extern long sceGsGetIMR(long);
extern long sceGsPutIMR(long);
extern void AddIntcHandler(int, void (*)(void), int);
extern int render_profiler_group;
extern int render_vars_group;
extern char gw_varedit_root[];
extern char render_vardef[];
extern char render_vardefs[];
extern int gw_profiler_group_create(const char *);
extern int gw_varedit_add(char *, int, char *);
extern void gw_varedit_add_table(char *, int, char *);
extern char *render_dl_names[];
extern void font_set(int);
extern void psDisplayListGetUsage(int, unsigned *);
extern int render_gs_log_idx;
extern int render_event_log_idx;
extern int render_finish_cnt;
extern int render_signal_cnt;
extern int render_kick_time;
extern int render_draw_start;
extern int render_draw_end;
extern volatile int render_dma_busy;
extern GsLog render_gs_log[];

#ifdef VERSION_US

extern int psDisplayListUnLock(void *dl);

#endif /* VERSION_US */

extern int render_dl;
extern int b_ztest_on;
extern int b_ztest_off;
extern int a_ztest_on;
extern int a_ztest_off;
extern int b_zmask_on;
extern int b_zmask_off;
extern int a_zmask_on;
extern int a_zmask_off;
extern int b_atest_on;
extern int b_atest_off;
extern int a_atest_on;
extern int a_atest_off;
extern void psGsSetDepthTest(int, int);
extern void psGsSetZMask(int);

extern int render_cur_dl;
extern DlSlot render_dls[];
extern void psGsSetDisplayList(int);
extern unsigned render_fb0;
extern unsigned render_fb1;
extern volatile int render_kick_slot;
extern volatile int render_queue_slot;
extern int render_flip_notify;
extern int render_submit_mode;
extern int render_submit_wait;
extern void psGsWriteReg(int, unsigned long v);
extern void psDisplayListClose(int);
extern void psGsGetVramLayout(int *);
extern int render_kick_hold;
extern int render_kick_wait;
extern DlMode render_dl_modes[];
extern long isceGsPutIMR(long);
extern int psGsGetFrameReg(int);
extern void sceGsSwapDBuff(void);
extern void psDisplayListAppend(int, int);
extern void psDisplayListFlushInt(int, int);
extern int render_sbw;
extern int render_dbw;
extern int render_rrw;
extern int render_rrh;
extern int render_busy;
extern int debug_disp_level;
extern void psDisplayListOpen(int);
extern int psGsSetAlphaTest(int, int, int, int);
extern char str_render_draw_down[];

#ifdef VERSION_US

extern void psDisplayListLock(int);

#endif /* VERSION_US */

static void render_clear_frame_regs(void);
static void render_set_frame_regs_b(void);

/* Slot being built this frame. */
/* 0x0015b248 */
int render_get_draw_slot(void)
{
    return render_draw_slot;
}

/* From the VRAM layout; prints them. */
/* 0x0015b258 */
static void render_get_frame_buffers(void)
{
    int fb[8];

    psGsGetVramLayout(fb);
    render_fb1 = fb[1];
    render_fb0 = fb[0];
    printf("GS_FB0_ADRS 0x%08x\n", fb[1]);
    printf("GS_FB1_ADRS 0x%08x\n", render_fb0);
}

/*
 * Two 3 MB packet buffers of 14 lists; GS interrupt handler as in render_init; clears the event
 * log.
 */
/* 0x0015b2b0 */
void render_init_all(void)
{
    int j;
    int k;
    int size;
    int i;

    render_flip_req = 0;
    listener_add(0, render_kick_display_list);
    render_pkt_bufs[0] = psPktBufInit(render_pkt_mem0, 0x300000);
    render_pkt_bufs[1] = psPktBufInit(render_pkt_mem1, 0x300000);
    for (j = 0; j < 2; j++) {
        for (k = 0; k < 14; k++) {
            size = render_dl_sizes[k];
            render_dls[j].list[k] = psDisplayListInit(heap_block_tag(game_malloc(size), "render.c", 342), size, render_pkt_bufs[j]);
        }
    }
    *(unsigned long *)0x12001000 |= 2;
    do { } while (0);
    sceGsPutIMR(sceGsGetIMR(2) & ~0x200);
    AddIntcHandler(0, render_gs_interrupt, 0);
    EnableIntc(0);
    for (i = 255; i >= 0; i--)
        render_event_log[i] = 0;
    render_get_frame_buffers();
}

/* 0x0015b460 */
void render_init(void)
{
    long mask = 2;

    *(unsigned long *)0x12001000 |= mask;
    do { } while (0);
    sceGsPutIMR(sceGsGetIMR(mask) & ~0x200);
    AddIntcHandler(0, render_gs_interrupt, 0);
    EnableIntc(0);
    render_get_frame_buffers();
}

/* "R-REGIST" profiler group and variables. */
/* 0x0015b4b8 */
void render_vars_register(void)
{
    render_profiler_group = gw_profiler_group_create("R-REGIST");
    render_vars_group = gw_varedit_add(gw_varedit_root, 0, render_vardef);
    gw_varedit_add_table(gw_varedit_root, render_vars_group, render_vardefs);
}

/*
 * Waits until the previous frame is kicked, opens the slot's 14 lists with render_dl_modes state,
 * reloads textures, fills background, updates lights. "Draw Down!!" if it waited for the GS.
 */
/* steer(cast) */
/* 0x0015b520 */
int render_submit_frame(int mode, int wait)
{
    int i;

    listener_dispatch(1, 0);
    render_submit_mode = mode;
    render_submit_wait = wait;
    if (render_flip_req == 0) {
        while (render_queue_slot != render_kick_slot)
            render_busy = 1;
        render_flip_req = 1;
        psPktBufReset(render_pkt_bufs[render_draw_slot]);
        for (i = 0; i < 14; i++) {
            int dl = render_dls[render_draw_slot].list[i];

            psDisplayListOpen(dl);
            psGsSetDisplayList(dl);
            if (i == 0) {
                psGsWriteReg(34, 0);
                psGsWriteReg(74, 0);
                psGsWriteReg(73, 0);
                psGsWriteReg(59, (unsigned long)1 << 39);
                psGsWriteReg(26, 1);
            }
            psGsSetZMask(render_dl_modes[i].zmask);
            psGsSetAlphaTest(1, 5, 128, 1);
            psGsSetDepthTest(1, 2);
            draw_set_test_mode(0);
        }
        render_cur_dl = 0;
        psGsSetDisplayList(render_dls[render_draw_slot].list[0]);
        model_tex_reload_all();
        psGsWriteReg(63, 0);
        render_fill_screen_background();
        RenderSetCurrentDisplayList(0);
        light_update();
        if (render_busy) {
            render_busy = 0;
            if ((unsigned)debug_disp_level < 2) {
                debug_set_origin(20, 10);
                return ((int (*)(const char *, ...))debug_printf)(str_render_draw_down);
            }
        }
    }
}

/*
 * Adds the frame-buffer copies, closes, queues the slot, switches slot. Only after a start with a
 * negative wait count.
 */
/* 0x0015b780 */
void render_flip(void)
{
    int i;
    int slot;

    listener_dispatch(2, 0);
    if (render_submit_mode == 0)
        render_clear_frame_regs();
    if (render_flip_req) {
        if (render_submit_wait < 0) {
            render_set_frame_regs_b();
            render_set_frame_regs();
            render_flip_req = 0;
            RenderSetCurrentDisplayList(13);
            psGsWriteReg(97, 1);
            i = 0;
            do {
                psDisplayListClose(render_dls[render_draw_slot].list[i]);
                i++;
            } while (i < 14);
            psPktBufClose(render_pkt_bufs[render_draw_slot]);
            {
                int one = 1;
                slot = render_draw_slot;
                render_queue_slot = slot;
                do { } while (0);
                slot ^= 1;
                render_draw_slot = slot;
                render_flip_notify = one;
            }
        }
    }
}

/* list 0..13; returns the previous one. */
/* 0x0015b890 */
int RenderSetCurrentDisplayList(unsigned list)
{
    int old;

    if (list >= 14) {
        ERROR_PRINTF("RenderSetCurrentDisplayList() Illigal List\n");
        return render_cur_dl;
    }
    psGsSetDisplayList(render_dls[render_draw_slot].list[list]);
    old = render_cur_dl;
    render_cur_dl = list;
    return old;
}

/* Last finished frame, 0..1. */
/* 0x0015b908 */
float render_get_packet_load(void)
{
    unsigned usage[2];

    psPktBufGetUsage(render_pkt_bufs[render_draw_slot ^ 1], usage);
    return (float)usage[0] / (float)usage[1];
}

/* Last finished frame: gauge, words used, name per list. Returns the y below the last row. */
/* 0x0015b9b0 */
float render_debug_gauges(float x, float y, float z, float width, float height)
{
    char buf[32];
    unsigned used[2];
    float right = x + width;
    int i;
    font_set(0);
    for (i = 0; i < 14; i++) {
        psDisplayListGetUsage(render_dls[render_draw_slot ^ 1].list[i], used);
        debug_draw_gauge(z, x, y, width, height, (float)used[0] / (float)used[1], RGBA(0xff, 0x80, 0, 0x80), RGBA(0xff, 0xff, 0xff, 0x80));
        sprintf(buf, "%4d", used[0]);
        font_print(right + 2.0f, y - 5.0f, z, RGBA(0x60, 0x60, 0x60, 0x80), buf);
        font_print(right + 40.0f + 2.0f, y - 5.0f, z, RGBA(0x60, 0x60, 0x60, 0x80), render_dl_names[i]);
        y = y + (height + 1.0f);
    }
    return y;
}

#ifdef VERSION_US
/*
 * Unless held or the GS is drawing: chains the queued slot's lists (render_dl_modes: always, or
 * one frame-buffer parity), swaps display buffers, kicks. Head list locked until GS interrupt.
 */
/* 0x0015bbc8 */
void render_kick_display_list(void)
{
    DlSlot *slot;
    long ie;
    int prev;
    int head;
    int i;
    unsigned long seed;
    int flag;

    if (render_kick_hold != 0 || render_dma_busy != 0)
        return;
    render_kick_wait = 0;
    if (render_kick_slot != render_queue_slot) {
        *(int *)&render_kick_slot = render_queue_slot;
    } else if (render_kick_slot < 0) {
        return;
    }
    slot = &render_dls[render_kick_slot];
    head = slot->list[0];
    ie = isceGsPutIMR(0xff00);
    flag = 1;
    seed = render_fb1;
    seed ^= (unsigned long)(psGsGetFrameReg(0) & 0x1ff) << 11;
    if (seed == 0) flag = 0;
    prev = slot->list[0];
    for (i = 1; i < 14; i++) {
        int cur = slot->list[i];
        int go = 0;
        switch (render_dl_modes[i].mode) {
        case 0: go = 1; break;
        case 1: if (flag) go = 1; break;
        case 2: if (!flag) go = 1; break;
        }
        if (go) { psDisplayListAppend(prev, cur); prev = cur; }
    }
    sceGsSwapDBuff();
    psDisplayListLock(head);
    psDisplayListFlushInt(head, 1);
    render_dma_busy = 1;
    do {
        if (render_event_log_idx < 256)
            render_event_log[render_event_log_idx++] = 50;
    } while (0);
    render_kick_time = perform_timer_read();
    if (render_flip_notify) { render_flip_notify = 0; heap_pool_tick(); }
    isceGsPutIMR(ie);
}
#else
/* 0x0015bbc8 */
void render_kick_display_list(void)
{
    DlSlot *slot;
    long ie;
    int prev;
    int head;
    int i;
    unsigned long seed;
    int flag;

    if (render_kick_hold == 0 && render_dma_busy == 0) {
        render_kick_wait = 0;
        if (render_kick_slot != render_queue_slot) {
            render_kick_slot = render_queue_slot;
        } else if (render_kick_slot < 0) {
            return;
        }
        slot = &render_dls[render_kick_slot];
        head = slot->list[0];
        if (head == 0) return;
        ie = isceGsPutIMR(0xff00);
        flag = 1;
        seed = render_fb1;
        seed ^= (unsigned long)(psGsGetFrameReg(0) & 0x1ff) << 11;
        if (seed == 0) flag = 0;
        prev = slot->list[0];
        for (i = 1; i < 14; i++) {
            int cur = slot->list[i];
            int go = 0;
            switch (render_dl_modes[i].mode) {
            case 0: go = 1; break;
            case 1: if (flag) go = 1; break;
            case 2: if (!flag) go = 1; break;
            }
            if (go) { psDisplayListAppend(prev, cur); prev = cur; }
        }
        sceGsSwapDBuff();
        psDisplayListFlushInt(head, 1);
        render_dma_busy = 1;
        do {
            if (render_event_log_idx < 256)
                render_event_log[render_event_log_idx++] = 50;
        } while (0);
        render_kick_time = perform_timer_read();
        if (render_flip_notify) { render_flip_notify = 0; heap_pool_tick(); }
        isceGsPutIMR(ie);
    }
}
#endif /* !VERSION_US */

/*
 * Frame done: counts, clears busy, draw time, system_gs_finish_hook. Signal: counts. First 64 CSR
 * values -> render_gs_log; every step -> 256-entry event log.
 */
/* 0x0015be10 */
int render_gs_interrupt(void)
{
    long ie = isceGsPutIMR(0xff00);
    int idx = render_gs_log_idx;

    if (idx < 64) {
        render_gs_log[idx].csr = GS_CSR;
        render_gs_log[idx].idx = render_event_log_idx;
        render_gs_log_idx = idx + 1;
    }
    LOG(10);
    if (GS_CSR & 2) {
        GS_CSR |= 2;
        render_finish_cnt++;
        render_dma_busy = 0;
        LOG(20);
        render_draw_start = render_kick_time;
        render_draw_end = perform_timer_read();
        system_gs_finish_hook();
#ifdef VERSION_US
        psDisplayListUnLock((void *)render_dls[render_kick_slot].list[0]);
#endif
        LOG(21);
    } else if ((int)(GS_CSR & 1)) {
        GS_CSR |= 1;
        LOG(40);
        render_signal_cnt++;
    }
    LOG(28);
    ExitHandler();
    isceGsPutIMR(ie);
    return 1;
}

/* Fraction of the frame time at frame_rate. */
/* steer(cast) */
/* 0x0015c058 */
float render_get_frame_scale(void)
{
    return perform_elapsed_cur(render_draw_start, render_draw_end) * (0.1f / ((float)frame_rate * 16.666666f));
}

/* Copies to the texture-force surface in four strips: first buffer in list 1, second in 2. */
/* steer(cast) */
/* 0x0015c0c0 */
static void render_clear_frame_regs(void)
{
    unsigned pages = ((unsigned (*)(void))texforce_surface_get_addr)() >> 6;
    unsigned fb0 = render_fb0 >> 6;
    unsigned fb1 = render_fb1 >> 6;
    int old;
    unsigned long reg;

    old = RenderSetCurrentDisplayList(1);
    reg = (unsigned long)(fb0 | 0x10000) | (unsigned long)pages << 32 | (unsigned long)1 << 48;
    psImageDlAddTransferRegs(reg, 0, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0080000001400000UL, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0100000002800000UL, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0180000003c00000UL, 0x2000000100UL, 2);
    RenderSetCurrentDisplayList(2);
    reg = (unsigned long)(fb1 | 0x10000) | (unsigned long)pages << 32 | (unsigned long)1 << 48;
    psImageDlAddTransferRegs(reg, 0, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0080000001400000UL, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0100000002800000UL, 0x2000000100UL, 2);
    psImageDlAddTransferRegs(reg, 0x0180000003c00000UL, 0x2000000100UL, 2);
    RenderSetCurrentDisplayList(old);
}

/* With a capture surface: a render_rrw x render_rrh copy per buffer, lists 10 (first) and 11. */
/* steer(cast) */
/* 0x0015c2a0 */
void render_set_frame_regs(void)
{
    unsigned pages = ((unsigned (*)(void))screen_get_surface_addr)() >> 6;
    unsigned long data;
    unsigned fb0;
    unsigned fb1;

    if (pages) {
        data = render_rrw | (unsigned long)render_rrh << 32;
        fb0 = render_fb0 >> 6;
        fb1 = render_fb1 >> 6;
        RenderSetCurrentDisplayList(10);
        psImageDlAddTransferRegs(fb0 | ((unsigned long)render_sbw << 16) | ((unsigned long)pages << 32) | ((unsigned long)render_dbw << 48), 0, data, 2);
        RenderSetCurrentDisplayList(11);
        psImageDlAddTransferRegs(fb1 | ((unsigned long)render_sbw << 16) | ((unsigned long)pages << 32) | ((unsigned long)render_dbw << 48), 0, data, 2);
    }
}

/* Lists 7 and 8, buffers swapped. */
/* steer(cast) */
/* 0x0015c3c8 */
static void render_set_frame_regs_b(void)
{
    unsigned pages = ((unsigned (*)(void))screen_get_surface_addr)() >> 6;
    unsigned long data;
    unsigned fb0;
    unsigned fb1;

    if (pages) {
        data = render_rrw | (unsigned long)render_rrh << 32;
        fb0 = render_fb1 >> 6;
        fb1 = render_fb0 >> 6;
        RenderSetCurrentDisplayList(7);
        psImageDlAddTransferRegs(fb0 | ((unsigned long)render_sbw << 16) | ((unsigned long)pages << 32) | ((unsigned long)render_dbw << 48), 0, data, 2);
        RenderSetCurrentDisplayList(8);
        psImageDlAddTransferRegs(fb1 | ((unsigned long)render_sbw << 16) | ((unsigned long)pages << 32) | ((unsigned long)render_dbw << 48), 0, data, 2);
    }
}

/* 0x0015c4f0 */
static void render_nop(void)
{
}

/*
 * Fog colour, 640x480 quad without fog, list render_dl. The b_* and a_* switches set depth test,
 * depth mask, alpha test before and after.
 */
/* steer(cast) */
/* 0x0015c4f8 */
void render_fill_screen_background(void)
{
    BgVtx vtx[4];
    int fog;
    int r, g, b;
    unsigned col;
    int saved;
    float eps = 1.0e-6f;

    fog = fog_get_back_color();
    r = ((int (*)(int))color_get_r)(fog);
    g = color_get_g(fog);
    b = color_get_b(fog);
    col = (r & 0xff) | (((g & 0xff) << 8) | ((b & 0xff) << 16));
    vtx[0].x = 0.0f; vtx[0].y = 0.0f; vtx[0].z = eps; vtx[0].c = col;
    vtx[1].x = 640.0f; vtx[1].y = 0.0f; vtx[1].z = eps; vtx[1].c = col;
    vtx[2].x = 0.0f; vtx[2].y = 480.0f; vtx[2].z = eps; vtx[2].c = col;
    vtx[3].x = 640.0f; vtx[3].y = 480.0f; vtx[3].z = eps; vtx[3].c = col;
    RenderSetCurrentDisplayList(render_dl);
    if (b_ztest_on) psGsSetDepthTest(1, 2);
    if (b_ztest_off) psGsSetDepthTest(1, 1);
    if (b_zmask_on) psGsSetZMask(1);
    if (b_zmask_off) psGsSetZMask(0);
    if (b_atest_on) psGsSetAlphaTest(1, 5, 128, 1);
    if (b_atest_off) psGsSetAlphaTest(0, 5, 128, 1);
    saved = fog_set_enable(0);
    ulGxBeginStrip(0);
    ulGxAddStrip(vtx, 4);
    ulGxEndStrip();
    fog_set_enable(saved);
    if (a_ztest_on) psGsSetDepthTest(1, 2);
    if (a_ztest_off) psGsSetDepthTest(1, 1);
    if (a_zmask_on) psGsSetZMask(1);
    if (a_zmask_off) psGsSetZMask(0);
    if (a_atest_on) psGsSetAlphaTest(1, 5, 128, 1);
    if (a_atest_off) psGsSetAlphaTest(0, 5, 128, 1);
}
