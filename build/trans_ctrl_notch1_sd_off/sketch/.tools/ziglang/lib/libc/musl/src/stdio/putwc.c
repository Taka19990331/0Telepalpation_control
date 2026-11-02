#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\putwc.c"
#include "stdio_impl.h"
#include <wchar.h>

wint_t putwc(wchar_t c, FILE *f)
{
	return fputwc(c, f);
}
