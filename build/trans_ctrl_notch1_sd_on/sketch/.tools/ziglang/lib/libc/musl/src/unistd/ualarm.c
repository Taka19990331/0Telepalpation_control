#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\ualarm.c"
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/time.h>

unsigned ualarm(unsigned value, unsigned interval)
{
	struct itimerval it = {
		.it_interval.tv_usec = interval,
		.it_value.tv_usec = value
	}, it_old;
	setitimer(ITIMER_REAL, &it, &it_old);
	return it_old.it_value.tv_sec*1000000 + it_old.it_value.tv_usec;
}
