#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\getchar_unlocked.c"
#include "stdio_impl.h"

int getchar_unlocked(void)
{
	return getc_unlocked(stdin);
}
