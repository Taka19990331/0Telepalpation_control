#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\sync_file_range.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int sync_file_range(int fd, off_t pos, off_t len, unsigned flags)
{
#if defined(SYS_sync_file_range2)
	return syscall(SYS_sync_file_range2, fd, flags,
		__SYSCALL_LL_E(pos), __SYSCALL_LL_E(len));
#elif defined(SYS_sync_file_range)
	return syscall(SYS_sync_file_range, fd,
		__SYSCALL_LL_O(pos), __SYSCALL_LL_E(len), flags);
#else
	return __syscall_ret(-ENOSYS);
#endif
}
