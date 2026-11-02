#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\sem_wait.c"
#include <semaphore.h>

int sem_wait(sem_t *sem)
{
	return sem_timedwait(sem, 0);
}
