#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

#define isupper_a(c) ((_ctype_ + 1)[(int)(c)] & 1)

extern const char _ctype_[];

static __inline__ int tolower_a(int c) { return isupper_a(c) ? c - 'A' + 'a' : c; }

/* Case-insensitive string compare using the ctype table at _ctype_ (upper-case bit 1). */
/* 0x001ed880 */
int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && tolower_a(*s1) == tolower_a(*s2)) {
        s1++;
        s2++;
    }
    return tolower_a(*(unsigned char *)s1) - tolower_a(*(unsigned char *)s2);
}
