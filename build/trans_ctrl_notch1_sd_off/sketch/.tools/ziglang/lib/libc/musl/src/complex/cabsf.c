#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cabsf.c"
#include "complex_impl.h"

float cabsf(float complex z)
{
	return hypotf(crealf(z), cimagf(z));
}
