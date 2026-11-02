#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\mlock2.c"
#define _GNU_SOURCE 1
#include <sys/mman.h>
#include "syscall.h"

int mlock2(const void *addr, size_t len, unsigned flags)
{
	if (flags == 0)
		return mlock(addr, len);
	return syscall(SYS_mlock2, addr, len, flags);
}
