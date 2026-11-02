#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_spin_init.c"
#include "pthread_impl.h"

int pthread_spin_init(pthread_spinlock_t *s, int shared)
{
	return *s = 0;
}
