#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cprojf.c"
#include "complex_impl.h"

float complex cprojf(float complex z)
{
	if (isinf(crealf(z)) || isinf(cimagf(z)))
		return CMPLXF(INFINITY, copysignf(0.0, cimagf(z)));
	return z;
}
