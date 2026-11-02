#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\swap.c"
#include <sys/swap.h>
#include "syscall.h"

int swapon(const char *path, int flags)
{
	return syscall(SYS_swapon, path, flags);
}

int swapoff(const char *path)
{
	return syscall(SYS_swapoff, path);
}
