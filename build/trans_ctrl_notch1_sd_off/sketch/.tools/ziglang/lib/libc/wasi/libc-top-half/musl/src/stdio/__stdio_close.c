#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\stdio\\__stdio_close.c"
#ifdef __wasilibc_unmodified_upstream // WASI has no syscall
#else
#include <unistd.h>
#endif
#include "stdio_impl.h"
#include "aio_impl.h"

#if defined(__wasilibc_unmodified_upstream) || defined(_REENTRANT)
static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);
#else
#define __aio_close(fd) (fd)
#endif

int __stdio_close(FILE *f)
{
#ifdef __wasilibc_unmodified_upstream // WASI has no syscall
	return syscall(SYS_close, __aio_close(f->fd));
#else
	return close(__aio_close(f->fd));
#endif
}
