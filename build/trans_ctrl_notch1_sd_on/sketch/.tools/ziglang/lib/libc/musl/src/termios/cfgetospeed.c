#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\cfgetospeed.c"
#define _BSD_SOURCE
#include <termios.h>
#include <sys/ioctl.h>

speed_t cfgetospeed(const struct termios *tio)
{
	return tio->c_cflag & CBAUD;
}

speed_t cfgetispeed(const struct termios *tio)
{
	return cfgetospeed(tio);
}
