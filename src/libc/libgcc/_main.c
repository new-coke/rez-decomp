#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"

static void __do_global_ctors(void);

extern func_ptr *libgcc_dtor_ptr;
extern func_ptr libgcc_ctor_list[];
extern int libgcc_main_initialized;
extern void __do_global_ctors(void);

/*
 * Runs the static destructor list: calls each function through the cursor libgcc_dtor_ptr until a
 * null entry.
 */
/* 0x001ea188 */
static void __do_global_dtors(void)
{
    while (*libgcc_dtor_ptr) {
        libgcc_dtor_ptr++;
        (*(libgcc_dtor_ptr - 1))();
    }
}

/*
 * Runs the static constructor list libgcc_ctor_list (count in entry 0, or counted up to the null
 * terminator when it is -1) from the last entry to the first.
 */
/* 0x001ea1e0 */
static void __do_global_ctors(void)
{
    do {
        unsigned long nptrs = (unsigned long)libgcc_ctor_list[0];
        unsigned i;

        if (nptrs == (unsigned long)-1)
            for (nptrs = 0; libgcc_ctor_list[nptrs + 1] != 0; nptrs++)
                ;
        for (i = nptrs; i >= 1; i--)
            libgcc_ctor_list[i]();
    } while (0);
}

/* Runs the static constructors (__do_global_ctors) once, guarded by libgcc_main_initialized. */
/* 0x001ea290 */
static void __main(void)
{
    if (!libgcc_main_initialized) {
        libgcc_main_initialized = 1;
        __do_global_ctors();
    }
}
