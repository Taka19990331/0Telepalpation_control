#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\sem_unlink.c"
#include <semaphore.h>
#include <sys/mman.h>

int sem_unlink(const char *name)
{
	return shm_unlink(name);
}
