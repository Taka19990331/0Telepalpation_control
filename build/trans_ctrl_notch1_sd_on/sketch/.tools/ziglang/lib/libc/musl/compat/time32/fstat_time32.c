#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\compat\\time32\\fstat_time32.c"
#include "time32.h"
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <stddef.h>

struct stat32;

int __fstat_time32(int fd, struct stat32 *restrict st32)
{
	struct stat st;
	int r = fstat(fd, &st);
	if (!r) memcpy(st32, &st, offsetof(struct stat, st_atim));
	return r;
}
