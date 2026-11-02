#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\casinh.c"
#include "complex_impl.h"

/* asinh(z) = -i asin(i z) */

double complex casinh(double complex z)
{
	z = casin(CMPLX(-cimag(z), creal(z)));
	return CMPLX(cimag(z), -creal(z));
}
