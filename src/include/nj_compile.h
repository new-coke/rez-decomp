/* Chunk-model compile to VU1 upload data. */

#ifndef REZ_NJ_COMPILE_H
#define REZ_NJ_COMPILE_H

struct NjCnkModel;
struct NjObject;
struct NjVlistInfo;
struct NjCnkObject;

void nj_compile_call_hook(void *obj);
struct NjCnkObject *nj_compile_object_refresh(struct NjCnkObject *dst, struct NjCnkObject *src);
struct NjCnkObject *nj_compile_object(struct NjCnkObject *obj);

#endif /* REZ_NJ_COMPILE_H */
