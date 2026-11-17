#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

struct _reent_mp_a {
    char pad_00[76];
    Bigint_a **_freelist;
};

#define STOREINC(a, b, c) (((unsigned short *)a)[1] = (unsigned short)(b), \
                           ((unsigned short *)a)[0] = (unsigned short)(c), a++)

extern int pow5mult_p05[];
extern double mprec_tens[];

/*
 * Balloc: takes a Bigint of 1<<k words from the reent free list (reent+76, created with _calloc_r
 * _calloc_r(ptr, 4, 16)) or allocates a new one; clears sign and length.
 */
/* 0x001f4220 */
Bigint_a *_Balloc(struct _reent_a *ptr, int k)
{
    int x;
    Bigint_a *rv;
    struct _reent_mp_a *p = (struct _reent_mp_a *)ptr;

    if (p->_freelist == NULL) {
        p->_freelist = (Bigint_a **)_calloc_r(ptr, sizeof(Bigint_a *), 16);
        if (p->_freelist == NULL)
            return NULL;
    }
    if ((rv = p->_freelist[k]) != 0) {
        p->_freelist[k] = rv->_next;
    } else {
        x = 1 << k;
        rv = (Bigint_a *)_calloc_r(ptr, 1, sizeof(Bigint_a) + (x - 1) * sizeof(int));
        if (rv == NULL)
            return NULL;
        rv->_k = k;
        rv->_maxwds = x;
    }
    rv->_sign = rv->_wds = 0;
    return rv;
}

/* Bfree: pushes a Bigint back on the reent free list (reent+76) indexed by its _k. */
/* 0x001f42c8 */
void _Bfree(struct _reent_a *ptr, Bigint_a *v)
{
    if (v) {
        v->_next = ((struct _reent_mp_a *)ptr)->_freelist[v->_k];
        ((struct _reent_mp_a *)ptr)->_freelist[v->_k] = v;
    }
}

/*
 * Multadd: b = b * m + a on a Bigint in 16-bit halves; grows it through Balloc/memcpy/Bfree
 * (_Balloc/memcpy/_Bfree) when the carry needs a new word.
 */
/* 0x001f42f8 */
Bigint_a *__multadd(struct _reent_a *ptr, Bigint_a *b, int m, int a)
{
    int i, wds;
    unsigned int *x, y;
    unsigned int xi, z;
    Bigint_a *b1;

    wds = b->_wds;
    x = b->_x;
    i = 0;
    do {
        xi = *x;
        y = (xi & 0xffff) * m + a;
        z = (xi >> 16) * m + (y >> 16);
        a = (int)(z >> 16);
        *x++ = (z << 16) + (y & 0xffff);
    } while (++i < wds);
    if (a) {
        if (wds >= b->_maxwds) {
            b1 = _Balloc(ptr, b->_k + 1);
            memcpy((char *)&b1->_sign, (char *)&b->_sign, b->_wds * sizeof(int) + 2 * sizeof(int));
            _Bfree(ptr, b);
            b = b1;
        }
        b->_x[wds++] = a;
        b->_wds = wds;
    }
    return b;
}

/*
 * S2b: builds a Bigint from a decimal digit string (first nine digits already in y9) with Balloc
 * (_Balloc) and multadd(b, 10, digit) (__multadd), skipping the decimal point after nd0 digits.
 */
/* 0x001f4410 */
Bigint_a *__s2b(struct _reent_a *ptr, const char *s, int nd0, int nd, unsigned int y9)
{
    Bigint_a *b;
    int i, k;
    int x, y;

    x = (nd + 8) / 9;
    for (k = 0, y = 1; x > y; y <<= 1, k++)
        ;
    b = _Balloc(ptr, k);
    b->_x[0] = y9;
    b->_wds = 1;
    i = 9;
    if (9 < nd0) {
        s += 9;
        do
            b = __multadd(ptr, b, 10, *s++ - '0');
        while (++i < nd0);
        s++;
    } else
        s += 10;
    for (; i < nd; i++)
        b = __multadd(ptr, b, 10, *s++ - '0');
    return b;
}

/* Hi0bits: number of leading zero bits of a 32-bit word (32 for 0). */
/* 0x001f4558 */
int __hi0bits(unsigned int x)
{
    int k = 0;

    if (!(x & 0xffff0000)) {
        k = 16;
        x <<= 16;
    }
    if (!(x & 0xff000000)) {
        k += 8;
        x <<= 8;
    }
    if (!(x & 0xf0000000)) {
        k += 4;
        x <<= 4;
    }
    if (!(x & 0xc0000000)) {
        k += 2;
        x <<= 2;
    }
    if (!(x & 0x80000000)) {
        k++;
        if (!(x & 0x40000000))
            return 32;
    }
    return k;
}

/* Lo0bits: shifts *y right past its trailing zero bits and returns their count (32 for 0). */
/* 0x001f45e0 */
int __lo0bits(unsigned int *y)
{
    int k;
    unsigned int x = *y;

    if (x & 7) {
        if (x & 1)
            return 0;
        if (x & 2) {
            *y = x >> 1;
            return 1;
        }
        *y = x >> 2;
        return 2;
    }
    k = 0;
    if (!(x & 0xffff)) {
        k = 16;
        x >>= 16;
    }
    if (!(x & 0xff)) {
        k += 8;
        x >>= 8;
    }
    if (!(x & 0xf)) {
        k += 4;
        x >>= 4;
    }
    if (!(x & 0x3)) {
        k += 2;
        x >>= 2;
    }
    if (!(x & 1)) {
        k++;
        x >>= 1;
        if (!x & 1)
            return 32;
    }
    *y = x;
    return k;
}

/* Makes a one-word Big holding the integer i (allocates size class 1 through _Balloc). */
/* 0x001f46a0 */
Big *__i2b(RtCtx *ptr, int i)
{
    Big *b;

    b = _Balloc(ptr, 1);
    b->x[0] = i;
    b->wds = 1;
    return b;
}

/* Multiplies two Bigs (16-bit half-word schoolbook product) into a new Big from _Balloc. */
/* 0x001f46d8 */
Big *__multiply(RtCtx *ptr, Big *a, Big *b)
{
    Big *c;
    int k, wa, wb, wc;
    unsigned int carry, y, z;
    unsigned int *x, *xa, *xae, *xb, *xbe, *xc, *xc0;
    unsigned int z2;

    if (a->wds < b->wds) {
        c = a;
        a = b;
        b = c;
    }
    k = a->k;
    wa = a->wds;
    wb = b->wds;
    wc = wa + wb;
    if (wc > a->maxwds)
        k++;
    c = _Balloc(ptr, k);
    for (x = c->x, xa = x + wc; x < xa; x++)
        *x = 0;
    xa = a->x;
    xae = xa + wa;
    xb = b->x;
    xbe = xb + wb;
    xc0 = c->x;
    for (; xb < xbe; xb++, xc0++) {
        if ((y = *xb & 0xffff) != 0) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = (*x & 0xffff) * y + (*xc & 0xffff) + carry;
                carry = z >> 16;
                z2 = (*x++ >> 16) * y + (*xc >> 16) + carry;
                carry = z2 >> 16;
                STOREINC(xc, z2, z);
            } while (x < xae);
            *xc = carry;
        }
        if ((y = *xb >> 16) != 0) {
            x = xa;
            xc = xc0;
            carry = 0;
            z2 = *xc;
            do {
                z = (*x & 0xffff) * y + (*xc >> 16) + carry;
                carry = z >> 16;
                STOREINC(xc, z, z2);
                z2 = (*x++ >> 16) * y + (*xc & 0xffff) + carry;
                carry = z2 >> 16;
            } while (x < xae);
            *xc = z2;
        }
    }
    for (xc0 = c->x, xc = xc0 + wc; wc > 0 && !*--xc; --wc)
        ;
    c->wds = wc;
    return c;
}

/*
 * Multiplies b by 5^k, using the small-factor table pow5mult_p05 (5, 25, 125) and the cached chain
 * of 625^(2^n) at ptr->p5s.
 */
/* 0x001f4908 */
Big *__pow5mult(RtCtx *ptr, Big *b, int k)
{
    Big *b1, *p5, *p51;
    int i;

    if ((i = k & 3) != 0)
        b = __multadd(ptr, b, pow5mult_p05[i - 1], 0);
    if (!(k >>= 2))
        return b;
    if (!(p5 = ptr->p5s)) {
        p5 = ptr->p5s = __i2b(ptr, 625);
        p5->next = 0;
    }
    for (;;) {
        if (k & 1) {
            b1 = __multiply(ptr, b, p5);
            _Bfree(ptr, b);
            b = b1;
        }
        if (!(k >>= 1))
            break;
        if (!(p51 = p5->next)) {
            p51 = p5->next = __multiply(ptr, p5, p5);
            p51->next = 0;
        }
        p5 = p51;
    }
    return b;
}

/* Shifts a Big left by k bits into a new Big and frees the old one (_Bfree). */
/* 0x001f4a08 */
Big *__lshift(RtCtx *ptr, Big *b, int k)
{
    int i, k1, n, n1;
    Big *b1;
    unsigned int *x, *x1, *xe, z;

    n = k >> 5;
    k1 = b->k;
    n1 = n + b->wds + 1;
    for (i = b->maxwds; n1 > i; i <<= 1)
        k1++;
    b1 = _Balloc(ptr, k1);
    x1 = b1->x;
    for (i = 0; i < n; i++)
        *x1++ = 0;
    x = b->x;
    xe = x + b->wds;
    if (k &= 0x1f) {
        k1 = 32 - k;
        z = 0;
        do {
            *x1++ = *x << k | z;
            z = *x++ >> k1;
        } while (x < xe);
        if ((*x1 = z) != 0)
            ++n1;
    } else
        do
            *x1++ = *x++;
        while (x < xe);
    b1->wds = n1 - 1;
    _Bfree(ptr, b);
    return b1;
}

/*
 * Compares two Bigs: returns the word-count difference, else -1/0/1 from the most significant
 * differing word.
 */
/* 0x001f4b80 */
int __mcmp(Big *a, Big *b)
{
    unsigned int *xa, *xa0, *xb, *xb0;
    int i, j;

    i = a->wds;
    j = b->wds;
    if (i -= j)
        return i;
    xa0 = a->x;
    xa = xa0 + j;
    xb0 = b->x;
    xb = xb0 + j;
    for (;;) {
        if (*--xa != *--xb)
            return *xa < *xb ? -1 : 1;
        if (xa <= xa0)
            break;
    }
    return 0;
}

/* Returns |a - b| as a new Big with the sign flag set when a < b (compares with __mcmp). */
/* 0x001f4be8 */
Big *__mdiff(RtCtx *ptr, Big *a, Big *b)
{
    Big *c;
    int i, wa, wb;
    int borrow, y;
    unsigned int *xa, *xae, *xb, *xbe, *xc;
    int z;

    i = __mcmp(a, b);
    if (!i) {
        c = _Balloc(ptr, 0);
        c->wds = 1;
        c->x[0] = 0;
        return c;
    }
    if (i < 0) {
        c = a;
        a = b;
        b = c;
        i = 1;
    } else
        i = 0;
    c = _Balloc(ptr, a->k);
    c->sign = i;
    wa = a->wds;
    xa = a->x;
    xae = xa + wa;
    wb = b->wds;
    xb = b->x;
    xbe = xb + wb;
    xc = c->x;
    borrow = 0;
    do {
        y = (*xa & 0xffff) - (*xb & 0xffff) + borrow;
        borrow = y >> 16;
        z = (*xa++ >> 16) - (*xb++ >> 16) + borrow;
        borrow = z >> 16;
        STOREINC(xc, z, y);
    } while (xb < xbe);
    while (xa < xae) {
        y = (*xa & 0xffff) + borrow;
        borrow = y >> 16;
        z = (*xa++ >> 16) + borrow;
        borrow = z >> 16;
        STOREINC(xc, z, y);
    }
    while (!*--xc)
        wa--;
    c->wds = wa;
    return c;
}

/* Returns the value of one unit in the last place of x (handles denormal results). */
/* 0x001f4d78 */
double __ulp(double dx)
{
    DWords x, a;
    int L;

    x.d = dx;
    L = (DW_HI(x) & 0x7ff00000) - (53 - 1) * 0x100000;
    if (L > 0) {
        DW_HI(a) = L;
        DW_LO(a) = 0;
    } else {
        L = -L >> 20;
        if (L < 20) {
            DW_HI(a) = 0x80000 >> L;
            DW_LO(a) = 0;
        } else {
            DW_HI(a) = 0;
            L -= 20;
            DW_LO(a) = L >= 31 ? 1 : 1 << (31 - L);
        }
    }
    return a.d;
}

/*
 * Converts the top bits of a Big to a double in [1, 2) and stores the binary exponent adjustment
 * in *e (__hi0bits counts leading zero bits).
 */
/* 0x001f4e10 */
double __b2d(Big *a, int *e)
{
    unsigned int *xa, *xa0, w, y, z;
    int k;
    DWords d;

    xa0 = a->x;
    xa = xa0 + a->wds;
    y = *--xa;
    k = __hi0bits(y);
    *e = 32 - k;
    if (k < 11) {
        DW_HI(d) = 0x3ff00000 | y >> (11 - k);
        w = xa > xa0 ? *--xa : 0;
        DW_LO(d) = y << (32 - 11 + k) | w >> (11 - k);
        goto ret_d;
    }
    z = xa > xa0 ? *--xa : 0;
    if (k -= 11) {
        DW_HI(d) = 0x3ff00000 | y << k | z >> (32 - k);
        y = xa > xa0 ? *--xa : 0;
        DW_LO(d) = z << k | y >> (32 - k);
    } else {
        DW_HI(d) = 0x3ff00000 | y;
        DW_LO(d) = z;
    }
ret_d:
    return d.d;
}

/*
 * Converts a double to a Big mantissa, returning the binary exponent in *e and the significant bit
 * count in *bits (__lo0bits strips trailing zero bits, __hi0bits counts leading ones).
 */
/* 0x001f4f90 */
Big *__d2b(RtCtx *ptr, double dd, int *e, int *bits)
{
    DWords d;
    Big *b;
    int de, i, k;
    unsigned int *x, y, z;

    d.d = dd;
    b = _Balloc(ptr, 1);
    x = b->x;
    z = DW_HI(d) & 0xfffff;
    DW_HI(d) &= 0x7fffffff;
    if ((de = (int)(DW_HI(d) >> 20)) != 0)
        z |= 0x100000;
    if ((y = DW_LO(d)) != 0) {
        if ((k = __lo0bits(&y)) != 0) {
            x[0] = y | z << (32 - k);
            z >>= k;
        } else
            x[0] = y;
        i = b->wds = (x[1] = z) ? 2 : 1;
    } else {
        k = __lo0bits(&z);
        x[0] = z;
        i = b->wds = 1;
        k += 32;
    }
    if (de) {
        *e = de - 1023 - (53 - 1) + k;
        *bits = 53 - k;
    } else {
        *e = de - 1023 - (53 - 1) + 1 + k;
        *bits = 32 * i - __hi0bits(x[i - 1]);
    }
    return b;
}

/*
 * Returns a / b as a double (both converted with __b2d, exponents rebalanced before the division).
 */
/* 0x001f5110 */
double __ratio(Big *a, Big *b)
{
    DWords da, db;
    int k, ka, kb;

    da.d = __b2d(a, &ka);
    db.d = __b2d(b, &kb);
    k = ka - kb + 32 * (a->wds - b->wds);
    if (k > 0)
        DW_HI(da) += k * 0x100000;
    else {
        k = -k;
        DW_HI(db) += k * 0x100000;
    }
    return da.d / db.d;
}

/*
 * Returns 10^dig: from the power-of-ten table mprec_tens below 24, otherwise by repeated
 * multiplication.
 */
/* 0x001f51d0 */
double _mprec_log10(int dig)
{
    double v = 1.0;

    if (dig < 24)
        return mprec_tens[dig];
    while (dig > 0) {
        v *= 10;
        dig--;
    }
    return v;
}
