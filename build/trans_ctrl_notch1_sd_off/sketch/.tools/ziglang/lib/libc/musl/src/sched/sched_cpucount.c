#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\sched\\sched_cpucount.c"
#define _GNU_SOURCE
#include <sched.h>

int __sched_cpucount(size_t size, const cpu_set_t *set)
{
	size_t i, j, cnt=0;
	const unsigned char *p = (const void *)set;
	for (i=0; i<size; i++) for (j=0; j<8; j++)
		if (p[i] & (1<<j)) cnt++;
	return cnt;
}
