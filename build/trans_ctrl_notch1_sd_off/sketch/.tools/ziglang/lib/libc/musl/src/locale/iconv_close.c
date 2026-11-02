#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\iconv_close.c"
#include <iconv.h>
#include <stdlib.h>

int iconv_close(iconv_t cd)
{
	if (!((size_t)cd & 1)) free((void *)cd);
	return 0;
}
