#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\dladdr.c"
#define _GNU_SOURCE
#include <dlfcn.h>

static int stub_dladdr(const void *addr, Dl_info *info)
{
	return 0;
}

weak_alias(stub_dladdr, dladdr);
