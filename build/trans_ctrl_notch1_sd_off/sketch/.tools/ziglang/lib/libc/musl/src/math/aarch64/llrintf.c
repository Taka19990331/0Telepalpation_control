#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\llrintf.c"
#include <math.h>

long long llrintf(float x)
{
	long long n;
	__asm__ (
		"frintx %s1, %s1\n"
		"fcvtzs %x0, %s1\n" : "=r"(n), "+w"(x));
	return n;
}
