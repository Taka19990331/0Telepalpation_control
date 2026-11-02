#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\tcflow.c"
#include <termios.h>
#include <sys/ioctl.h>

int tcflow(int fd, int action)
{
	return ioctl(fd, TCXONC, action);
}
