#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\if_nametoindex.c"
#define _GNU_SOURCE
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include "syscall.h"

unsigned if_nametoindex(const char *name)
{
	struct ifreq ifr;
	int fd, r;

	if ((fd = socket(AF_UNIX, SOCK_DGRAM|SOCK_CLOEXEC, 0)) < 0) return 0;
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
	r = ioctl(fd, SIOCGIFINDEX, &ifr);
	__syscall(SYS_close, fd);
	return r < 0 ? 0 : ifr.ifr_ifindex;
}
