#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\cfsetospeed.c"
#define _BSD_SOURCE
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int cfsetospeed(struct termios *tio, speed_t speed)
{
	if (speed & ~CBAUD) {
		errno = EINVAL;
		return -1;
	}
	tio->c_cflag &= ~CBAUD;
	tio->c_cflag |= speed;
	return 0;
}

int cfsetispeed(struct termios *tio, speed_t speed)
{
	return speed ? cfsetospeed(tio, speed) : 0;
}

weak_alias(cfsetospeed, cfsetspeed);
