#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_rwlock_tryrdlock.c"
#include "pthread_impl.h"

int __pthread_rwlock_tryrdlock(pthread_rwlock_t *rw)
{
	if (rw->_rw_lock == 0x7fffffff) return EBUSY;
	if (rw->_rw_lock == 0x7ffffffe) return EAGAIN;
	rw->_rw_lock++;
	return 0;
}

weak_alias(__pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
