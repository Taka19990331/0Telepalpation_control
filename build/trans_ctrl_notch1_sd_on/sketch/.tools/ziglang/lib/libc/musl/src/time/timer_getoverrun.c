#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\time\\timer_getoverrun.c"
#include <time.h>
#include <limits.h>
#include "pthread_impl.h"

int timer_getoverrun(timer_t t)
{
	if ((intptr_t)t < 0) {
		pthread_t td = (void *)((uintptr_t)t << 1);
		t = (void *)(uintptr_t)(td->timer_id & INT_MAX);
	}
	return syscall(SYS_timer_getoverrun, t);
}
