#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ipc\\msgsnd.c"
#include <sys/msg.h>
#include "syscall.h"
#include "ipc.h"

int msgsnd(int q, const void *m, size_t len, int flag)
{
#ifndef SYS_ipc
	return syscall_cp(SYS_msgsnd, q, m, len, flag);
#else
	return syscall_cp(SYS_ipc, IPCOP_msgsnd, q, len, flag, m);
#endif
}
