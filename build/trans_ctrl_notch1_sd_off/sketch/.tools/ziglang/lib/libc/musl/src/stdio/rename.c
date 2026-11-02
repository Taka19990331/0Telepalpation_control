#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\rename.c"
#include <stdio.h>
#include <fcntl.h>
#include "syscall.h"

int rename(const char *old, const char *new)
{
#if defined(SYS_rename)
	return syscall(SYS_rename, old, new);
#elif defined(SYS_renameat)
	return syscall(SYS_renameat, AT_FDCWD, old, AT_FDCWD, new);
#else
	return syscall(SYS_renameat2, AT_FDCWD, old, AT_FDCWD, new, 0);
#endif
}
