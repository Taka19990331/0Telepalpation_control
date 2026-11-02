#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setegid.c"
#include <unistd.h>
#include "libc.h"
#include "syscall.h"

int setegid(gid_t egid)
{
	return __setxid(SYS_setresgid, -1, egid, -1);
}
