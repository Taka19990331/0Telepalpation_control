#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\sched\\sched_get_priority_max.c"
#include <sched.h>
#include "syscall.h"

int sched_get_priority_max(int policy)
{
	return syscall(SYS_sched_get_priority_max, policy);
}

int sched_get_priority_min(int policy)
{
	return syscall(SYS_sched_get_priority_min, policy);
}
