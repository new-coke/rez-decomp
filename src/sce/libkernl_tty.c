/* Debug console, EE side: DECI2 TTY channel (C stdin/stdout), serial port, small library printf. */

#include "types_libc.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libkernl_tty.h"
#include "kernel_syscalls.h"
#include "libkernl_deci2.h"
#include "libkernl_intr.h"

/* Always zero. */
#define AZ(p) ((((int)(p)) & 1) >> 1)

#define ARG8(type) (ap += 8, *(type *)(ap - 8))

extern ByteRing libkernl_tty_ring;
extern TtyCtl libkernl_tty_ctl;
extern unsigned char libkernl_tty_sbuf[];

extern unsigned char libkernl_tty_rbuf[];
extern int libkernl_line_len;
extern char libkernl_line_buf[128];
extern void (*libkernl_putc_hook)(int c);
extern double __fixunsdfdi(double x);
extern long __moddi3(long a, long b);
extern long __divdi3(long a, long b);
extern unsigned long __umoddi3(unsigned long a, unsigned long b);
extern unsigned long __udivdi3(unsigned long a, unsigned long b);

/* 0x001f6ec8 */
static ByteRing *libkernl_tty_ring_init(int size)
{
    ByteRing *ring = &libkernl_tty_ring;

    ring->size = size;
    ring->count = 0;
    ring->wp = ring->buf;
    ring->rp = ring->buf;
    return ring;
}

/* One byte stored at the write pointer; wraps. */
/* 0x001f6ef0 */
static void libkernl_tty_ring_put(ByteRing *ring)
{
    ring->count++;
    if (++ring->wp == ring->buf + ring->size)
        ring->wp = ring->buf;
}

/* One byte taken at the read pointer; wraps. */
/* 0x001f6f30 */
static void libkernl_tty_ring_get(ByteRing *ring)
{
    ring->count--;
    if (++ring->rp == ring->buf + ring->size)
        ring->rp = ring->buf;
}

/*
 * Read: receives param bytes, or with param 0 (packet complete) moves the characters into the
 * input ring. Send-ready sends the next part; send-done ends the write.
 */
/* 0x001f6f70 */
static void libkernl_tty_handler(int event, int param, TtyCtl *tty)
{
    int i;
    TtyPkt *pkt;

    switch (event) {
    case 1:
    case 2:
        if (param) {
            if ((unsigned int)(tty->rlen + param) > 320)
                libkernl_printf("TTY: packet size larger than expect\n");
            i = sceDeci2ExRecv(tty->sock, tty->rbuf + tty->rlen, param);
            if (i < 0)
                libkernl_printf("TTY: receive error");
            tty->rlen += i;
        } else {
            pkt = (TtyPkt *)tty->rbuf;
            for (i = 12; i < pkt->len; i++) {
                *tty->ring->wp = tty->rbuf[i];
                libkernl_tty_ring_put(tty->ring);
            }
            tty->rlen = 0;
        }
        break;
    case 3: {
        int n;

        n = sceDeci2ExSend(tty->sock, tty->wptr, tty->wlen);
        if (n < 0) {
            libkernl_printf("TTY: send err %d\n", n);
            tty->busy = 0;
        } else {
            tty->wptr += n;
            tty->wlen -= n;
        }
        break;
    }
    case 4:
        if (tty->wlen)
            libkernl_printf("TTY: err ti->wlen=%08x\n", tty->wlen);
        tty->busy = 0;
        break;
    }
}

/* Up to 256 bytes ('\\n' -> "\\r\\n"), polled with interrupts off. -1 if busy or the send fails. */
/* steer(cast) */
/* 0x001f7108 */
int libkernl_tty_write(const char *buf, int n)
{
    int i = 0;
    int cnt = 0;
    unsigned char *sbuf;
    unsigned char *dst;

    if (libkernl_tty_ctl.busy)
        return -1;
    DIntr();
    sbuf = (unsigned char *)((unsigned int)libkernl_tty_sbuf | 0x20000000);
    libkernl_tty_ctl.busy = 1;
    libkernl_tty_ctl.wptr = sbuf;
    dst = sbuf + 12;
    while (n--) {
        if (*buf == '\n') {
            *dst++ = '\r';
            if (++i >= 256)
                break;
        }
        *dst++ = *buf++;
        cnt++;
        if (++i >= 256)
            break;
    }
    libkernl_tty_ctl.wlen = i + 12;
    ((TtyPkt *)sbuf)->len = libkernl_tty_ctl.wlen;
    if (sceDeci2ReqSend(libkernl_tty_ctl.sock, ((TtyPkt *)sbuf)->dest) < 0) {
        libkernl_tty_ctl.busy = 0;
        EIntr();
        return -1;
    }
    while (libkernl_tty_ctl.busy)
        ((void (*)(int))sceDeci2Poll)(libkernl_tty_ctl.sock);
    EIntr();
    return cnt;
}

/* Waits per character; stops after '\\n' or '\\r' (kept). Returns the count. */
/* 0x001f7258 */
int libkernl_tty_read(char *buf, int n)
{
    int i;
    char *p;

    for (i = 0; i < n; i++) {
        p = buf + i;
        while (((volatile ByteRing *)libkernl_tty_ctl.ring)->count == 0)
            ;
        *p = *libkernl_tty_ctl.ring->rp;
        libkernl_tty_ring_get(libkernl_tty_ctl.ring);
        if (*p == '\n' || *p == '\r')
            return i + 1;
    }
    return i;
}

/* DECI2 TTY socket, uncached buffers, 'E' -> 'H' header, 256-byte input ring. 0 if no socket. */
/* 0x001f7328 */
int libkernl_tty_open(void)
{
    TtyCtl *tty = &libkernl_tty_ctl;
    TtyPkt *pkt;

    FlushCache(0);
    libkernl_tty_ctl.sock = sceDeci2Open(0x210, tty, libkernl_tty_handler);
    if (libkernl_tty_ctl.sock < 0)
        return 0;
    tty->busy = 0;
    tty->wlen = 0;
    tty->rlen = 0;
    tty->rbuf = (unsigned char *)((unsigned int)libkernl_tty_rbuf | 0x20000000);
    pkt = (TtyPkt *)((unsigned int)libkernl_tty_sbuf | 0x20000000);
    tty->wptr = (unsigned char *)pkt;
    pkt->proto = 0x210;
    pkt->src = 'E';
    pkt->dest = 'H';
    pkt->rsvd = 0;
    pkt->unk_08 = 0;
    tty->ring = libkernl_tty_ring_init(256);
    return 1;
}

/* Waits for transmit room. */
/* 0x001f73e8 */
static int libkernl_sio_putc(int c)
{
    while (*(volatile unsigned int *)0x1000f130 & 0x8000)
        ;
    *(volatile unsigned char *)0x1000f180 = c;
    return c;
}

/* Collects in libkernl_line_buf; prints at '\\n' or 126 pending characters. */
/* 0x001f7420 */
static void libkernl_line_putc(int c)
{
    int i;

    if (libkernl_line_len >= 126) {
        libkernl_line_len = 0;
        libkernl_line_buf[127] = 0;
        libkernl_dbg_puts(libkernl_line_buf);
    }
    if (c == '\n') {
        i = libkernl_line_len;
        libkernl_line_len = 0;
        libkernl_line_buf[i] = c;
        libkernl_line_buf[i + 1] = 0;
        libkernl_dbg_puts(libkernl_line_buf);
    } else
        libkernl_line_buf[libkernl_line_len++] = c;
}

/* '\\n' -> "\\r\\n". */
/* 0x001f74d0 */
static void libkernl_sio_putc_crlf(int c)
{
    if (c == '\n') {
        libkernl_sio_putc('\r');
        libkernl_sio_putc('\n');
    } else
        libkernl_sio_putc(c);
}

/* [0, 8192), two guard bits rounding; below 2^-53 -> 0, 2^13 and up -> 9999. */
/* 0x001f7508 */
static int libkernl_double_to_int(double val)
{
    union {
        double d;
        long l;
    } bits;
    long shift;
    unsigned long mant;

    bits.d = val;
    mant = bits.l;
    shift = mant << 1 >> 53;
    shift -= 1075;
    if (shift < -53)
        return 0;
    if (shift >= 13)
        return 9999;
    mant = mant << 12 >> 12;
    mant |= 1L << 52;
    if (shift < 0) {
        shift = -shift;
        mant >>= (int)(shift - 2);
        if ((mant & 3) == 3)
            mant = (mant >> 2) + 1;
        else
            mant >>= 2;
    } else
        mant <<= (int)shift;
    return mant;
}

/* 0.dddddd e+N/e-N: scales into [0.1, 1) counting powers of ten. */
/* 0x001f7598 */
static void libkernl_print_double(double x)
{
    int exp10 = 0;
    int digits;

    if (x < 0.0) {
        x = 0.0 - x;
        libkernl_putc_hook('-');
    }
    if (x < 0.1) {
        while (x < 0.1) {
            x *= 10.0;
            exp10--;
        }
    } else if (x >= 1.0) {
        while (x >= 1.0) {
            x /= 10.0;
            exp10++;
        }
    }
    digits = libkernl_double_to_int(__fixunsdfdi(x * 1000000.0));
    libkernl_printf("0.%d", digits);
    if (exp10 >= 0)
        libkernl_printf("e+%d", exp10);
    else
        libkernl_printf("e%d", exp10);
}

/*
 * Via libkernl_putc_hook: %d %u %o %x (h/l, zero-padded width 0N/0NN up to 31), %c, %s ("" ->
 * "(null)"), %e/%f of a float; others skipped. Every argument takes an 8-byte slot.
 */
/* 0x001f7700 */
static void libkernl_vprintf(const char *fmt, va_list ap)
{
    char buf[32];
    char *s;
    char *str;
    char *pad;
    const char *spec;
    long sval;
    unsigned long uval;
    int lmod;
    int width;
    int digit2;
    unsigned long digit;
    float fval;

    while (*fmt) {
        pad = 0;
        lmod = 0;
        if (*fmt == '%') {
        again:
            spec = fmt + 1;
        next:
            fmt = spec;
            switch ((char)(*fmt - '0')) {
            case '0' - '0':
                width = spec[1] - '0';
                digit2 = spec[2];
                if ((unsigned char)width < 10) {
                    if ((unsigned int)(digit2 - '0') < 10) {
                        {
                            int k = width * 10 - '0';
                            width = k + digit2;
                        }
                        fmt = spec + 2;
                        if (width >= 32)
                            width = 31;
                    } else {
                        fmt = spec + 1;
                    }
                    {
                        char *end = buf + 31;
                        pad = end - width;
                    }
                    if (width <= 0)
                        goto again;
                    spec = fmt + 1;
                fill:
                    buf[31 - width] = '0';
                    width--;
                    if (width > 0)
                        goto fill;
                    goto next;
                }
                goto inc;
            case 'l' - '0':
                lmod = 'l';
            inc:
                spec = spec + 1;
                goto next;
            case 'h' - '0':
                lmod = 'h';
                goto inc;
            case 'o' - '0':
                if (lmod == 'l')
                    uval = ARG8(unsigned long);
                else if (lmod == 'h')
                    uval = ARG8(unsigned short);
                else
                    uval = ARG8(unsigned int);
                s = &buf[31];
                buf[31] = 0;
                if (uval == 0) {
                    *--s = '0';
                    spec++;
                } else {
                    spec++;
                    do {
                        *--s = (uval & 7) + '0';
                        uval >>= 3;
                    } while (uval);
                }
                if (pad && pad < s)
                    s = pad;
                while (*s)
                    libkernl_putc_hook(*s++);
                fmt = spec; continue;
            case 'x' - '0':
                if (lmod == 'l')
                    uval = ARG8(unsigned long);
                else if (lmod == 'h')
                    uval = ARG8(unsigned short);
                else
                    uval = ARG8(unsigned int);
                s = &buf[31];
                buf[31] = 0;
                if (uval == 0) {
                    *--s = '0';
                    spec++;
                } else {
                    spec++;
                    do {
                        digit = uval & 15;
                        if (digit >= 10)
                            *--s = digit + 'a' - 10;
                        else
                            *--s = digit + '0';
                        uval >>= 4;
                    } while (uval);
                }
                if (pad && pad < s)
                    s = pad;
                while (*s)
                    libkernl_putc_hook(*s++);
                fmt = spec; continue;
            case 'd' - '0':
                if (lmod == 'l')
                    sval = ARG8(long);
                else if (lmod == 'h')
                    sval = ARG8(short);
                else
                    sval = ARG8(int);
                s = &buf[31];
                buf[31] = 0;
                if (sval == 0) {
                    *--s = '0';
                    spec++;
                } else {
                    if (sval < 0) {
                        sval = -sval;
                        libkernl_putc_hook('-');
                    }
                    while (sval) {
                        *--s = __moddi3(sval, 10) + '0';
                        sval = __divdi3(sval, 10);
                    }
                    spec++;
                }
                if (pad && pad < s)
                    s = pad;
                while (*s)
                    libkernl_putc_hook(*s++);
                fmt = spec; continue;
            case 'u' - '0':
                if (lmod == 'l')
                    uval = ARG8(unsigned long);
                else if (lmod == 'h')
                    uval = ARG8(unsigned short);
                else
                    uval = ARG8(unsigned int);
                s = &buf[31];
                buf[31] = 0;
                if (uval == 0) {
                    *--s = '0';
                    spec++;
                } else {
                    spec++;
                    do {
                        *--s = __umoddi3(uval, 10) + '0';
                        uval = __udivdi3(uval, 10);
                    } while (uval);
                }
                if (pad && pad < s)
                    s = pad;
                while (*s)
                    libkernl_putc_hook(*s++);
                fmt = spec; continue;
            case 'e' - '0':
            case 'f' - '0':
                fval = ARG8(float);
                if (fval == 0.0f) {
                    libkernl_putc_hook('0');
                    spec++;
                } else {
                    spec++;
                    libkernl_print_double(fval);
                }
                fmt = spec; continue;
            case 's' - '0':
                str = ARG8(char *);
                if (!*str) {
                    libkernl_putc_hook('(');
                    spec++;
                    libkernl_putc_hook('n');
                    libkernl_putc_hook('u');
                    libkernl_putc_hook('l');
                    libkernl_putc_hook('l');
                    libkernl_putc_hook(')');
                } else {
                    s = str;
                    spec++;
                    do
                        libkernl_putc_hook(*s++);
                    while (*s);
                }
                fmt = spec; continue;
            case 'c' - '0':
                sval = ARG8(char);
                libkernl_putc_hook(sval);
                spec++;
                goto step;
            case '1' - '0' ... 'b' - '0':
            case 'g' - '0':
            case 'i' - '0' ... 'k' - '0':
            case 'm' - '0':
            case 'n' - '0':
            case 'p' - '0' ... 'r' - '0':
            case 't' - '0':
            case 'v' - '0':
            case 'w' - '0':
                goto skip;
            }
        } else {
            libkernl_putc_hook(*fmt);
            spec = fmt + 1;
            goto step;
        }
        spec = fmt + 1;
        goto step;
    skip:
        spec++;
    step:
        fmt = spec;
        fmt += AZ(fmt); /* steer(zero) */
    }
}

/* Through the current character hook. */
/* 0x001f7cc8 */
int libkernl_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    return ((int (*)(const char *, va_list))libkernl_vprintf)(fmt, ap); /* steer(cast) */
}

/* Development tool console, a line at a time. */
/* 0x001f7d00 */
int libkernl_printf_line(const char *fmt, ...)
{
    va_list ap;
    void (*old)(int);
    int ret;

    old = libkernl_putc_hook;
    libkernl_putc_hook = libkernl_line_putc;
    va_start(ap, fmt);
    ret = ((int (*)(const char *, va_list))libkernl_vprintf)(fmt, ap); /* steer(cast) */
    libkernl_putc_hook = old;
    return ret;
}
