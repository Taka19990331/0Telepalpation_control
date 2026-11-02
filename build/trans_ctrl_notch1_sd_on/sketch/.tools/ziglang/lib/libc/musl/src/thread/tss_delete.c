#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\tss_delete.c"
#include <threads.h>
#include <pthread.h>

void tss_delete(tss_t key)
{
	__pthread_key_delete(key);
}
