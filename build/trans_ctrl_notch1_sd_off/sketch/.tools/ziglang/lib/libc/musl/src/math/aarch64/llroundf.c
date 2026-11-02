#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\llroundf.c"
#include <math.h>

long long llroundf(float x)
{
	long long n;
	__asm__ ("fcvtas %x0, %s1" : "=r"(n) : "w"(x));
	return n;
}
