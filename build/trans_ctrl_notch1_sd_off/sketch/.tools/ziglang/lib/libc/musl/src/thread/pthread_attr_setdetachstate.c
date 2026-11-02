#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_attr_setdetachstate.c"
#include "pthread_impl.h"

int pthread_attr_setdetachstate(pthread_attr_t *a, int state)
{
	if (state > 1U) return EINVAL;
	a->_a_detach = state;
	return 0;
}
