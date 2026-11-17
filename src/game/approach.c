/* Approach helpers (values, vectors, 16-bit angles toward a target) and damped springs. */

#include "types_game.h"
#include "approach.h"

/* 0x001011d8 */
void vec3_approach(Vec3 *target, Vec3 *cur, float t)
{
    float az = target->z;
    float ay = target->y;
    float ax = target->x;
    float bx = cur->x;
    float by = cur->y;
    float bz = cur->z;
    cur->x = bx + (ax - bx) * t;
    cur->y = by + (ay - by) * t;
    cur->z = bz + (az - bz) * t;
}

/* Differences wrapped to signed 16 bits: short way round. */
/* 0x00101228 */
void ang3_approach(IVec3 *target, IVec3 *cur, float t)
{
    int ax, ay, az;
    int bx, by, bz;

    do {
        ax = target->x;
        ay = target->y;
        az = target->z;
    } while (0);
    do {
        bx = cur->x;
        by = cur->y;
        bz = cur->z;
    } while (0);
    ax -= bx;
    ay -= by;
    az -= bz;
    ax = (ax & 0x8000) ? (ax | (int)0xffff0000) : (ax & 0xffff);
    ay = (ay & 0x8000) ? (ay | (int)0xffff0000) : (ay & 0xffff);
    az = (az & 0x8000) ? (az | (int)0xffff0000) : (az & 0xffff);
    bx += (int)((float)ax * t);
    by += (int)((float)ay * t);
    bz += (int)((float)az * t);
    cur->x = bx;
    cur->y = by;
    cur->z = bz;
}

/* 0x001012f0 */
float float_lerp(float target, float base, float t)
{
    return base + (target - base) * t;
}

/* 0x00101300 */
int int_lerp_f(float t, int target, int base)
{
    return base + (int)((float)(target - base) * t);
}

/* Short way round. */
/* 0x00101320 */
int ang_approach(int target, int cur, float t)
{
    int diff = ((target - cur) & 0x8000) ? ((target - cur) | 0xffff0000) : ((target - cur) & 0xffff);
    return cur + (int)((float)diff * t);
}

/* vel += acc + (target - pos) * stiff - vel * damp; pos += vel; acc cleared. */
/* 0x00101358 */
void spring3_step(float stiff, float damp, Vec3 *target, Vec3 *pos, Vec3 *vel, Vec3 *acc)
{
    float ax, ay, az, vx, vy, vz, px, py, pz, tx, ty, tz;

    do {
        ax = acc->x;
        ay = acc->y;
        az = acc->z;
    } while (0);
    do {
        vx = vel->x;
        vy = vel->y;
        vz = vel->z;
    } while (0);
    do {
        px = pos->x;
        py = pos->y;
        pz = pos->z;
    } while (0);
    tx = target->x;
    ty = target->y;
    tz = target->z;
    vx += ax + (tx - px) * stiff - vx * damp;
    vy += ay + (ty - py) * stiff - vy * damp;
    vz += az + (tz - pz) * stiff - vz * damp;
    px += vx;
    py += vy;
    pz += vz;
    pos->x = px;
    pos->y = py;
    pos->z = pz;
    vel->x = vx;
    vel->y = vy;
    vel->z = vz;
    acc->x = 0;
    acc->y = 0;
    acc->z = 0;
}

/* 1D spring3_step. */
/* 0x00101408 */
void spring_step_pos(float *pos, float *vel, float *acc, float target, float stiff, float damp)
{
    float px = *pos;
    float vx = *vel;

    vx += *acc + (target - px) * stiff - vx * damp;
    px += vx;
    *pos = px;
    *vel = vx;
    *acc = 0;
}

/* out = (target - base) * t. */
/* 0x00101440 */
void vec3_sub_scale(float t, Vec3 *target, Vec3 *base, Vec3 *out)
{
    float ax, ay, az, bx, by, bz;

    do {
        ax = target->x;
        ay = target->y;
        az = target->z;
    } while (0);
    do {
        bx = base->x;
        by = base->y;
        bz = base->z;
    } while (0);
    ax -= bx;
    ay -= by;
    az -= bz;
    bx = ax * t;
    by = ay * t;
    bz = az * t;
    out->x = bx;
    out->y = by;
    out->z = bz;
}

/* 0x00101480 */
static void float_sub_scale(float *out, float target, float base, float scale)
{
    *out = (target - base) * scale;
}

/* Velocity only; caller moves pos. */
/* 0x00101490 */
void spring3_step_vel(Vec3 *target, Vec3 *pos, Vec3 *vel, Vec3 *acc, float stiff, float damp)
{
    float ax, ay, az, vx, vy, vz, px, py, pz, tx, ty, tz;

    do {
        ax = acc->x;
        ay = acc->y;
        az = acc->z;
    } while (0);
    do {
        vx = vel->x;
        vy = vel->y;
        vz = vel->z;
    } while (0);
    do {
        px = pos->x;
        py = pos->y;
        pz = pos->z;
    } while (0);
    tx = target->x;
    ty = target->y;
    tz = target->z;
    vx += ax + (tx - px) * stiff - vx * damp;
    vy += ay + (ty - py) * stiff - vy * damp;
    vz += az + (tz - pz) * stiff - vz * damp;
    vel->x = vx;
    vel->y = vy;
    vel->z = vz;
    acc->x = 0;
    acc->y = 0;
    acc->z = 0;
}

/* Velocity only. */
/* 0x00101528 */
static void spring_step_vel(float *pos, float *vel, float *acc, float target, float stiff, float damp)
{
    *vel += *acc + (target - *pos) * stiff - *vel * damp;
    *acc = 0;
}
