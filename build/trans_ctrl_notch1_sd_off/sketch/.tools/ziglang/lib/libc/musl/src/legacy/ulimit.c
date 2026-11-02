#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\legacy\\ulimit.c"
#include <sys/resource.h>
#include <ulimit.h>
#include <stdarg.h>

long ulimit(int cmd, ...)
{
	struct rlimit rl;
	getrlimit(RLIMIT_FSIZE, &rl);
	if (cmd == UL_SETFSIZE) {
		long val;
		va_list ap;
		va_start(ap, cmd);
		val = va_arg(ap, long);
		va_end(ap);
		rl.rlim_cur = 512ULL * val;
		if (setrlimit(RLIMIT_FSIZE, &rl)) return -1;
	}
	return rl.rlim_cur / 512;
}
