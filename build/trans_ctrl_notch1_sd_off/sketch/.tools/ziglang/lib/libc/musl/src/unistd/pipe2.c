#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\pipe2.c"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"

int pipe2(int fd[2], int flag)
{
	if (!flag) return pipe(fd);
	int ret = __syscall(SYS_pipe2, fd, flag);
	if (ret != -ENOSYS) return __syscall_ret(ret);
	if (flag & ~(O_CLOEXEC|O_NONBLOCK)) return __syscall_ret(-EINVAL);
	ret = pipe(fd);
	if (ret) return ret;
	if (flag & O_CLOEXEC) {
		__syscall(SYS_fcntl, fd[0], F_SETFD, FD_CLOEXEC);
		__syscall(SYS_fcntl, fd[1], F_SETFD, FD_CLOEXEC);
	}
	if (flag & O_NONBLOCK) {
		__syscall(SYS_fcntl, fd[0], F_SETFL, O_NONBLOCK);
		__syscall(SYS_fcntl, fd[1], F_SETFL, O_NONBLOCK);
	}
	return 0;
}
