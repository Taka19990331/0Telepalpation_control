#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\ilogb.c"
#include <limits.h>
#include "libm.h"

int ilogb(double x)
{
	#pragma STDC FENV_ACCESS ON
	union {double f; uint64_t i;} u = {x};
	uint64_t i = u.i;
	int e = i>>52 & 0x7ff;

	if (!e) {
		i <<= 12;
		if (i == 0) {
			FORCE_EVAL(0/0.0f);
			return FP_ILOGB0;
		}
		/* subnormal x */
		for (e = -0x3ff; i>>63 == 0; e--, i<<=1);
		return e;
	}
	if (e == 0x7ff) {
		FORCE_EVAL(0/0.0f);
		return i<<12 ? FP_ILOGBNAN : INT_MAX;
	}
	return e - 0x3ff;
}
