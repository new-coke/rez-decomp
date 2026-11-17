/* Game-wide maths constants and unit conversions. */

#ifndef REZ_MATHDEF_H
#define REZ_MATHDEF_H

#define PI          3.1415927f
#define TWO_PI      6.2831855f

#ifndef FLT_EPSILON
#define FLT_EPSILON 1.1920929e-07f
#endif

/*
 * Angles are 16-bit binary: 65536 units = one turn. Each constant is the literal the original
 * compiles; ANG_PER_RAD, RAD_PER_ANG and RAD_PER_DEG are not the correctly rounded values.
 * RAD_TO_ANG multiplies before dividing; it rounds differently from x * ANG_PER_RAD.
 */
#define ANG_TURN        65536.0f
#define ANG_HALF        32768.0f
#define ANG_QUARTER     16384.0f
#define ANG_PER_DEG     182.04445f
#define DEG_PER_ANG     0.0054931640625f
#define ANG_PER_RAD     10430.381f
#define RAD_PER_ANG     9.587378e-05f
#define RAD_PER_DEG     0.017453289f
#define DEG_TO_ANG(x)   ((x) * ANG_PER_DEG)
#define RAD_TO_ANG(x)   ((x) * 65536.0f / TWO_PI)

/* Integer angles, for rotations and fields in angle units */
#define ANG_180         0x8000
#define ANG_90          0x4000

/* NTSC: 60 fields a second, and game timers count them */
#define FPS             60.0f
#define FRAME_SEC       0.016666668f

/* Uniform in [0, 1) from the two 15-bit generators of math_util.c. */
#define RAND_FLOAT_A()  (rand_lcg_next_a() / 32769.0f)
#define RAND_FLOAT_B()  (rand_lcg_next_b() / 32769.0f)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* REZ_MATHDEF_H */
