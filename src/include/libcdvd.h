/* CD/DVD drive library. */

#ifndef REZ_LIBCDVD_H
#define REZ_LIBCDVD_H

struct CdFile_a;

int sceCdSearchFile(struct CdFile_a *file, const char *name);
int sceCdSync(int mode);
int sceCdInit(int mode);
int sceCdDiskReady(int mode);
int libcdvd_scmd34(int param);
int sceCdGetError(void);
int sceCdBreak(void);

#endif /* REZ_LIBCDVD_H */
