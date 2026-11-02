#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\dlclose.c"
#include <dlfcn.h>
#include "dynlink.h"

int dlclose(void *p)
{
	return __dl_invalid_handle(p);
}
