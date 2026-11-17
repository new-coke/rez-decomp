/* DVD drive device for cvFs. */

#ifndef REZ_DVCI_H
#define REZ_DVCI_H

#include "types_cri.h"

extern DvciCache dvci_file_cache;
extern DvciHn dvci_obj_tbl[40];

void *dvci_get_interface(void);
int dvci_load_filelist(const char *fname, char *buf, unsigned int size, int namelen);
int dvci_is_cached(const char *fname);
void dvci_set_read_mode(int trycount, int spindlctrl, int datapattern, int flag);
void dvci_set_cur_dir(const char *dir);


#endif /* REZ_DVCI_H */
