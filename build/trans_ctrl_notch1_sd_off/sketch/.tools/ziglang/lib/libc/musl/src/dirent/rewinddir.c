#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\dirent\\rewinddir.c"
#include <dirent.h>
#include <unistd.h>
#include "__dirent.h"
#include "lock.h"

void rewinddir(DIR *dir)
{
	LOCK(dir->lock);
	lseek(dir->fd, 0, SEEK_SET);
	dir->buf_pos = dir->buf_end = 0;
	dir->tell = 0;
	UNLOCK(dir->lock);
}
