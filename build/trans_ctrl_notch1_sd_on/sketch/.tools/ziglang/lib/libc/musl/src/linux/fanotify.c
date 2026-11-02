#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\fanotify.c"
#include "syscall.h"
#include <sys/fanotify.h>

int fanotify_init(unsigned flags, unsigned event_f_flags)
{
	return syscall(SYS_fanotify_init, flags, event_f_flags);
}

int fanotify_mark(int fanotify_fd, unsigned flags, unsigned long long mask,
	          int dfd, const char *pathname)
{
	return syscall(SYS_fanotify_mark, fanotify_fd, flags, __SYSCALL_LL_E(mask), dfd, pathname);
}

