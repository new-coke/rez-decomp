#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

#define Storeinc_a(a, b, c) (((unsigned short *)a)[1] = (unsigned short)b, \
                             ((unsigned short *)a)[0] = (unsigned short)c, a++)

typedef struct Dtoa_Bigint_a {
    struct Dtoa_Bigint_a *_next;
    int _k, _maxwds, _sign, _wds;
    unsigned int _x[1];
} Dtoa_Bigint_a;

struct _reent_dtoa_a {
    char pad_00[64];
    Dtoa_Bigint_a *_result;
    int _result_k;
};

#define word0(x) ((x).i[1])
#define word1(x) ((x).i[0])
#define Exp_shift1 20
#define Exp_msk1 0x100000
#define Exp_mask 0x7ff00000
#define P 53
#define Bias 1023
#define Exp_11 0x3ff00000
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Bletch 0x10
#define Bndry_mask 0xfffff
#define Log2P 1
#define Sign_bit 0x80000000
#define Quick_max 14
#define Int_max 14
#define n_bigtens 5
#define Bcopy(x, y) memcpy((char *)&x->_sign, (char *)&y->_sign, y->_wds * sizeof(int) + 2 * sizeof(int))

/* bigtens */
#define tens mprec_tens

#define bigtens mprec_bigtens

extern const double mprec_tens[];

/* tens */
extern const double mprec_bigtens[];

/*
 * Quorem: one quotient digit of b / S for _dtoa_r: subtracts q * S from b in 16-bit halves, fixes
 * q up by one after cmp (__mcmp), trims leading zero words; returns q.
 */
/* 0x001f1d78 */
int quorem(Bigint_a *b, Bigint_a *S)
{
    int n;
    int borrow, y;
    unsigned int carry, q, ys;
    unsigned int *bx, *bxe, *sx, *sxe;
    int z;
    unsigned int si, zs;

    n = S->_wds;
    if (b->_wds < n)
        return 0;
    sx = S->_x;
    sxe = sx + --n;
    bx = b->_x;
    bxe = bx + n;
    q = *bxe / (*sxe + 1);
    if (q) {
        borrow = 0;
        carry = 0;
        do {
            si = *sx++;
            ys = (si & 0xffff) * q + carry;
            zs = (si >> 16) * q + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) + borrow;
            borrow = y >> 16;
            z = (*bx >> 16) - (zs & 0xffff) + borrow;
            borrow = z >> 16;
            Storeinc_a(bx, z, y);
        } while (sx <= sxe);
        if (!*bxe) {
            bx = b->_x;
            while (--bxe > bx && !*bxe)
                --n;
            b->_wds = n;
        }
    }
    if (__mcmp(b, S) >= 0) {
        q++;
        borrow = 0;
        carry = 0;
        bx = b->_x;
        sx = S->_x;
        do {
            si = *sx++;
            ys = (si & 0xffff) + carry;
            zs = (si >> 16) + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) + borrow;
            borrow = y >> 16;
            z = (*bx >> 16) - (zs & 0xffff) + borrow;
            borrow = z >> 16;
            Storeinc_a(bx, z, y);
        } while (sx <= sxe);
        bx = b->_x;
        bxe = bx + n;
        if (!*bxe) {
            while (--bxe > bx && !*bxe)
                --n;
            b->_wds = n;
        }
    }
    return q;
}

/* David Gay's double-to-decimal conversion on soft double. */
/* 0x001f1f90 */
char *_dtoa_r(struct _reent_a *ptr, double _d, int mode, int ndigits, int *decpt, int *sign, char **rve)
{
    int bbits, b2, b5, be, dig, i, ieps, ilim, ilim0, ilim1, j, j1, k, k0,
        k_check, leftright, m2, m5, s2, s5, spec_case, try_quick;
    union double_union_a d, d2, eps;
    int L;
    int denorm;
    unsigned int x;
    Dtoa_Bigint_a *b, *b1, *delta, *mlo, *mhi, *S;
    double ds;
    char *s, *s0;
    struct _reent_dtoa_a *p = (struct _reent_dtoa_a *)ptr;

    d.d = _d;

    if (p->_result) {
        p->_result->_k = p->_result_k;
        p->_result->_maxwds = 1 << p->_result_k;
        _Bfree(ptr, p->_result);
        p->_result = 0;
    }

    if (word0(d) & Sign_bit) {
        *sign = 1;
        word0(d) &= ~Sign_bit;
    } else
        *sign = 0;

    if ((word0(d) & Exp_mask) == Exp_mask) {
        *decpt = 9999;
        s = !word1(d) && !(word0(d) & 0xfffff) ? "Infinity" : "NaN";
        if (rve)
            *rve = s[3] ? s + 8 : s + 3;
        return s;
    }
    if (!d.d) {
        *decpt = 1;
        s = "0";
        if (rve)
            *rve = s + 1;
        return s;
    }

    b = __d2b(ptr, d.d, &be, &bbits);
    if ((i = (int)(word0(d) >> Exp_shift1 & (Exp_mask >> Exp_shift1))) != 0) {
        d2.d = d.d;
        word0(d2) &= Frac_mask1;
        word0(d2) |= Exp_11;
        i -= Bias;
        denorm = 0;
    } else {
        i = bbits + be + (Bias + (P - 1) - 1);
        x = (i > 32) ? (word0(d) << (64 - i)) | (word1(d) >> (i - 32))
                     : (word1(d) << (32 - i));
        d2.d = x;
        word0(d2) -= 31 * Exp_msk1;
        i -= (Bias + (P - 1) - 1) + 1;
        denorm = 1;
    }
    ds = (d2.d - 1.5) * 0.289529654602168 + 0.1760912590558 + i * 0.301029995663981;
    k = (int)ds;
    if (ds < 0. && ds != k)
        k--;
    k_check = 1;
    if (k >= 0 && k <= Ten_pmax) {
        if (d.d < tens[k])
            k--;
        k_check = 0;
    }
    j = bbits - i - 1;
    if (j >= 0) {
        b2 = 0;
        s2 = j;
    } else {
        b2 = -j;
        s2 = 0;
    }
    if (k >= 0) {
        b5 = 0;
        s5 = k;
        s2 += k;
    } else {
        b2 -= k;
        b5 = -k;
        s5 = 0;
    }
    if (mode < 0 || mode > 9)
        mode = 0;
    try_quick = 1;
    if (mode > 5) {
        mode -= 4;
        try_quick = 0;
    }
    leftright = 1;
    switch (mode) {
    case 0:
    case 1:
        ilim = ilim1 = -1;
        i = 18;
        ndigits = 0;
        break;
    case 2:
        leftright = 0;
        /* no break */
    case 4:
        if (ndigits <= 0)
            ndigits = 1;
        ilim = ilim1 = i = ndigits;
        break;
    case 3:
        leftright = 0;
        /* no break */
    case 5:
        i = ndigits + k + 1;
        ilim = i;
        ilim1 = i - 1;
        if (i <= 0)
            i = 1;
    }
    j = sizeof(unsigned int);
    for (p->_result_k = 0; sizeof(Dtoa_Bigint_a) - sizeof(unsigned int) + j <= i;
         j <<= 1)
        p->_result_k++;
    p->_result = _Balloc(ptr, p->_result_k);
    s = s0 = (char *)p->_result;

    if (ilim >= 0 && ilim <= Quick_max && try_quick) {
        i = 0;
        d2.d = d.d;
        k0 = k;
        ilim0 = ilim;
        ieps = 2;
        if (k > 0) {
            ds = tens[k & 0xf];
            j = k >> 4;
            if (j & Bletch) {
                j &= Bletch - 1;
                d.d /= bigtens[n_bigtens - 1];
                ieps++;
            }
            for (; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    ds *= bigtens[i];
                }
            d.d /= ds;
        } else if ((j1 = -k) != 0) {
            d.d *= tens[j1 & 0xf];
            for (j = j1 >> 4; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    d.d *= bigtens[i];
                }
        }
        if (k_check && d.d < 1. && ilim > 0) {
            if (ilim1 <= 0)
                goto fast_failed;
            ilim = ilim1;
            k--;
            d.d *= 10.;
            ieps++;
        }
        eps.d = ieps * d.d + 7.;
        word0(eps) -= (P - 1) * Exp_msk1;
        if (ilim == 0) {
            S = mhi = 0;
            d.d -= 5.;
            if (d.d > eps.d)
                goto one_digit;
            if (d.d < -eps.d)
                goto no_digits;
            goto fast_failed;
        }
        if (leftright) {
            eps.d = 0.5 / tens[ilim - 1] - eps.d;
            for (i = 0;;) {
                L = d.d;
                d.d -= L;
                *s++ = '0' + (int)L;
                if (d.d < eps.d)
                    goto ret1;
                if (1. - d.d < eps.d)
                    goto bump_up;
                if (++i >= ilim)
                    break;
                eps.d *= 10.;
                d.d *= 10.;
            }
        } else {
            eps.d *= tens[ilim - 1];
            for (i = 1;; i++, d.d *= 10.) {
                L = d.d;
                d.d -= L;
                *s++ = '0' + (int)L;
                if (i == ilim) {
                    if (d.d > 0.5 + eps.d)
                        goto bump_up;
                    else if (d.d < 0.5 - eps.d) {
                        while (*--s == '0')
                            ;
                        s++;
                        goto ret1;
                    }
                    break;
                }
            }
        }
fast_failed:
        s = s0;
        d.d = d2.d;
        k = k0;
        ilim = ilim0;
    }

    if (be >= 0 && k <= Int_max) {
        ds = tens[k];
        if (ndigits < 0 && ilim <= 0) {
            S = mhi = 0;
            if (ilim < 0 || d.d <= 5 * ds)
                goto no_digits;
            goto one_digit;
        }
        for (i = 1;; i++) {
            L = d.d / ds;
            d.d -= L * ds;
            *s++ = '0' + (int)L;
            if (i == ilim) {
                d.d += d.d;
                if (d.d > ds || (d.d == ds && L & 1)) {
bump_up:
                    while (*--s == '9')
                        if (s == s0) {
                            k++;
                            *s = '0';
                            break;
                        }
                    ++*s++;
                }
                break;
            }
            if (!(d.d *= 10.))
                break;
        }
        goto ret1;
    }

    m2 = b2;
    m5 = b5;
    mhi = mlo = 0;
    if (leftright) {
        if (mode < 2) {
            i = denorm ? be + (Bias + (P - 1) - 1 + 1) : 1 + P - bbits;
        } else {
            j = ilim - 1;
            if (m5 >= j)
                m5 -= j;
            else {
                s5 += j -= m5;
                b5 += j;
                m5 = 0;
            }
            if ((i = ilim) < 0) {
                m2 -= i;
                i = 0;
            }
        }
        b2 += i;
        s2 += i;
        mhi = __i2b(ptr, 1);
    }
    if (m2 > 0 && s2 > 0) {
        i = m2 < s2 ? m2 : s2;
        b2 -= i;
        m2 -= i;
        s2 -= i;
    }
    if (b5 > 0) {
        if (leftright) {
            if (m5 > 0) {
                mhi = __pow5mult(ptr, mhi, m5);
                b1 = __multiply(ptr, mhi, b);
                _Bfree(ptr, b);
                b = b1;
            }
            if ((j = b5 - m5) != 0)
                b = __pow5mult(ptr, b, j);
        } else
            b = __pow5mult(ptr, b, b5);
    }
    S = __i2b(ptr, 1);
    if (s5 > 0)
        S = __pow5mult(ptr, S, s5);

    if (mode < 2) {
        if (!word1(d) && !(word0(d) & Bndry_mask)
            && word0(d) & Exp_mask) {
            b2 += Log2P;
            s2 += Log2P;
            spec_case = 1;
        } else
            spec_case = 0;
    }

    if ((i = ((s5 ? 32 - __hi0bits(S->_x[S->_wds - 1]) : 1) + s2) & 0x1f) != 0)
        i = 32 - i;
    if (i > 4) {
        i -= 4;
        b2 += i;
        m2 += i;
        s2 += i;
    } else if (i < 4) {
        i += 28;
        b2 += i;
        m2 += i;
        s2 += i;
    }
    if (b2 > 0)
        b = __lshift(ptr, b, b2);
    if (s2 > 0)
        S = __lshift(ptr, S, s2);
    if (k_check) {
        if (__mcmp(b, S) < 0) {
            k--;
            b = __multadd(ptr, b, 10, 0);
            if (leftright)
                mhi = __multadd(ptr, mhi, 10, 0);
            ilim = ilim1;
        }
    }
    if (ilim <= 0 && mode > 2) {
        if (ilim < 0 || __mcmp(b, S = __multadd(ptr, S, 5, 0)) <= 0) {
no_digits:
            k = -1 - ndigits;
            goto ret;
        }
one_digit:
        *s++ = '1';
        k++;
        goto ret;
    }
    if (leftright) {
        if (m2 > 0)
            mhi = __lshift(ptr, mhi, m2);

        mlo = mhi;
        if (spec_case) {
            mhi = _Balloc(ptr, mhi->_k);
            Bcopy(mhi, mlo);
            mhi = __lshift(ptr, mhi, Log2P);
        }

        for (i = 1;; i++) {
            dig = quorem(b, S) + '0';
            j = __mcmp(b, mlo);
            delta = __mdiff(ptr, S, mhi);
            j1 = delta->_sign ? 1 : __mcmp(b, delta);
            _Bfree(ptr, delta);
            if (j1 == 0 && !mode && !(word1(d) & 1)) {
                if (dig == '9')
                    goto round_9_up;
                if (j > 0)
                    dig++;
                *s++ = dig;
                goto ret;
            }
            if (j < 0 || (j == 0 && !mode && !(word1(d) & 1))) {
                if (j1 > 0) {
                    b = __lshift(ptr, b, 1);
                    j1 = __mcmp(b, S);
                    if ((j1 > 0 || (j1 == 0 && dig & 1))
                        && dig++ == '9')
                        goto round_9_up;
                }
                *s++ = dig;
                goto ret;
            }
            if (j1 > 0) {
                if (dig == '9') {
round_9_up:
                    *s++ = '9';
                    goto roundoff;
                }
                *s++ = dig + 1;
                goto ret;
            }
            *s++ = dig;
            if (i == ilim)
                break;
            b = __multadd(ptr, b, 10, 0);
            if (mlo == mhi)
                mlo = mhi = __multadd(ptr, mhi, 10, 0);
            else {
                mlo = __multadd(ptr, mlo, 10, 0);
                mhi = __multadd(ptr, mhi, 10, 0);
            }
        }
    } else
        for (i = 1;; i++) {
            *s++ = dig = quorem(b, S) + '0';
            if (i >= ilim)
                break;
            b = __multadd(ptr, b, 10, 0);
        }

    b = __lshift(ptr, b, 1);
    j = __mcmp(b, S);
    if (j > 0 || (j == 0 && dig & 1)) {
roundoff:
        while (*--s == '9')
            if (s == s0) {
                k++;
                *s++ = '1';
                goto ret;
            }
        ++*s++;
    } else {
        while (*--s == '0')
            ;
        s++;
    }
ret:
    _Bfree(ptr, S);
    if (mhi) {
        if (mlo && mlo != mhi)
            _Bfree(ptr, mlo);
        _Bfree(ptr, mhi);
    }
ret1:
    _Bfree(ptr, b);
    *s = 0;
    *decpt = k + 1;
    if (rve)
        *rve = s;
    return s0;
}
