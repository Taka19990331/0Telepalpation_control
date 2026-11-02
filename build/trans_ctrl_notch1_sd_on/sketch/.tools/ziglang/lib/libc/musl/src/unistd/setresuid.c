#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\setresuid.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"
#include "libc.h"

int setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	return __setxid(SYS_setresuid, ruid, euid, suid);
}
