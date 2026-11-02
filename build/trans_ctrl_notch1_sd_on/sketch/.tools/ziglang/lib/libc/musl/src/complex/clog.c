#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\clog.c"
#include "complex_impl.h"

// FIXME

/* log(z) = log(|z|) + i arg(z) */

double complex clog(double complex z)
{
	double r, phi;

	r = cabs(z);
	phi = carg(z);
	return CMPLX(log(r), phi);
}
