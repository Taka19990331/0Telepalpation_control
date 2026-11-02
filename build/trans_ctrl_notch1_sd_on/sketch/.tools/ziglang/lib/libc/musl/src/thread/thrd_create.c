#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\thrd_create.c"
#include "pthread_impl.h"
#include <threads.h>

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	int ret = __pthread_create(thr, __ATTRP_C11_THREAD, (void *(*)(void *))func, arg);
	switch (ret) {
	case 0:      return thrd_success;
	case EAGAIN: return thrd_nomem;
	default:     return thrd_error;
	}
}
