#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\sched\\sched_yield.c"
#include <sched.h>
#include "syscall.h"

int sched_yield()
{
	return syscall(SYS_sched_yield);
}
