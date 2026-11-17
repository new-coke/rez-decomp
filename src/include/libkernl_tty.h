/* TTY channel read/write, library printf. */

#ifndef REZ_LIBKERNL_TTY_H
#define REZ_LIBKERNL_TTY_H

#include <stdarg.h>
#include "types_cri.h"

struct ByteRing;
struct TtyCtl;

int libkernl_tty_write(const char *buf, int n);
int libkernl_tty_read(char *buf, int n);
int libkernl_tty_open(void);
int libkernl_printf(const char *fmt, ...);
int libkernl_printf_line(const char *fmt, ...);


#endif /* REZ_LIBKERNL_TTY_H */
