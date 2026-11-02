#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\sigpending.c"
#include <signal.h>
#include "syscall.h"

int sigpending(sigset_t *set)
{
	return syscall(SYS_rt_sigpending, set, _NSIG/8);
}
