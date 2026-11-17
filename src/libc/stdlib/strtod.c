#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

#define word0(x) ((x).i[1])
#define word1(x) ((x).i[0])
#define Exp_msk1 0x100000
#define Exp_mask 0x7ff00000
#define P 53
#define Bias 1023
#define Emin (-1022)
#define Ten_pmax 22
#define Bndry_mask 0xfffff
#define Bndry_mask1 0xfffff
#define LSB 1
#define Log2P 1
#define Tiny0 0
#define Tiny1 1
#define Big0 0x7fefffff
#define Big1 0xffffffff
#define n_bigtens 5
#define DBL_DIG 15
#define DBL_MAX_10_EXP 308
#define DBL_MAX_EXP 1024
#define FLT_RADIX 2
#define FLT_ROUNDS 1
#define rounded_product(a, b) a *= b
#define rounded_quotient(a, b) a /= b
#define Bcopy(x, y) memcpy((char *)&x->_sign, (char *)&y->_sign, y->_wds * sizeof(int) + 2 * sizeof(int))

/* HUGE_VAL */
#define tens mprec_tens

#define bigtens mprec_bigtens
#define tinytens mprec_tinytens

extern const double mprec_tens[];

/* tens */
extern const double mprec_bigtens[];

/* bigtens */
extern const double mprec_tinytens[];

/* tinytens */
extern const double expf_huge_val;

extern struct _reent_a *_impure_ptr;

/*
 * David Gay's correctly rounded decimal-to-double conversion on soft double. Stores the end pointer
 * in *se. A sign with nothing after it jumps to the "case 0" exit (goto ret0, as in Gay's
 * original).
 */
/* 0x001ee568 */
double _strtod_r(struct _reent_a *ptr, const char *s00, char **se)
{
    int bb2, bb5, bbe, bd2, bd5, bbbits, bs2, c, dsign, e1, esign, i, j,
        k, nd, nd0, nf, nz, nz0, sign;
    long e;
    const char *s, *s0, *s1;
    double aadj, aadj1, adj;
    long L;
    unsigned int y;
    unsigned long z;
    union double_union_a rv, rv0;
    Bigint_a *bb, *bb1, *bd, *bd0, *bs, *delta;

    sign = nz0 = nz = 0;
    rv.d = 0.;
    for (s = s00;; s++)
        switch (*s) {
        case '-':
            sign = 1;
            /* no break */
        case '+':
            if (*++s == '\0')
                goto ret0;
            /* no break */
        default:
            goto break2;
        case 0:
ret0:
            s = s00;
            goto ret;
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
        case ' ':
            continue;
        }
break2:
    if (*s == '0') {
        nz0 = 1;
        while (*++s == '0')
            ;
        if (!*s)
            goto ret;
    }
    s0 = s;
    y = z = 0;
    for (nd = nf = 0; (c = *s) >= '0' && c <= '9'; nd++, s++)
        if (nd < 9)
            y = 10 * y + c - '0';
        else if (nd < 16)
            z = 10 * z + c - '0';
    nd0 = nd;
    if (c == '.') {
        c = *++s;
        if (!nd) {
            for (; c == '0'; c = *++s)
                nz++;
            if (c > '0' && c <= '9') {
                s0 = s;
                nf += nz;
                nz = 0;
                goto have_dig;
            }
            goto dig_done;
        }
        for (; c >= '0' && c <= '9'; c = *++s) {
have_dig:
            nz++;
            if (c -= '0') {
                nf += nz;
                for (i = 1; i < nz; i++)
                    if (nd++ < 9)
                        y *= 10;
                    else if (nd <= DBL_DIG + 1)
                        z *= 10;
                if (nd++ < 9)
                    y = 10 * y + c;
                else if (nd <= DBL_DIG + 1)
                    z = 10 * z + c;
                nz = 0;
            }
        }
    }
dig_done:
    e = 0;
    if (c == 'e' || c == 'E') {
        if (!nd && !nz && !nz0) {
            s = s00;
            goto ret;
        }
        s00 = s;
        esign = 0;
        switch (c = *++s) {
        case '-':
            esign = 1;
        case '+':
            c = *++s;
        }
        if (c >= '0' && c <= '9') {
            while (c == '0')
                c = *++s;
            if (c > '0' && c <= '9') {
                e = c - '0';
                s1 = s;
                while ((c = *++s) >= '0' && c <= '9')
                    e = 10 * e + c - '0';
                if (s - s1 > 8)
                    e = 9999999L;
                if (esign)
                    e = -e;
            } else
                e = 0;
        } else
            s = s00;
    }
    if (!nd) {
        if (!nz && !nz0)
            s = s00;
        goto ret;
    }
    e1 = e -= nf;

    if (!nd0)
        nd0 = nd;
    k = nd < DBL_DIG + 1 ? nd : DBL_DIG + 1;
    rv.d = y;
    if (k > 9)
        rv.d = tens[k - 9] * rv.d + z;
    bd0 = 0;
    if (nd <= DBL_DIG && FLT_ROUNDS == 1) {
        if (!e)
            goto ret;
        if (e > 0) {
            if (e <= Ten_pmax) {
                rounded_product(rv.d, tens[e]);
                goto ret;
            }
            i = DBL_DIG - nd;
            if (e <= Ten_pmax + i) {
                e -= i;
                rv.d *= tens[i];
                rounded_product(rv.d, tens[e]);
                goto ret;
            }
        } else if (e >= -Ten_pmax) {
            rounded_quotient(rv.d, tens[-e]);
            goto ret;
        }
    }
    e1 += nd - k;

    if (e1 > 0) {
        if ((i = e1 & 15) != 0)
            rv.d *= tens[i];
        if (e1 &= ~15) {
            if (e1 > DBL_MAX_10_EXP) {
ovfl:
                *(int *)ptr = 34;
                rv.d = expf_huge_val;
                if (bd0)
                    goto retfree;
                goto ret;
            }
            if (e1 >>= 4) {
                for (j = 0; e1 > 1; j++, e1 >>= 1)
                    if (e1 & 1)
                        rv.d *= bigtens[j];
                word0(rv) -= P * Exp_msk1;
                rv.d *= bigtens[j];
                if ((z = word0(rv) & Exp_mask)
                    > Exp_msk1 * (DBL_MAX_EXP + Bias - P))
                    goto ovfl;
                if (z > Exp_msk1 * (DBL_MAX_EXP + Bias - 1 - P)) {
                    word0(rv) = Big0;
                    word1(rv) = Big1;
                } else
                    word0(rv) += P * Exp_msk1;
            }
        }
    } else if (e1 < 0) {
        e1 = -e1;
        if ((i = e1 & 15) != 0)
            rv.d /= tens[i];
        if (e1 &= ~15) {
            e1 >>= 4;
            if (e1 >= 1 << n_bigtens)
                goto undfl;
            for (j = 0; e1 > 1; j++, e1 >>= 1)
                if (e1 & 1)
                    rv.d *= tinytens[j];
            rv0.d = rv.d;
            rv.d *= tinytens[j];
            if (!rv.d) {
                rv.d = 2. * rv0.d;
                rv.d *= tinytens[j];
                if (!rv.d) {
undfl:
                    rv.d = 0.;
                    *(int *)ptr = 34;
                    if (bd0)
                        goto retfree;
                    goto ret;
                }
                word0(rv) = Tiny0;
                word1(rv) = Tiny1;
            }
        }
    }

    bd0 = __s2b(ptr, s0, nd0, nd, y);

    for (;;) {
        bd = _Balloc(ptr, bd0->_k);
        Bcopy(bd, bd0);
        bb = __d2b(ptr, rv.d, &bbe, &bbbits);
        bs = __i2b(ptr, 1);

        if (e >= 0) {
            bb2 = bb5 = 0;
            bd2 = bd5 = e;
        } else {
            bb2 = bb5 = -e;
            bd2 = bd5 = 0;
        }
        if (bbe >= 0)
            bb2 += bbe;
        else
            bd2 -= bbe;
        bs2 = bb2;
        i = bbe + bbbits - 1;
        if (i < Emin)
            j = bbe + (P - Emin);
        else
            j = P + 1 - bbbits;
        bb2 += j;
        bd2 += j;
        i = bb2 < bd2 ? bb2 : bd2;
        if (i > bs2)
            i = bs2;
        if (i > 0) {
            bb2 -= i;
            bd2 -= i;
            bs2 -= i;
        }
        if (bb5 > 0) {
            bs = __pow5mult(ptr, bs, bb5);
            bb1 = __multiply(ptr, bs, bb);
            _Bfree(ptr, bb);
            bb = bb1;
        }
        if (bb2 > 0)
            bb = __lshift(ptr, bb, bb2);
        if (bd5 > 0)
            bd = __pow5mult(ptr, bd, bd5);
        if (bd2 > 0)
            bd = __lshift(ptr, bd, bd2);
        if (bs2 > 0)
            bs = __lshift(ptr, bs, bs2);
        delta = __mdiff(ptr, bb, bd);
        dsign = delta->_sign;
        delta->_sign = 0;
        i = __mcmp(delta, bs);
        if (i < 0) {
            if (dsign || word1(rv) || word0(rv) & Bndry_mask)
                break;
            delta = __lshift(ptr, delta, Log2P);
            if (__mcmp(delta, bs) > 0)
                goto drop_down;
            break;
        }
        if (i == 0) {
            if (dsign) {
                if ((word0(rv) & Bndry_mask1) == Bndry_mask1
                    && word1(rv) == 0xffffffff) {
                    word0(rv) = (word0(rv) & Exp_mask) + Exp_msk1;
                    word1(rv) = 0;
                    break;
                }
            } else if (!(word0(rv) & Bndry_mask) && !word1(rv)) {
drop_down:
                L = (word0(rv) & Exp_mask) - Exp_msk1;
                word0(rv) = L | Bndry_mask1;
                word1(rv) = 0xffffffff;
                break;
            }
            if (!(word1(rv) & LSB))
                break;
            if (dsign)
                rv.d += __ulp(rv.d);
            else {
                rv.d -= __ulp(rv.d);
                if (!rv.d)
                    goto undfl;
            }
            break;
        }
        if ((aadj = __ratio(delta, bs)) <= 2.) {
            if (dsign)
                aadj = aadj1 = 1.;
            else if (word1(rv) || word0(rv) & Bndry_mask) {
                if (word1(rv) == Tiny1 && !word0(rv))
                    goto undfl;
                aadj = 1.;
                aadj1 = -1.;
            } else {
                if (aadj < 2. / FLT_RADIX)
                    aadj = 1. / FLT_RADIX;
                else
                    aadj *= 0.5;
                aadj1 = -aadj;
            }
        } else {
            aadj *= 0.5;
            aadj1 = dsign ? aadj : -aadj;
            if (FLT_ROUNDS == 0)
                aadj1 += 0.5;
        }
        y = word0(rv) & Exp_mask;

        if (y == Exp_msk1 * (DBL_MAX_EXP + Bias - 1)) {
            rv0.d = rv.d;
            word0(rv) -= P * Exp_msk1;
            adj = aadj1 * __ulp(rv.d);
            rv.d += adj;
            if ((word0(rv) & Exp_mask) >=
                Exp_msk1 * (DBL_MAX_EXP + Bias - P)) {
                if (word0(rv0) == Big0 && word1(rv0) == Big1)
                    goto ovfl;
                word0(rv) = Big0;
                word1(rv) = Big1;
                goto cont;
            } else
                word0(rv) += P * Exp_msk1;
        } else {
            if (y <= (P - 1) * Exp_msk1 && aadj >= 1.) {
                aadj1 = (double)(int)(aadj + 0.5);
                if (!dsign)
                    aadj1 = -aadj1;
            }
            adj = aadj1 * __ulp(rv.d);
            rv.d += adj;
        }
        z = word0(rv) & Exp_mask;
        if (y == z) {
            L = aadj;
            aadj -= L;
            if (dsign || word1(rv) || word0(rv) & Bndry_mask) {
                if (aadj < .4999999 || aadj > .5000001)
                    break;
            } else if (aadj < .4999999 / FLT_RADIX)
                break;
        }
cont:
        _Bfree(ptr, bb);
        _Bfree(ptr, bd);
        _Bfree(ptr, bs);
        _Bfree(ptr, delta);
    }
retfree:
    _Bfree(ptr, bb);
    _Bfree(ptr, bd);
    _Bfree(ptr, bs);
    _Bfree(ptr, bd0);
    _Bfree(ptr, delta);
ret:
    if (se)
        *se = (char *)s;
    return sign ? -rv.d : rv.d;
}

/* Strtod: forwards to the reentrant _strtod_r with the current reentrancy block _impure_ptr. */
/* 0x001ef468 */
double strtod(const char *s, char **ptr)
{
    return _strtod_r(_impure_ptr, s, ptr);
}

/* Strtof-style wrapper: strtod converted to float. */
/* 0x001ef498 */
float libc_strtod_float(const char *s, char **ptr)
{
    return strtod(s, ptr);
}
