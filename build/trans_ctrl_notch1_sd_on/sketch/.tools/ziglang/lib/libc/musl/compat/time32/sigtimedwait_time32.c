#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\sigtimedwait_time32.c"
#include "time32.h"
#include <time.h>
#include <signal.h>

int __sigtimedwait_time32(const sigset_t *restrict set, siginfo_t *restrict si, const struct timespec32 *restrict ts32)
{
	return sigtimedwait(set, si, !ts32 ? 0 : (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}));
}
