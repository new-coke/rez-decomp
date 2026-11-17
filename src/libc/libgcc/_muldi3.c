#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "sdk_compat.h"

typedef union {
    struct {
        int low;
        int high;
    } s;
    long ll;
} DIunion;

/*
 * 64-bit multiply from 32-bit halves: low x low as a widening unsigned multiply, plus the cross
 * products added into the high word. The low x low product is longlong.h's __umulsidi3, a
 * statement expression around its MIPS umul_ppmm (sdk_compat.h).
 */
/* 0x001ec8e0 */
static long __muldi3(long u, long v)
{
    DIunion w;
    DIunion uu, vv;

    uu.ll = u, vv.ll = v;
    w.ll = ({DIunion __w;
             umul_ppmm(__w.s.high, __w.s.low, uu.s.low, vv.s.low);
             __w.ll; });
    w.s.high += ((unsigned int)uu.s.low * (unsigned int)vv.s.high
                 + (unsigned int)uu.s.high * (unsigned int)vv.s.low);
    return w.ll;
}
