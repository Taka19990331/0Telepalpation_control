#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\fstat.c"
#define _BSD_SOURCE
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"

int __fstat(int fd, struct stat *st)
{
	if (fd<0) return __syscall_ret(-EBADF);
	return __fstatat(fd, "", st, AT_EMPTY_PATH);
}

weak_alias(__fstat, fstat);
