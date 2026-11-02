#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\ftime.c"
#include <sys/timeb.h>
#include <time.h>

int ftime(struct timeb *tp)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	tp->time = ts.tv_sec;
	tp->millitm = ts.tv_nsec / 1000000;
	tp->timezone = tp->dstflag = 0;
	return 0;
}
