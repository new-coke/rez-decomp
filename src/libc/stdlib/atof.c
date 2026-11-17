#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"

/* Converts a string to double: strtod(s, NULL) (strtod). */
/* 0x001ec978 */
double atof(const char *s)
{
    return strtod(s, 0);
}
