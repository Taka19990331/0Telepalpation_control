#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\arch_prctl.c"
#include "syscall.h"
#ifdef SYS_arch_prctl
int arch_prctl(int code, unsigned long addr)
{
	return syscall(SYS_arch_prctl, code, addr);
}
#endif
