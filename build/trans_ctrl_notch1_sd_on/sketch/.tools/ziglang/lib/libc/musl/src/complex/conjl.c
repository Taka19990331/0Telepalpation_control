#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\conjl.c"
#include "complex_impl.h"

long double complex conjl(long double complex z)
{
	return CMPLXL(creall(z), -cimagl(z));
}
