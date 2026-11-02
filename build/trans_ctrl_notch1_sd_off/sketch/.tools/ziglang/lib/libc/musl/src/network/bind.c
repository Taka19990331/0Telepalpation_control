#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\bind.c"
#include <sys/socket.h>
#include "syscall.h"

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return socketcall(bind, fd, addr, len, 0, 0, 0);
}
