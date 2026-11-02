#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\ccosf.c"
#include "complex_impl.h"

float complex ccosf(float complex z)
{
	return ccoshf(CMPLXF(-cimagf(z), crealf(z)));
}
