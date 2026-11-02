#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\misc\\login_tty.c"
#include <utmp.h>
#include <sys/ioctl.h>
#include <unistd.h>

int login_tty(int fd)
{
	setsid();
	if (ioctl(fd, TIOCSCTTY, (char *)0)) return -1;
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd>2) close(fd);
	return 0;
}
