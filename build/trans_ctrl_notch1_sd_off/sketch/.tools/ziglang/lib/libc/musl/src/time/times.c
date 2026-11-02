#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\times.c"
#include <sys/times.h>
#include "syscall.h"

clock_t times(struct tms *tms)
{
	return __syscall(SYS_times, tms);
}
