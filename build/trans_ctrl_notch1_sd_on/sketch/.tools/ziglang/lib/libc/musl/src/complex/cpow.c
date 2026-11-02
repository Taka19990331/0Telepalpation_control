#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\complex\\cpow.c"
#include "complex_impl.h"

/* pow(z, c) = exp(c log(z)), See C99 G.6.4.1 */

double complex cpow(double complex z, double complex c)
{
	return cexp(c * clog(z));
}
