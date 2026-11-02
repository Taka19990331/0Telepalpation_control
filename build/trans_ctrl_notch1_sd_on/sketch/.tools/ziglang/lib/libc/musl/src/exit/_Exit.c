#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\exit\\_Exit.c"
#include <stdlib.h>
#include "syscall.h"

_Noreturn void _Exit(int ec)
{
	__syscall(SYS_exit_group, ec);
	for (;;) __syscall(SYS_exit, ec);
}
