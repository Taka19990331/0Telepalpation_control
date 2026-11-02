#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cargl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double cargl(long double complex z)
{
	return carg(z);
}
#else
long double cargl(long double complex z)
{
	return atan2l(cimagl(z), creall(z));
}
#endif
