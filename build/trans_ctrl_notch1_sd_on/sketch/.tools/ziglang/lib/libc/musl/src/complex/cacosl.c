#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cacosl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double complex cacosl(long double complex z)
{
	return cacos(z);
}
#else
// FIXME
#define PI_2 1.57079632679489661923132169163975144L
long double complex cacosl(long double complex z)
{
	z = casinl(z);
	return CMPLXL(PI_2 - creall(z), -cimagl(z));
}
#endif
