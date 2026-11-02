#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_rwlock_init.c"
#include "pthread_impl.h"

int pthread_rwlock_init(pthread_rwlock_t *restrict rw, const pthread_rwlockattr_t *restrict a)
{
	*rw = (pthread_rwlock_t){0};
	if (a) rw->_rw_shared = a->__attr[0]*128;
	return 0;
}
