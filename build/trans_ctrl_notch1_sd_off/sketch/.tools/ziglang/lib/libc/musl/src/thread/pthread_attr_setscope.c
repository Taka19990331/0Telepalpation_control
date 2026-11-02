#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_attr_setscope.c"
#include "pthread_impl.h"

int pthread_attr_setscope(pthread_attr_t *a, int scope)
{
	switch (scope) {
	case PTHREAD_SCOPE_SYSTEM:
		return 0;
	case PTHREAD_SCOPE_PROCESS:
		return ENOTSUP;
	default:
		return EINVAL;
	}
}
