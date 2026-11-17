/*
 * ADX error reporting (last message, game callback, SVM print) and shared helpers: number
 * formatting, data cache invalidation, sector read server call.
 */

#include "types_cri.h"
#include "adx_error.h"
#include "lsc.h"
#include "svm.h"
#include "libc.h"
#include "libkernl_cache.h"
#include "srd.h"

/* 0x001d6f10 */
void adx_error_init(void)
{
    memset(adx_error_buf, 0, 256);
    adx_error_fn = 0;
    adx_error_obj = 0;
}

/* 0x001d6f40 */
void adx_error_finish(void)
{
    memset(adx_error_buf, 0, 256);
    adx_error_fn = 0;
    adx_error_obj = 0;
}

/* For ADX and for SVM. */
/* 0x001d6f70 */
void adx_set_error_cb(void (*fn)(void *obj, char *msg), void *obj)
{
    adx_error_fn = fn;
    adx_error_obj = obj;
    svm_set_error_cb(fn, obj);
}

/* Keeps the message, calls the error callback, prints it. */
/* 0x001d6f88 */
void adx_error(const char *msg)
{
    strncpy(adx_error_buf, msg, 255);
    if (adx_error_fn != 0)
        adx_error_fn(adx_error_obj, adx_error_buf);
    svm_error_printf(adx_error_buf);
}

/* 0x001d6fd8 */
void adx_error_cat(const char *prefix, const char *detail)
{
    strncpy(adx_error_buf, prefix, 255);
    strncat(adx_error_buf, detail, 255);
    if (adx_error_fn != 0)
        adx_error_fn(adx_error_obj, adx_error_buf);
    svm_error_printf(adx_error_buf);
}

/*
 * Meant as svm_ltoa into buf. Bug: raw digit values go into buf and adx_num_buf, which nothing
 * fills, is copied back.
 */
/* 0x001d7040 */
static void adx_itoa(int num, char *buf, int size)
{
    int i;
    int n;
    char *digit;

    for (i = 0; i < 32; i++) {
        digit = &buf[i];
        *digit = num % 10;
        num = num / 10;
        if (num == 0) {
            *digit = 0;
            break;
        }
    }
    n = strlen(adx_num_buf);
    n = (n < size - 1) ? n : size - 1;
    for (i = 0; i < n; i++)
        buf[i] = adx_num_buf[n - 1 - i];
    buf[i] = 0;
}

/* "first second" */
/* 0x001d7138 */
void adx_format_2num(int first, int second, char *buf, int size)
{
    adx_itoa(first, buf, size);
    strncat(buf, " ", size - strlen(buf) - 1);
    adx_itoa(second, buf + strlen(buf), 4 - strlen(buf));
}

/* 0x001d71d0 */
void adx_invalidate_dcache(char *buf, long size)
{
    InvalidDCache(buf, buf + (int)size - 1);
}

/* 0x001d71e8 */
void adx_exec_srd(void)
{
    srd_exec_server();
}

/* 0x001d71f0 */
static int adx_is_srd_busy(void)
{
    return srd_is_busy();
}
