#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\ttyname.c"
#include <unistd.h>
#include <errno.h>
#include <limits.h>

char *ttyname(int fd)
{
	static char buf[TTY_NAME_MAX];
	int result;
	if ((result = ttyname_r(fd, buf, sizeof buf))) {
		errno = result;
		return NULL;
	}
	return buf;
}
