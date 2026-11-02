#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\gmtime32_r.c"
#include "time32.h"
#include <time.h>

struct tm *__gmtime32_r(time32_t *t, struct tm *tm)
{
	return gmtime_r(&(time_t){*t}, tm);
}
