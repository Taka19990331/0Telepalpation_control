#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setresgid.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"
#include "libc.h"

int setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	return __setxid(SYS_setresgid, rgid, egid, sgid);
}
