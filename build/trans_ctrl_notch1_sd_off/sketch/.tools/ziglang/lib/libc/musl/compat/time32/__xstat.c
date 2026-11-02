#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\__xstat.c"
#include "time32.h"
#include <sys/stat.h>

struct stat32;

int __fxstat(int ver, int fd, struct stat32 *buf)
{
	return __fstat_time32(fd, buf);
}

int __fxstatat(int ver, int fd, const char *path, struct stat32 *buf, int flag)
{
	return __fstatat_time32(fd, path, buf, flag);
}

int __lxstat(int ver, const char *path, struct stat32 *buf)
{
	return __lstat_time32(path, buf);
}

int __xstat(int ver, const char *path, struct stat32 *buf)
{
	return __stat_time32(path, buf);
}
