#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\preadv.c"
#define _BSD_SOURCE
#include <sys/uio.h>
#include <unistd.h>
#include "syscall.h"

ssize_t preadv(int fd, const struct iovec *iov, int count, off_t ofs)
{
	return syscall_cp(SYS_preadv, fd, iov, count,
		(long)(ofs), (long)(ofs>>32));
}
