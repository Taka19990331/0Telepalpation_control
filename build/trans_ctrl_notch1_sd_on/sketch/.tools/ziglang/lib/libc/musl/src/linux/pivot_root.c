#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\pivot_root.c"
#include "syscall.h"

int pivot_root(const char *new, const char *old)
{
	return syscall(SYS_pivot_root, new, old);
}
