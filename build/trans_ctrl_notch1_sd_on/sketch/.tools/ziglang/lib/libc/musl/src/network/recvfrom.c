#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\recvfrom.c"
#include <sys/socket.h>
#include "syscall.h"

ssize_t recvfrom(int fd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict addr, socklen_t *restrict alen)
{
	return socketcall_cp(recvfrom, fd, buf, len, flags, addr, alen);
}
