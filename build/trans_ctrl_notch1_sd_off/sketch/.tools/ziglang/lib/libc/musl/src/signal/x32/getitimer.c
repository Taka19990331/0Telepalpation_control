#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\x32\\getitimer.c"
#include <sys/time.h>
#include "syscall.h"

int getitimer(int which, struct itimerval *old)
{
	return syscall(SYS_getitimer, which, old);
}
