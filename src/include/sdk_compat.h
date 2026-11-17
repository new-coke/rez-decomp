/*
 * Sony SDK and toolchain macros the game uses, restated from their documented behaviour. The
 * only header allowed inline assembly.
 */

#include "qword.h"
#ifndef REZ_SDK_COMPAT_H
#define REZ_SDK_COMPAT_H

/* eekernel.h ExitHandler(): ends an interrupt handler */
#define ExitHandler() __asm__ volatile ("sync.l\n\tei")

/* Waits for outstanding loads and stores (first half of ExitHandler). */
#define SYNC_L() __asm__ volatile ("sync.l")

/* Eight-nop delay after a successful libuliop module load; host build compiles it unchanged. */
#define NOP8() __asm__ volatile ("nop; nop; nop; nop; nop; nop; nop; nop")

/* longlong.h umul_ppmm (libgcc 64-bit division): 32 x 32 -> 64, high word to w1, low to w0. */
#define umul_ppmm(w1, w0, u, v) __asm__ ("multu %2,%3" : "=l" ((unsigned int)(w0)), "=h" ((unsigned int)(w1)) : "d" ((unsigned int)(u)), "d" ((unsigned int)(v)))

/* eetypes.h u_long128: a 128-bit quadword */
typedef unsigned int u_long128 REZ_QWORD;

#endif
