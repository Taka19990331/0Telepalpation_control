#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\c_locale.c"
#include "locale_impl.h"
#include <stdint.h>

static const uint32_t empty_mo[] = { 0x950412de, 0, -1, -1, -1 };

const struct __locale_map __c_dot_utf8 = {
	.map = empty_mo,
	.map_size = sizeof empty_mo,
	.name = "C.UTF-8"
};

const struct __locale_struct __c_locale = { 0 };
const struct __locale_struct __c_dot_utf8_locale = {
	.cat[LC_CTYPE] = &__c_dot_utf8
};
