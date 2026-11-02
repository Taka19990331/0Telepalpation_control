#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ctype\\iswupper.c"
#include <wctype.h>

int iswupper(wint_t wc)
{
	return towlower(wc) != wc;
}

int __iswupper_l(wint_t c, locale_t l)
{
	return iswupper(c);
}

weak_alias(__iswupper_l, iswupper_l);
