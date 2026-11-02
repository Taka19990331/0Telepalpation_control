#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\mktime32.c"
#include "time32.h"
#include <time.h>
#include <errno.h>
#include <stdint.h>

time32_t __mktime32(struct tm *tm)
{
	struct tm tmp = *tm;
	time_t t = mktime(&tmp);
	if (t < INT32_MIN || t > INT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	*tm = tmp;
	return t;
}
