#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_condattr_setclock.c"
#include "pthread_impl.h"

int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t clk)
{
	if (clk < 0 || clk-2U < 2) return EINVAL;
	a->__attr &= 0x80000000;
	a->__attr |= clk;
	return 0;
}
