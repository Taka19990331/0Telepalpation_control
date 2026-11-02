#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\wait.c"
#include <sys/wait.h>

pid_t wait(int *status)
{
	return waitpid((pid_t)-1, status, 0);
}
