#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\call_once.c"
#include <threads.h>
#include <pthread.h>

void call_once(once_flag *flag, void (*func)(void))
{
	__pthread_once(flag, func);
}
