#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\unshare.c"
#define _GNU_SOURCE
#include <sched.h>
#include "syscall.h"

int unshare(int flags)
{
	return syscall(SYS_unshare, flags);
}
