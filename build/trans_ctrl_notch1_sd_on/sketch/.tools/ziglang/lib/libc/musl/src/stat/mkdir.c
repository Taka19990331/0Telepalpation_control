#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\mkdir.c"
#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"

int mkdir(const char *path, mode_t mode)
{
#ifdef SYS_mkdir
	return syscall(SYS_mkdir, path, mode);
#else
	return syscall(SYS_mkdirat, AT_FDCWD, path, mode);
#endif
}
