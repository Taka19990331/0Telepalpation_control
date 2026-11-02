#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\adjtimex_time32.c"
#include "time32.h"
#include <time.h>
#include <sys/timex.h>

struct timex32;

int __adjtimex_time32(struct timex32 *tx32)
{
	return __clock_adjtime32(CLOCK_REALTIME, tx32);
}
