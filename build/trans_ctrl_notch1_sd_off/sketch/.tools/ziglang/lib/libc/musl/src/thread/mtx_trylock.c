#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\mtx_trylock.c"
#include "pthread_impl.h"
#include <threads.h>

int mtx_trylock(mtx_t *m)
{
	if (m->_m_type == PTHREAD_MUTEX_NORMAL)
		return (a_cas(&m->_m_lock, 0, EBUSY) & EBUSY) ? thrd_busy : thrd_success;

	int ret = __pthread_mutex_trylock((pthread_mutex_t *)m);
	switch (ret) {
	default:    return thrd_error;
	case 0:     return thrd_success;
	case EBUSY: return thrd_busy;
	}
}
