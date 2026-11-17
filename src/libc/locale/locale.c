#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libc_internal.h"

struct _reent_locale_a {
    char pad_00[48];
    int _current_category;
    const char *_current_locale;
};

extern char locale_lc_ctype[];
extern struct _reent_a *_impure_ptr;

/*
 * _setlocale_r: only the "C" and "" locales are accepted (strcmp strcmp); stores category/locale
 * in the reent (offsets 48/52) and returns "C"; NULL locale queries.
 */
/* 0x001f3dc0 */
char *_setlocale_r(struct _reent_a *p, int category, const char *locale)
{
    if (locale) {
        if (strcmp(locale, "C") && strcmp(locale, ""))
            return 0;
        ((struct _reent_locale_a *)p)->_current_category = category;
        ((struct _reent_locale_a *)p)->_current_locale = locale;
    }
    return "C";
}

/* _localeconv_r: returns the static lconv locale_lc_ctype. */
/* 0x001f3e48 */
char *_localeconv_r(struct _reent_a *data)
{
    return locale_lc_ctype;
}

/* Setlocale: _setlocale_r(_impure_ptr _impure_ptr, category, locale) (_setlocale_r). */
/* 0x001f3e58 */
char *setlocale(int category, const char *locale)
{
    return _setlocale_r(_impure_ptr, category, locale);
}

/* Localeconv: _localeconv_r(_impure_ptr _impure_ptr) (_localeconv_r). */
/* 0x001f3e88 */
char *localeconv(void)
{
    return _localeconv_r(_impure_ptr);
}
