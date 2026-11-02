#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\stdio\\putc.c"
#include <stdio.h>
#include "putc.h"

int putc(int c, FILE *f)
{
	return do_putc(c, f);
}

weak_alias(putc, _IO_putc);
