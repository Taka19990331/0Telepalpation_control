#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\ctime_r.c"
#include <time.h>

char *ctime_r(const time_t *t, char *buf)
{
	struct tm tm, *tm_p = localtime_r(t, &tm);
	return tm_p ? asctime_r(tm_p, buf) : 0;
}
