#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\vscanf.c"
#include <stdio.h>
#include <stdarg.h>

int vscanf(const char *restrict fmt, va_list ap)
{
	return vfscanf(stdin, fmt, ap);
}

weak_alias(vscanf,__isoc99_vscanf);
