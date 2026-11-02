#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdlib\\ecvt.c"
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

char *ecvt(double x, int n, int *dp, int *sign)
{
	static char buf[16];
	char tmp[32];
	int i, j;

	if (n-1U > 15) n = 15;
	sprintf(tmp, "%.*e", n-1, x);
	i = *sign = (tmp[0]=='-');
	for (j=0; tmp[i]!='e'; j+=(tmp[i++]!='.'))
		buf[j] = tmp[i];
	buf[j] = 0;
	*dp = atoi(tmp+i+1)+1;

	return buf;
}
