#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\clogl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double complex clogl(long double complex z)
{
	return clog(z);
}
#else
// FIXME
long double complex clogl(long double complex z)
{
	long double r, phi;

	r = cabsl(z);
	phi = cargl(z);
	return CMPLXL(logl(r), phi);
}
#endif
