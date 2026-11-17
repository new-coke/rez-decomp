/* Kernel patches, deferred-operation thread, kernel call wrappers. */

#ifndef REZ_LIBKERNL_PATCH_H
#define REZ_LIBKERNL_PATCH_H

struct DeferQueue;

void libkernl_install_patches(void);
int libkernl_start_defer_thread(void);
void RFU090(void *dst, void *src, int size);
void *RFU091(int num);
void RFU116_SetSyscall(int num, void *func);
int iWakeupThread(int tid);

#endif /* REZ_LIBKERNL_PATCH_H */
