#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\settimeofday_time32.c"
#define _BSD_SOURCE
#include "time32.h"
#include <sys/time.h>

int __settimeofday_time32(const struct timeval32 *tv32, const void *tz)
{
	return settimeofday(!tv32 ? 0 : (&(struct timeval){
		.tv_sec = tv32->tv_sec,
		.tv_usec = tv32->tv_usec}), 0);
}
