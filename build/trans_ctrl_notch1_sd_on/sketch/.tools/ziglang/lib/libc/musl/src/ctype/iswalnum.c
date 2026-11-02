#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ctype\\iswalnum.c"
#include <wctype.h>

int iswalnum(wint_t wc)
{
	return iswdigit(wc) || iswalpha(wc);
}

int __iswalnum_l(wint_t c, locale_t l)
{
	return iswalnum(c);
}

weak_alias(__iswalnum_l, iswalnum_l);
