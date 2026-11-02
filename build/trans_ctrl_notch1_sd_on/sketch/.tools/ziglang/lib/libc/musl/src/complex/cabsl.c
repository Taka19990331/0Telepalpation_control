#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cabsl.c"
#include "complex_impl.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
long double cabsl(long double complex z)
{
	return cabs(z);
}
#else
long double cabsl(long double complex z)
{
	return hypotl(creall(z), cimagl(z));
}
#endif
