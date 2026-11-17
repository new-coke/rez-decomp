/* Debug log print, assertion handler. */

#ifndef REZ_PS_LOG_H
#define REZ_PS_LOG_H

void psLogPrintf(int level, const char *fmt, ...);
void psAssertFail(const char *file, int line);

#endif /* REZ_PS_LOG_H */
