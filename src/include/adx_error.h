/* ADX error reporting, shared helpers. */

#ifndef REZ_ADX_ERROR_H
#define REZ_ADX_ERROR_H
#include "types_cri.h"

extern char adx_error_buf[];
extern void *adx_error_obj;

void adx_error_init(void);
void adx_error_finish(void);
void adx_error(const char *msg);
void adx_error_cat(const char *prefix, const char *detail);
void adx_format_2num(int first, int second, char *buf, int size);
void adx_invalidate_dcache(char *buf, long size);
void adx_exec_srd(void);

extern void (*adx_error_fn)(void *obj, char *msg);

#endif /* REZ_ADX_ERROR_H */
