#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\pthread_kill.c"
#include "pthread_impl.h"
#include "lock.h"

int pthread_kill(pthread_t t, int sig)
{
	int r;
	sigset_t set;
	/* Block not just app signals, but internal ones too, since
	 * pthread_kill is used to implement pthread_cancel, which
	 * must be async-cancel-safe. */
	__block_all_sigs(&set);
	LOCK(t->killlock);
	r = t->tid ? -__syscall(SYS_tkill, t->tid, sig)
		: (sig+0U >= _NSIG ? EINVAL : 0);
	UNLOCK(t->killlock);
	__restore_sigs(&set);
	return r;
}
