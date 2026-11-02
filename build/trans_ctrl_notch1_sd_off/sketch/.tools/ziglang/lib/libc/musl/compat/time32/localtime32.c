#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\localtime32.c"
#include "time32.h"
#include <time.h>

struct tm *__localtime32(time32_t *t)
{
	return localtime(&(time_t){*t});
}
