#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\wasi\\libc-top-half\\musl\\src\\dirent\\versionsort.c"
#define _GNU_SOURCE
#include <string.h>
#include <dirent.h>

int versionsort(const struct dirent **a, const struct dirent **b)
{
	return strverscmp((*a)->d_name, (*b)->d_name);
}

#undef versionsort64
weak_alias(versionsort, versionsort64);
