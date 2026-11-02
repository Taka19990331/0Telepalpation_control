#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\cnd_broadcast.c"
#include <threads.h>
#include <pthread.h>

int cnd_broadcast(cnd_t *c)
{
	/* This internal function never fails, and always returns zero,
	 * which matches the value thrd_success is defined with. */
	return __private_cond_signal((pthread_cond_t *)c, -1);
}
