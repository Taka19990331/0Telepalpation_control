#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\powerpc64\\fma.c"
#include <math.h>

double fma(double x, double y, double z)
{
	__asm__ ("fmadd %0, %1, %2, %3" : "=d"(x) : "d"(x), "d"(y), "d"(z));
	return x;
}
