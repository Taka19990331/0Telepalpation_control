#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\legacy\\getdtablesize.c"
#define _GNU_SOURCE
#include <unistd.h>
#include <limits.h>
#include <sys/resource.h>

int getdtablesize(void)
{
	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
	return rl.rlim_cur < INT_MAX ? rl.rlim_cur : INT_MAX;
}
