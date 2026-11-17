/* Ninja matrix stack. */

#ifndef REZ_NJ_MATRIX_H
#define REZ_NJ_MATRIX_H

#include "types_nj.h"

struct NjVec3;

int njPushMatrixEx(void);
int njPopMatrixEx(void);
void njUnitMatrix(NjMatrix *mtx);
void njTranslateEx(struct NjVec3 *vec);
void njScaleEx(struct NjVec3 *vec);
void njRotateX(NjMatrix *mtx, int ang);
void njRotateY(NjMatrix *mtx, int ang);
void njRotateZ(NjMatrix *mtx, int ang);
void nj_matrix_rotate_cur_ang3(int *ang);
void njGetMatrix(NjMatrix *mtx);
void njSetMatrix(NjMatrix *dst, NjMatrix *src);
void njMultiMatrix(NjMatrix *mtx, NjMatrix *rhs);

#endif /* REZ_NJ_MATRIX_H */
