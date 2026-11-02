#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\select_time32.c"
#include "time32.h"
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>

int __select_time32(int n, fd_set *restrict rfds, fd_set *restrict wfds, fd_set *restrict efds, struct timeval32 *restrict tv32)
{
	return select(n, rfds, wfds, efds, !tv32 ? 0 : (&(struct timeval){
		.tv_sec = tv32->tv_sec, .tv_usec = tv32->tv_usec}));
}
