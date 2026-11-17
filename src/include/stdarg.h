/*
 * <stdarg.h> for the EE EABI (va-mips.h: single-float, 64-bit slots). va_start points at the
 * save slot of the first unnamed argument; the prologue spills unnamed register arguments right
 * below the incoming stack arguments.
 */

#ifndef REZ_STDARG_H
#define REZ_STDARG_H
typedef char *va_list;

#define __va_reg_size 8
#define va_start(ap, last)                                                  \
    ((ap) = ((va_list)__builtin_next_arg(last)                              \
             - (__builtin_args_info(2) < 8                                  \
                    ? (8 - __builtin_args_info(2)) * __va_reg_size : 0)))
#define va_end(ap) ((void)0)

#endif
