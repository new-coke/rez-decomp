/* IOP module loader client. */

#ifndef REZ_LIBKERNL_LOADFILE_H
#define REZ_LIBKERNL_LOADFILE_H

int sceSifLoadFileReset(void);
int sif_loadfile_start_module_noresult(int id, int len, const char *args);
int sceSifLoadModule(const char *path, int len, const char *args);

#endif /* REZ_LIBKERNL_LOADFILE_H */
