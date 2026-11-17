#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

/*
 * Reentrant calloc: allocates n*size with _malloc_r (malloc) and clears the chunk's usable size
 * (inline stores up to 36 bytes, else memset memset).
 */
/* 0x001ec9c8 */
void *_calloc_r(struct _reent_a *r, unsigned int n, unsigned int elem_size)
{
    unsigned int csz;
    unsigned int sz = n * elem_size;
    void *mem = _malloc_r(r, sz);

    if (mem == 0)
        return 0;
    else {
        csz = ((unsigned int *)mem)[-1] & ~3;
        do {
            unsigned int mzsz = (csz - 4);

            if (mzsz <= 9 * sizeof(mzsz)) {
                unsigned int *mz = (unsigned int *)(mem);

                if (mzsz >= 5 * sizeof(mzsz)) {
                    *mz++ = 0;
                    *mz++ = 0;
                    if (mzsz >= 7 * sizeof(mzsz)) {
                        *mz++ = 0;
                        *mz++ = 0;
                        if (mzsz >= 9 * sizeof(mzsz)) {
                            *mz++ = 0;
                            *mz++ = 0;
                        }
                    }
                }
                *mz++ = 0;
                *mz++ = 0;
                *mz = 0;
            } else
                memset((mem), 0, mzsz);
        } while (0);
        return mem;
    }
}
