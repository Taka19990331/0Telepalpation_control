#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\thread\\pthread_attr_setguardsize.c"
#include "pthread_impl.h"

int pthread_attr_setguardsize(pthread_attr_t *a, size_t size)
{
#ifdef __wasilibc_unmodified_upstream
	if (size > SIZE_MAX/8) return EINVAL;
#else
	/* WASI doesn't have memory protection required for stack guards. */
	if (size > 0) return EINVAL;
#endif
	a->_a_guardsize = size;
	return 0;
}
