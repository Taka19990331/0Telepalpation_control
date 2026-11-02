#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\brk.c"
#define _BSD_SOURCE
#include <unistd.h>
#include <errno.h>
#include "syscall.h"

int brk(void *end)
{
	return __syscall_ret(-ENOMEM);
}
