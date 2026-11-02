#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_attr_setinheritsched.c"
#include "pthread_impl.h"
#include "syscall.h"

int pthread_attr_setinheritsched(pthread_attr_t *a, int inherit)
{
	if (inherit > 1U) return EINVAL;
	a->_a_sched = inherit;
	return 0;
}
