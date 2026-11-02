#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\readahead.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

ssize_t readahead(int fd, off_t pos, size_t len)
{
	return syscall(SYS_readahead, fd, __SYSCALL_LL_O(pos), len);
}
