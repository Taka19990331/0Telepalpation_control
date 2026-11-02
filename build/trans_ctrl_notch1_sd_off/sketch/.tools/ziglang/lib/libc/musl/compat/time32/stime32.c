#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\stime32.c"
#define _GNU_SOURCE
#include "time32.h"
#include <time.h>

int __stime32(const time32_t *t)
{
	return stime(&(time_t){*t});
}
