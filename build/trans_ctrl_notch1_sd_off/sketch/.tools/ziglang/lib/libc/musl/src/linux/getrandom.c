#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\getrandom.c"
#include <sys/random.h>
#include "syscall.h"

ssize_t getrandom(void *buf, size_t buflen, unsigned flags)
{
	return syscall_cp(SYS_getrandom, buf, buflen, flags);
}
