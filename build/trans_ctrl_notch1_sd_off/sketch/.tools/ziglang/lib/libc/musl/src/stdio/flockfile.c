#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\flockfile.c"
#include "stdio_impl.h"
#include "pthread_impl.h"

void flockfile(FILE *f)
{
	if (!ftrylockfile(f)) return;
	__lockfile(f);
	__register_locked_file(f, __pthread_self());
}
