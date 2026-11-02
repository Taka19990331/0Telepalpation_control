#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\aarch64\\fma.c"
#include <math.h>

double fma(double x, double y, double z)
{
	__asm__ ("fmadd %d0, %d1, %d2, %d3" : "=w"(x) : "w"(x), "w"(y), "w"(z));
	return x;
}
