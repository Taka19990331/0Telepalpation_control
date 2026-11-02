#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\aio_suspend_time32.c"
#include "time32.h"
#include <time.h>
#include <aio.h>

int __aio_suspend_time32(const struct aiocb *const cbs[], int cnt, const struct timespec32 *ts32)
{
	return aio_suspend(cbs, cnt, ts32 ? (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}) : 0);
}
