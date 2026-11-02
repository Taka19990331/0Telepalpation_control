#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigsuspend.c"
#include <signal.h>
#include "syscall.h"

int sigsuspend(const sigset_t *mask)
{
	return syscall_cp(SYS_rt_sigsuspend, mask, _NSIG/8);
}
