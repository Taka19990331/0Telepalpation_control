#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_getspecific.c"
#include "pthread_impl.h"
#include <threads.h>

static void *__pthread_getspecific(pthread_key_t k)
{
	struct pthread *self = __pthread_self();
	return self->tsd[k];
}

weak_alias(__pthread_getspecific, pthread_getspecific);
weak_alias(__pthread_getspecific, tss_get);
