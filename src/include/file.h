/* Background read queue, direct reads, pack files, file heap. */
#ifndef REZ_FILE_H
#define REZ_FILE_H

struct FileEnt;

void filer_boot(void);
void filer_init(void);
int filer_is_ready(void);
void filer_set_flags(int bits);
void filer_reset_flags(void);
int filer_is_reading(void);
void filer_server_step(void);
int file_request_simple(int req, int sync);
int file_load_sync(int *req, int dest, int sync);
int file_request_status(void *req);
int file_request_all_done(void);
void file_request_cancel(int *req);
void filer_cancel_all(void);
int file_get_size(void *req);
int file_handle_open(void *req);
void file_handle_close(void *fh);
int file_handle_size(void *fh);
int file_handle_read(void *fh, int nsct, void *buf);
int file_handle_is_idle(void *fh);
int file_handle_seek(int fh, int pos, int type);
int PackFileGetNum(char *pack);
int PackFileCountId(char *pack, int id);
char *PackFileGet(char *pack, int idx, int *size);
char *PackFileGetById(char *pack, int id, int idx, int *size);
void file_desc_copy(int *dst, int name, int *src);
int file_list_count(int *p);
void file_list_request(int *list);
int file_list_is_loaded(int *list);
int file_heap_alloc(int size);
void file_heap_free(void *mem);

#endif /* REZ_FILE_H */
