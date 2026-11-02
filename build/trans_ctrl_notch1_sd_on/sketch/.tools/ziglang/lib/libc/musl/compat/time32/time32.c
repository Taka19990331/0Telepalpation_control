#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\time32.c"
#include "time32.h"
#include <time.h>
#include <errno.h>
#include <stdint.h>

time32_t __time32(time32_t *p)
{
	time_t t = time(0);
	if (t < INT32_MIN || t > INT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	if (p) *p = t;
	return t;
}
