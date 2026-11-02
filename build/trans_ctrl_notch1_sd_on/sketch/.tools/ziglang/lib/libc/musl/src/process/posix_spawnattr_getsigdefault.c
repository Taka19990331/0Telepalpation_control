#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawnattr_getsigdefault.c"
#include <spawn.h>

int posix_spawnattr_getsigdefault(const posix_spawnattr_t *restrict attr, sigset_t *restrict def)
{
	*def = attr->__def;
	return 0;
}
