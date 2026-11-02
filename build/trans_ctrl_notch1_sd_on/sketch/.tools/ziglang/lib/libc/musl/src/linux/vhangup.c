#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\vhangup.c"
#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"

int vhangup(void)
{
	return syscall(SYS_vhangup);
}
