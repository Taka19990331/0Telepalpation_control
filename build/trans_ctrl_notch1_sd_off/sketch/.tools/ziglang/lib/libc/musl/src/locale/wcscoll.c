#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\wcscoll.c"
#include <wchar.h>
#include <locale.h>
#include "locale_impl.h"

/* FIXME: stub */
int __wcscoll_l(const wchar_t *l, const wchar_t *r, locale_t locale)
{
	return wcscmp(l, r);
}

int wcscoll(const wchar_t *l, const wchar_t *r)
{
	return __wcscoll_l(l, r, CURRENT_LOCALE);
}

weak_alias(__wcscoll_l, wcscoll_l);
