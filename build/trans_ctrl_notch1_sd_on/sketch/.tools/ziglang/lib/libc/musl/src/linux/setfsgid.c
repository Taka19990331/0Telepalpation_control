#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\setfsgid.c"
#include <sys/fsuid.h>
#include "syscall.h"

int setfsgid(gid_t gid)
{
	return syscall(SYS_setfsgid, gid);
}
