/* C runtime system-call glue, boot-time vblank waits. */

#ifndef REZ_LIBKERNL_GLUE_H
#define REZ_LIBKERNL_GLUE_H

struct StatBuf;

void crt_tty_reset(void);
void crt_wait_vblank(void);
long crt_wait_vblank_flag(void);
int _write(int fd, const void *buf, int n);
int _read(int fd, void *buf, int n);
int _close(void);
int _lseek(void);
int isatty(void);
int _fstat(int fd, struct StatBuf *st);
int _getpid(void);
int _kill(int pid, int sig);
void *_sbrk(int incr);

#endif /* REZ_LIBKERNL_GLUE_H */
