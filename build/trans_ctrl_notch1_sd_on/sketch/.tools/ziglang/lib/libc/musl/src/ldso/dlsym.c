#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\dlsym.c"
#include <dlfcn.h>
#include "dynlink.h"

void *dlsym(void *restrict p, const char *restrict s)
{
	return __dlsym(p, s, 0);
}
