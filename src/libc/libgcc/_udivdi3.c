#include "types_libc.h"
#include "types_sce.h"
#include "sdk_compat.h"

/* Leading-zero count. The dead byte-scan loop is part of the original. */
#define CLZ(count, x)                                                         \
    do {                                                                      \
        unsigned int xr_ = (x);                                               \
        unsigned int a_;                                                      \
        if (32 <= 32) {                                                       \
            a_ = xr_ < (1U << 16) ? (xr_ < (1U << 8) ? 0 : 8)                 \
                                  : (xr_ < (1U << 24) ? 16 : 24);             \
        } else {                                                              \
            for (a_ = 32 - 8; a_ > 0; a_ -= 8)                                \
                if (((xr_ >> a_) & 0xff) != 0)                                \
                    break;                                                    \
        }                                                                     \
        (count) = 32 - (udivdi3_clz_tab[xr_ >> a_] + a_);                          \
    } while (0)

#define UDIV_QRNND(q, r, n1, n0, d)                                           \
    do {                                                                      \
        unsigned int d1_, d0_, q1_, q0_;                                      \
        unsigned int r1_, r0_, m_;                                            \
        d1_ = (d) >> 16;                                                      \
        d0_ = (d) & 0xffff;                                                   \
        r1_ = (n1) % d1_;                                                     \
        q1_ = (n1) / d1_;                                                     \
        m_ = q1_ * d0_;                                                       \
        r1_ = r1_ * 0x10000L | ((n0) >> 16);                                   \
        if (r1_ < m_) {                                                       \
            q1_--, r1_ += (d);                                                \
            if (r1_ >= (d))                                                   \
                if (r1_ < m_)                                                 \
                    q1_--, r1_ += (d);                                        \
        }                                                                     \
        r1_ -= m_;                                                            \
        r0_ = r1_ % d1_;                                                      \
        q0_ = r1_ / d1_;                                                      \
        m_ = q0_ * d0_;                                                       \
        r0_ = r0_ * 0x10000L | ((n0) & 0xffff);                                \
        if (r0_ < m_) {                                                       \
            q0_--, r0_ += (d);                                                \
            if (r0_ >= (d))                                                   \
                if (r0_ < m_)                                                 \
                    q0_--, r0_ += (d);                                        \
        }                                                                     \
        r0_ -= m_;                                                            \
        (q) = q1_ * 0x10000L | q0_;                                            \
        (r) = r0_;                                                            \
    } while (0)

#define SUB_DDMMSS(sh, sl, ah, al, bh, bl)                                    \
    do {                                                                      \
        unsigned int x_;                                                      \
        x_ = (al) - (bl);                                                     \
        (sh) = (ah) - (bh) - (x_ > (al));                                     \
        (sl) = x_;                                                            \
    } while (0)

extern const unsigned char udivdi3_clz_tab[256];

static inline unsigned long __udivmoddi4(unsigned long n, unsigned long d, unsigned long *rp)
{
    LWords ww;
    LWords nn, dd;
    unsigned int d0, d1, n0, n1, n2;
    unsigned int q0, q1;
    unsigned int b, bm;
    LWords rr;

    nn.ll = n;
    dd.ll = d;

    d0 = dd.s.low;
    d1 = dd.s.high;
    n0 = nn.s.low;
    n1 = nn.s.high;

    if (d1 == 0) {
        if (d0 > n1) {
            CLZ(bm, d0);
            if (bm != 0) {
                d0 = d0 << bm;
                n1 = (n1 << bm) | (n0 >> (32 - bm));
                n0 = n0 << bm;
            }
            UDIV_QRNND(q0, n0, n1, n0, d0);
            q1 = 0;
        } else {
            if (d0 == 0)
                d0 = 1 / d0;
            CLZ(bm, d0);
            if (bm == 0) {
                n1 -= d0;
                q1 = 1;
            } else {
                b = 32 - bm;
                d0 = d0 << bm;
                n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 = n0 << bm;
                UDIV_QRNND(q1, n1, n2, n1, d0);
            }
            UDIV_QRNND(q0, n0, n1, n0, d0);
        }

        if (rp != 0) {
            rr.s.low = n0 >> bm;
            rr.s.high = 0;
            *rp = rr.ll;
        }
    } else {
        if (d1 > n1) {
            q0 = 0;
            q1 = 0;

            if (rp != 0) {
                rr.s.low = n0;
                rr.s.high = n1;
                *rp = rr.ll;
            }
        } else {
            CLZ(bm, d1);
            if (bm == 0) {
                if (n1 > d1 || n0 >= d0) {
                    q0 = 1;
                    SUB_DDMMSS(n1, n0, n1, n0, d1, d0);
                } else
                    q0 = 0;
                q1 = 0;

                if (rp != 0) {
                    rr.s.low = n0;
                    rr.s.high = n1;
                    *rp = rr.ll;
                }
            } else {
                unsigned int m1, m0;

                b = 32 - bm;
                d1 = (d1 << bm) | (d0 >> b);
                d0 = d0 << bm;
                n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 = n0 << bm;
                UDIV_QRNND(q0, n1, n2, n1, d1);
                umul_ppmm(m1, m0, q0, d0);
                if (m1 > n1 || (m1 == n1 && m0 > n0)) {
                    q0--;
                    SUB_DDMMSS(m1, m0, m1, m0, d1, d0);
                }
                q1 = 0;

                if (rp != 0) {
                    SUB_DDMMSS(n1, n0, n1, n0, m1, m0);
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
 * Unsigned 64-bit division done in 32-bit halves: normalises the divisor with a leading-zero count
 * from the byte table udivdi3_clz_tab, divides with two 16-bit-digit steps and corrects the
 * quotient by the 32x32->64 product (the EE has no 64-bit divide; called for unsigned long '/').
 * The 32x32->64 product in the quotient check is longlong.h's umul_ppmm (one multu read with
 * mfhi/mflo; sdk_compat.h). Written as libgcc2.c has it: __udivdi3 calls the static inline
 * __udivmoddi4 with rp = 0, whose remainder blocks are dead; the one after the normalised quotient
 * correction holds a sub_ddmmss do-while whose loop note survives and aligns the tail label.
 */
/* 0x001fe128 */
unsigned long __udivdi3(unsigned long n, unsigned long d)
{
    return __udivmoddi4(n, d, (unsigned long *)0);
}
