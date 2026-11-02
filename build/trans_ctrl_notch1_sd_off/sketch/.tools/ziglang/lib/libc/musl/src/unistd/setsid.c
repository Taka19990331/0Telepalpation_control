#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setsid.c"
#include <unistd.h>
#include "syscall.h"

pid_t setsid(void)
{
	return syscall(SYS_setsid);
}
