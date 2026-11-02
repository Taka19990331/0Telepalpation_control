#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\ioperm.c"
#include "syscall.h"

#ifdef SYS_ioperm
#include <sys/io.h>

int ioperm(unsigned long from, unsigned long num, int turn_on)
{
	return syscall(SYS_ioperm, from, num, turn_on);
}
#endif
