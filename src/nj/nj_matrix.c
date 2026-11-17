/* Ninja matrices: stack, transforms (65536 = full turn), copy, multiply, inverse; 0 = current. */

#include "ninja.h"
#include "libc.h"
#include "nj_matrix.h"
#include "ul_vu0.h"

extern void ulMatrixStackInit(void);
extern void *ulMatrixPush(void);
extern void ulMatrixLoad(void *m);
extern void *ulMatrixPop(void);
extern void vu0_matrix_identity(NjMatrix *m);
extern void vu0_matrix_inverse_rigid(NjMatrix *d, NjMatrix *s);
extern void ulMatrixUnitRotation(NjMatrix *m);
extern void ulMatrixGetTranslation(NjMatrix *m);
extern void ulMatrixApplyPoint(NjMatrix *m, NjVec3 *in, NjVec3 *out);
extern void ulMatrixApplyVector(NjMatrix *m, NjVec3 *in, NjVec3 *out);

/* 0x001a1a48 */
static void nj_matrix_stack_init(void)
{
    ulMatrixStackInit();
}

/* mtx, if given, becomes current; returns 1. */
/* 0x001a1a50 */
static int njPushMatrix(void *mtx)
{
    if (mtx) {
        ulMatrixPush();
        ulMatrixLoad(mtx);
    } else {
        ulMatrixPush();
    }
    return 1;
}

/* 0x001a1a98 */
int njPushMatrixEx(void)
{
    ulMatrixPush();
    return 1;
}

/* n times; returns 1. */
/* 0x001a1ab8 */
static int njPopMatrix(unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++)
        ulMatrixPop();
    return 1;
}

/* 0x001a1b08 */
int njPopMatrixEx(void)
{
    ulMatrixPop();
    return 1;
}

/* 0x001a1b28 */
void njUnitMatrix(NjMatrix *mtx)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_identity(mtx);
}

/* 0x001a1b38 */
static void njTranslate(NjMatrix *mtx, float x, float y, float z)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_translate(mtx, x, y, z);
}

/* 0x001a1b48 */
static void njTranslateV(NjMatrix *mtx, NjVec3 *vec)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_translate(mtx, vec->x, vec->y, vec->z);
}

/* 0x001a1b60 */
void njTranslateEx(NjVec3 *vec)
{
    vu0_matrix_translate(ulMatrixCur, vec->x, vec->y, vec->z);
}

/* 0x001a1b78 */
static void njScale(NjMatrix *mtx, float x, float y, float z)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_scale(mtx, x, y, z);
}

/* 0x001a1b88 */
static void njScaleV(NjMatrix *mtx, NjVec3 *vec)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_scale(mtx, vec->x, vec->y, vec->z);
}

/* 0x001a1ba0 */
void njScaleEx(NjVec3 *vec)
{
    vu0_matrix_scale(ulMatrixCur, vec->x, vec->y, vec->z);
}

/* 0x001a1bb8 */
void njRotateX(NjMatrix *mtx, int ang)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_rotate_x(mtx, ang);
}

/* 0x001a1bc8 */
void njRotateY(NjMatrix *mtx, int ang)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_rotate_y(mtx, ang);
}

/* 0x001a1bd8 */
void njRotateZ(NjMatrix *mtx, int ang)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_rotate_z(mtx, ang);
}

/* z, then y, then x. */
/* 0x001a1be8 */
static void nj_matrix_rotate_zyx(NjMatrix *mtx, int ax, int ay, int az)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_rotate_z(mtx, az);
    vu0_matrix_rotate_y(mtx, ay);
    vu0_matrix_rotate_x(mtx, ax);
}

/* ang[0..2] = x, y, z. */
/* 0x001a1c48 */
void nj_matrix_rotate_cur_ang3(int *ang)
{
    nj_matrix_rotate_zyx(0, ang[0], ang[1], ang[2]);
}

/* mtx must be 16-byte aligned (warns). */
/* 0x001a1c60 */
void njGetMatrix(NjMatrix *mtx)
{
    if ((int)mtx & 0xf)
        printf("njGetMatrix(%08x): pointer is not 16byte align!\n", mtx);
    *mtx = *ulMatrixCur;
}

/* Both 16-byte aligned (warns). */
/* 0x001a1ce0 */
void njSetMatrix(NjMatrix *dst, NjMatrix *src)
{
    if (!dst)
        dst = ulMatrixCur;
    if (((int)src & 0xf) || ((int)dst & 0xf))
        printf("njSetMatrix(%08x,%08x): pointer is not 16byte align!\n", dst, src);
    *dst = *src;
}

/* In place. */
/* 0x001a1d78 */
void njMultiMatrix(NjMatrix *mtx, NjMatrix *rhs)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_mul(mtx, mtx, rhs);
}

/* In place, rotation + translation only; returns 1. */
/* 0x001a1d90 */
static int njInvertMatrix(NjMatrix *mtx)
{
    if (!mtx)
        mtx = ulMatrixCur;
    vu0_matrix_inverse_rigid(mtx, mtx);
    return 1;
}

/* 0x001a1dc0 */
static void nj_matrix_unit_rotation(NjMatrix *mtx)
{
    if (!mtx)
        mtx = ulMatrixCur;
    ulMatrixUnitRotation(mtx);
}

/* 0x001a1dd0 */
static void nj_matrix_get_translation(NjMatrix *mtx)
{
    if (!mtx)
        mtx = ulMatrixCur;
    ulMatrixGetTranslation(mtx);
}

/* Returns the previous length. */
/* 0x001a1de0 */
static float njUnitVector(NjVec3 *vec)
{
    float len, inv;

    len = sqrtf(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
    inv = 1.0f / len;
    vec->x *= inv;
    vec->y *= inv;
    vec->z *= inv;
    return len;
}

/* 0x001a1e80 */
static float njInnerProduct(NjVec3 *lhs, NjVec3 *rhs)
{
    return lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z;
}

/* Returns its length. */
/* 0x001a1eb0 */
static float njOuterProduct(NjVec3 *lhs, NjVec3 *rhs, NjVec3 *out)
{
    float x, y, z;

    out->x = x = lhs->y * rhs->z - lhs->z * rhs->y;
    out->y = y = lhs->z * rhs->x - lhs->x * rhs->z;
    out->z = z = lhs->x * rhs->y - lhs->y * rhs->x;
    return sqrtf(x * x + y * y + z * z);
}

/* 0x001a1f60 */
static void njAddVector(NjVec3 *dst, NjVec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

/* 0x001a1f98 */
static void njSubVector(NjVec3 *dst, NjVec3 *src)
{
    dst->x -= src->x;
    dst->y -= src->y;
    dst->z -= src->z;
}

/* With translation. */
/* 0x001a1fd0 */
static void njCalcPoint(NjMatrix *mtx, NjVec3 *in, NjVec3 *out)
{
    if (!mtx)
        mtx = ulMatrixCur;
    ulMatrixApplyPoint(mtx, in, out);
}

/* 0x001a1fe0 */
static void njCalcPoints(NjMatrix *mtx, NjVec3 *in, NjVec3 *out, int n)
{
    int i;

    if (!mtx)
        mtx = ulMatrixCur;
    for (i = 0; i < n; i++)
        ulMatrixApplyPoint(mtx, in++, out++);
}

/* Rotation only. */
/* 0x001a2050 */
static void njCalcVector(NjMatrix *mtx, NjVec3 *in, NjVec3 *out)
{
    if (!mtx)
        mtx = ulMatrixCur;
    ulMatrixApplyVector(mtx, in, out);
}
