#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\thread\\cnd_init.c"
#include <threads.h>

int cnd_init(cnd_t *c)
{
	*c = (cnd_t){ 0 };
	return thrd_success;
}
