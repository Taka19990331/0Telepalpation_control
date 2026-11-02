#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\wctob.c"
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

int wctob(wint_t c)
{
	if (c < 128U) return c;
	if (MB_CUR_MAX==1 && IS_CODEUNIT(c)) return (unsigned char)c;
	return EOF;
}
