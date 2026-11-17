#include "debug_token_equal.h"

/* 1 if str equals line's first word (ends at NUL or space). */
/* 0x0010fdc8 */
static int token_equal(const char *str, const char *line)
{
    while (*str != 0 && *line != 0 && *line != ' ') {
        if (*str++ != *line++)
            return 0;
    }
    if (*str == 0 && (*line == 0 || *line == ' '))
        return 1;
    return 0;
}
