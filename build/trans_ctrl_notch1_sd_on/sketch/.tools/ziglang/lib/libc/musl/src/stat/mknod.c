#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\mknod.c"
#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"

int mknod(const char *path, mode_t mode, dev_t dev)
{
#ifdef SYS_mknod
	return syscall(SYS_mknod, path, mode, dev);
#else
	return syscall(SYS_mknodat, AT_FDCWD, path, mode, dev);
#endif
}
