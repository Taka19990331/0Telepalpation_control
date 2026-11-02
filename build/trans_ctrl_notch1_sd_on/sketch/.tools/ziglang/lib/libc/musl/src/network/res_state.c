#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\res_state.c"
#include <resolv.h>

/* This is completely unused, and exists purely to satisfy broken apps. */

struct __res_state *__res_state()
{
	static struct __res_state res;
	return &res;
}
