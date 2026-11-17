/* Memory stream joint. */

#ifndef REZ_SJMEM_H
#define REZ_SJMEM_H

#include "types_cri.h"

struct SjChunk;

extern int sjmem_init_count;
extern SjMem sjmem_obj_tbl[];
extern char sjmem_uuid[];
extern SjIf sjmem_vtbl;

void sjmem_lib_init(void);
void sjmem_lib_finish(void);

#endif /* REZ_SJMEM_H */
