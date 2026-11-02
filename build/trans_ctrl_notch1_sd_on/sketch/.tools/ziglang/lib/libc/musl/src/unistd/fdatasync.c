#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\fdatasync.c"
#include <unistd.h>
#include "syscall.h"

int fdatasync(int fd)
{
	return syscall_cp(SYS_fdatasync, fd);
}
