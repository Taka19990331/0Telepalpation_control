#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\asctime.c"
#include <time.h>

char *asctime(const struct tm *tm)
{
	static char buf[26];
	return __asctime_r(tm, buf);
}
