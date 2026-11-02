#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\kill.c"
#include <signal.h>
#include "syscall.h"

int kill(pid_t pid, int sig)
{
	return syscall(SYS_kill, pid, sig);
}
