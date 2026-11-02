#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\adjtimex.c"
#include <sys/timex.h>
#include <time.h>

int adjtimex(struct timex *tx)
{
	return clock_adjtime(CLOCK_REALTIME, tx);
}
