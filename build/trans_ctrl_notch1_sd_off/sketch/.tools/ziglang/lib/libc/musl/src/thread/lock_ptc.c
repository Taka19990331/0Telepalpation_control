#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\lock_ptc.c"
#include <pthread.h>

static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;

void __inhibit_ptc()
{
	pthread_rwlock_wrlock(&lock);
}

void __acquire_ptc()
{
	pthread_rwlock_rdlock(&lock);
}

void __release_ptc()
{
	pthread_rwlock_unlock(&lock);
}
