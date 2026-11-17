/* Ninja system layer: timers, heap, memory fill/copy; Dreamcast hardware calls are empty. */

#include "ninja.h"
#include "libc.h"
#include "nj_system.h"
#include "kernel_syscalls.h"
#include "ps_heap.h"

extern int psTimerGetCount(void);
extern int psTimerDiff(int a, int b);
extern float psTimerToUsec(int t);
extern void psTimerInit(int mode);
extern void psTimerExit(void);
extern void *nj_heap;
extern char nj_pad_status[];
extern char nj_pad_slot[];
extern char nj_keyboard_report_buf[];
extern int nj_keyboard_status;
extern void nj_copy256(NjQword *dst, NjQword *src, int n);

/* ps-layer timer. */
/* 0x001a3300 */
int nj_timer_get_count(void)
{
    return psTimerGetCount();
}

/* Allows the 16-bit counter to wrap once. */
/* 0x001a3320 */
int nj_timer_diff(int start, int end)
{
    return psTimerDiff(end, start);
}

/* Whole microseconds. */
/* 0x001a3348 */
int nj_timer_to_usec(int ticks)
{
    return psTimerToUsec(ticks);
}

/* 0x001a3370 */
static void nj_timer_init(int mode)
{
    psTimerInit(mode);
}

/* 0x001a3378 */
static void nj_timer_exit(void)
{
    psTimerExit();
}

/* 0x001a3380 */
static int nj_timer_return_zero(void)
{
    return 0;
}

/* 0x001a3388 */
static void nj_timer_nop(void)
{
}

/* 0x001a3390 */
int nj_clock_read(void)
{
    return 0;
}

/* 0x001a3398 */
static void *nj_heap_get(void)
{
    return nj_heap;
}

/* 0x001a33a0 */
void nj_heap_create(void *mem, int size)
{
    nj_heap = psHeapInit(mem, size);
}

/* If there is one. */
/* steer(cast) */
/* 0x001a33c0 */
static void nj_heap_release(void)
{
    if (nj_heap) {
        ((void (*)(void *))psHeapRelease)(nj_heap);
        nj_heap = 0;
    }
}

/* 0x001a33e8 */
void *nj_malloc(int size)
{
    return psHeapAlloc(nj_heap, size);
}

/* 0x001a3410 */
static int nj_heap_return_zero(void)
{
    return 0;
}

/* 0x001a3418 */
static void *nj_alloc_array(int n, int size)
{
    return psHeapAlloc(nj_heap, n * size);
}

/* 0x001a3440 */
static void nj_free(void *ptr)
{
    psHeapFree(ptr);
}

/* 0x001a3448 */
static void nj_heap_nop(void)
{
}

/* Status the Ninja pad calls report. */
/* 0x001a3450 */
static void nj_pad_clear_status(void)
{
    memset(nj_pad_status, 0, 52);
}

/* 0x001a3468 */
static void nj_pad_clear_status2(void)
{
    memset(nj_pad_status, 0, 52);
}

/* 0x001a3480 */
static void nj_pad_nop(void)
{
}

/* 0x001a3488 */
static void nj_pad_nop_2(void)
{
}

/* 0x001a3490 */
static void nj_pad_nop_3(void)
{
}

/* 0x001a3498 */
static void *nj_pad_get_status(void)
{
    return nj_pad_status;
}

/* 0x001a34a8 */
void *nj_pad_slot_info(void)
{
    return nj_pad_slot;
}

/* 0x001a34b8 */
static void nj_pad_nop_4(void)
{
}

/* 0x001a34c0 */
static int nj_pad_return_zero(void)
{
    return 0;
}

/* 0x001a34c8 */
static void nj_pad_nop_5(void)
{
}

/* 0x001a34d0 */
static int nj_pad_return_zero_2(void)
{
    return 0;
}

/* 0x001a34d8 */
static void nj_pad_nop_6(void)
{
}

/* 0x001a34e0 */
static void nj_pad_nop_7(void)
{
}

/* 0x001a34e8 */
static void *nj_pad_get_status2(void)
{
    return nj_pad_status;
}

/* 0x001a34f8 */
static void nj_pad_nop_8(void)
{
}

/* 0x001a3500 */
static void nj_pad_nop_9(void)
{
}

/* 0x001a3508 */
static int nj_pad_return_zero_3(void)
{
    return 0;
}

/* 0x001a3510 */
static void nj_pad_nop_10(void)
{
}

/* 0x001a3518 */
static int nj_pad_return_zero_4(void)
{
    return 0;
}

/* 0x001a3520 */
static void nj_pad_nop_11(void)
{
}

/* 0x001a3528 */
static int nj_pad_return_zero_5(void)
{
    return 0;
}

/* 0x001a3530 */
static void nj_pad_nop_12(void)
{
}

/* 0x001a3538 */
static void nj_pad_nop_13(void)
{
}

/* 0x001a3540 */
static void nj_pad_nop_14(void)
{
}

/* 0x001a3548 */
static int nj_pad_return_zero_6(void)
{
    return 0;
}

/* 0x001a3550 */
static int nj_pad_return_zero_7(void)
{
    return 0;
}

/* 0x001a3558 */
static int nj_pad_return_zero_8(void)
{
    return 0;
}

/* 0x001a3560 */
static void nj_pad_nop_15(void)
{
}

/* 0x001a3568 */
static void nj_pad_nop_16(void)
{
}

/* 0x001a3570 */
int nj_lcd_get_orientation(void)
{
    return 0;
}

/* 0x001a3578 */
static int nj_lcd_return_zero(void)
{
    return 0;
}

/* 0x001a3580 */
int nj_lcd_write(void)
{
    return 0;
}

/* 0x001a3588 */
int nj_lcd_is_ready(void)
{
    return 1;
}

/* 0x001a3590 */
void nj_pad_init(void)
{
}

/* 0x001a3598 */
static void nj_keyboard_nop(void)
{
}

/* 0x001a35a0 */
void *nj_keyboard_report(void)
{
    return nj_keyboard_report_buf;
}

/* 0x001a35b0 */
int *nj_keyboard_get_status(void)
{
    return &nj_keyboard_status;
}

/* 0x001a35c0 */
static void nj_keyboard_nop_2(void)
{
}

/* 0x001a35c8 */
static int nj_keyboard_return_zero(void)
{
    return 0;
}

/* 0x001a35d0 */
static int nj_keyboard_return_zero_2(void)
{
    return 0;
}

/* 0x001a35d8 */
static void nj_keyboard_nop_3(void)
{
}

/* 0x001a35e0 */
static int nj_keyboard_return_zero_3(void)
{
    return 0;
}

/* 0x001a35e8 */
static void nj_keyboard_nop_4(void)
{
}

/* Truncated to signed 16 bits. */
/* 0x001a35f0 */
short nj_to_short(int x)
{
    return x;
}

/* 0x001a3600 */
static void nj_flush_icache(void)
{
    FlushCache(2);
}

/* 0x001a3608 */
static void nj_flush_dcache(void)
{
    FlushCache(0);
}

/* 0x001a3610 */
static int nj_sound_return_zero(void)
{
    return 0;
}

/* 0x001a3618 */
static int nj_sound_return_zero_2(void)
{
    return 0;
}

/* 0x001a3620 */
int nj_sound_get_info(void)
{
    return 0;
}

/* 0x001a3628 */
int nj_sound_apply(void)
{
    return 0;
}

/* 0x001a3630 */
int nj_sound_set_mono(void)
{
    return 0;
}

/* 0x001a3638 */
static int nj_sound_return_zero_3(void)
{
    return 0;
}

/* 0x001a3640 */
static int nj_sound_return_zero_4(void)
{
    return 0;
}

/* 0x001a3648 */
static int nj_sound_return_zero_5(void)
{
    return 0;
}

/* 0x001a3650 */
static int nj_sound_return_zero_6(void)
{
    return 0;
}

/* 0x001a3658 */
static int nj_sound_return_zero_7(void)
{
    return 0;
}

/* 0x001a3660 */
static int nj_sound_return_zero_8(void)
{
    return 0;
}

/* 0x001a3668 */
static void nj_sound_nop(void)
{
}

/* 0x001a3670 */
static void nj_sound_nop_2(void)
{
}

/* 0x001a3678 */
static void nj_sound_nop_3(void)
{
}

/* 0x001a3680 */
static void nj_sound_nop_4(void)
{
}

/* 0x001a3688 */
static int nj_sound_return_zero_9(void)
{
    return 0;
}

/* 0x001a3690 */
int nj_sound_return_one(void)
{
    return 1;
}

/* 0x001a3698 */
int nj_sound_return_one_2(void)
{
    return 1;
}

/* 0x001a36a0 */
static void nj_sound_nop_5(void)
{
}

/* 0x001a36a8 */
static int nj_sound_return_zero_10(void)
{
    return 0;
}

/* 0x001a36b0 */
static int nj_sound_return_zero_11(void)
{
    return 0;
}

/* 0x001a36b8 */
int nj_video_get_broadcast(void)
{
    return 0;
}

/* 0x001a36c0 */
int nj_video_get_cable(void)
{
    return 3;
}

/* 0x001a36c8 */
int nj_video_return_one(void)
{
    return 1;
}

/* 0x001a36d0 */
static int nj_video_return_zero(void)
{
    return 0;
}

/* 0x001a36d8 */
static void nj_video_nop(void)
{
}

/* 0x001a36e0 */
static void nj_video_nop_2(void)
{
}

/* First two arguments unused. */
/* 0x001a36e8 */
void nj_video_clear_outputs(int ignored_a, int ignored_b, unsigned long *out_a, unsigned long *out_b)
{
    *out_a = 0;
    *out_b = 0;
}

/* 0x001a36f8 */
void nj_fill8(void *dst, int val, int n)
{
    memset(dst, (unsigned char)val, n);
}

/* 0x001a3700 */
void nj_fill16(unsigned short *dst, unsigned short val, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = val;
}

/* 0x001a3730 */
void nj_fill32(unsigned int *dst, unsigned int val, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = val;
}

/* n 8-byte units of 32-bit val. */
/* 0x001a3760 */
void nj_fill64(unsigned int *dst, unsigned int val, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        dst[0] = val;
        dst[1] = val;
        dst += 2;
    }
}

/* n 16-byte units of 32-bit val. */
/* 0x001a3790 */
void nj_fill128(unsigned int *dst, unsigned int val, int n)
{
    unsigned int *inner;

    if (n > 0) {
        inner = dst + 1;
    loop:
        dst[0] = val;
        inner[0] = val;
        inner[1] = val;
        dst[3] = val;
        inner += 4;
        dst += 4;
        if (--n != 0)
            goto loop;
    }
}

/* n 32-byte units of 32-bit val. */
/* 0x001a37c0 */
void nj_fill256(unsigned int *dst, unsigned int val, int n)
{
    unsigned int *inner;

    if (n > 0) {
        inner = dst + 1;
    loop:
        dst[0] = val;
        inner[0] = val;
        inner[1] = val;
        inner[2] = val;
        inner[3] = val;
        inner[4] = val;
        inner[5] = val;
        dst[7] = val;
        inner += 8;
        dst += 8;
        if (--n != 0)
            goto loop;
    }
}

/* 0x001a3800 */
void nj_fill256_thunk(unsigned int *dst, unsigned int val, int n)
{
    nj_fill256(dst, val, n);
}

/* 0x001a3808 */
void nj_copy8(void *dst, const void *src, int n)
{
    memcpy(dst, src, n);
}

/* 0x001a3810 */
void nj_copy16(unsigned short *dst, unsigned short *src, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* 0x001a3840 */
void nj_copy32(unsigned int *dst, unsigned int *src, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* 0x001a3870 */
void nj_copy64(unsigned long *dst, unsigned long *src, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* 0x001a38a0 */
void nj_copy128(NjQword *dst, NjQword *src, int n)
{
    int i;

    for (i = 0; i < n; i++)
        *dst++ = *src++;
}

/* n 32-byte units. */
/* 0x001a38d0 */
void nj_copy256(NjQword *dst, NjQword *src, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        *dst++ = *src++;
        *dst++ = *src++;
    }
}

/* 0x001a3908 */
static void nj_copy256_thunk(NjQword *dst, NjQword *src, int n)
{
    nj_copy256(dst, src, n);
}
