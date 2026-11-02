#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cprojl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double complex cprojl(long double complex z)
{
	return cproj(z);
}
#else
long double complex cprojl(long double complex z)
{
	if (isinf(creall(z)) || isinf(cimagl(z)))
		return CMPLXL(INFINITY, copysignl(0.0, cimagl(z)));
	return z;
}
#endif
