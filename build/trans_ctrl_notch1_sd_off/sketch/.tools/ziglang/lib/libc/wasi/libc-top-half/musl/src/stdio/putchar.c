#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\stdio\\putchar.c"
#include <stdio.h>
#include "putc.h"

int putchar(int c)
{
	return do_putc(c, stdout);
}
