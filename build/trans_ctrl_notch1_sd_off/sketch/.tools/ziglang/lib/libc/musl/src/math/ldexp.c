#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\ldexp.c"
#include <math.h>

double ldexp(double x, int n)
{
	return scalbn(x, n);
}
