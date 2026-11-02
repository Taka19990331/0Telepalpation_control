#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\nearbyint.c"
#include <math.h>

double nearbyint(double x)
{
	__asm__ ("frinti %d0, %d1" : "=w"(x) : "w"(x));
	return x;
}
