#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_rwlock_unlock.c"
#include "pthread_impl.h"

int __pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
	if (rw->_rw_lock == 0x7fffffff)
		rw->_rw_lock = 0;
	else
		rw->_rw_lock--;
	return 0;
}

weak_alias(__pthread_rwlock_unlock, pthread_rwlock_unlock);
