#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawnattr_init.c"
#include <spawn.h>

int posix_spawnattr_init(posix_spawnattr_t *attr)
{
	*attr = (posix_spawnattr_t){ 0 };
	return 0;
}
