#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\termios\\tcsetwinsize.c"
#include <termios.h>
#include <sys/ioctl.h>
#include "syscall.h"

int tcsetwinsize(int fd, const struct winsize *wsz)
{
	return syscall(SYS_ioctl, fd, TIOCSWINSZ, wsz);
}
