#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\readlinkat.c"
#include <unistd.h>
#include "syscall.h"

ssize_t readlinkat(int fd, const char *restrict path, char *restrict buf, size_t bufsize)
{
	char dummy[1];
	if (!bufsize) {
		buf = dummy;
		bufsize = 1;
	}
	int r = __syscall(SYS_readlinkat, fd, path, buf, bufsize);
	if (buf == dummy && r > 0) r = 0;
	return __syscall_ret(r);
}
