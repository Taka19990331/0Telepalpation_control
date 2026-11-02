#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\getdents.c"
#define _BSD_SOURCE
#include <dirent.h>
#include <limits.h>
#include "syscall.h"

int getdents(int fd, struct dirent *buf, size_t len)
{
	if (len>INT_MAX) len = INT_MAX;
	return syscall(SYS_getdents, fd, buf, len);
}
