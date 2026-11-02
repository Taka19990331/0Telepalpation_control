#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\dirent\\dirfd.c"
#include <dirent.h>
#include "__dirent.h"

int dirfd(DIR *d)
{
	return d->fd;
}
