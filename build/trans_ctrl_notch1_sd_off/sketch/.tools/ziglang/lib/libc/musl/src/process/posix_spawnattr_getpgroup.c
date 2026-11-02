#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawnattr_getpgroup.c"
#include <spawn.h>

int posix_spawnattr_getpgroup(const posix_spawnattr_t *restrict attr, pid_t *restrict pgrp)
{
	*pgrp = attr->__pgrp;
	return 0;
}
