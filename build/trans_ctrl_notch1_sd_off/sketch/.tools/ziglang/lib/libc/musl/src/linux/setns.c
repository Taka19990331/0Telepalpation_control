#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\setns.c"
#define _GNU_SOURCE
#include <sched.h>
#include "syscall.h"

int setns(int fd, int nstype)
{
	return syscall(SYS_setns, fd, nstype);
}
