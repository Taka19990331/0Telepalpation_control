#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\listen.c"
#include <sys/socket.h>
#include "syscall.h"

int listen(int fd, int backlog)
{
	return socketcall(listen, fd, backlog, 0, 0, 0, 0);
}
