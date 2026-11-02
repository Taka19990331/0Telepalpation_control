#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_attr_setschedpolicy.c"
#include "pthread_impl.h"

int pthread_attr_setschedpolicy(pthread_attr_t *a, int policy)
{
	a->_a_policy = policy;
	return 0;
}
