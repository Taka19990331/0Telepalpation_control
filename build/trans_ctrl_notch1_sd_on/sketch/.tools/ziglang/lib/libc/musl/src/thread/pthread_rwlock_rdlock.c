#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_rwlock_rdlock.c"
#include "pthread_impl.h"

int __pthread_rwlock_rdlock(pthread_rwlock_t *rw)
{
	return __pthread_rwlock_timedrdlock(rw, 0);
}

weak_alias(__pthread_rwlock_rdlock, pthread_rwlock_rdlock);
