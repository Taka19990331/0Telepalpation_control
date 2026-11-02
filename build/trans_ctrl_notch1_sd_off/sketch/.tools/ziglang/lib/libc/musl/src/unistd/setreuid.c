#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setreuid.c"
#include <unistd.h>
#include "syscall.h"
#include "libc.h"

int setreuid(uid_t ruid, uid_t euid)
{
	return __setxid(SYS_setreuid, ruid, euid, 0);
}
