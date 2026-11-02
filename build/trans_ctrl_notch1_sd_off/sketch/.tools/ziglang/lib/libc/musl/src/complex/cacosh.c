#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cacosh.c"
#include "complex_impl.h"

/* acosh(z) = i acos(z) */

double complex cacosh(double complex z)
{
	int zineg = signbit(cimag(z));

	z = cacos(z);
	if (zineg) return CMPLX(cimag(z), -creal(z));
	else       return CMPLX(-cimag(z), creal(z));
}
