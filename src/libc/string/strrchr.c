#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/*
 * Returns a pointer to the last occurrence of a character in a string (the terminator counts), or
 * NULL.
 */
/* 0x001ee4a8 */
char *strrchr(const char *s, int i)
{
    const char *last = NULL;
    char c = i;

    while (*s) {
        if (*s == c)
            last = s;
        s++;
    }
    if (*s == c)
        last = s;
    return (char *)last;
}
