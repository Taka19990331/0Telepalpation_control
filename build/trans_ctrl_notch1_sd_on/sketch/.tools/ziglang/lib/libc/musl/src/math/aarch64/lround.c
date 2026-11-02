#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\lround.c"
#include <math.h>

long lround(double x)
{
	long n;
	__asm__ ("fcvtas %x0, %d1" : "=r"(n) : "w"(x));
	return n;
}
