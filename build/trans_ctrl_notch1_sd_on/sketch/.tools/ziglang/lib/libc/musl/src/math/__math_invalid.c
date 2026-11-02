#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\__math_invalid.c"
#include "libm.h"

double __math_invalid(double x)
{
	return (x - x) / (x - x);
}
