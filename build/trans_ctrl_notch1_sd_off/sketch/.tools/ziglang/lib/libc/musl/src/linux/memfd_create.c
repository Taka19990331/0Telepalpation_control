#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\memfd_create.c"
#define _GNU_SOURCE 1
#include <sys/mman.h>
#include "syscall.h"

int memfd_create(const char *name, unsigned flags)
{
	return syscall(SYS_memfd_create, name, flags);
}
