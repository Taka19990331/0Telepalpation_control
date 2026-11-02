#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setgid.c"
#include <unistd.h>
#include "syscall.h"
#include "libc.h"

int setgid(gid_t gid)
{
	return __setxid(SYS_setgid, gid, 0, 0);
}
