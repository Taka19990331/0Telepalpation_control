#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\utime.c"
#include <utime.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>

int utime(const char *path, const struct utimbuf *times)
{
	return utimensat(AT_FDCWD, path, times ? ((struct timespec [2]){
		{ .tv_sec = times->actime }, { .tv_sec = times->modtime }})
		: 0, 0);
}
