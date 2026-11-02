#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\conjf.c"
#include "complex_impl.h"

float complex conjf(float complex z)
{
	return CMPLXF(crealf(z), -cimagf(z));
}
