#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\gmtime32.c"
#include "time32.h"
#include <time.h>

struct tm *__gmtime32(time32_t *t)
{
	return gmtime(&(time_t){*t});
}
