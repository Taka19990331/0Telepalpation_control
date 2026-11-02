#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_spin_lock.c"
#include "pthread_impl.h"

int pthread_spin_lock(pthread_spinlock_t *s)
{
	if (*s) return EDEADLK;
	*s = 1;
	return 0;
}
