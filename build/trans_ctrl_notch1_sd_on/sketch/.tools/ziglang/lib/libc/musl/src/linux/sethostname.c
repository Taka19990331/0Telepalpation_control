#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\sethostname.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"

int sethostname(const char *name, size_t len)
{
	return syscall(SYS_sethostname, name, len);
}
