#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\sockatmark.c"
#include <sys/socket.h>
#include <sys/ioctl.h>

int sockatmark(int s)
{
	int ret;
	if (ioctl(s, SIOCATMARK, &ret) < 0)
		return -1;
	return ret;
}
