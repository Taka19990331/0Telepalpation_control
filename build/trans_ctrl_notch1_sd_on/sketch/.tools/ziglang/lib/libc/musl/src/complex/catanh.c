#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\catanh.c"
#include "complex_impl.h"

/* atanh = -i atan(i z) */

double complex catanh(double complex z)
{
	z = catan(CMPLX(-cimag(z), creal(z)));
	return CMPLX(cimag(z), -creal(z));
}
