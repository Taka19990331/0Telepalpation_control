#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\legacy\\getloadavg.c"
#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/sysinfo.h>

int getloadavg(double *a, int n)
{
	struct sysinfo si;
	if (n <= 0) return n ? -1 : 0;
	sysinfo(&si);
	if (n > 3) n = 3;
	for (int i=0; i<n; i++)
		a[i] = 1.0/(1<<SI_LOAD_SHIFT) * si.loads[i];
	return n;
}
