#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\wprintf.c"
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>

int wprintf(const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vwprintf(fmt, ap);
	va_end(ap);
	return ret;
}
