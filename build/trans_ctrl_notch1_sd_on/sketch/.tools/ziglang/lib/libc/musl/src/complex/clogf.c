#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\clogf.c"
#include "complex_impl.h"

// FIXME

float complex clogf(float complex z)
{
	float r, phi;

	r = cabsf(z);
	phi = cargf(z);
	return CMPLXF(logf(r), phi);
}
