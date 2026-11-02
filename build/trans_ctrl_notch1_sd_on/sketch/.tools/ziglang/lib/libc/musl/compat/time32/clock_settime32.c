#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\clock_settime32.c"
#include "time32.h"
#include <time.h>

int __clock_settime32(clockid_t clk, const struct timespec32 *ts32)
{
	return clock_settime(clk, (&(struct timespec){
		.tv_sec = ts32->tv_sec,
		.tv_nsec = ts32->tv_nsec}));
}
