#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cpowf.c"
#include "complex_impl.h"

float complex cpowf(float complex z, float complex c)
{
	return cexpf(c * clogf(z));
}
