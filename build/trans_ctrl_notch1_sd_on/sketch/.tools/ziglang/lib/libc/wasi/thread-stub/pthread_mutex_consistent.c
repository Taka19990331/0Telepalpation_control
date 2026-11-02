#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_mutex_consistent.c"
#include "pthread_impl.h"

int pthread_mutex_consistent(pthread_mutex_t *m)
{
	/* cannot be a robust mutex, as they're entirely unsupported in WASI */
	return EINVAL;

}
