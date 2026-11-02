#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\sendto.c"
#include <sys/socket.h>
#include "syscall.h"

ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t alen)
{
	return socketcall_cp(sendto, fd, buf, len, flags, addr, alen);
}
