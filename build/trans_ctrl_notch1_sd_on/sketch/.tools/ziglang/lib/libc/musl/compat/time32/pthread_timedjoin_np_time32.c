#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\pthread_timedjoin_np_time32.c"
#define _GNU_SOURCE
#include "time32.h"
#include <time.h>
#include <pthread.h>

int __pthread_timedjoin_np_time32(pthread_t t, void **res, const struct timespec32 *at32)
{
	return pthread_timedjoin_np(t, res, !at32 ? 0 : (&(struct timespec){
		.tv_sec = at32->tv_sec, .tv_nsec = at32->tv_nsec}));
}
