#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\ldso\\dlinfo.c"
#define _GNU_SOURCE
#include <dlfcn.h>
#include "dynlink.h"

int dlinfo(void *dso, int req, void *res)
{
	if (__dl_invalid_handle(dso)) return -1;
	if (req != RTLD_DI_LINKMAP) {
		__dl_seterr("Unsupported request %d", req);
		return -1;
	}
	*(struct link_map **)res = dso;
	return 0;
}
