#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\socketpair.c"
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int socketpair(int domain, int type, int protocol, int fd[2])
{
	int r = socketcall(socketpair, domain, type, protocol, fd, 0, 0);
	if (r<0 && (errno==EINVAL || errno==EPROTONOSUPPORT)
	    && (type&(SOCK_CLOEXEC|SOCK_NONBLOCK))) {
		r = socketcall(socketpair, domain,
			type & ~(SOCK_CLOEXEC|SOCK_NONBLOCK),
			protocol, fd, 0, 0);
		if (r < 0) return r;
		if (type & SOCK_CLOEXEC) {
			__syscall(SYS_fcntl, fd[0], F_SETFD, FD_CLOEXEC);
			__syscall(SYS_fcntl, fd[1], F_SETFD, FD_CLOEXEC);
		}
		if (type & SOCK_NONBLOCK) {
			__syscall(SYS_fcntl, fd[0], F_SETFL, O_NONBLOCK);
			__syscall(SYS_fcntl, fd[1], F_SETFL, O_NONBLOCK);
		}
	}
	return r;
}
