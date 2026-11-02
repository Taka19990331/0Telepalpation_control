#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\process\\execv.c"
#include <unistd.h>

extern char **__environ;

int execv(const char *path, char *const argv[])
{
	return execve(path, argv, __environ);
}
