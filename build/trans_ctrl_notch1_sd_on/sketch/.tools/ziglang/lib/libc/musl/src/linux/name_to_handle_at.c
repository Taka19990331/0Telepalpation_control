#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\name_to_handle_at.c"
#define _GNU_SOURCE
#include <fcntl.h>
#include "syscall.h"

int name_to_handle_at(int dirfd, const char *pathname,
	struct file_handle *handle, int *mount_id, int flags)
{
	return syscall(SYS_name_to_handle_at, dirfd,
		pathname, handle, mount_id, flags);
}
