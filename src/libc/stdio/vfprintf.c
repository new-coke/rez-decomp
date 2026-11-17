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

/* strlen */
#define BUF (308 + 39 + 1)

#define DEFPREC 6
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
#define FPT 0x100

extern struct _reent_a *_impure_ptr;

/* _impure_ptr */
extern int locale_mb_cur_max;

/*
 * Flushes a pending uio vector through __sfvwrite and resets it; returns its result, 0 when
 * nothing is pending.
 */
/* 0x001f02d0 */
int __sprint(void *fp, struct __suio_a *uio)
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
 * Formats an unbuffered stream through a fake FILE with a 1024-byte stack buffer: vfprintf, then
 * fflush; copies the __SERR flag (0x40) back to the real stream.
 */
/* 0x001f0318 */
int __sbprintf(FILE_a *fp, const char *fmt, void *ap)
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
    ret = vfprintf(&fake, fmt, ap);
    if (ret >= 0 && fflush(&fake))
        ret = -1;
    if (fake._flags & 0x40)
        fp->_flags |= 0x40;
    return (ret);
}

/*
 * Vfprintf: binds the stream to _impure_ptr when it has no reent, runs __sinit if the reent's
 * __sdidinit flag (offset 56) is clear, then formats through the core _vfprintf_r(reent, fp, fmt,
 * ap).
 */
/* 0x001f03d0 */
int vfprintf(FILE_a *fp, const char *fmt, void *ap)
{
    do {
        if (fp->_data == NULL)
            fp->_data = _impure_ptr;
        if (!((struct _reent_init_a *)fp->_data)->__sdidinit)
            __sinit(fp->_data);
    } while (0);
    return _vfprintf_r(fp->_data, fp, fmt, ap);
}

/*
 * Formatted output: as the integer-only core (_vfiprintf_r) plus %e %E %f %g %G: Inf/NaN via isinf
 * / isnan, digits from cvt, exponent text from exponent, the locale decimal point from localeconv.
 * Flushes through __sprint; unbuffered streams use __sbprintf.
 */
/* steer(cast) */
/* steer(cast) */
/* 0x001f0448 */
int _vfprintf_r(struct _reent_a *data, FILE_a *fp, const char *fmt0, va_list ap)
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
    char *decimal_point = *((char **(*)(void))localeconv)();
    char softsign;
    double _double;
    int expt;
    int expsize;
    int ndig;
    char expstr[7];
    unsigned long _uquad;
    enum { OCT, DEC, HEX } base;
    int dprec;
    int realsz;
    int size;
    char *xdigs;
#define NIOV 8
    struct __suio_a uio;
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
            if (__sprint(fp, &uio)) \
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
        if (uio.uio_resid && __sprint(fp, &uio)) \
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
        return (__sbprintf(fp, fmt0, ap));

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
        case 'L':
            flags |= LONGDBL;
            goto rflag;
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
        case 'e':
        case 'E':
        case 'f':
        case 'g':
        case 'G':
            if (prec == -1) {
                prec = DEFPREC;
            } else if ((ch == 'g' || ch == 'G') && prec == 0) {
                prec = 1;
            }

            if (flags & LONGDBL) {
                _double = (double)va_arg(ap, long double);
            } else {
                _double = va_arg(ap, double);
            }

            if (isinf(_double)) {
                if (_double < 0)
                    sign = '-';
                cp = "Inf";
                size = 3;
                break;
            }
            if (isnan(_double)) {
                cp = "NaN";
                size = 3;
                break;
            }

            flags |= FPT;
            cp = cvt(data, _double, prec, flags, &softsign,
                              &expt, ch, &ndig);
            if (ch == 'g' || ch == 'G') {
                if (expt <= -4 || expt > prec)
                    ch = (ch == 'g') ? 'e' : 'E';
                else
                    ch = 'g';
            }
            if (ch <= 'e') {
                --expt;
                expsize = exponent(expstr, expt, ch);
                size = expsize + ndig;
                if (ndig > 1 || flags & ALT)
                    ++size;
            } else if (ch == 'f') {
                if (expt > 0) {
                    size = expt;
                    if (prec || flags & ALT)
                        size += prec + 1;
                } else
                    size = prec + 2;
            } else if (expt >= ndig) {
                size = expt;
                if (flags & ALT)
                    ++size;
            } else
                size = ndig + (expt > 0 ?
                               1 : 2 - expt);

            if (softsign)
                sign = '-';
            break;
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

        if ((flags & FPT) == 0) {
            PRINT(cp, size);
        } else {
            if (ch >= 'f') {
                if (_double == 0) {
                    PRINT("0", 1);
                    if (expt < ndig || (flags & ALT) != 0) {
                        PRINT(decimal_point, 1);
                        PAD(ndig - 1, zeroes);
                    }
                } else if (expt <= 0) {
                    PRINT("0", 1);
                    PRINT(decimal_point, 1);
                    PAD(-expt, zeroes);
                    PRINT(cp, ndig);
                } else if (expt >= ndig) {
                    PRINT(cp, ndig);
                    PAD(expt - ndig, zeroes);
                    if (flags & ALT)
                        PRINT(".", 1);
                } else {
                    PRINT(cp, expt);
                    cp += expt;
                    PRINT(".", 1);
                    PRINT(cp, ndig - expt);
                }
            } else {
                if (ndig > 1 || flags & ALT) {
                    ox[0] = *cp++;
                    ox[1] = '.';
                    PRINT(ox, 2);
                    if (_double || flags & ALT == 0) {
                        PRINT(cp, ndig - 1);
                    } else
                        PAD(ndig - 1, zeroes);
                } else
                    PRINT(cp, 1);
                PRINT(expstr, expsize);
            }
        }

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

/*
 * Cvt: converts a double for %e/%f/%g through _dtoa_r (mode 3 for f, 2 otherwise, one more digit
 * for e/E), sets the sign character from the sign bit, pads trailing zeros unless %g without ALT,
 * stores the digit count in *length and returns the digits.
 */
/* 0x001f1960 */
char *cvt(struct _reent_a *data, double value, int ndigits, int flags, char *sign, int *decpt, int ch, int *length)
{
    int mode, dsgn;
    char *digits, *bp, *rve;
    union double_union_a tmp;

    if (ch == 'f') {
        mode = 3;
    } else {
        if (ch == 'e' || ch == 'E') {
            ndigits++;
        }
        mode = 2;
    }
    tmp.d = value;
    if (tmp.i[1] & 0x80000000) {
        value = -value;
        *sign = '-';
    } else
        *sign = '\000';
    digits = _dtoa_r(data, value, mode, ndigits, decpt, &dsgn, &rve);
    if ((ch != 'g' && ch != 'G') || flags & 1) {
        bp = digits + ndigits;
        if (ch == 'f') {
            if (*digits == '0' && value)
                *decpt = -ndigits + 1;
            bp += *decpt;
        }
        if (value == 0)
            rve = bp;
        while (rve < bp)
            *rve++ = '0';
    }
    *length = rve - digits;
    return (digits);
}

/*
 * Exponent: writes the exponent part of a %e/%g conversion (fmtch, sign, at least two digits via a
 * 308-byte stack buffer) and returns its length.
 */
/* 0x001f1b10 */
int exponent(char *p0, int exp, int fmtch)
{
    char *p, *t;
    char expbuf[308];

    p = p0;
    *p++ = fmtch;
    if (exp < 0) {
        exp = -exp;
        *p++ = '-';
    } else
        *p++ = '+';
    t = expbuf + 308;
    if (exp > 9) {
        do {
            *--t = exp % 10 + '0';
        } while ((exp /= 10) > 9);
        *--t = exp + '0';
        for (; t < expbuf + 308; *p++ = *t++)
            ;
    } else {
        *p++ = '0';
        *p++ = exp + '0';
    }
    return (p - p0);
}
