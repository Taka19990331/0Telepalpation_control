#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\tcflush.c"
#include <termios.h>
#include <sys/ioctl.h>

int tcflush(int fd, int queue)
{
	return ioctl(fd, TCFLSH, queue);
}
