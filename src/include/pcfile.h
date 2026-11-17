/* Files queued for the host PC to fetch. */

#ifndef REZ_PCFILE_H
#define REZ_PCFILE_H

void pcfile_init(void);
void pcfile_save_set(char *name, char *data, int size);
void pcfile_update(void);

#endif /* REZ_PCFILE_H */
