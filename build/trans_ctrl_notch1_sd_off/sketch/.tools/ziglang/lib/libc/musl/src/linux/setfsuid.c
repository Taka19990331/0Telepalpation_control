#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\setfsuid.c"
#include <sys/fsuid.h>
#include "syscall.h"

int setfsuid(uid_t uid)
{
	return syscall(SYS_setfsuid, uid);
}
