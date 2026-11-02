#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\dirent\\closedir.c"
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include "__dirent.h"

int closedir(DIR *dir)
{
	int ret = close(dir->fd);
	free(dir);
	return ret;
}
