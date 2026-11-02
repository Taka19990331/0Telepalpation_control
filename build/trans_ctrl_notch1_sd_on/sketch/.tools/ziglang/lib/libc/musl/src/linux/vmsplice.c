#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\vmsplice.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

ssize_t vmsplice(int fd, const struct iovec *iov, size_t cnt, unsigned flags)
{
	return syscall(SYS_vmsplice, fd, iov, cnt, flags);
}
