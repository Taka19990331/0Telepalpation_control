#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\posix_spawnattr_setpgroup.c"
#include <spawn.h>

int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgrp)
{
	attr->__pgrp = pgrp;
	return 0;
}
