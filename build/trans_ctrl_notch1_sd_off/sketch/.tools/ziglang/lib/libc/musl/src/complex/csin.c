#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\csin.c"
#include "complex_impl.h"

/* sin(z) = -i sinh(i z) */

double complex csin(double complex z)
{
	z = csinh(CMPLX(-cimag(z), creal(z)));
	return CMPLX(cimag(z), -creal(z));
}
