#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawnattr_getflags.c"
#include <spawn.h>

int posix_spawnattr_getflags(const posix_spawnattr_t *restrict attr, short *restrict flags)
{
	*flags = attr->__flags;
	return 0;
}
