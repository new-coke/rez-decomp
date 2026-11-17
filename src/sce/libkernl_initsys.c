/*
 * EE kernel library start-up: semaphores, kernel patches, deferred-operation thread, before main.
 */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_initsys.h"
#include "libkernl_patch.h"

extern int libkernl_sema0;
extern int libkernl_sema1;
extern int CreateSema(SemaParam *p);

/* libkernl_sema0 and libkernl_sema1, binary, initial count 1. */
/* 0x001fdcf0 */
static void libkernl_create_semas(void)
{
    SemaParam sema0_param;
    SemaParam sema1_param;

    sema0_param.max_count = 1;
    sema0_param.init_count = 1;
    sema1_param.max_count = 1;
    sema1_param.init_count = 1;
    libkernl_sema0 = CreateSema(&sema0_param);
    libkernl_sema1 = CreateSema(&sema1_param);
}

/* 0x001fdd38 */
void libkernl_startup(void)
{
    libkernl_create_semas();
    libkernl_install_patches();
    libkernl_start_defer_thread();
}
