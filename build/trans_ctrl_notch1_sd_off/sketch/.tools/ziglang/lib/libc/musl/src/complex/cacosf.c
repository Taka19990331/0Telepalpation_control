#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cacosf.c"
#include "complex_impl.h"

// FIXME

static const float float_pi_2 = M_PI_2;

float complex cacosf(float complex z)
{
	z = casinf(z);
	return CMPLXF(float_pi_2 - crealf(z), -cimagf(z));
}
