#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\freelocale.c"
#include <stdlib.h>
#include "locale_impl.h"

void freelocale(locale_t l)
{
	if (__loc_is_allocated(l)) free(l);
}

weak_alias(freelocale, __freelocale);
