#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\ftruncate.c"
#include <unistd.h>
#include "syscall.h"

int ftruncate(int fd, off_t length)
{
	return syscall(SYS_ftruncate, fd, __SYSCALL_LL_O(length));
}
