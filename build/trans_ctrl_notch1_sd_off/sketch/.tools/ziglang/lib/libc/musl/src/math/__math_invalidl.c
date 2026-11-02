#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\__math_invalidl.c"
#include <float.h>
#include "libm.h"

#if LDBL_MANT_DIG != DBL_MANT_DIG
long double __math_invalidl(long double x)
{
	return (x - x) / (x - x);
}
#endif
