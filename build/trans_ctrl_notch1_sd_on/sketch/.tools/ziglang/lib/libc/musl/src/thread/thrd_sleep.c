#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\thrd_sleep.c"
#include <threads.h>
#include <time.h>
#include <errno.h>
#include "syscall.h"

int thrd_sleep(const struct timespec *req, struct timespec *rem)
{
	int ret = -__clock_nanosleep(CLOCK_REALTIME, 0, req, rem);
	switch (ret) {
	case 0:      return 0;
	case -EINTR: return -1; /* value specified by C11 */
	default:     return -2;
	}
}
