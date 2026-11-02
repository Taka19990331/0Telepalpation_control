#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\sem_timedwait_time32.c"
#include "time32.h"
#include <time.h>
#include <semaphore.h>

int __sem_timedwait_time32(sem_t *sem, const struct timespec32 *restrict ts32)
{
	return sem_timedwait(sem, !ts32 ? 0 : (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}));
}
