/* Remote (IOP) stream joint. */

#ifndef REZ_SJRMT_H
#define REZ_SJRMT_H

struct SjChunk;

extern int sjrmt_init_count[];
extern int sjrmt_rpc_rbuf[];
extern int sjrmt_rpc_sbuf[];
extern int sjrmt_uuid_buf[];
extern volatile int sjrmt_ver_str[];

void *sjrmt_create(int mode, void *buf, int size);
void sjrmt_destroy(void *sj);
void sjrmt_reset(void *sj);
int sjrmt_get_num_data(void *sj, int id);
void sjrmt_lib_init(void);
void sjrmt_lib_finish(void);

#endif /* REZ_SJRMT_H */
