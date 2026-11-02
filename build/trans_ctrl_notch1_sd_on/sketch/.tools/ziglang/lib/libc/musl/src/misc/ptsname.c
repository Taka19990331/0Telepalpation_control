#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\misc\\ptsname.c"
#include <stdlib.h>
#include <errno.h>

char *ptsname(int fd)
{
	static char buf[9 + sizeof(int)*3 + 1];
	int err = __ptsname_r(fd, buf, sizeof buf);
	if (err) {
		errno = err;
		return 0;
	}
	return buf;
}
