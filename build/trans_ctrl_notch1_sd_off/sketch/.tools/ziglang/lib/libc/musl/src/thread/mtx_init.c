#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\mtx_init.c"
#include "pthread_impl.h"
#include <threads.h>

int mtx_init(mtx_t *m, int type)
{
	*m = (mtx_t){
		._m_type = ((type&mtx_recursive) ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL),
	};
	return thrd_success;
}
