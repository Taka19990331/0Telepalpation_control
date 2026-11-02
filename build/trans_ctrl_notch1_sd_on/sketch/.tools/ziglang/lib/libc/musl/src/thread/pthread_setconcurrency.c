#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_setconcurrency.c"
#include <pthread.h>
#include <errno.h>

int pthread_setconcurrency(int val)
{
	if (val < 0) return EINVAL;
	if (val > 0) return EAGAIN;
	return 0;
}
