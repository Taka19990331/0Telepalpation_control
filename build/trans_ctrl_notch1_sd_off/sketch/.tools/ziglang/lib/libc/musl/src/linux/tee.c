#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\tee.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

ssize_t tee(int src, int dest, size_t len, unsigned flags)
{
	return syscall(SYS_tee, src, dest, len, flags);
}
