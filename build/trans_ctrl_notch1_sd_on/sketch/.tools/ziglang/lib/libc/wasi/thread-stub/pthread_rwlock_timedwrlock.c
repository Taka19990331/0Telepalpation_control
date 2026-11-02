#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_rwlock_timedwrlock.c"
#include "pthread_impl.h"

int __pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rw, const struct timespec *restrict at)
{
	return pthread_rwlock_wrlock(rw);
}

weak_alias(__pthread_rwlock_timedwrlock, pthread_rwlock_timedwrlock);
