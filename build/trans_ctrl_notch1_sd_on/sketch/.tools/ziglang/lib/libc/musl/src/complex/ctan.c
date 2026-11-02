#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\ctan.c"
#include "complex_impl.h"

/* tan(z) = -i tanh(i z) */

double complex ctan(double complex z)
{
	z = ctanh(CMPLX(-cimag(z), creal(z)));
	return CMPLX(cimag(z), -creal(z));
}
