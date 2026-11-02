#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\pause.c"
#include <unistd.h>
#include "syscall.h"

int pause(void)
{
#ifdef SYS_pause
	return syscall_cp(SYS_pause);
#else
	return syscall_cp(SYS_ppoll, 0, 0, 0, 0);
#endif
}
