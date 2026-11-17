#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include <stdarg.h>
#include "libc.h"
#include "libc_internal.h"
#include "libkernl_glue.h"

#ifndef va_arg      /* the shared stdarg.h has no va_arg: EE EABI, 8-byte argument slots, little endian */
#define __va_rounded_size_a(T) (((sizeof(T) + __va_reg_size - 1) / __va_reg_size) * __va_reg_size)
#define va_arg(AP, T) ((AP) += __va_rounded_size_a(T), *(T *)(void *)((AP) - __va_rounded_size_a(T)))
#endif

struct Vfiprintf__suio_a {
    struct __siov_a *uio_iov;
    int uio_iovcnt;
    int uio_resid;
};

/* strlen */
#define BUF 40

#define to_digit(c) ((c) - '0')
#define is_digit(c) ((unsigned)to_digit(c) <= 9)
#define to_char(n) ((n) + '0')
#define ALT 0x001
#define HEXPREFIX 0x002
#define LADJUST 0x004
#define LONGDBL 0x008
#define LONGINT 0x010
#define QUADINT 0x020
#define SHORTINT 0x040
#define ZEROPAD 0x080

extern struct _reent_a *_impure_ptr;

/* _impure_ptr */
extern int locale_mb_cur_max;

/*
 * Flushes a pending uio vector through __sfvwrite and resets it; returns its result, 0 when
 * nothing is pending.
 */
/* 0x001ef588 */
int libc_vfiprintf_sprint(void *fp, struct __suio_a *uio)
{
    int err;

    if (uio->uio_resid == 0) {
        uio->uio_iovcnt = 0;
        return (0);
    }
    err = __sfvwrite(fp, uio);
    uio->uio_resid = 0;
    uio->uio_iovcnt = 0;
    return (err);
}

/*
 * Formats an unbuffered stream through a fake FILE with a 1024-byte stack buffer: vfiprintf, then
 * fflush; copies the __SERR flag (0x40) back to the real stream.
 */
/* 0x001ef5d0 */
int libc_vfiprintf_sbprintf(FILE_a *fp, const char *fmt, void *ap)
{
    int ret;
    FILE_a fake;
    unsigned char buf[1024];

    fake._data = fp->_data;
    fake._flags = fp->_flags & ~2;
    fake._file = fp->_file;
    fake._cookie = fp->_cookie;
    fake._write = fp->_write;
    fake._bf._base = fake._p = buf;
    fake._bf._size = fake._w = sizeof(buf);
    fake._lbfsize = 0;
    ret = vfiprintf(&fake, fmt, ap);
    if (ret >= 0 && fflush(&fake))
        ret = -1;
    if (fake._flags & 0x40)
        fp->_flags |= 0x40;
    return (ret);
}

/*
 * Vfiprintf: binds the stream to _impure_ptr when it has no reent, runs __sinit if the reent's
 * __sdidinit flag (offset 56) is clear, then formats through the core _vfiprintf_r(reent, fp, fmt,
 * ap).
 */
/* 0x001ef688 */
int vfiprintf(FILE_a *fp, const char *fmt, void *ap)
{
    do {
        if (fp->_data == NULL)
            fp->_data = _impure_ptr;
        if (!((struct _reent_init_a *)fp->_data)->__sdidinit)
            __sinit(fp->_data);
    } while (0);
    return _vfiprintf_r(fp->_data, fp, fmt, ap);
}

/*
 * Integer-only formatted output: scans the format with _mbtowc_r ( MB_CUR_MAX locale_mb_cur_max),
 * parses flags, width, precision and h/l/ll, converts %c %d %i %n %o %p %s %u %x %X (64-bit
 * unsigned long arithmetic through the libgcc 64-bit divide helpers), pads with blanks/zeroes and
 * writes through an 8-entry uio vector flushed by __sprint (libc_vfiprintf_sprint). Unbuffered
 * streams go through __sbprintf (libc_vfiprintf_sbprintf); streams that cannot be written fail via
 * __swsetup. Returns the character count or -1 on error (__SERR).
 */
/* steer(cast) */
/* 0x001ef700 */
int _vfiprintf_r(struct _reent_a *data, FILE_a *fp, const char *fmt0, va_list ap)
{
    char *fmt;
    int ch;
    int n, m;
    char *cp;
    struct __siov_a *iovp;
    int flags;
    int ret;
    int width;
    int prec;
    char sign;
    int wc;
    unsigned long _uquad;
    enum { OCT, DEC, HEX } base;
    int dprec;
    int realsz;
    int size;
    char *xdigs;
#define NIOV 8
    struct Vfiprintf__suio_a uio;
    struct __siov_a iov[NIOV];
    char buf[BUF];
    char ox[2];
    int state = 0;

#define PADSIZE 16
    static const char blanks[PADSIZE] =
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    static const char zeroes[PADSIZE] =
        {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};

#define PRINT(ptr, len) { \
        iovp->iov_base = (ptr); \
        iovp->iov_len = (len); \
        uio.uio_resid += (len); \
        iovp++; \
        if (++uio.uio_iovcnt >= NIOV) { \
            if (libc_vfiprintf_sprint(fp, &uio)) \
                goto error; \
            iovp = iov; \
        } \
    }
#define PAD(howmany, with) { \
        if ((n = (howmany)) > 0) { \
            while (n > PADSIZE) { \
                PRINT(with, PADSIZE); \
                n -= PADSIZE; \
            } \
            PRINT(with, n); \
        } \
    }
#define FLUSH() { \
        if (uio.uio_resid && libc_vfiprintf_sprint(fp, &uio)) \
            goto error; \
        uio.uio_iovcnt = 0; \
        iovp = iov; \
    }

#define SARG() \
    (flags & LONGINT ? va_arg(ap, long) : \
     flags & SHORTINT ? (long)(short)va_arg(ap, int) : \
     (long)va_arg(ap, int))
#define UARG() \
    (flags & LONGINT ? va_arg(ap, unsigned long) : \
     flags & SHORTINT ? (unsigned long)(unsigned short)va_arg(ap, int) : \
     (unsigned long)va_arg(ap, unsigned int))

    if (((fp->_flags & 8) == 0 || fp->_bf._base == NULL) && __swsetup(fp))
        return (-1);

    if ((fp->_flags & (2 | 8 | 0x10)) == (2 | 8) &&
        fp->_file >= 0)
        return (libc_vfiprintf_sbprintf(fp, fmt0, ap));

    fmt = (char *)fmt0;
    uio.uio_iov = iovp = iov;
    uio.uio_resid = 0;
    uio.uio_iovcnt = 0;
    ret = 0;

    for (;;) {
        cp = fmt;
        while ((n = ((int (*)(struct _reent_a *, int *, const char *, unsigned int, int *))_mbtowc_r)(_impure_ptr, &wc, fmt, locale_mb_cur_max, &state)) > 0) {
            fmt += n;
            if (wc == '%') {
                fmt--;
                break;
            }
        }
        if ((m = fmt - cp) != 0) {
            PRINT(cp, m);
            ret += m;
        }
        if (n <= 0)
            goto done;
        fmt++;

        flags = 0;
        dprec = 0;
        width = 0;
        prec = -1;
        sign = '\0';

rflag:
        ch = *fmt++;
reswitch:
        switch (ch) {
        case ' ':
            if (!sign)
                sign = ' ';
            goto rflag;
        case '#':
            flags |= ALT;
            goto rflag;
        case '*':
            if ((width = va_arg(ap, int)) >= 0)
                goto rflag;
            width = -width;
            /* FALLTHROUGH */
        case '-':
            flags |= LADJUST;
            goto rflag;
        case '+':
            sign = '+';
            goto rflag;
        case '.':
            if ((ch = *fmt++) == '*') {
                n = va_arg(ap, int);
                prec = n < 0 ? -1 : n;
                goto rflag;
            }
            n = 0;
            while (is_digit(ch)) {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            }
            prec = n < 0 ? -1 : n;
            goto reswitch;
        case '0':
            flags |= ZEROPAD;
            goto rflag;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            n = 0;
            do {
                n = 10 * n + to_digit(ch);
                ch = *fmt++;
            } while (is_digit(ch));
            width = n;
            goto reswitch;
        case 'h':
            flags |= SHORTINT;
            goto rflag;
        case 'l':
            if (*fmt == 'l') {
                fmt++;
                flags |= QUADINT;
            } else {
                flags |= LONGINT;
            }
            goto rflag;
        case 'q':
            flags |= QUADINT;
            goto rflag;
        case 'c':
            *(cp = buf) = va_arg(ap, int);
            size = 1;
            sign = '\0';
            break;
        case 'D':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'd':
        case 'i':
            _uquad = SARG();
            if ((long)_uquad < 0) {
                _uquad = -_uquad;
                sign = '-';
            }
            base = DEC;
            goto number;
        case 'n':
            if (flags & LONGINT)
                *va_arg(ap, long *) = ret;
            else if (flags & SHORTINT)
                *va_arg(ap, short *) = ret;
            else
                *va_arg(ap, int *) = ret;
            continue;
        case 'O':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'o':
            _uquad = UARG();
            base = OCT;
            goto nosign;
        case 'p':
            _uquad = (unsigned long)va_arg(ap, void *);
            base = HEX;
            xdigs = "0123456789abcdef";
            flags |= HEXPREFIX;
            ch = 'x';
            goto nosign;
        case 's':
            if ((cp = va_arg(ap, char *)) == NULL)
                cp = "(null)";
            if (prec >= 0) {
                char *p = memchr(cp, 0, prec);

                if (p != NULL) {
                    size = p - cp;
                    if (size > prec)
                        size = prec;
                } else
                    size = prec;
            } else
                size = strlen(cp);
            sign = '\0';
            break;
        case 'U':
            flags |= LONGINT;
            /*FALLTHROUGH*/
        case 'u':
            _uquad = UARG();
            base = DEC;
            goto nosign;
        case 'X':
            xdigs = "0123456789ABCDEF";
            goto hex;
        case 'x':
            xdigs = "0123456789abcdef";
hex:
            _uquad = UARG();
            base = HEX;
            if (flags & ALT && _uquad != 0)
                flags |= HEXPREFIX;
nosign:
            sign = '\0';
number:
            if ((dprec = prec) >= 0)
                flags &= ~ZEROPAD;
            cp = buf + BUF;
            if (_uquad != 0 || prec != 0) {
                switch (base) {
                case OCT:
                    do {
                        *--cp = to_char(_uquad & 7);
                        _uquad >>= 3;
                    } while (_uquad);
                    if (flags & ALT && *cp != '0')
                        *--cp = '0';
                    break;
                case DEC:
                    while (_uquad >= 10) {
                        *--cp = to_char(_uquad % 10);
                        _uquad /= 10;
                    }
                    *--cp = to_char(_uquad);
                    break;
                case HEX:
                    do {
                        *--cp = xdigs[_uquad & 15];
                        _uquad >>= 4;
                    } while (_uquad);
                    break;
                default:
                    cp = "bug in vfprintf: bad base";
                    size = strlen(cp);
                    goto skipsize;
                }
            }
            size = buf + BUF - cp;
skipsize:
            break;
        default:
            if (ch == '\0')
                goto done;
            cp = buf;
            *cp = ch;
            size = 1;
            sign = '\0';
            break;
        }

        realsz = dprec > size ? dprec : size;
        if (sign)
            realsz++;
        else if (flags & HEXPREFIX)
            realsz += 2;

        if ((flags & (LADJUST | ZEROPAD)) == 0)
            PAD(width - realsz, blanks);

        if (sign) {
            PRINT(&sign, 1);
        } else if (flags & HEXPREFIX) {
            ox[0] = '0';
            ox[1] = ch;
            PRINT(ox, 2);
        }

        if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD)
            PAD(width - realsz, zeroes);

        PAD(dprec - size, zeroes);

        PRINT(cp, size);

        if (flags & LADJUST)
            PAD(width - realsz, blanks);

        ret += width > realsz ? width : realsz;

        FLUSH();
    }
done:
    FLUSH();
error:
    return ((fp->_flags & 0x40) ? -1 : ret);
}
