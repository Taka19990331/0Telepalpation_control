#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\putwchar.c"
#include "stdio_impl.h"
#include <wchar.h>

wint_t putwchar(wchar_t c)
{
	return fputwc(c, stdout);
}

weak_alias(putwchar, putwchar_unlocked);
