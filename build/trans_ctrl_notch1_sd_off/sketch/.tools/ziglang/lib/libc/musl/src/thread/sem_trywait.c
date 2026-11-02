#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\sem_trywait.c"
#include <semaphore.h>
#include <limits.h>
#include "pthread_impl.h"

int sem_trywait(sem_t *sem)
{
	int val;
	while ((val=sem->__val[0]) & SEM_VALUE_MAX) {
		if (a_cas(sem->__val, val, val-1)==val) return 0;
	}
	errno = EAGAIN;
	return -1;
}
