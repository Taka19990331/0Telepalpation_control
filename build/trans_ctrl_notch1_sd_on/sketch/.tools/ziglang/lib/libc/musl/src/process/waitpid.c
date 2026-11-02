#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\waitpid.c"
#include <sys/wait.h>
#include "syscall.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
	return sys_wait4_cp(pid, status, options, 0);
}
