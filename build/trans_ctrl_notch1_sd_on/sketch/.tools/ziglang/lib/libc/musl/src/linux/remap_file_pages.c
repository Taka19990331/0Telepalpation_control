#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\linux\\remap_file_pages.c"
#define _GNU_SOURCE
#include <sys/mman.h>
#include "syscall.h"

int remap_file_pages(void *addr, size_t size, int prot, size_t pgoff, int flags)
{
	return syscall(SYS_remap_file_pages, addr, size, prot, pgoff, flags);
}
