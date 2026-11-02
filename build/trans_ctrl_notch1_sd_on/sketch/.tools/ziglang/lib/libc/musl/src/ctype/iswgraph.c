#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ctype\\iswgraph.c"
#include <wctype.h>

int iswgraph(wint_t wc)
{
	/* ISO C defines this function as: */
	return !iswspace(wc) && iswprint(wc);
}

int __iswgraph_l(wint_t c, locale_t l)
{
	return iswgraph(c);
}

weak_alias(__iswgraph_l, iswgraph_l);
