#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\tcsendbreak.c"
#include <termios.h>
#include <sys/ioctl.h>

int tcsendbreak(int fd, int dur)
{
	/* nonzero duration is implementation-defined, so ignore it */
	return ioctl(fd, TCSBRK, 0);
}
