#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_barrierattr_init.c"
#include "pthread_impl.h"

int pthread_barrierattr_init(pthread_barrierattr_t *a)
{
	*a = (pthread_barrierattr_t){0};
	return 0;
}
