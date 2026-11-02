#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\sysinfo.c"
#include <sys/sysinfo.h>
#include "syscall.h"

int __lsysinfo(struct sysinfo *info)
{
	return syscall(SYS_sysinfo, info);
}

weak_alias(__lsysinfo, sysinfo);
