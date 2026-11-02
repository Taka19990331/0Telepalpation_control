#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_setcancelstate.c"
#include "pthread_impl.h"

int __pthread_setcancelstate(int new, int *old)
{
	if (new > 2U) return EINVAL;
	struct pthread *self = __pthread_self();
	if (old) *old = self->canceldisable;
	self->canceldisable = new;
	return 0;
}

weak_alias(__pthread_setcancelstate, pthread_setcancelstate);
