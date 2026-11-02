#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_rwlockattr_init.c"
#include "pthread_impl.h"

int pthread_rwlockattr_init(pthread_rwlockattr_t *a)
{
	*a = (pthread_rwlockattr_t){0};
	return 0;
}
