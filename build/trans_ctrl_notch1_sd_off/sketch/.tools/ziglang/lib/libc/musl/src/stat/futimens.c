#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\futimens.c"
#include <sys/stat.h>

int futimens(int fd, const struct timespec times[2])
{
	return utimensat(fd, 0, times, 0);
}
