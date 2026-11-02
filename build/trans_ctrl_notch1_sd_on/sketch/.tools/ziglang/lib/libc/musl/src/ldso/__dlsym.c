#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\__dlsym.c"
#include <dlfcn.h>
#include "dynlink.h"

static void *stub_dlsym(void *restrict p, const char *restrict s, void *restrict ra)
{
	__dl_seterr("Symbol not found: %s", s);
	return 0;
}

weak_alias(stub_dlsym, __dlsym);

#if _REDIR_TIME64
weak_alias(stub_dlsym, __dlsym_redir_time64);
#endif
