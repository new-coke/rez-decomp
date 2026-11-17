#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/*
 * Finds the first occurrence of a substring (simple quadratic search); an empty haystack matches
 * only an empty needle.
 */
/* 0x001ee4f8 */
char *strstr(const char *searchee, const char *lookfor)
{
    if (*searchee == 0) {
        if (*lookfor)
            return (char *)NULL;
        return (char *)searchee;
    }
    while (*searchee) {
        unsigned int i;

        i = 0;
        while (1) {
            if (lookfor[i] == 0)
                return (char *)searchee;
            if (lookfor[i] != searchee[i])
                break;
            i++;
        }
        searchee++;
    }
    return (char *)NULL;
}
