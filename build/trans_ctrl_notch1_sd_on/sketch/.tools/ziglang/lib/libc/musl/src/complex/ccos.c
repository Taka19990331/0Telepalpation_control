#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\ccos.c"
#include "complex_impl.h"

/* cos(z) = cosh(i z) */

double complex ccos(double complex z)
{
	return ccosh(CMPLX(-cimag(z), creal(z)));
}
