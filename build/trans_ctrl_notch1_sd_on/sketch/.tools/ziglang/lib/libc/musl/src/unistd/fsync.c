#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\fsync.c"
#include <unistd.h>
#include "syscall.h"

int fsync(int fd)
{
	return syscall_cp(SYS_fsync, fd);
}
