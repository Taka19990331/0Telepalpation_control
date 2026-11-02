#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\ppoll_time32.c"
#include "time32.h"
#define _GNU_SOURCE
#include <time.h>
#include <poll.h>

int __ppoll_time32(struct pollfd *fds, nfds_t n, const struct timespec32 *ts32, const sigset_t *mask)
{
	return ppoll(fds, n, !ts32 ? 0 : (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}), mask);
}
