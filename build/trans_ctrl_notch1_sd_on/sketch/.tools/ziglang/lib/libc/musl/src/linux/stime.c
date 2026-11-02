#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\stime.c"
#define _GNU_SOURCE
#include <time.h>
#include <sys/time.h>

int stime(const time_t *t)
{
	struct timeval tv = { .tv_sec = *t, .tv_usec = 0 };
	return settimeofday(&tv, (void *)0);
}
