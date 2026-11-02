#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\pthread_mutex_timedlock_time32.c"
#include "time32.h"
#include <time.h>
#include <pthread.h>

int __pthread_mutex_timedlock_time32(pthread_mutex_t *restrict m, const struct timespec32 *restrict ts32)
{
	return pthread_mutex_timedlock(m, !ts32 ? 0 : (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}));
}
