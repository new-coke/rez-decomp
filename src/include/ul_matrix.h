/* Vectors, matrices, matrix stack, look-at. */

#ifndef REZ_UL_MATRIX_H
#define REZ_UL_MATRIX_H

void ulVec3Cross(struct Vec3 *out, struct Vec3 *lhs, struct Vec3 *rhs);
float ulVec3Dot(struct Vec3 *lhs, struct Vec3 *rhs);
void ulMatrixApplyVector(float *mtx, float *in, float *out);
void ulMatrixApplyPoint(float *mtx, float *in, float *out);
float *ulMatrixLoad(float *mtx);
char *ulMatrixPop(void);
float *ulMatrixStackInit(float *base, int cap);
void ulMatrixCopyRotation(float *dst, float *src);
void ulMatrixSetTranslation(float *mtx, float x, float y, float z);
void ulMatrixGetTranslation(float *mtx, float *out);
void ulMatrixUnitRotation(volatile float *mtx);
float *ulMatrixLookAt(float *mtx, float *eye, float *target, float roll);

#endif /* REZ_UL_MATRIX_H */
