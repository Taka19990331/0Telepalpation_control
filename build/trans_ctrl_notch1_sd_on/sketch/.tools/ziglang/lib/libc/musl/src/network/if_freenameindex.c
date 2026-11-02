#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\if_freenameindex.c"
#include <net/if.h>
#include <stdlib.h>

void if_freenameindex(struct if_nameindex *idx)
{
	free(idx);
}
