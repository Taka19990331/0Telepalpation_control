#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\ldexpf.c"
#include <math.h>

float ldexpf(float x, int n)
{
	return scalbnf(x, n);
}
