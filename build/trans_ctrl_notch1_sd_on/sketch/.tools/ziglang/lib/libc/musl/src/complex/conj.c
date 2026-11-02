#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\conj.c"
#include "complex_impl.h"

double complex conj(double complex z)
{
	return CMPLX(creal(z), -cimag(z));
}
