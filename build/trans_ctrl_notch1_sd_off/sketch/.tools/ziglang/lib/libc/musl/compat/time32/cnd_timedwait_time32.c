#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\cnd_timedwait_time32.c"
#include "time32.h"
#include <time.h>
#include <threads.h>

int __cnd_timedwait_time32(cnd_t *restrict c, mtx_t *restrict m, const struct timespec32 *restrict ts32)
{
	return cnd_timedwait(c, m, ts32 ? (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}) : 0);
}
