#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\chmod.c"
#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"

int chmod(const char *path, mode_t mode)
{
#ifdef SYS_chmod
	return syscall(SYS_chmod, path, mode);
#else
	return syscall(SYS_fchmodat, AT_FDCWD, path, mode);
#endif
}
