#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_barrier_wait.c"
#include "pthread_impl.h"

int pthread_barrier_wait(pthread_barrier_t *b)
{
	if (!b->_b_limit) return PTHREAD_BARRIER_SERIAL_THREAD;
	__builtin_trap();
}
