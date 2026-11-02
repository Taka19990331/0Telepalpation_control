#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_barrierattr_setpshared.c"
#include "pthread_impl.h"

int pthread_barrierattr_setpshared(pthread_barrierattr_t *a, int pshared)
{
	if (pshared > 1U) return EINVAL;
	a->__attr = pshared ? INT_MIN : 0;
	return 0;
}
