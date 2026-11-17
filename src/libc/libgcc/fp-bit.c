#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"

static float pack_f(fp_number_type_f *src);
static void unpack_f(FLO_union_type_f *src, fp_number_type_f *dst);
static fp_number_type_f *_fpadd_parts_f(fp_number_type_f *a, fp_number_type_f *b, fp_number_type_f *tmp);
static int __fpcmp_parts_f(fp_number_type_f *a, fp_number_type_f *b);

extern fp_number_type_f fp_thenan_sf;
extern void unpack_f(FLO_union_type_f *src, fp_number_type_f *dst);
extern fp_number_type_f *_fpadd_parts_f(fp_number_type_f *a, fp_number_type_f *b, fp_number_type_f *tmp);
extern float pack_f(fp_number_type_f *src);
extern int __fpcmp_parts_f(fp_number_type_f *a, fp_number_type_f *b);
extern double __make_dp(fp_class_type class, unsigned int sign, int exp, fractype frac);

/*
 * Packs an unpacked number (class, sign, exponent, 32-bit fraction with 7 guard bits) into an IEEE
 * float: NaNs made quiet, infinities and zeros, denormal shifting, round-to-nearest-even and
 * overflow to infinity.
 */
/* 0x001ebd00 */
static float pack_f(fp_number_type_f *src)
{
    FLO_union_type_f dst;
    fractype_f fraction = src->fraction.ll;
    int sign = src->sign;
    int exp = 0;

    if (fpf_isnan(src)) {
        exp = 0xff;
        fraction |= (fractype_f)0x100000;
    } else if (fpf_isinf(src)) {
        exp = 0xff;
        fraction = 0;
    } else if (fpf_iszero(src)) {
        exp = 0;
        fraction = 0;
    } else if (fraction == 0) {
        exp = 0;
    } else {
        if (src->normal_exp < -126) {
            int shift = -126 - src->normal_exp;

            exp = 0;
            if (shift > 32 - 7)
                fraction = 0;
            else
                fraction >>= shift;
            fraction >>= 7;
        } else if (src->normal_exp > 127) {
            exp = 0xff;
            fraction = 0;
        } else {
            exp = src->normal_exp + 127;
            if ((fraction & 0x7f) == 0x40) {
                if (fraction & (1 << 7))
                    fraction += 0x3f + 1;
            } else {
                fraction += 0x3f;
            }
            if (fraction >= ((fractype_f)1 << 31)) {
                fraction >>= 1;
                exp += 1;
            }
            fraction >>= 7;
        }
    }
    dst.bits.fraction = fraction;
    dst.bits.exp = exp;
    dst.bits.sign = sign;
    return dst.value;
}

/*
 * Unpacks an IEEE float into class, sign, unbiased exponent and a fraction with the implicit bit
 * and 7 guard bits (denormals read as zero; NaNs keep their payload and are classed quiet or
 * signalling).
 */
/* 0x001ebe10 */
static void unpack_f(FLO_union_type_f *src, fp_number_type_f *dst)
{
    fractype_f fraction;
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
    } else if (exp == 0xff) {
        if (fraction == 0) {
            dst->class = CLASS_INFINITY;
        } else {
            if (fraction & (fractype_f)0x100000)
                dst->class = CLASS_QNAN;
            else
                dst->class = CLASS_SNAN;
            dst->fraction.ll = fraction;
        }
    } else {
        dst->normal_exp = exp - 127;
        dst->class = CLASS_NUMBER;
        dst->fraction.ll = (fraction << 7) | ((fractype_f)1 << 30);
    }
}

/*
 * Adds two unpacked numbers into tmp (NaN/infinity/zero cases, exponent alignment with sticky
 * shifts, signed fraction add, renormalisation); opposite infinities give the shared NaN
 * fp_thenan_sf.
 */
/* 0x001ebea0 */
static fp_number_type_f *_fpadd_parts_f(fp_number_type_f *a, fp_number_type_f *b, fp_number_type_f *tmp)
{
    int tfraction;
    int a_normal_exp;
    int b_normal_exp;
    fractype_f a_fraction;
    fractype_f b_fraction;

    if (fpf_isnan(a))
        return a;
    if (fpf_isnan(b))
        return b;
    if (fpf_isinf(a)) {
        if (fpf_isinf(b) && a->sign != b->sign)
            return &fp_thenan_sf;
        return a;
    }
    if (fpf_isinf(b))
        return b;
    if (fpf_iszero(b)) {
        if (fpf_iszero(a)) {
            *tmp = *a;
            tmp->sign = a->sign & b->sign;
            return tmp;
        }
        return a;
    }
    if (fpf_iszero(a))
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
        if (diff < 32) {
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
        while (tmp->fraction.ll < ((fractype_f)1 << 30) && tmp->fraction.ll) {
            tmp->fraction.ll <<= 1;
            tmp->normal_exp--;
        }
    } else {
        tmp->sign = a->sign;
        tmp->normal_exp = a_normal_exp;
        tmp->fraction.ll = a_fraction + b_fraction;
    }
    tmp->class = CLASS_NUMBER;
    if (tmp->fraction.ll >= ((fractype_f)1 << 31)) {
        FP_LSHIFT(tmp->fraction.ll);
        tmp->normal_exp++;
    }
    return tmp;
}

/*
 * Float addition: unpacks both operands (unpack_f), adds the parts (_fpadd_parts_f) and packs the
 * result (pack_f).
 */
/* 0x001ec0d8 */
static float fpadd(float arg_a, float arg_b)
{
    fp_number_type_f a;
    fp_number_type_f b;
    fp_number_type_f tmp;
    fp_number_type_f *res;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    unpack_f((FLO_union_type_f *)&arg_b, &b);
    res = _fpadd_parts_f(&a, &b, &tmp);
    return pack_f(res);
}

/* Float subtraction: unpacks both operands, flips the sign of the second and adds the parts. */
/* 0x001ec130 */
static float fpsub(float arg_a, float arg_b)
{
    fp_number_type_f a;
    fp_number_type_f b;
    fp_number_type_f tmp;
    fp_number_type_f *res;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    unpack_f((FLO_union_type_f *)&arg_b, &b);
    b.sign ^= 1;
    res = _fpadd_parts_f(&a, &b, &tmp);
    return pack_f(res);
}

static __inline__ fp_number_type_f *
_fpmul_parts(fp_number_type_f *a, fp_number_type_f *b, fp_number_type_f *tmp)
{
    fractype_f low = 0;
    fractype_f high = 0;

    if (fpf_isnan(a)) {
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fpf_isnan(b)) {
        b->sign = a->sign != b->sign;
        return b;
    }
    if (fpf_isinf(a)) {
        if (fpf_iszero(b))
            return &fp_thenan_sf;
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fpf_isinf(b)) {
        if (fpf_iszero(a))
            return &fp_thenan_sf;
        b->sign = a->sign != b->sign;
        return b;
    }
    if (fpf_iszero(a)) {
        a->sign = a->sign != b->sign;
        return a;
    }
    if (fpf_iszero(b)) {
        b->sign = a->sign != b->sign;
        return b;
    }
    {
        unsigned long answer = (unsigned long)a->fraction.ll * (unsigned long)b->fraction.ll;

        high = answer >> 32;
        low = answer;
    }
    tmp->normal_exp = a->normal_exp + b->normal_exp + 32 - (23 + 7);
    tmp->sign = a->sign != b->sign;
    while (high >= ((fractype_f)1 << 31)) {
        tmp->normal_exp++;
        if (high & 1) {
            low >>= 1;
            low |= (fractype_f)1 << 31;
        }
        high >>= 1;
    }
    while (high < ((fractype_f)1 << 30)) {
        tmp->normal_exp--;
        high <<= 1;
        if (low & ((fractype_f)1 << 31))
            high |= 1;
        low <<= 1;
    }
    if ((high & 0x7f) == 0x40) {
        if (high & (1 << 7)) {
            high += 0x3f + 1;
        } else if (low) {
            high += 0x3f + 1;
        }
    }
    tmp->fraction.ll = high;
    tmp->class = CLASS_NUMBER;
    return tmp;
}

/*
 * Float multiplication: special cases (NaN, infinity, zero; inf*0 gives the shared NaN
 * fp_thenan_sf), then a 32x32->64-bit fraction product, normalisation and round-to-nearest-even
 * into 7 guard bits.
 */
/* 0x001ec198 */
static float fpmul(float arg_a, float arg_b)
{
    fp_number_type_f a;
    fp_number_type_f b;
    fp_number_type_f tmp;
    fp_number_type_f *res;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    unpack_f((FLO_union_type_f *)&arg_b, &b);
    res = _fpmul_parts(&a, &b, &tmp);
    return pack_f(res);
}

static __inline__ fp_number_type_f *
_fpdiv_parts(fp_number_type_f *a, fp_number_type_f *b)
{
    fractype_f bit;
    fractype_f numerator;
    fractype_f denominator;
    fractype_f quotient;

    if (fpf_isnan(a))
        return a;
    if (fpf_isnan(b))
        return b;
    a->sign = a->sign ^ b->sign;
    if (fpf_isinf(a) || fpf_iszero(a)) {
        if (a->class == b->class)
            return &fp_thenan_sf;
        return a;
    }
    if (fpf_isinf(b)) {
        a->fraction.ll = 0;
        a->normal_exp = 0;
        return a;
    }
    if (fpf_iszero(b)) {
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
        bit = (fractype_f)1 << 30;
        quotient = 0;
        while (bit) {
            if (numerator >= denominator) {
                quotient |= bit;
                numerator -= denominator;
            }
            bit >>= 1;
            numerator *= 2;
        }
        if ((quotient & 0x7f) == 0x40) {
            if (quotient & (1 << 7)) {
                quotient += 0x3f + 1;
            } else if (numerator) {
                quotient += 0x3f + 1;
            }
        }
        a->fraction.ll = quotient;
        return (a);
    }
}

/*
 * Float division: special cases (NaN, inf/inf and 0/0 give the shared NaN fp_thenan_sf, x/inf = 0,
 * x/0 = inf), then a restoring bit-by-bit fraction division with round-to-nearest-even.
 */
/* 0x001ec390 */
static float fpdiv(float arg_a, float arg_b)
{
    fp_number_type_f a;
    fp_number_type_f b;
    fp_number_type_f *res;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    unpack_f((FLO_union_type_f *)&arg_b, &b);
    res = _fpdiv_parts(&a, &b);
    return pack_f(res);
}

/*
 * Three-way comparison of two unpacked numbers (-1, 0, 1; 1 when either is a NaN), by class, sign,
 * exponent and fraction.
 */
/* 0x001ec4f0 */
static int __fpcmp_parts_f(fp_number_type_f *a, fp_number_type_f *b)
{
    if (fpf_isnan(a) || fpf_isnan(b))
        return 1;
    if (fpf_isinf(a) && fpf_isinf(b))
        return b->sign - a->sign;
    if (fpf_isinf(a))
        return a->sign ? -1 : 1;
    if (fpf_isinf(b))
        return b->sign ? 1 : -1;
    if (fpf_iszero(a) && fpf_iszero(b))
        return 0;
    if (fpf_iszero(a)) {
        if (b->sign)
            return 1;
        return -1;
    }
    if (fpf_iszero(b))
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

/* Three-way float comparison: unpacks both operands and compares the parts (__fpcmp_parts_f). */
/* 0x001ec608 */
static int fpcmp(float arg_a, float arg_b)
{
    fp_number_type_f a;
    fp_number_type_f b;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    unpack_f((FLO_union_type_f *)&arg_b, &b);
    return __fpcmp_parts_f(&a, &b);
}

/*
 * Converts a 32-bit int to float (INT_MIN special-cased to -2147483648.0f, normalised by shifting
 * into bit 30).
 */
/* 0x001ec658 */
static float litofp(int arg_a)
{
    fp_number_type_f in;

    in.class = CLASS_NUMBER;
    in.sign = arg_a < 0;
    if (!arg_a) {
        in.class = CLASS_ZERO;
    } else {
        in.normal_exp = 23 + 7;
        if (in.sign) {
            if (arg_a == (int)0x80000000)
                return -2147483648.0;
            in.fraction.ll = (-arg_a);
        } else
            in.fraction.ll = arg_a;
        while (in.fraction.ll < ((fractype_f)1 << (23 + 7))) {
            in.fraction.ll <<= 1;
            in.normal_exp -= 1;
        }
    }
    return pack_f(&in);
}

/*
 * Converts a float to a 32-bit int (truncating; NaN and zero give 0, out-of-range values
 * saturate).
 */
/* 0x001ec710 */
static int fptoli(float arg_a)
{
    fp_number_type_f a;
    int tmp;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    if (fpf_iszero(&a))
        return 0;
    if (fpf_isnan(&a))
        return 0;
    if (fpf_isinf(&a))
        return a.sign ? (-0x7fffffff) - 1 : 0x7fffffff;
    if (a.normal_exp < 0)
        return 0;
    if (a.normal_exp > 30)
        return a.sign ? (-0x7fffffff) - 1 : 0x7fffffff;
    tmp = a.fraction.ll >> ((23 + 7) - a.normal_exp);
    return a.sign ? (-tmp) : (tmp);
}

/*
 * Converts a float to a 32-bit unsigned int (negative, NaN and zero give 0, large values saturate
 * to 0xffffffff).
 */
/* 0x001ec7a0 */
unsigned int fptoui(float arg_a)
{
    fp_number_type_f a;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    if (fpf_iszero(&a))
        return 0;
    if (fpf_isnan(&a))
        return 0;
    if (a.sign)
        return 0;
    if (fpf_isinf(&a))
        return 0xffffffff;
    if (a.normal_exp < 0)
        return 0;
    if (a.normal_exp > 31)
        return 0xffffffff;
    else if (a.normal_exp > (23 + 7))
        return a.fraction.ll << (a.normal_exp - (23 + 7));
    else
        return a.fraction.ll >> ((23 + 7) - a.normal_exp);
}

/* Negates a float (flips the sign of the unpacked value and repacks). */
/* 0x001ec838 */
static float fpneg(float arg_a)
{
    fp_number_type_f a;

    unpack_f((FLO_union_type_f *)&arg_a, &a);
    a.sign = !a.sign;
    return pack_f(&a);
}

/* Builds a float from class, sign, exponent and fraction (packs with pack_f); used by dptofp. */
/* 0x001ec870 */
float __make_fp(fp_class_type class, unsigned int sign, int exp, fractype_f frac)
{
    fp_number_type_f in;

    in.class = class;
    in.sign = sign;
    in.normal_exp = exp;
    in.fraction.ll = frac;
    return pack_f(&in);
}

/*
 * Converts a float to double: unpacks it and builds the double (__make_dp) with the fraction
 * shifted up by 30 bits.
 */
/* 0x001ec8a0 */
static double fptodp(float arg_a)
{
    fp_number_type_f in;

    unpack_f((FLO_union_type_f *)&arg_a, &in);
    return __make_dp(in.class, in.sign, in.normal_exp, ((fractype)in.fraction.ll) << 30);
}
