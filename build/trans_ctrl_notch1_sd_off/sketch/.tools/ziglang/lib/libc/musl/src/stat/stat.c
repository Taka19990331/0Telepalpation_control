#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\stat.c"
#include <sys/stat.h>
#include <fcntl.h>

int stat(const char *restrict path, struct stat *restrict buf)
{
	return fstatat(AT_FDCWD, path, buf, 0);
}
