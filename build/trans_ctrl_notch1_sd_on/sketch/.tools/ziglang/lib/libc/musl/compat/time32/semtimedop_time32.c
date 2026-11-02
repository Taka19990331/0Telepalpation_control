#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\semtimedop_time32.c"
#include "time32.h"
#define _GNU_SOURCE
#include <sys/sem.h>
#include <time.h>

int __semtimedop_time32(int id, struct sembuf *buf, size_t n, const struct timespec32 *ts32)
{
	return semtimedop(id, buf, n, !ts32 ? 0 : (&(struct timespec){
		.tv_sec = ts32->tv_sec, .tv_nsec = ts32->tv_nsec}));
}
