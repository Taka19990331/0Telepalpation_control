#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ctype\\iswxdigit.c"
#include <wctype.h>

int iswxdigit(wint_t wc)
{
	return (unsigned)(wc-'0') < 10 || (unsigned)((wc|32)-'a') < 6;
}

int __iswxdigit_l(wint_t c, locale_t l)
{
	return iswxdigit(c);
}

weak_alias(__iswxdigit_l, iswxdigit_l);
