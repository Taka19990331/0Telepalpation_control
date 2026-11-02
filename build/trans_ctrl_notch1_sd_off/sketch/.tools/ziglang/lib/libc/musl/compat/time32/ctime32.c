#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\ctime32.c"
#include "time32.h"
#include <time.h>

char *__ctime32(time32_t *t)
{
	return ctime(&(time_t){*t});
}
