#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\stdio\\getc.c"
#include <stdio.h>
#include "getc.h"

int getc(FILE *f)
{
	return do_getc(f);
}

weak_alias(getc, _IO_getc);
