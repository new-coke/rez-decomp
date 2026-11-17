/* Ring buffer stream joint. */

#ifndef REZ_SJRBF_H
#define REZ_SJRBF_H

#include "types_cri.h"

struct SjChunk;

extern int sjrbf_init_count;
extern SjRbf sjrbf_obj_tbl[];
extern char sjrbf_uuid[];
extern SjIf sjrbf_vtbl;

void sjrbf_lib_init(void);
void sjrbf_lib_finish(void);
SjRbf *sjrbf_create(char *buf, int bsize, int xsize);
char *sjrbf_get_buf(SjRbf *sj);
int sjrbf_get_bsize(SjRbf *sj);
int sjrbf_get_xsize(SjRbf *sj);

#endif /* REZ_SJRBF_H */
