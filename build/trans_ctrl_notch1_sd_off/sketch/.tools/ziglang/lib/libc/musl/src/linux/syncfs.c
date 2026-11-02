#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\syncfs.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"

int syncfs(int fd)
{
	return syscall(SYS_syncfs, fd);
}
