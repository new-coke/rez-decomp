/* Public newlib functions (and float math) linked into the executable. */

#ifndef REZ_LIBC_H
#define REZ_LIBC_H

#include "types_libc.h"
#include "types_sce.h"
#include "types_main.h"
#include <stdarg.h>

double atan(double x);
double libm_fabs(double x);
int isnan(double x);
double rint(double x);
float atanf(float x);
float ceilf(float x);
float copysignf(float x, float y);
float cosf(float x);
float libm_fabsf(float x);
int finitef(float x);
float floorf(float x);
int isnanf(float x);
float modff(float x, float *iptr);
float scalbnf(float x, int n);
float sinf(float x);
float tanf(float x);
double atan2(double y, double x);
double sqrt(double x);
float atan2f(float y, float x);
float expf(float x);
float fmodf(float x, float y);
float logf(float x);
float powf(float x, float y);
float sqrtf(float x);
char *setlocale(int category, const char *locale);
char *localeconv(void);
int isinf(double x);
int raise(int sig);
SigFunc signal(int sig, SigFunc func);
int fflush(FILE_a *fp);
int fiprintf(void *fp, const char *fmt, ...);
int printf(const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int vfiprintf(FILE_a *fp, const char *fmt, void *ap);
int vfprintf(FILE_a *fp, const char *fmt, void *ap);
int vsprintf(char *str, const char *fmt, va_list ap);
double atof(const char *s);
void *calloc(unsigned int n, unsigned int size);
double strtod(const char *s, char **ptr);
int strcasecmp(const char *s1, const char *s2);
char *strrchr(const char *s, int i);
char *strstr(const char *searchee, const char *lookfor);
char *strtok(char *s, const char *delim);
char *strtok_r(char *s, const char *delim, char **lasts);
void *memchr(const void *s, int c, unsigned int n);
int memcmp(const void *s1, const void *s2, unsigned int n);
void *memcpy(void *dst, const void *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
void *memset(void *s, int c, unsigned int n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dst, const char *src);
unsigned int strlen(const char *s);
char *strncat(char *dst, const char *src, unsigned int n);
int strncmp(const char *s1, const char *s2, unsigned int n);
char *strncpy(char *dst, const char *src, unsigned int n);
float fabsf(float x);

#endif /* REZ_LIBC_H */
