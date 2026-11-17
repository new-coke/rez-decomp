/* Vector and 4x4 matrix helpers, matrix stack, axis rotations and the camera look-at matrix. */

#include "types_common.h"
#include "qword.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_matrix.h"
#include "ul_console.h"
#include "ul_vu0.h"

static float *ulMatrixInvRotateZ(float *mtx, float angle);
static float *ulMatrixInvRotateY(float *mtx, float angle);
static float *ulMatrixInvRotateX(float *mtx, float angle);
static float *ulMatrixInvTranslate(float *mtx, float x, float y, float z);
static float *ulMatrixIdentity(float *mtx);

typedef int Q REZ_QWORD;

extern int ulMatrixMaxDepth;
extern float *ulMatrixCur;
extern int ulMatrixDepth;
extern int ulMatrixCapacity;
extern float *ulMatrixBase;
extern void vu0_matrix_identity(float *m);

/* 0x001b4eb0 */
static void ulVec3Zero(float *vec)
{
    vec[0] = vec[1] = vec[2] = 0.0f;
}

/* 0x001b4ec8 */
static void ulVec3Add(float *out, float *lhs, float *rhs)
{
    out[0] = lhs[0] + rhs[0];
    out[1] = lhs[1] + rhs[1];
    out[2] = lhs[2] + rhs[2];
}

/* out may be an input. */
/* 0x001b4f00 */
void ulVec3Cross(Vec3 *out, Vec3 *lhs, Vec3 *rhs)
{
    float x;
    float y;
    float z;

    x = lhs->y * rhs->z - lhs->z * rhs->y;
    y = lhs->z * rhs->x - lhs->x * rhs->z;
    z = lhs->x * rhs->y - lhs->y * rhs->x;
    out->y = y;
    out->z = z;
    out->x = x;
}

/* 0x001b4f50 */
float ulVec3Dot(Vec3 *lhs, Vec3 *rhs)
{
    return lhs->x * rhs->x + lhs->y * rhs->y + lhs->z * rhs->z;
}

/* Deepest since ulMatrixStackInit. */
/* 0x001b4f80 */
static int ulMatrixGetMaxDepth(void)
{
    return ulMatrixMaxDepth;
}

/* mtx 0 = current matrix; one line per column of the stored rows. */
/* 0x001b4f88 */
static void ulMatrixPrintConsole(float *mtx)
{
    float *src;
    float *row0;
    float *row1;
    float *row2;
    float *row3;
    int i;

    src = ulMatrixCur;
    if (mtx)
        src = mtx;
    row0 = src;
    row1 = src + 4;
    row2 = src + 8;
    row3 = src + 12;
    for (i = 0; i < 4; i++) {
        ulConsolePrintf("|% 8.3f % 8.3f % 8.3f % 8.3f|\n", row0[i], row1[i], row2[i], row3[i]);
    }
}

/* mtx 0 = current matrix. */
/* 0x001b5068 */
static void ulMatrixPrint(float *mtx)
{
    float *src;
    float *row0;
    float *row1;
    float *row2;
    float *row3;
    int i;

    src = ulMatrixCur;
    if (mtx)
        src = mtx;
    row0 = src;
    row1 = src + 4;
    row2 = src + 8;
    row3 = src + 12;
    for (i = 0; i < 4; i++) {
        printf("|% 8.3f % 8.3f % 8.3f % 8.3f|\n", row0[i], row1[i], row2[i], row3[i]);
    }
}

/* Rotation part only; out may equal in. */
/* 0x001b5148 */
void ulMatrixApplyVector(float *mtx, float *in, float *out)
{
    float x, y, z;

    x = mtx[0] * in[0] + mtx[4] * in[1];
    y = mtx[1] * in[0] + mtx[5] * in[1];
    z = mtx[2] * in[0] + mtx[6] * in[1];
    x += mtx[8] * in[2];
    y += mtx[9] * in[2];
    z += mtx[10] * in[2];
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

/* With translation; out may equal in. */
/* 0x001b51c8 */
void ulMatrixApplyPoint(float *mtx, float *in, float *out)
{
    float x, y, z;

    x = mtx[0] * in[0] + mtx[4] * in[1];
    y = mtx[1] * in[0] + mtx[5] * in[1];
    z = mtx[2] * in[0] + mtx[6] * in[1];
    x += mtx[8] * in[2];
    y += mtx[9] * in[2];
    z += mtx[10] * in[2];
    x += mtx[12];
    y += mtx[13];
    z += mtx[14];
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

/* Into the current matrix; returns mtx. */
/* 0x001b5260 */
float *ulMatrixLoad(float *mtx)
{
    vu0_matrix_copy(ulMatrixCur, mtx);
    return mtx;
}

/* Returns the new current matrix; hangs on underflow. */
/* 0x001b5290 */
char *ulMatrixPop(void)
{
    if (ulMatrixDepth <= 0) {
        printf("ulMatrixPop: matrix stack underflow!\n");
        for (;;)
            ;
    }
    ulMatrixDepth--;
    (*(char * *)&ulMatrixCur) -= 64; /* steer(cast) */
    return (char *)ulMatrixCur;
}

/* Copies the current matrix into the next entry, which becomes current. Hangs on overflow. */
/* 0x001b52f0 */
Q *ulMatrixPush(void)
{
    int depth;

    depth = ulMatrixDepth;
    if (!(depth < ulMatrixCapacity)) {
        printf("ulMatrixPush: matrix stack overflow!\n");
        for (;;)
            ;
    }
    (*(Q * *)&ulMatrixCur)[4] = (*(Q * *)&ulMatrixCur)[0]; /* steer(cast) */
    (*(Q * *)&ulMatrixCur)[5] = (*(Q * *)&ulMatrixCur)[1]; /* steer(cast) */
    (*(Q * *)&ulMatrixCur)[6] = (*(Q * *)&ulMatrixCur)[2]; /* steer(cast) */
    (*(Q * *)&ulMatrixCur)[7] = (*(Q * *)&ulMatrixCur)[3]; /* steer(cast) */
    (*(Q * *)&ulMatrixCur) += 4; /* steer(cast) */
    ulMatrixDepth = depth + 1;
    if (ulMatrixMaxDepth < ulMatrixDepth)
        ulMatrixMaxDepth = ulMatrixDepth;
    return (Q *)ulMatrixCur;
}

/* cap matrices; the first is current, set to identity. */
/* 0x001b5388 */
float *ulMatrixStackInit(float *base, int cap)
{
    ulMatrixCapacity = cap;
    ulMatrixBase = base;
    ulMatrixMaxDepth = 0;
    ulMatrixCur = base;
    ulMatrixDepth = 0;
    vu0_matrix_identity(base);
    return ulMatrixBase;
}

/* Translation and projective parts reset. */
/* 0x001b53c0 */
void ulMatrixCopyRotation(float *dst, float *src)
{
    vu0_matrix_copy(dst, src);
    dst[12] = dst[13] = dst[14] = dst[3] = dst[7] = dst[11] = 0.0f;
    dst[15] = 1.0f;
}

/* 0x001b5410 */
void ulMatrixSetTranslation(float *mtx, float x, float y, float z)
{
    mtx[13] = y;
    mtx[14] = z;
    mtx[12] = x;
}

/* 0x001b5420 */
void ulMatrixGetTranslation(float *mtx, float *out)
{
    out[0] = mtx[12];
    out[1] = mtx[13];
    out[2] = mtx[14];
}

/* Keeps translation. volatile keeps the stores in source order. */
/* 0x001b5440 */
void ulMatrixUnitRotation(volatile float *mtx)
{
    mtx[0] = 1.0f;
    mtx[5] = 1.0f;
    mtx[10] = 1.0f;
    mtx[4] = 0;
    mtx[8] = 0;
    mtx[1] = 0;
    mtx[9] = 0;
    mtx[6] = 0;
    mtx[2] = 0;
}

/* mtx 0 = current; radians. */
/* 0x001b5470 */
static void ulMatrixRotateZ(float *mtx, float angle)
{
    float *dst;

    dst = ulMatrixCur;
    if (mtx)
        dst = mtx;
    vu0_matrix_rotate_z_sc(dst, sinf(angle), cosf(angle));
}

/* mtx 0 = current; radians. */
/* 0x001b54c8 */
static void ulMatrixRotateY(float *mtx, float angle)
{
    float *dst;

    dst = ulMatrixCur;
    if (mtx)
        dst = mtx;
    vu0_matrix_rotate_y_sc(dst, sinf(angle), cosf(angle));
}

/* mtx 0 = current; radians. */
/* 0x001b5520 */
static void ulMatrixRotateX(float *mtx, float angle)
{
    float *dst;

    dst = ulMatrixCur;
    if (mtx)
        dst = mtx;
    vu0_matrix_rotate_x_sc(dst, sinf(angle), cosf(angle));
}

/* Inverse roll, pitch, yaw, then inverse eye translation; angles quantised to 16 bits. */
/* 0x001b5578 */
float *ulMatrixLookAt(float *mtx, float *eye, float *target, float roll)
{
    float dx;
    float dy;
    float dz;
    int yaw;
    int pitch;

    dx = target[0] - eye[0];
    dz = target[2] - eye[2];
    dy = target[1] - eye[1];
    yaw = (int)(atan2(-dx, -dz) * 65536.0 / TWO_PI);
    pitch = (int)(atan2(dy, sqrtf(dx * dx + dz * dz)) * 65536.0 / TWO_PI);
    ulMatrixIdentity(mtx);
    ulMatrixInvRotateZ(mtx, (float)((int)RAD_TO_ANG(roll) & 0xffff) * TWO_PI * 1.52587890625e-05f);
    ulMatrixInvRotateX(mtx, (float)(pitch & 0xffff) * TWO_PI * 1.52587890625e-05f);
    ulMatrixInvRotateY(mtx, (float)(yaw & 0xffff) * TWO_PI * 1.52587890625e-05f);
    ulMatrixInvTranslate(mtx, eye[0], eye[1], eye[2]);
    return mtx;
}

/* By -angle. */
/* 0x001b5770 */
static float *ulMatrixInvRotateZ(float *mtx, float angle)
{
    ulMatrixRotateZ(mtx, -angle);
    return mtx;
}

/* By -angle. */
/* 0x001b57a0 */
static float *ulMatrixInvRotateY(float *mtx, float angle)
{
    ulMatrixRotateY(mtx, -angle);
    return mtx;
}

/* By -angle. */
/* 0x001b57d0 */
static float *ulMatrixInvRotateX(float *mtx, float angle)
{
    ulMatrixRotateX(mtx, -angle);
    return mtx;
}

/* By (-x, -y, -z). */
/* 0x001b5800 */
static float *ulMatrixInvTranslate(float *mtx, float x, float y, float z)
{
    vu0_matrix_translate(mtx, -x, -y, -z);
    return mtx;
}

/* 0x001b5838 */
static float *ulMatrixIdentity(float *mtx)
{
    vu0_matrix_identity(mtx);
    return mtx;
}
