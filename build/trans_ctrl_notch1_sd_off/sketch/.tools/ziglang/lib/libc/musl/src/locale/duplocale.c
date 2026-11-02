#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\duplocale.c"
#include <stdlib.h>
#include <string.h>
#include "locale_impl.h"
#include "libc.h"

locale_t __duplocale(locale_t old)
{
	locale_t new = malloc(sizeof *new);
	if (!new) return 0;
	if (old == LC_GLOBAL_LOCALE) old = &libc.global_locale;
	*new = *old;
	return new;
}

weak_alias(__duplocale, duplocale);
