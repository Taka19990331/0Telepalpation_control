#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\vwscanf.c"
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

int vwscanf(const wchar_t *restrict fmt, va_list ap)
{
	return vfwscanf(stdin, fmt, ap);
}

weak_alias(vwscanf,__isoc99_vwscanf);
