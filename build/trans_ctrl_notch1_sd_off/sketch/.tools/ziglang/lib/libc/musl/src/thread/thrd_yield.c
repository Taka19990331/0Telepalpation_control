#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\thrd_yield.c"
#include <threads.h>
#include "syscall.h"

void thrd_yield()
{
	__syscall(SYS_sched_yield);
}
