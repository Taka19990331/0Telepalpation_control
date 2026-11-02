#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\x32\\fma.c"
#include <math.h>

#if __FMA__

double fma(double x, double y, double z)
{
	__asm__ ("vfmadd132sd %1, %2, %0" : "+x" (x) : "x" (y), "x" (z));
	return x;
}

#elif __FMA4__

double fma(double x, double y, double z)
{
	__asm__ ("vfmaddsd %3, %2, %1, %0" : "=x" (x) : "x" (x), "x" (y), "x" (z));
	return x;
}

#else

#include "../fma.c"

#endif
