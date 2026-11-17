#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/*
 * Reentrant tokenizer: skips leading delimiters, terminates the token at the next delimiter and
 * keeps the continuation in *lasts.
 */
/* 0x001ef4e8 */
char *strtok_r(char *s, const char *delim, char **lasts)
{
    char *spanp;
    int c, sc;
    char *tok;

    if (s == NULL && (s = *lasts) == NULL)
        return (NULL);
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
        if (c == sc)
            goto cont;
    }
    if (c == 0) {
        *lasts = NULL;
        return (NULL);
    }
    tok = s - 1;
    for (;;) {
        c = *s++;
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *lasts = s;
                return (tok);
            }
        } while (sc != 0);
    }
}
