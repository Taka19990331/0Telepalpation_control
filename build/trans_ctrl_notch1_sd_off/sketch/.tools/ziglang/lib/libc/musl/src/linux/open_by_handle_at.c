#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\open_by_handle_at.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

int open_by_handle_at(int mount_fd, struct file_handle *handle, int flags)
{
	return syscall(SYS_open_by_handle_at, mount_fd, handle, flags);
}
