#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\h_errno.c"
#include <netdb.h>
#include "pthread_impl.h"

#undef h_errno
int h_errno;

int *__h_errno_location(void)
{
	if (!__pthread_self()->stack) return &h_errno;
	return &__pthread_self()->h_errno_val;
}
