#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_equal.c"
#include <pthread.h>
#include <threads.h>

static int __pthread_equal(pthread_t a, pthread_t b)
{
	return a==b;
}

weak_alias(__pthread_equal, pthread_equal);
weak_alias(__pthread_equal, thrd_equal);
