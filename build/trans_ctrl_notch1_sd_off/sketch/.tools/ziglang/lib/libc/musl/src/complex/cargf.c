#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cargf.c"
#include "complex_impl.h"

float cargf(float complex z)
{
	return atan2f(cimagf(z), crealf(z));
}
