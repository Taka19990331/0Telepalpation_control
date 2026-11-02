#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\nanosleep.c"
#include <time.h>
#include "syscall.h"

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return __syscall_ret(-__clock_nanosleep(CLOCK_REALTIME, 0, req, rem));
}
