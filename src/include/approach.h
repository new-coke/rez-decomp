/* Approach, lerp and damped-spring helpers. */

#ifndef REZ_APPROACH_H
#define REZ_APPROACH_H

struct Vec3;
struct IVec3;

void vec3_approach(struct Vec3 *target, struct Vec3 *cur, float t);
void ang3_approach(struct IVec3 *target, struct IVec3 *cur, float t);
float float_lerp(float target, float base, float t);
int int_lerp_f(float t, int target, int base);
int ang_approach(int target, int cur, float t);
void spring3_step(float stiff, float damp, struct Vec3 *target, struct Vec3 *pos, struct Vec3 *vel, struct Vec3 *acc);
void spring_step_pos(float *pos, float *vel, float *acc, float target, float stiff, float damp);
void vec3_sub_scale(float t, struct Vec3 *target, struct Vec3 *base, struct Vec3 *out);
void spring3_step_vel(struct Vec3 *target, struct Vec3 *pos, struct Vec3 *vel, struct Vec3 *acc, float stiff, float damp);

#endif /* REZ_APPROACH_H */
