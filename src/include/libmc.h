/* Memory card library. */

#ifndef REZ_LIBMC_H
#define REZ_LIBMC_H

struct SifClientA;
struct McTblEntry;

int sceMcInit(void);
int sceMcOpen(int port, int slot, const char *name, int mode);
int sceMcMkdir(int port, int slot, const char *name);
int sceMcClose(int fd);
int sceMcSeek(int fd, int offset, int origin);
int sceMcRead(int fd, void *buf, int size);
int sceMcWrite(int fd, unsigned char *buf, int size);
int sceMcSync(int mode, int *cmd, int *result);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);
int sceMcGetDir(int port, int slot, const char *name, int flags, int maxent, void *table);
int sceMcChdir(int port, int slot, const char *name, char *cur);
int sceMcFormat(int port, int slot);
int sceMcDelete(int port, int slot, const char *name);
int sceMcSetFileInfo(int port, int slot, const char *name, struct McTblEntry *info, unsigned int flags);
int sceMcRename(int port, int slot, const char *name, const char *newname);
int sceMcUnformat(int port, int slot);

#endif /* REZ_LIBMC_H */
