#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\ftime32.c"
#include "time32.h"
#include <sys/timeb.h>
#include <errno.h>
#include <stdint.h>

struct timeb32 {
	int32_t time;
	unsigned short millitm;
	short timezone, dstflag;
};

int __ftime32(struct timeb32 *tp)
{
	struct timeb tb;
	if (ftime(&tb) < 0) return -1;
	if (tb.time < INT32_MIN || tb.time > INT32_MAX) {
		errno = EOVERFLOW;
		return -1;
	}
	tp->time = tb.time;
	tp->millitm = tb.millitm;
	tp->timezone = tb.timezone;
	tp->dstflag = tb.dstflag;
	return 0;
}
