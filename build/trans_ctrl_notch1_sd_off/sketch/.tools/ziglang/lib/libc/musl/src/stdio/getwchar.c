#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\getwchar.c"
#include "stdio_impl.h"
#include <wchar.h>

wint_t getwchar(void)
{
	return fgetwc(stdin);
}

weak_alias(getwchar, getwchar_unlocked);
