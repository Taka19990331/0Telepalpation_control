#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\__stdio_close.c"
#include "stdio_impl.h"
#include "aio_impl.h"

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int __stdio_close(FILE *f)
{
	return syscall(SYS_close, __aio_close(f->fd));
}
