#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\funlockfile.c"
#include "stdio_impl.h"
#include "pthread_impl.h"

void funlockfile(FILE *f)
{
	if (f->lockcount == 1) {
		__unlist_locked_file(f);
		f->lockcount = 0;
		__unlockfile(f);
	} else {
		f->lockcount--;
	}
}
