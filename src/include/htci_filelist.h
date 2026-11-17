/* Host paths, host file list cache. */

#ifndef REZ_HTCI_FILELIST_H
#define REZ_HTCI_FILELIST_H

#include "types_cri.h"

void htci_make_host_path(const char *fname, char *out);
int htci_read_filelist(const char *fname, char *buf);
void htci_cache_lookup(const char *fname, HtciFile *file);
void htci_set_cur_dir(const char *dir);
void htci_set_dev_prefix(const char *prefix);

#endif /* REZ_HTCI_FILELIST_H */
