#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\clock_gettime32.c"
#include "time32.h"
#include <time.h>
#include <errno.h>
#include <stdint.h>

int __clock_gettime32(clockid_t clk, struct timespec32 *ts32)
{
	struct timespec ts;
	int r = clock_gettime(clk, &ts);
	if (r) return r;
	if (ts.tv_sec < INT32_MIN || ts.tv_sec > INT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	ts32->tv_sec = ts.tv_sec;
	ts32->tv_nsec = ts.tv_nsec;
	return 0;
}
