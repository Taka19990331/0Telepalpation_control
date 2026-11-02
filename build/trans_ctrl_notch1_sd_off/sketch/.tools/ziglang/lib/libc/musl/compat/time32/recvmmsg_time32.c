#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\recvmmsg_time32.c"
#include "time32.h"
#define _GNU_SOURCE
#include <time.h>
#include <sys/socket.h>

int __recvmmsg_time32(int fd, struct mmsghdr *msgvec, unsigned int vlen, unsigned int flags, struct timespec32 *ts32)
{
	return recvmmsg(fd, msgvec, vlen, flags, ts32 ? (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}) : 0);
}
