/* Ninja cameras: position, view vector, roll, fov -> view matrix and projection; look-at. */

#include "ninja.h"
#include "mathdef.h"
#include "libc.h"
#include "nj_camera.h"
#include "nj_matrix.h"
#include "nj_view.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"

static void nj_matrix_rotate_x_inv(int ang);
static void nj_matrix_rotate_y_inv(int ang);
static void nj_matrix_rotate_z_inv(int ang);
static void nj_matrix_move(float x, float y, float z);

extern void vu0_matrix_identity(NjMatrix *m);

/* Origin, looking down -z, no roll, 60-degree fov, mode 1. */
/* 0x001a28e8 */
static void nj_camera_init(NjCamera *cam)
{
    cam->vx = cam->vy = cam->px = cam->py = cam->pz = 0.0f;
    cam->vz = -1.0f;
    cam->roll = 0;
    cam->ang = 0x2aaa;
    cam->mode = 1;
}

/* View from the camera at point v (mode 2) or along v (other modes), rolled; also sets the fov. */
/* 0x001a2928 */
void nj_camera_apply(NjCamera *cam)
{
    NjVec3 target;
    NjMatrix mtx;

    if (cam->mode == 2) {
        target.x = cam->vx;
        target.y = cam->vy;
        target.z = cam->vz;
    } else {
        target.x = cam->px + cam->vx;
        target.y = cam->py + cam->vy;
        target.z = cam->pz + cam->vz;
    }
    ulMatrixLookAt(&mtx, cam, &target, (float)cam->roll * (3.141592f / ANG_HALF));
    ulMatrixLoad(&mtx);
    njSetPerspective(cam->ang);
    ulGxSetMatrix(1, ulMatrixCur);
}

/* View rotations turn the world opposite to the viewer. */
/* 0x001a29f0 */
static void nj_view_rotate_x(int ang)
{
    nj_matrix_rotate_x_inv(ang);
}

/* 0x001a29f8 */
static void nj_view_rotate_y(int ang)
{
    nj_matrix_rotate_y_inv(ang);
}

/* Except roll: the double negation turns the current matrix by +ang. */
/* 0x001a2a00 */
static void nj_view_rotate_z(int ang)
{
    nj_matrix_rotate_z_inv(-ang);
}

/* Viewer forward by dist (world moves back along -z). */
/* 0x001a2a08 */
static void nj_view_move_z(float dist)
{
    nj_matrix_move(0.0f, 0.0f, -dist);
}

/* Along its view vector. */
/* 0x001a2a18 */
static void nj_camera_move(NjCamera *cam, float dist)
{
    cam->px += cam->vx * dist;
    cam->py += cam->vy * dist;
    cam->pz += cam->vz * dist;
}

/* View vector about x; 65536 = full turn. */
/* 0x001a2a58 */
static void nj_camera_rotate_x(NjCamera *cam, int ang)
{
    NjVec3 dir;
    NjMatrix mtx;

    vu0_matrix_identity(&mtx);
    vu0_matrix_rotate_x(&mtx, ang);
    dir.x = cam->vx;
    dir.y = cam->vy;
    dir.z = cam->vz;
    ulMatrixApplyVector(&mtx, &dir, &dir);
    cam->vx = dir.x;
    cam->vy = dir.y;
    cam->vz = dir.z;
}

/* View vector about y. */
/* 0x001a2ae8 */
static void nj_camera_rotate_y(NjCamera *cam, int ang)
{
    NjVec3 dir;
    NjMatrix mtx;

    vu0_matrix_identity(&mtx);
    vu0_matrix_rotate_y(&mtx, ang);
    dir.x = cam->vx;
    dir.y = cam->vy;
    dir.z = cam->vz;
    ulMatrixApplyVector(&mtx, &dir, &dir);
    cam->vx = dir.x;
    cam->vy = dir.y;
    cam->vz = dir.z;
}

/* Current matrix *= look-at from eye to target; up made perpendicular to the view direction. */
/* 0x001a2b78 */
static void nj_camera_look_at(NjVec3 *eye, NjVec3 *target, NjVec3 *up)
{
    NjMatrix mtx;
    float px;
    float py;
    float pz;
    float zx;
    float zy;
    float zz;
    float ux;
    float uy;
    float uz;
    float xx;
    float xy;
    float xz;
    float len_sq;
    float dot;
    float inv;

    px = eye->x;
    py = eye->y;
    pz = eye->z;
    zx = px - target->x;
    zy = py - target->y;
    zz = pz - target->z;
    inv = (float)(1.0 / sqrt(zx * zx + zy * zy + zz * zz));
    zx *= inv;
    zy *= inv;
    zz *= inv;
    ux = up->x;
    uy = up->y;
    uz = up->z;
    dot = ux * zx + uy * zy + uz * zz;
    ux -= dot * zx;
    uy -= dot * zy;
    uz -= dot * zz;
    len_sq = ux * ux;
    len_sq += uy * uy;
    len_sq += uz * uz;
    inv = (float)(1.0 / sqrt(len_sq));
    ux *= inv;
    uy *= inv;
    uz *= inv;
    xx = uy * zz - uz * zy;
    xy = uz * zx - ux * zz;
    xz = ux * zy - uy * zx;
    vu0_matrix_identity(&mtx);
    do {
        mtx.m[0] = xx;
        mtx.m[4] = xy;
        mtx.m[8] = xz;
        mtx.m[1] = ux;
        mtx.m[5] = uy;
        mtx.m[9] = uz;
        mtx.m[2] = zx;
        mtx.m[6] = zy;
        mtx.m[10] = zz;
    } while (0);
    do {
        mtx.m[12] = -xx * px - xy * py - xz * pz;
    } while (0);
    do {
        mtx.m[13] = -ux * px - uy * py - uz * pz;
    } while (0);
    do {
        mtx.m[14] = -zx * px - zy * py - zz * pz;
    } while (0);
    vu0_matrix_mul(ulMatrixCur, &mtx, ulMatrixCur);
}

/* By -ang. */
/* 0x001a2db0 */
static void nj_matrix_rotate_x_inv(int ang)
{
    njRotateX(0, -ang);
}

/* By -ang. */
/* 0x001a2dc0 */
static void nj_matrix_rotate_y_inv(int ang)
{
    njRotateY(0, -ang);
}

/* By -ang. */
/* 0x001a2dd0 */
static void nj_matrix_rotate_z_inv(int ang)
{
    njRotateZ(0, -ang);
}

/* Adds to the current matrix translation. */
/* 0x001a2de0 */
static void nj_matrix_move(float x, float y, float z)
{
    NjVec3 pos;

    ulMatrixGetTranslation(ulMatrixCur, &pos);
    pos.x += x;
    pos.y += y;
    pos.z += z;
    ulMatrixSetTranslation(ulMatrixCur, pos.x, pos.y, pos.z);
}

/* 0x001a2e60 */
static void nj_camera_nop(void)
{
}
