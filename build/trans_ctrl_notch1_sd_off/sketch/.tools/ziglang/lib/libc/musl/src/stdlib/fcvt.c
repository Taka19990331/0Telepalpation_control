#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdlib\\fcvt.c"
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *fcvt(double x, int n, int *dp, int *sign)
{
	char tmp[1500];
	int i, lz;

	if (n > 1400U) n = 1400;
	sprintf(tmp, "%.*f", n, x);
	i = (tmp[0] == '-');
	if (tmp[i] == '0') lz = strspn(tmp+i+2, "0");
	else lz = -(int)strcspn(tmp+i, ".");

	if (n<=lz) {
		*sign = i;
		*dp = 1;
		if (n>14U) n = 14;
		return "000000000000000"+14-n;
	}

	return ecvt(x, n-lz, dp, sign);
}
