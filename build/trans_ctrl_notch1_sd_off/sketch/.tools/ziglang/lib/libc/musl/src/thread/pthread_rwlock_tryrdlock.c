#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_rwlock_tryrdlock.c"
#include "pthread_impl.h"

int __pthread_rwlock_tryrdlock(pthread_rwlock_t *rw)
{
	int val, cnt;
	do {
		val = rw->_rw_lock;
		cnt = val & 0x7fffffff;
		if (cnt == 0x7fffffff) return EBUSY;
		if (cnt == 0x7ffffffe) return EAGAIN;
	} while (a_cas(&rw->_rw_lock, val, val+1) != val);
	return 0;
}

weak_alias(__pthread_rwlock_tryrdlock, pthread_rwlock_tryrdlock);
