/* IOP module loading helpers and the library's debug printf. */

#include <stdarg.h>
#include "libc.h"
#include "ul_module.h"
#include "libkernl_loadfile.h"
#include "ul_fs.h"

/* From dir, reporting each. Returns n. */
/* 0x001b9490 */
static int ulLoadModuleList(char **names, int n, const char *dir)
{
    char path[256];
    int i;

    for (i = 0; i < n; i++) {
        sprintf(path, "%s/%s", dir, names[i]);
        if (sceSifLoadModule(path, 0, 0) < 0)
            printf("module load error! [%s]\n", path);
        else
            printf("Load IOP module [%s]\n", path);
    }
    return n;
}

/* Same as ulFsLoadModule. */
/* 0x001b9550 */
static int ulLoadModule(int name, int arglen, int args)
{
    char path[256];
    int ret;

    ulFsMakePath(name, path);
    ret = sceSifLoadModule(path, arglen, args);
    if (ret >= 0)
        ulDebugPrintf("Load IOP module [%s]\n", path);
    else
        ulDebugPrintf("module load error! [%s]\n", path);
    return ret;
}

/* At most 255 characters per call. */
/* 0x001b95c8 */
void ulDebugPrintf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    printf("%s", buf);
}
