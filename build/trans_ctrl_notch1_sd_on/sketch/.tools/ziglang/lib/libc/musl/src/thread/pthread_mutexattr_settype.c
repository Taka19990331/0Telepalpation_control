#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_mutexattr_settype.c"
#include "pthread_impl.h"

int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type)
{
	if ((unsigned)type > 2) return EINVAL;
	a->__attr = (a->__attr & ~3) | type;
	return 0;
}
