#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\s390x\\fma.c"
#include <math.h>

double fma(double x, double y, double z)
{
	__asm__ ("madbr %0, %1, %2" : "+f"(z) : "f"(x), "f"(y));
	return z;
}
