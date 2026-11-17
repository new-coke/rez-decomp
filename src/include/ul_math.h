/* Colour packing, distance, 16-bit-angle trig. */

#ifndef REZ_UL_MATH_H
#define REZ_UL_MATH_H

void ulColorUnpack(unsigned int col, float *out);
unsigned int ulColorPack(float *col);
float ulVec3Distance(struct Vec3 *from, struct Vec3 *to);
int ulAsin(float x);
float ulTan(int angle);
float ulSinPoly(int angle);
float ulSin(int angle);
float ulCos(int angle);
void ulSinCos(int angle, float *sin_out, float *cos_out);
void ulSinCosf(float angle, float *s, float *c);
float ulRoundAway(float x);

#endif /* REZ_UL_MATH_H */
