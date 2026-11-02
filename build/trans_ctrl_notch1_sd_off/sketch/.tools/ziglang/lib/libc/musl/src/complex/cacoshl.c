#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cacoshl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double complex cacoshl(long double complex z)
{
	return cacosh(z);
}
#else
long double complex cacoshl(long double complex z)
{
	int zineg = signbit(cimagl(z));

	z = cacosl(z);
	if (zineg) return CMPLXL(cimagl(z), -creall(z));
	else       return CMPLXL(-cimagl(z), creall(z));
}
#endif
