#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\dlopen.c"
#include <dlfcn.h>
#include "dynlink.h"

static void *stub_dlopen(const char *file, int mode)
{
	__dl_seterr("Dynamic loading not supported");
	return 0;
}

weak_alias(stub_dlopen, dlopen);
