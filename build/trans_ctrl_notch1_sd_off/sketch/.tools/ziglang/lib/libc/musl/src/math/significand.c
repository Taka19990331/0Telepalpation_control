#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\significand.c"
#define _GNU_SOURCE
#include <math.h>

double significand(double x)
{
	return scalbn(x, -ilogb(x));
}
