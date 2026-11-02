#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\llrint.c"
#include <math.h>

long long llrint(double x)
{
	long long n;
	__asm__ (
		"frintx %d1, %d1\n"
		"fcvtzs %x0, %d1\n" : "=r"(n), "+w"(x));
	return n;
}
