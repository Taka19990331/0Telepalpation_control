#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\lgamma.c"
#include <math.h>
#include "libm.h"

double lgamma(double x)
{
	return __lgamma_r(x, &__signgam);
}
