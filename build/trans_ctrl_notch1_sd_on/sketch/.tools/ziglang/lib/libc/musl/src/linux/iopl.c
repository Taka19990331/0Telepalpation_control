#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\iopl.c"
#include "syscall.h"

#ifdef SYS_iopl
#include <sys/io.h>

int iopl(int level)
{
	return syscall(SYS_iopl, level);
}
#endif
