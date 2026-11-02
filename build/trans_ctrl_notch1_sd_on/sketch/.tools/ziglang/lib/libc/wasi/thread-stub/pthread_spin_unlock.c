#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_spin_unlock.c"
#include "pthread_impl.h"

int pthread_spin_unlock(pthread_spinlock_t *s)
{
	*s = 0;
	return 0;
}
