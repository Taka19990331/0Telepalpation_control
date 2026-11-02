#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\getpeername.c"
#include <sys/socket.h>
#include "syscall.h"

int getpeername(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
	return socketcall(getpeername, fd, addr, len, 0, 0, 0);
}
