#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\i386\\rintl.c"
#include <math.h>

long double rintl(long double x)
{
	__asm__ ("frndint" : "+t"(x));
	return x;
}
