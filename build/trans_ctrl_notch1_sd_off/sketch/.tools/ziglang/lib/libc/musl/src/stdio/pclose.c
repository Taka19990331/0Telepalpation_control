#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\pclose.c"
#include "stdio_impl.h"
#include <errno.h>
#include <unistd.h>

int pclose(FILE *f)
{
	int status, r;
	pid_t pid = f->pipe_pid;
	fclose(f);
	while ((r=__sys_wait4(pid, &status, 0, 0)) == -EINTR);
	if (r<0) return __syscall_ret(r);
	return status;
}
