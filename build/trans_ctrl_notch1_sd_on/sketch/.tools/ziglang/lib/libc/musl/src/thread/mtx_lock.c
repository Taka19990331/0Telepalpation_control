#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\mtx_lock.c"
#include "pthread_impl.h"
#include <threads.h>

int mtx_lock(mtx_t *m)
{
	if (m->_m_type == PTHREAD_MUTEX_NORMAL && !a_cas(&m->_m_lock, 0, EBUSY))
		return thrd_success;
	/* Calling mtx_timedlock with a null pointer is an extension.
	 * It is convenient, here to avoid duplication of the logic
	 * for return values. */
	return mtx_timedlock(m, 0);
}
