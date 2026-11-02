#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\alarm.c"
#include <unistd.h>
#include <sys/time.h>
#include "syscall.h"

unsigned alarm(unsigned seconds)
{
	struct itimerval it = { .it_value.tv_sec = seconds }, old = { 0 };
	setitimer(ITIMER_REAL, &it, &old);
	return old.it_value.tv_sec + !!old.it_value.tv_usec;
}
