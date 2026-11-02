#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cacos.c"
#include "complex_impl.h"

// FIXME: Hull et al. "Implementing the complex arcsine and arccosine functions using exception handling" 1997

/* acos(z) = pi/2 - asin(z) */

double complex cacos(double complex z)
{
	z = casin(z);
	return CMPLX(M_PI_2 - creal(z), -cimag(z));
}
