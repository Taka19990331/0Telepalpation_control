#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_mutex_destroy.c"
#include "pthread_impl.h"

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	/* If the mutex being destroyed is process-shared and has nontrivial
	 * type (tracking ownership), it might be in the pending slot of a
	 * robust_list; wait for quiescence. */
	if (mutex->_m_type > 128) __vm_wait();
	return 0;
}
