/* Newlib internals: _r forms, stdio, malloc, signal, dtoa; fdlibm kernels. */

#ifndef REZ_LIBC_INTERNAL_H
#define REZ_LIBC_INTERNAL_H

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "types_main.h"
#include <stdarg.h>

int *__errno(void);
double __ieee754_atan2(double y, double x);
double __ieee754_sqrt(double x);
float __ieee754_atan2f(float y, float x);
float __ieee754_expf(float x);
float __ieee754_fmodf(float x, float y);
float __ieee754_logf(float x);
float __ieee754_powf(float x, float y);
int __ieee754_rem_pio2f(float x, float *y);
float __ieee754_sqrtf(float x);
float __kernel_cosf(float x, float y);
int __kernel_rem_pio2f(float *x, float *y, int e0, int nx, int prec, const int *ipio2);
float __kernel_sinf(float x, float y, int iy);
float __kernel_tanf(float x, float y, int iy);
int matherr(struct exception *x);
char *_setlocale_r(struct _reent_a *p, int category, const char *locale);
char *_localeconv_r(struct _reent_a *data);
int _close_r(RtCtx *ptr, int fd);
int _fstat_r(RtCtx *ptr, int fd, void *st);
long _lseek_r(RtCtx *ptr, int fd, long pos, int whence);
long _read_r(RtCtx *ptr, int fd, void *buf, unsigned int n);
void *_sbrk_r(struct _reent_a *ptr, int incr);
int _kill_r(RtCtx *ptr, int pid, int sig);
int _getpid_r(RtCtx *ptr);
long _write_r(RtCtx *ptr, int fd, const void *buf, unsigned int n);
int _init_signal_r(RtCtx *ptr);
SigFunc _signal_r(RtCtx *ptr, int sig, SigFunc func);
int _raise_r(RtCtx *ptr, int sig);
int __sigtramp_r(RtCtx *ptr, int sig);
int _init_signal(void);
int __sigtramp(int sig);
void std(FILE_a *ptr, int flags, int file, struct _reent_a *data);
struct _glue_a *__sfmoreglue(struct _reent_a *d, int n);
FILE_a *__sfp(struct _reent_a *d);
void _cleanup_r(struct _reent_a *ptr);
void _cleanup(void);
void __sinit(struct _reent_a *ptr);
int __sfvwrite(FILE_a *fp, struct __suio_a *uio);
int _fwalk(struct _reent_a *ptr, int (*function)(FILE_a *));
void __smakebuf(FILE_a *fp);
int _printf_r(struct _reent_a *r, const char *fmt, ...);
int _sprintf_r(struct _reent_a *ptr, char *str, const char *fmt, ...);
int __sread(void *cookie, char *buf, int n);
int __swrite(void *cookie, const char *buf, int n);
long __sseek(void *cookie, long offset, int whence);
int __sclose(void *cookie);
int libc_vfiprintf_sprint(void *fp, struct __suio_a *uio);
int libc_vfiprintf_sbprintf(FILE_a *fp, const char *fmt, void *ap);
int _vfiprintf_r(struct _reent_a *data, FILE_a *fp, const char *fmt0, va_list ap);
int __sprint(void *fp, struct __suio_a *uio);
int __sbprintf(FILE_a *fp, const char *fmt, void *ap);
int _vfprintf_r(struct _reent_a *data, FILE_a *fp, const char *fmt0, va_list ap);
char *cvt(struct _reent_a *data, double value, int ndigits, int flags, char *sign, int *decpt, int ch, int *length);
int exponent(char *p0, int exp, int fmtch);
int __swsetup(FILE_a *fp);
void libc_abort(void);
void __assert(const char *file, int line, const char *failedexpr);
void *_calloc_r(struct _reent_a *r, unsigned int n, unsigned int elem_size);
int quorem(Bigint_a *b, Bigint_a *S);
char *_dtoa_r(struct _reent_a *ptr, double _d, int mode, int ndigits, int *decpt, int *sign, char **rve);
void _free_r(struct _reent_a *reent_ptr, void *mem);
void malloc_extend_top(struct _reent_a *reent_ptr, unsigned int nb);
void *_malloc_r(struct _reent_a *reent_ptr, unsigned int bytes);
int _malloc_trim_r(struct _reent_a *reent_ptr, unsigned int pad);
int _mbtowc_r(struct _reent_a *r, int *pwc, const char *s, unsigned int n);
void __malloc_lock(void);
void __malloc_unlock(void);
Bigint_a *_Balloc(struct _reent_a *ptr, int k);
void _Bfree(struct _reent_a *ptr, Bigint_a *v);
Bigint_a *__multadd(struct _reent_a *ptr, Bigint_a *b, int m, int a);
Bigint_a *__s2b(struct _reent_a *ptr, const char *s, int nd0, int nd, unsigned int y9);
int __hi0bits(unsigned int x);
int __lo0bits(unsigned int *y);
Big *__i2b(RtCtx *ptr, int i);
Big *__multiply(RtCtx *ptr, Big *a, Big *b);
Big *__pow5mult(RtCtx *ptr, Big *b, int k);
Big *__lshift(RtCtx *ptr, Big *b, int k);
int __mcmp(Big *a, Big *b);
Big *__mdiff(RtCtx *ptr, Big *a, Big *b);
double __ulp(double dx);
double __b2d(Big *a, int *e);
Big *__d2b(RtCtx *ptr, double dd, int *e, int *bits);
double __ratio(Big *a, Big *b);
double _mprec_log10(int dig);
double _strtod_r(struct _reent_a *ptr, const char *s00, char **se);
float libc_strtod_float(const char *s, char **ptr);

#endif /* REZ_LIBC_INTERNAL_H */
