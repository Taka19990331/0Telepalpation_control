#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\putchar_unlocked.c"
#include "stdio_impl.h"

int putchar_unlocked(int c)
{
	return putc_unlocked(c, stdout);
}
