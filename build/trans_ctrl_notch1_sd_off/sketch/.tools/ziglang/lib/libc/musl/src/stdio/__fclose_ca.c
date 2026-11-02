#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\__fclose_ca.c"
#include "stdio_impl.h"

int __fclose_ca(FILE *f)
{
	return f->close(f);
}
