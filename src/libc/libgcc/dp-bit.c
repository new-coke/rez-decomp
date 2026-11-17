#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"

static double pack_d(fp_number_type *src);
static void unpack_d(FLO_union_type *src, fp_number_type *dst);
static fp_number_type *_fpadd_parts(fp_number_type *a, fp_number_type *b, fp_number_type *tmp);
static int __fpcmp_parts_d(fp_number_type *a, fp_number_type *b);

extern fp_number_type fp_thenan_df;
extern void unpack_d(FLO_union_type *src, fp_number_type *dst);
extern fp_number_type *_fpadd_parts(fp_number_type *a, fp_number_type *b, fp_number_type *tmp);
extern double pack_d(fp_number_type *src);
extern int __fpcmp_parts_d(fp_number_type *a, fp_number_type *b);
extern float __make_fp(fp_class_type class, unsigned int sign, int exp, unsigned int frac);

/*
 * Packs an unpacked number (class, sign, exponent, 64-bit fraction with 8 guard bits) into an IEEE
 * double: NaNs made quiet, infinities and zeros, denormal shifting, round-to-nearest-even and
 * overflow to infinity.
 */
/* 0x001eb008 */
static double pack_d(fp_number_type *src)
{
    FLO_union_type dst;
    fractype fraction = src->fraction.ll;
    int sign = src->sign;
    int exp = 0;

    if (fp_isnan(src)) {
        exp = 0x7ff;
        fraction |= (fractype)0x8000000000000;
    } else if (fp_isinf(src)) {
        exp = 0x7ff;
        fraction = 0;
    } else if (fp_iszero(src)) {
        exp = 0;
        fraction = 0;
    } else if (fraction == 0) {
        exp = 0;
    } else {
        if (src->normal_exp < -1022) {
            int shift = -1022 - src->normal_exp;

            exp = 0;
            if (shift > 64 - 8)
                fraction = 0;
            else
                fraction >>= shift;
            fraction >>= 8;
        } else if (src->normal_exp > 1023) {
            exp = 0x7ff;
            fraction = 0;
        } else {
            exp = src->normal_exp + 1023;
            if ((fraction & 0xff) == 0x80) {
                if (fraction & (1 << 8))
                    fraction += 0x7f + 1;
            } else {
                fraction += 0x7f;
            }
            if (fraction >= ((fractype)1 << 61)) {
                fraction >>= 1;
                exp += 1;
            }
            fraction >>= 8;
        }
    }
    dst.bits.fraction = fraction;
    dst.bits.exp = exp;
    dst.bits.sign = sign;
    return dst.value;
}

/*
 * Unpacks an IEEE double into class, sign, unbiased exponent and a fraction with the implicit bit
 * and 8 guard bits (denormals read as zero; NaNs keep their payload and are classed quiet or
 * signalling).
 */
/* 0x001eb138 */
static void unpack_d(FLO_union_type *src, fp_number_type *dst)
{
    fractype fraction;
    int exp;
    int sign;

    fraction = src->bits.fraction;
    exp = src->bits.exp;
    sign = src->bits.sign;
    dst->sign = sign;
    if (exp == 0) {
        do {
            dst->class = CLASS_ZERO;
        } while (0);
    } else if (exp == 0x7ff) {
        if (fraction == 0) {
            dst->class = CLASS_INFINITY;
        } else {
            if (fraction & (fractype)0x8000000000000)
                dst->class = CLASS_QNAN;
            else
                dst->class = CLASS_SNAN;
            dst->fraction.ll = fraction;
        }
    } else {
        dst->normal_exp = exp - 1023;
        dst->class = CLASS_NUMBER;
        dst->fraction.ll = (fraction << 8) | ((fractype)1 << 60);
    }
}

/*
 * Adds two unpacked numbers into tmp (NaN/infinity/zero cases, exponent alignment with sticky
 * shifts, signed fraction add, renormalisation); opposite infinities give the shared NaN
 * fp_thenan_df.
 */
/* 0x001eb1d8 */
static fp_number_type *_fpadd_parts(fp_number_type *a, fp_number_type *b, fp_number_type *tmp)
{
    intfrac tfraction;
    int a_normal_exp;
    int b_normal_exp;
    fractype a_fraction;
    fractype b_fraction;

    if (fp_isnan(a))
        return a;
    if (fp_isnan(b))
        return b;
    if (fp_isinf(a)) {
        if (fp_isinf(b) && a->sign != b->sign)
            return &fp_thenan_df;
        return a;
    }
    if (fp_isinf(b))
        return b;
    if (fp_iszero(b)) {
        if (fp_iszero(a)) {
            *tmp = *a;
            tmp->sign = a->sign & b->sign;
            return tmp;
        }
        return a;
    }
    if (fp_iszero(a))
        return b;
    {
        int diff;

        a_normal_exp = a->normal_exp;
        b_normal_exp = b->normal_exp;
        a_fraction = a->fraction.ll;
        b_fraction = b->fraction.ll;
        diff = a_normal_exp - b_normal_exp;
        if (diff < 0)
            diff = -diff;
        if (diff < 64) {
            while (a_normal_exp > b_normal_exp) {
                b_normal_exp++;
                FP_LSHIFT(b_fraction);
            }
            while (b_normal_exp > a_normal_exp) {
                a_normal_exp++;
                FP_LSHIFT(a_fraction);
            }
        } else {
            if (a_normal_exp > b_normal_exp) {
                b_normal_exp = a_normal_exp;
                b_fraction = 0;
            } else {
                a_normal_exp = b_normal_exp;
                a_fraction = 0;
            }
        }
    }
    if (a->sign != b->sign) {
        if (a->sign)
            tfraction = -a_fraction + b_fraction;
        else
            tfraction = a_fraction - b_fraction;
        if (tfraction >= 0) {
            tmp->sign = 0;
            tmp->normal_exp = a_normal_exp;
            tmp->fraction.ll = tfraction;
        } else {
            tmp->sign = 1;
            tmp->normal_exp = a_normal_exp;
            tmp->fraction.ll = -tfraction;
        }
        while (tmp->fraction.ll < ((fractype)1 << 60) && tmp->fraction.ll) {
            tmp->fraction.ll <<= 1;
            tmp->normal_exp--;
        }
    } else {
        tmp->sign = a->sign;
        tmp->normal_exp = a_normal_exp;
        tmp->fraction.ll = a_fraction + b_fraction;
    }
    tmp->class = CLASS_NUMBER;
    if (tmp->fraction.ll >= ((fractype)1 << 61)) {
        FP_LSHIFT(tmp->fraction.ll);
        tmp->normal_exp++;
    }
    return tmp;
}

/*
 * Double addition: unpacks both operands (unpack_d), adds the parts (_fpadd_parts) and packs the
 * result (pack_d).
 */
/* 0x001eb418 */
static double dpadd(double arg_a, double arg_b)
{
    fp_number_type a;
    fp_number_type b;
    fp_number_type tmp;
    fp_number_type *res;

    unpack_d((FLO_union_type *)&arg_a, &a);
    unpack_d((FLO_union_type *)&arg_b, &b);
    res = _fpadd_parts(&a, &b, &tmp);
    return pack_d(res);
}

/* Double subtraction: unpacks both operands, flips the sign of the second and adds the parts. */
/* 0x001eb470 */
static double dpsub(double arg_a, double arg_b)
{
    fp_number_type a;
    fp_number_type b;
    fp_number_type tmp;
    fp_number_type *res;

    unpack_d((FLO_union_type *)&arg_a, &a);
    unpack_d((FLO_union_type *)&arg_b, &b);
    b.sign ^= 1;
    res = _fpadd_parts(&a, &b, &tmp);
    return pack_d(res);
}

static __inline__ fp_number_type *
_fpmul_parts(fp_number_type *a, fp_number_type *b, fp_number_type *tmp)
{
    fractype low = 0;
    fractype high = 0;

    if (fp_isnan(a)) {
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fp_isnan(b)) {
        b->sign = a->sign != b->sign;
        return b;
    }
    if (fp_isinf(a)) {
        if (fp_iszero(b))
            return &fp_thenan_df;
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fp_isinf(b)) {
        if (fp_iszero(a))
            return &fp_thenan_df;
        b->sign = a->sign != b->sign;
        return b;
    }
    if (fp_iszero(a)) {
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fp_iszero(b)) {
        b->sign = a->sign != b->sign;
        return b;
    }
    {
        unsigned long nl = a->fraction.ll & 0xffffffff;
        unsigned long nh = a->fraction.ll >> 32;
        unsigned long ml = b->fraction.ll & 0xffffffff;
        unsigned long mh = b->fraction.ll >> 32;
        unsigned long pp_ll = ml * nl;
        unsigned long pp_hl = mh * nl;
        unsigned long pp_lh = ml * nh;
        unsigned long pp_hh = mh * nh;
        unsigned long res2 = 0;
        unsigned long res0 = 0;
        unsigned long ps_hh__ = pp_hl + pp_lh;

        if (ps_hh__ < pp_hl)
            res2 += 0x100000000;
        pp_hl = (ps_hh__ << 32) & 0xffffffff00000000;
        res0 = pp_ll + pp_hl;
        if (res0 < pp_ll)
            res2++;
        res2 += ((ps_hh__ >> 32) & 0xffffffff) + pp_hh;
        high = res2;
        low = res0;
    }
    tmp->normal_exp = a->normal_exp + b->normal_exp;
    tmp->sign = a->sign != b->sign;
    tmp->normal_exp += 4;
    while (high >= ((fractype)1 << 61)) {
        tmp->normal_exp++;
        if (high & 1) {
            low >>= 1;
            low |= (fractype)1 << 63;
        }
        high >>= 1;
    }
    while (high < ((fractype)1 << 60)) {
        tmp->normal_exp--;
        high <<= 1;
        if (low & ((fractype)1 << 63))
            high |= 1;
        low <<= 1;
    }
    if ((high & 0xff) == 0x80) {
        if (high & (1 << 8)) {
            high += 0x7f + 1;
        } else if (low) {
            high += 0x7f + 1;
        }
    }
    tmp->fraction.ll = high;
    tmp->class = CLASS_NUMBER;
    return tmp;
}

/*
 * Double multiplication: special cases (NaN, infinity, zero; inf*0 gives the shared NaN
 * fp_thenan_df), then a 64x64->128-bit fraction product from four 32x32 partial products
 * (__muldi3), normalisation and round-to-nearest-even into 8 guard bits.
 */
/* 0x001eb4d8 */
static double dpmul(double arg_a, double arg_b)
{
    fp_number_type a;
    fp_number_type b;
    fp_number_type tmp;
    fp_number_type *res;

    unpack_d((FLO_union_type *)&arg_a, &a);
    unpack_d((FLO_union_type *)&arg_b, &b);
    res = _fpmul_parts(&a, &b, &tmp);
    return pack_d(res);
}

static __inline__ fp_number_type *
_fpdiv_parts(fp_number_type *a, fp_number_type *b)
{
    fractype bit;
    fractype numerator;
    fractype denominator;
    fractype quotient;

    if (fp_isnan(a))
        return a;
    if (fp_isnan(b))
        return b;
    a->sign = a->sign ^ b->sign;
    if (fp_isinf(a) || fp_iszero(a)) {
        if (a->class == b->class)
            return &fp_thenan_df;
        return a;
    }
    if (fp_isinf(b)) {
        a->fraction.ll = 0;
        a->normal_exp = 0;
        return a;
    }
    if (fp_iszero(b)) {
        a->class = CLASS_INFINITY;
        return a;
    }
    {
        a->normal_exp = a->normal_exp - b->normal_exp;
        numerator = a->fraction.ll;
        denominator = b->fraction.ll;
        if (numerator < denominator) {
            numerator *= 2;
            a->normal_exp--;
        }
        bit = (fractype)1 << 60;
        quotient = 0;
        while (bit) {
            if (numerator >= denominator) {
                quotient |= bit;
                numerator -= denominator;
            }
            bit >>= 1;
            numerator *= 2;
        }
        if ((quotient & 0xff) == 0x80) {
            if (quotient & (1 << 8)) {
                quotient += 0x7f + 1;
            } else if (numerator) {
                quotient += 0x7f + 1;
            }
        }
        a->fraction.ll = quotient;
        return (a);
    }
}

/*
 * Double division: special cases (NaN, inf/inf and 0/0 give the shared NaN fp_thenan_df, x/inf =
 * 0, x/0 = inf), then a restoring bit-by-bit fraction division with round-to-nearest-even.
 */
/* 0x001eb780 */
static double dpdiv(double arg_a, double arg_b)
{
    fp_number_type a;
    fp_number_type b;
    fp_number_type *res;

    unpack_d((FLO_union_type *)&arg_a, &a);
    unpack_d((FLO_union_type *)&arg_b, &b);
    res = _fpdiv_parts(&a, &b);
    return pack_d(res);
}

/*
 * Three-way comparison of two unpacked numbers (-1, 0, 1; 1 when either is a NaN), by class, sign,
 * exponent and fraction.
 */
/* 0x001eb8e8 */
static int __fpcmp_parts_d(fp_number_type *a, fp_number_type *b)
{
    if (fp_isnan(a) || fp_isnan(b))
        return 1;
    if (fp_isinf(a) && fp_isinf(b))
        return b->sign - a->sign;
    if (fp_isinf(a))
        return a->sign ? -1 : 1;
    if (fp_isinf(b))
        return b->sign ? 1 : -1;
    if (fp_iszero(a) && fp_iszero(b))
        return 0;
    if (fp_iszero(a)) {
        if (b->sign)
            return 1;
        return -1;
    }
    if (fp_iszero(b))
        return a->sign ? -1 : 1;
    if (a->sign != b->sign)
        return a->sign ? -1 : 1;
    if (a->normal_exp > b->normal_exp)
        return a->sign ? -1 : 1;
    if (a->normal_exp < b->normal_exp)
        return a->sign ? 1 : -1;
    if (a->fraction.ll > b->fraction.ll)
        return a->sign ? -1 : 1;
    if (a->fraction.ll < b->fraction.ll)
        return a->sign ? 1 : -1;
    return 0;
}

/* Three-way double comparison: unpacks both operands and compares the parts (__fpcmp_parts_d). */
/* 0x001eba00 */
static int dpcmp(double arg_a, double arg_b)
{
    fp_number_type a;
    fp_number_type b;

    unpack_d((FLO_union_type *)&arg_a, &a);
    unpack_d((FLO_union_type *)&arg_b, &b);
    return __fpcmp_parts_d(&a, &b);
}

/*
 * Converts a 32-bit int to double (INT_MIN special-cased to -2147483648.0, normalised by shifting
 * into bit 60).
 */
/* 0x001eba50 */
static double litodp(int arg_a)
{
    fp_number_type in;

    in.class = CLASS_NUMBER;
    in.sign = arg_a < 0;
    if (!arg_a) {
        in.class = CLASS_ZERO;
    } else {
        in.normal_exp = 52 + 8;
        if (in.sign) {
            if (arg_a == (int)0x80000000)
                return -2147483648.0;
            in.fraction.ll = (-arg_a);
        } else
            in.fraction.ll = arg_a;
        while (in.fraction.ll < ((fractype)1 << (52 + 8))) {
            in.fraction.ll <<= 1;
            in.normal_exp -= 1;
        }
    }
    return pack_d(&in);
}

/*
 * Converts a double to a 32-bit int (truncating; NaN and zero give 0, out-of-range values
 * saturate).
 */
/* 0x001ebb08 */
static int dptoli(double arg_a)
{
    fp_number_type a;
    int tmp;

    unpack_d((FLO_union_type *)&arg_a, &a);
    if (fp_iszero(&a))
        return 0;
    if (fp_isnan(&a))
        return 0;
    if (fp_isinf(&a))
        return a.sign ? (-0x7fffffff) - 1 : 0x7fffffff;
    if (a.normal_exp < 0)
        return 0;
    if (a.normal_exp > 30)
        return a.sign ? (-0x7fffffff) - 1 : 0x7fffffff;
    tmp = a.fraction.ll >> ((52 + 8) - a.normal_exp);
    return a.sign ? (-tmp) : (tmp);
}

/*
 * Converts a double to a 32-bit unsigned int (negative, NaN and zero give 0, large values saturate
 * to 0xffffffff).
 */
/* 0x001ebba0 */
static unsigned int dptoul(double arg_a)
{
    fp_number_type a;

    unpack_d((FLO_union_type *)&arg_a, &a);
    if (fp_iszero(&a))
        return 0;
    if (fp_isnan(&a))
        return 0;
    if (a.sign)
        return 0;
    if (fp_isinf(&a))
        return 0xffffffff;
    if (a.normal_exp < 0)
        return 0;
    if (a.normal_exp > 31)
        return 0xffffffff;
    else if (a.normal_exp > (52 + 8))
        return a.fraction.ll << (a.normal_exp - (52 + 8));
    else
        return a.fraction.ll >> ((52 + 8) - a.normal_exp);
}

/* Negates a double (flips the sign of the unpacked value and repacks). */
/* 0x001ebc40 */
static double dpneg(double arg_a)
{
    fp_number_type a;

    unpack_d((FLO_union_type *)&arg_a, &a);
    a.sign = !a.sign;
    return pack_d(&a);
}

/*
 * Builds a double from class, sign, exponent and fraction (packs with pack_d); used by the
 * float-to-double conversion.
 */
/* 0x001ebc78 */
double __make_dp(fp_class_type class, unsigned int sign, int exp, fractype frac)
{
    fp_number_type in;

    in.class = class;
    in.sign = sign;
    in.normal_exp = exp;
    in.fraction.ll = frac;
    return pack_d(&in);
}

/*
 * Converts a double to float: keeps the top fraction bits (sticky bit for any discarded ones) and
 * builds the float with __make_fp.
 */
/* 0x001ebca8 */
static float dptofp(double arg_a)
{
    fp_number_type in;
    unsigned int sffrac;

    unpack_d((FLO_union_type *)&arg_a, &in);
    sffrac = in.fraction.ll >> 30;
    if ((in.fraction.ll & (((unsigned int)1 << 30) - 1)) != 0)
        sffrac |= 1;
    return __make_fp(in.class, in.sign, in.normal_exp, sffrac);
}
