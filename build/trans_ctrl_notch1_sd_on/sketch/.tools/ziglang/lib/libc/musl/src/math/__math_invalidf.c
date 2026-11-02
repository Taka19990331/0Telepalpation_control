#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\__math_invalidf.c"
#include "libm.h"

float __math_invalidf(float x)
{
	return (x - x) / (x - x);
}
