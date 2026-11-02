#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\errno\\__errno_location.c"
#include <errno.h>
#include "pthread_impl.h"

int *__errno_location(void)
{
	return &__pthread_self()->errno_val;
}

weak_alias(__errno_location, ___errno_location);
