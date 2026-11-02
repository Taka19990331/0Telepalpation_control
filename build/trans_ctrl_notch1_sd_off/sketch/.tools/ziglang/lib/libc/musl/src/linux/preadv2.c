#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\preadv2.c"
#define _GNU_SOURCE
#include <sys/uio.h>
#include <unistd.h>
#include "syscall.h"

ssize_t preadv2(int fd, const struct iovec *iov, int count, off_t ofs, int flags)
{
#ifdef SYS_preadv
	if (!flags) {
		if (ofs==-1) return readv(fd, iov, count);
		return syscall_cp(SYS_preadv, fd, iov, count,
			(long)(ofs), (long)(ofs>>32));
	}
#endif
	return syscall_cp(SYS_preadv2, fd, iov, count,
		(long)(ofs), (long)(ofs>>32), flags);
}
