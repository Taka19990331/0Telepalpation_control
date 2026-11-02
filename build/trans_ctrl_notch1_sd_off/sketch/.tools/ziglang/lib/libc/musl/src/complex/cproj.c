#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cproj.c"
#include "complex_impl.h"

double complex cproj(double complex z)
{
	if (isinf(creal(z)) || isinf(cimag(z)))
		return CMPLX(INFINITY, copysign(0.0, cimag(z)));
	return z;
}
