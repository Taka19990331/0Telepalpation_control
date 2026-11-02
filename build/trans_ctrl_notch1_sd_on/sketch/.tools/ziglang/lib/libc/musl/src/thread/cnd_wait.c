#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\cnd_wait.c"
#include <threads.h>

int cnd_wait(cnd_t *c, mtx_t *m)
{
	/* Calling cnd_timedwait with a null pointer is an extension.
	 * It is convenient here to avoid duplication of the logic
	 * for return values. */
	return cnd_timedwait(c, m, 0);
}
