#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\tss_set.c"
#include "pthread_impl.h"
#include <threads.h>

int tss_set(tss_t k, void *x)
{
	struct pthread *self = __pthread_self();
	/* Avoid unnecessary COW */
	if (self->tsd[k] != x) {
		self->tsd[k] = x;
		self->tsd_used = 1;
	}
	return thrd_success;
}
