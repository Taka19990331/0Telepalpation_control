#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stat\\umask.c"
#include <sys/stat.h>
#include "syscall.h"

mode_t umask(mode_t mode)
{
	return syscall(SYS_umask, mode);
}
