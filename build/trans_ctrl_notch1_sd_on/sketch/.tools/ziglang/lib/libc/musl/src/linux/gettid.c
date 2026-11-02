#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\gettid.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "pthread_impl.h"

pid_t gettid(void)
{
	return __pthread_self()->tid;
}
