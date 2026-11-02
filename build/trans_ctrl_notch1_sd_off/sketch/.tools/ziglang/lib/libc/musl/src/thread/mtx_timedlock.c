#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\mtx_timedlock.c"
#include <threads.h>
#include <pthread.h>
#include <errno.h>

int mtx_timedlock(mtx_t *restrict m, const struct timespec *restrict ts)
{
	int ret = __pthread_mutex_timedlock((pthread_mutex_t *)m, ts);
	switch (ret) {
	default:        return thrd_error;
	case 0:         return thrd_success;
	case ETIMEDOUT: return thrd_timedout;
	}
}
