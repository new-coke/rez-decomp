#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "sdk_compat.h"

#define CLZ_TAB moddi3_clz_tab

/* __clz_tab */
#define SI_TYPE_SIZE 32

#define __BITS4 (SI_TYPE_SIZE / 4)
#define __ll_B (1L << (SI_TYPE_SIZE / 2))
#define __ll_lowpart(t) ((USItype_a)(t) % __ll_B)
#define __ll_highpart(t) ((USItype_a)(t) / __ll_B)

#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
    do { \
        USItype_a __x; \
        __x = (al) - (bl); \
        (sh) = (ah) - (bh) - (__x > (al)); \
        (sl) = __x; \
    } while (0)

/* longlong.h has this as an inline-assembly multu for MIPS; this is its C meaning. */
#define udiv_qrnnd(q, r, n1, n0, d) \
    do { \
        USItype_a __d1, __d0, __q1, __q0; \
        USItype_a __r1, __r0, __m; \
        __d1 = __ll_highpart(d); \
        __d0 = __ll_lowpart(d); \
        \
        __r1 = (n1) % __d1; \
        __q1 = (n1) / __d1; \
        __m = (USItype_a)__q1 * __d0; \
        __r1 = __r1 * __ll_B | __ll_highpart(n0); \
        if (__r1 < __m) { \
            __q1--, __r1 += (d); \
            if (__r1 >= (d)) \
                if (__r1 < __m) \
                    __q1--, __r1 += (d); \
        } \
        __r1 -= __m; \
        \
        __r0 = __r1 % __d1; \
        __q0 = __r1 / __d1; \
        __m = (USItype_a)__q0 * __d0; \
        __r0 = __r0 * __ll_B | __ll_lowpart(n0); \
        if (__r0 < __m) { \
            __q0--, __r0 += (d); \
            if (__r0 >= (d)) \
                if (__r0 < __m) \
                    __q0--, __r0 += (d); \
        } \
        __r0 -= __m; \
        \
        (q) = (USItype_a)__q1 * __ll_B | __q0; \
        (r) = __r0; \
    } while (0)

#define count_leading_zeros(count, x) \
    do { \
        USItype_a __xr = (x); \
        USItype_a __a; \
        \
        if (SI_TYPE_SIZE <= 32) { \
            __a = __xr < ((USItype_a)1 << 2 * __BITS4) \
                ? (__xr < ((USItype_a)1 << __BITS4) ? 0 : __BITS4) \
                : (__xr < ((USItype_a)1 << 3 * __BITS4) ? 2 * __BITS4 : 3 * __BITS4); \
        } else { \
            for (__a = SI_TYPE_SIZE - 8; __a > 0; __a -= 8) \
                if (((__xr >> __a) & 0xff) != 0) \
                    break; \
        } \
        \
        (count) = SI_TYPE_SIZE - (CLZ_TAB[__xr >> __a] + __a); \
    } while (0)

extern const unsigned char CLZ_TAB[];

static __inline__ DItype_a __negdi2(DItype_a u)
{
    DIunion_a w;
    DIunion_a uu;

    uu.ll = u;

    w.s.low = -uu.s.low;
    w.s.high = -uu.s.high - ((USItype_a)w.s.low > 0);

    return w.ll;
}

static __inline__ UDItype_a __udivmoddi4(UDItype_a n, UDItype_a d, UDItype_a *rp)
{
    DIunion_a ww;
    DIunion_a nn, dd;
    DIunion_a rr;
    USItype_a d0, d1, n0, n1, n2;
    USItype_a q0, q1;
    USItype_a b, bm;

    nn.ll = n;
    dd.ll = d;

    d0 = dd.s.low;
    d1 = dd.s.high;
    n0 = nn.s.low;
    n1 = nn.s.high;

    if (d1 == 0) {
        if (d0 > n1) {
            /* 0q = nn / 0D */

            count_leading_zeros(bm, d0);

            if (bm != 0) {
                /* Normalize, i.e. make the most significant bit of the denominator set. */

                d0 = d0 << bm;
                n1 = (n1 << bm) | (n0 >> (SI_TYPE_SIZE - bm));
                n0 = n0 << bm;
            }

            udiv_qrnnd(q0, n0, n1, n0, d0);
            q1 = 0;

            /* Remainder in n0 >> bm. */
        } else {
            /* qq = NN / 0d */

            if (d0 == 0)
                d0 = 1 / d0; /* Divide intentionally by zero. */

            count_leading_zeros(bm, d0);

            if (bm == 0) {
                /* From (n1 >= d0) and (the most significant bit of d0 is set), conclude (the most
                   significant bit of n1 is set) and (the leading quotient digit q1 = 1). */

                n1 -= d0;
                q1 = 1;
            } else {
                /* Normalize. */

                b = SI_TYPE_SIZE - bm;

                d0 = d0 << bm;
                n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 = n0 << bm;

                udiv_qrnnd(q1, n1, n2, n1, d0);
            }

            /* n1 != d0... */

            udiv_qrnnd(q0, n0, n1, n0, d0);

            /* Remainder in n0 >> bm. */
        }

        if (rp != 0) {
            rr.s.low = n0 >> bm;
            rr.s.high = 0;
            *rp = rr.ll;
        }
    } else {
        if (d1 > n1) {
            /* 00 = nn / DD */

            q0 = 0;
            q1 = 0;

            /* Remainder in n1n0. */
            if (rp != 0) {
                rr.s.low = n0;
                rr.s.high = n1;
                *rp = rr.ll;
            }
        } else {
            /* 0q = NN / dd */

            count_leading_zeros(bm, d1);
            if (bm == 0) {
                /* From (n1 >= d1) and (the most significant bit of d1 is set), conclude (the most
                   significant bit of n1 is set) and (the quotient digit q0 = 0 or 1). The
                   condition below relies on n1 >= d1 (true due to program flow). */
                if (n1 > d1 || n0 >= d0) {
                    q0 = 1;
                    sub_ddmmss(n1, n0, n1, n0, d1, d0);
                } else
                    q0 = 0;

                q1 = 0;

                if (rp != 0) {
                    rr.s.low = n0;
                    rr.s.high = n1;
                    *rp = rr.ll;
                }
            } else {
                USItype_a m1, m0;
                /* Normalize. */

                b = SI_TYPE_SIZE - bm;

                d1 = (d1 << bm) | (d0 >> b);
                d0 = d0 << bm;
                n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 = n0 << bm;

                udiv_qrnnd(q0, n1, n2, n1, d1);
                umul_ppmm(m1, m0, q0, d0);

                if (m1 > n1 || (m1 == n1 && m0 > n0)) {
                    q0--;
                    sub_ddmmss(m1, m0, m1, m0, d1, d0);
                }

                q1 = 0;

                /* Remainder in (n1n0 - m1m0) >> bm. */
                if (rp != 0) {
                    sub_ddmmss(n1, n0, n1, n0, m1, m0);
                    rr.s.low = (n1 << b) | (n0 >> bm);
                    rr.s.high = n1 >> bm;
                    *rp = rr.ll;
                }
            }
        }
    }

    ww.s.low = q0;
    ww.s.high = q1;
    return ww.ll;
}

/*
 * Signed 64-bit remainder (sign of the dividend): takes the magnitudes with __negdi2, divides them
 * with the inlined __udivmoddi4 (32-bit halves, normalising with count_leading_zeros through this
 * object's own copy of __clz_tab at 0x2c4800) and negates the result when the sign calls for it.
 * The macros are the egcs-era longlong.h ones (__ll_B is 1L << 16, a long, so the half-word
 * products are formed in 64 bits). umul_ppmm is longlong.h's MIPS one, an inline-assembly multu
 * read with mfhi/mflo (sdk_compat.h).
 */
/* 0x001ea9a0 */
DItype_a __moddi3(DItype_a u, DItype_a v)
{
    word_type_a c = 0;
    DIunion_a uu, vv;
    DItype_a w;

    uu.ll = u;
    vv.ll = v;

    if (uu.s.high < 0)
        c = ~c,
        uu.ll = __negdi2(uu.ll);
    if (vv.s.high < 0)
        vv.ll = __negdi2(vv.ll);

    (void)__udivmoddi4(uu.ll, vv.ll, &w);
    if (c)
        w = __negdi2(w);

    return w;
}
