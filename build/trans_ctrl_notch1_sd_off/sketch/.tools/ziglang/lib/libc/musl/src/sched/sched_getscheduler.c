#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\sched\\sched_getscheduler.c"
#include <sched.h>
#include <errno.h>
#include "syscall.h"

int sched_getscheduler(pid_t pid)
{
	return __syscall_ret(-ENOSYS);
}
