/* Chunk list stream joint. */

#ifndef REZ_SJUNI_H
#define REZ_SJUNI_H

#include "types_cri.h"

struct SjChunk;

extern int sjuni_init_count;
extern SjUni sjuni_obj_tbl[];
extern char sjuni_uuid[];
extern SjIf sjuni_vtbl;

void sjuni_lib_init(void);
void sjuni_lib_finish(void);
void *sjuni_create(int mode, void *work, int size);
void sj_split_chunk(struct SjChunk *ck, int n, struct SjChunk *head, struct SjChunk *tail);

#endif /* REZ_SJUNI_H */
