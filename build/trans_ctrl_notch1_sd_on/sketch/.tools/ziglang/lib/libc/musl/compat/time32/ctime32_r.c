#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\ctime32_r.c"
#include "time32.h"
#include <time.h>

char *__ctime32_r(time32_t *t, char *buf)
{
	return ctime_r(&(time_t){*t}, buf);
}
