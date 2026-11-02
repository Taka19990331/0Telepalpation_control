#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_attr_setstacksize.c"
#include "pthread_impl.h"

int pthread_attr_setstacksize(pthread_attr_t *a, size_t size)
{
	if (size-PTHREAD_STACK_MIN > SIZE_MAX/4) return EINVAL;
	a->_a_stackaddr = 0;
	a->_a_stacksize = size;
	return 0;
}
