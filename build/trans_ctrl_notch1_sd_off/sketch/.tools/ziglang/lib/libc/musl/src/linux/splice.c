#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\splice.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

ssize_t splice(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned flags)
{
	return syscall(SYS_splice, fd_in, off_in, fd_out, off_out, len, flags);
}
