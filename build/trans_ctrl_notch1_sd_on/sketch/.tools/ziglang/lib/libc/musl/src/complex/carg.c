#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\carg.c"
#include "complex_impl.h"

double carg(double complex z)
{
	return atan2(cimag(z), creal(z));
}
