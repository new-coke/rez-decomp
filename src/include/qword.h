/* 128-bit quadword attribute (TImode, as in eetypes.h) and its zero. */

#ifndef REZ_QWORD_H
#define REZ_QWORD_H
#define REZ_QWORD __attribute__((mode(TI)))
#define REZ_QWORD_ZERO(T) 0
#endif
