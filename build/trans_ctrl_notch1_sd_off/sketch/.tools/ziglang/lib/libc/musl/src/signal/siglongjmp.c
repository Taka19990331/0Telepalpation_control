#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\signal\\siglongjmp.c"
#include <setjmp.h>
#include <signal.h>
#include "syscall.h"
#include "pthread_impl.h"

_Noreturn void siglongjmp(sigjmp_buf buf, int ret)
{
	longjmp(buf, ret);
}
