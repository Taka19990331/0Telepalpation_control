#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_condattr_init.c"
#include "pthread_impl.h"

int pthread_condattr_init(pthread_condattr_t *a)
{
	*a = (pthread_condattr_t){0};
	return 0;
}
