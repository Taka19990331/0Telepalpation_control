#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\quotactl.c"
#include <sys/quota.h>
#include "syscall.h"

int quotactl(int cmd, const char *special, int id, char *addr)
{
	return syscall(SYS_quotactl, cmd, special, id, addr);
}
