#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\thrd_exit.c"
#include <threads.h>
#include <pthread.h>
#include <stdint.h>

_Noreturn void thrd_exit(int result)
{
	__pthread_exit((void*)(intptr_t)result);
}
