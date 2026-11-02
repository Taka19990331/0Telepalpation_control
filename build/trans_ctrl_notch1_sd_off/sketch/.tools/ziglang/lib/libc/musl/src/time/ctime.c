#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\ctime.c"
#include <time.h>

char *ctime(const time_t *t)
{
	struct tm *tm = localtime(t);
	if (!tm) return 0;
	return asctime(tm);
}
