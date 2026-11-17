/*
 * Replay recorder: pad buttons and stick as run-length streams, recorded during a stage and played
 * back into the pad; packed with a header (stage number, config state) into a replay file.
 */

#include "types_game.h"
#include "debug.h"
#include "replay.h"
#include "config.h"
#include "math_util.h"
#include "pad.h"
#include "stage.h"

#define LINE(fmt, v) do { debug_printf(fmt, v); debug_newline(); } while (0)

extern Replay *replay_work;
extern void soft_reset(const char *);
extern char replay_buf[];
extern char replay_ay_buf[];
extern int replay_blink_cnt;
extern unsigned debug_disp_level;
extern char *replay_mode_names[];

/* Idle, suspended, both button-repeat histories cleared. */
/* 0x0015c750 */
void replay_reset(void)
{
    Replay *replay = replay_work;
    char *p;
    char *hist1;
    int i;

    soft_reset("Replay");
    replay->flag = 1;
    hist1 = replay->hist1;
    do { } while (0);
    p = replay->hist0;
    replay->mode = REPLAY_IDLE;
    i = 32;
    do {
        *p++ = 0;
        *hist1++ = 0;
    } while (--i);
}

/* 0x0015c7b8 */
int replay_is_playing(void)
{
    return *(*(int * *)&replay_work) == 2; /* steer(cast) */
}

/* Recording: writes its first entry. */
/* 0x0015c7d0 */
void replay_dig_stream_init(Replay *replay, ReplayStream *stream, unsigned short *buf)
{
    ReplayMode mode = replay->mode;

    stream->idx = 0;
    stream->run = 0;
    stream->ptr = buf;
    if (mode == REPLAY_REC) {
        stream->cnt = stream->idx = mode;
        buf[1] = mode;
        buf[0] = 0;
    }
}

/* Recording: writes its first entry. */
/* 0x0015c800 */
void replay_ana_stream_init(Replay *replay, ReplayStream *stream, signed char *buf)
{
    ReplayMode mode = replay->mode;

    stream->idx = 0;
    stream->run = 0;
    stream->ptr = buf;
    if (mode == REPLAY_REC) {
        buf[0] = 0;
        ((signed char *)stream->ptr)[1] = mode;
        stream->cnt = stream->idx = mode;
    }
}

/* Frame counter reset; digital and both analog streams restarted. */
/* 0x0015c838 */
void replay_streams_init(Replay *replay)
{
    char *buf = replay_buf;

    replay->frame = 0;
    replay_dig_stream_init(replay, &replay->dig, buf);
    replay_ana_stream_init(replay, &replay->ax, buf + 32256);
    replay_ana_stream_init(replay, &replay->ay, replay_ay_buf);
}

/* Also suspends. */
/* 0x0015c898 */
void replay_set_mode(ReplayMode mode)
{
    replay_work->mode = mode;
    replay_work->flag = 1;
}

/* 0x0015c8b0 */
void replay_suspend(void)
{
    (*(int * *)&replay_work)[1] = 1; /* steer(cast) */
}

/* 0x0015c8c8 */
void replay_resume(void)
{
    (*(int * *)&replay_work)[1] = 0; /* steer(cast) */
}

/* Seeds both random generators with fixed values so a replay sees the recorded random numbers. */
/* 0x0015c8d8 */
void ReplayStart(void)
{
    Replay *replay = replay_work;

    DEBUG_PRINT("-----[ ReplayStart ]-----\n");
    replay_streams_init(replay);
    replay->flag = 0;
    rand_lcg_seed_a(0x12345678);
    rand_lcg_seed_b(0x87654321);
}

/* After recording: prints counts and size, fills the header (magic 1234, counts, stage, config). */
/* 0x0015c928 */
void ReplayEnd(void)
{
    Replay *replay = replay_work;
    int *hdr;
    unsigned size;
    int a, b;

    replay->flag = 1;
    DEBUG_PRINT("-----[ ReplayEnd ]-----\n");
    if (replay->mode == REPLAY_REC) {
        hdr = replay->hdr;
        size = replay->dig.cnt * 4 + 28;
        b = replay->ay.cnt;
        a = replay->ax.cnt;
        size += (a + b) * 2;
        DEBUG_PRINTF("data  count  %6d\n", replay->count);
        DEBUG_PRINTF("dig   count  %6d\n", replay->dig.idx);
        DEBUG_PRINTF("ana x count  %6d\n", replay->ax.idx);
        DEBUG_PRINTF("ana y count  %6d\n", replay->ay.idx);
        DEBUG_PRINTF("Replay Total Data Size  %6d\n", size);
        DEBUG_PRINTF("            Block Size  %6d\n", (size + 511) >> 9);
        hdr[0] = 1234;
        hdr[1] = replay->count;
        hdr[2] = replay->dig.cnt;
        hdr[3] = replay->ax.cnt;
        hdr[4] = replay->ay.cnt;
        hdr[5] = stage_get_num();
        config_replay_state_get(&hdr[6]);
        replay->mode = REPLAY_IDLE;
    } else {
        replay->mode = REPLAY_IDLE;
    }
}

/* 0x0015ca40 */
int replay_get_frame(void)
{
    return (*(int * *)&replay_work)[2]; /* steer(cast) */
}

/* Playing, not suspended: next frame's buttons and stick into pad; idle after the last frame. */
/* 0x0015ca50 */
void replay_frame(ReplayPad *pad)
{
    Replay *replay = replay_work;

    if (replay->flag == 0) {
        switch (replay->mode) {
        case REPLAY_IDLE:
        case REPLAY_REC:
            break;
        case REPLAY_PLAY:
            if (replay->frame < replay->count) {
                replay_read_pad(pad);
                replay_read_ana((short *)pad);
                replay->frame++;
            } else {
                replay->mode = REPLAY_IDLE;
            }
            break;
        }
    }
}

/* Blinks the mode name; debug flag 1: frame and stream counts, data size. */
/* 0x0015cad8 */
void replay_debug_disp(void)
{
    int blink;
    int b;
    Replay *replay = replay_work;
    int dig_cnt;
    int size;
    int a;

    if (debug_disp_level == 0 && replay->mode != 0) {
        blink = replay_blink_cnt;
        replay_blink_cnt = blink + 1;
        if ((blink >> 4) & 1) {
            debug_set_origin(-11, 0);
            debug_puts(replay_mode_names[replay->mode]);
            debug_newline();
        }
        if (debug_flags_test(1)) {
            if (debug_disp_level == 0 && replay->mode != 0) {
                debug_set_origin(-11, 1);
                LINE("total%6d", replay->count);
                LINE("count%6d", replay->frame);
                LINE("dig  %6d", replay->dig.idx);
                LINE("a_x  %6d", replay->ax.idx);
                LINE("a_y  %6d", replay->ay.idx);
                do {
                    a = replay->ax.cnt;
                    b = replay->ay.cnt;
                    dig_cnt = replay->dig.cnt;
                    size = dig_cnt * 4 + 28;
                    size += (a + b) * 2;
                    debug_printf("size %6d", size);
                    debug_newline();
                } while (0);
            }
        }
    }
}

/* Entries {bits, repeat count}; updates cur, press, release, prev and the repeat histories. */
/* 0x0015cc40 */
void replay_read_pad(ReplayPad *pad)
{
    Replay *replay = replay_work;
    ReplayStream *stream = &replay->dig;
    unsigned short *p;
    unsigned short *cur;
    int run;
    unsigned bits;
    unsigned old;

    cur = stream->ptr;
    p = cur;
    if (replay->dig.idx < stream->cnt) {
        if (p[1] <= (run = stream->run)) {
            stream->run = 0;
            p += 2;
            stream->ptr = cur + 2;
            replay->dig.idx++;
            run = stream->run;
        }
        bits = p[0];
        stream->run = run + 1;
        old = pad->cur;
        pad->cur = bits;
        pad->release = old & ~bits;
        pad->press = ~old & bits;
        pad->prev = old;
        pad_button_repeat_update(replay->hist0, pad);
        pad_button_repeat_update_alt(replay->hist1, pad);
    }
}

/* Entries {value, repeat count}. */
/* 0x0015cd18 */
void replay_read_ana_stream(ReplayStream *stream, short *out)
{
    int idx = stream->idx;
    unsigned char *cur = stream->ptr;
    unsigned char *p = cur;
    int run;

    if (idx < stream->cnt) {
        if (p[1] <= (run = stream->run)) {
            p += 2;
            stream->ptr = cur + 2;
            stream->idx = idx + 1;
            stream->run = 0;
            run = 0;
        }
        stream->run = run + 1;
        *out = (signed char)p[0];
    }
}

/* x -> out[0], y -> out[1]. */
/* 0x0015cd80 */
void replay_read_ana(short *out)
{
    Replay *replay = replay_work;

    replay_read_ana_stream(&replay->ax, out);
    replay_read_ana_stream(&replay->ay, out + 1);
}

/* 28-byte header, 4 bytes per button entry, 2 per analog entry. */
/* 0x0015cdc8 */
int replay_calc_size(void)
{
    Replay *replay = replay_work;
    int a, b, dig_cnt, size;

    dig_cnt = replay->dig.cnt;
    b = replay->ay.cnt;
    a = replay->ax.cnt;
    size = dig_cnt * 4 + 28;
    size += (a + b) * 2;
    return size;
}

/* Returns the stream data start (2-byte aligned). */
/* 0x0015cdf8 */
char *ReplaySaveHeader(Replay *replay, char *buf)
{
    char *end = (char *)(((unsigned)buf + 29) & 0xfffffffe);

    *(ReplayHdr *)buf = *(ReplayHdr *)replay->hdr;
    return end;
}

/* Unaligned; returns the new dst. */
/* 0x0015ce48 */
ReplayDigEnt *ReplaySaveDigital(Replay *replay, ReplayDigEnt *dst, ReplayDigEnt *src, ReplayStream *stream)
{
    int n = stream->cnt;

    while (--n != -1)
        *dst++ = *src++;
    return dst;
}

/* Returns the new dst. */
/* 0x0015ce90 */
ReplayAnaEnt *ReplaySaveAnalog(Replay *replay, ReplayAnaEnt *dst, ReplayAnaEnt *src, ReplayStream *stream)
{
    int n = stream->cnt;

    while (--n != -1)
        *dst++ = *src++;
    return dst;
}

/* Header, buttons, analog x, analog y; prints sizes; error if not replay_calc_size bytes. */
/* 0x0015ced8 */
void ReplayMakeData(char *buf)
{
    Replay *replay = replay_work;
    char *dst;
    char *base;
    int a;
    int b;
    int dig_cnt;
    int size;

    dst = ReplaySaveHeader(replay, buf);
    base = replay_buf;
    dst = ReplaySaveDigital(replay, dst, base, &replay->dig);
    dst = ReplaySaveAnalog(replay, dst, base + 32256, &replay->ax);
    dst = ReplaySaveAnalog(replay, dst, replay_ay_buf, &replay->ay);
    DEBUG_PRINTF("ReplaySaveHeader%5d\n", 28);
    DEBUG_PRINTF("digital size    %5d\n", replay->dig.cnt << 2);
    DEBUG_PRINTF("analog_x size   %5d\n", replay->ax.cnt << 1);
    DEBUG_PRINTF("analog_y size   %5d\n", replay->ay.cnt << 1);
    dig_cnt = replay->dig.cnt;
    b = replay->ay.cnt;
    a = replay->ax.cnt;
    {
        int dig_size = dig_cnt * 4 + 28;
        size = dig_size + (a + b) * 2;
    }
    if (dst - buf != size)
        ERROR_PRINTF("ReplayMakeSizeError calc %d / make %d\n", size, dst - buf);
}

/*
 * Magic 1234: takes counts, next stage, config state, sets *ok, returns the data start; else
 * clears the frame count and *ok.
 */
/* 0x0015cff8 */
char *ReplayLoadHeader(Replay *replay, int *src, int *ok)
{
    if (src[0] != 1234) {
        *ok = 0;
        replay->count = 0;
        return (char *)src;
    }
    replay->count = src[1];
    replay->dig.cnt = src[2];
    replay->ax.cnt = src[3];
    replay->ay.cnt = src[4];
    stage_next_set(src[5]);
    config_replay_state_set(src + 6);
    *ok = 1;
    return (char *)(((unsigned)src + 29) & 0xfffffffe);
}

/* src unaligned; returns the advanced src. */
/* 0x0015d088 */
ReplayDigEnt *ReplayLoadDigital(Replay *replay, ReplayDigEnt *src, ReplayDigEnt *dst, ReplayStream *stream)
{
    int n = stream->cnt;

    while (--n != -1)
        *dst++ = *src++;
    return src;
}

/* Returns the advanced src. */
/* 0x0015d0d0 */
ReplayAnaEnt *ReplayLoadAnalog(Replay *replay, ReplayAnaEnt *src, ReplayAnaEnt *dst, ReplayStream *stream)
{
    int n = stream->cnt;

    while (--n != -1)
        *dst++ = *src++;
    return src;
}

/* Bad header: prints "the replay data version differs" (in Japanese) and the version word. */
/* 0x0015d118 */
void ReplayLoadData(char *buf)
{
    Replay *replay = replay_work;
    int ok;

    replay_streams_init(replay);
    buf = ReplayLoadHeader(replay, buf, &ok);
    if (ok == 0) {
        ERROR_PRINTF("\203\212\203v\203\214\203C\203f\201[\203^\202\314\203o\201[\203W\203\207\203\223\202\252\210\341\202\244\202\314\202\276\202\237\n");
        DEBUG_PRINTF("version %d\n", *(int *)buf);
    } else {
        char *base = replay_buf;

        buf = ReplayLoadDigital(replay, buf, base, &replay->dig);
        buf = ReplayLoadAnalog(replay, buf, base + 32256, &replay->ax);
        ReplayLoadAnalog(replay, buf, replay_ay_buf, &replay->ay);
    }
}
