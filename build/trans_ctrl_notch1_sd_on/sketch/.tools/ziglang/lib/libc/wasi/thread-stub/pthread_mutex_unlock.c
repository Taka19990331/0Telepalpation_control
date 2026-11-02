#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\thread-stub\\pthread_mutex_unlock.c"
#include "pthread_impl.h"

int __pthread_mutex_unlock(pthread_mutex_t *m)
{
	if (!m->_m_count) return EPERM;
	m->_m_count--;
	return 0;
}

weak_alias(__pthread_mutex_unlock, pthread_mutex_unlock);
