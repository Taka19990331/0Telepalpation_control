#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\compiler\\aro\\include\\stdckdint.h"
/* <stdckdint.h> for the Aro C compiler */

#pragma once

#define __STDC_VERSION_STDCKDINT_H__ 202311L

#define ckd_add(result, a, b) __builtin_add_overflow(a, b, result)
#define ckd_sub(result, a, b) __builtin_sub_overflow(a, b, result)
#define ckd_mul(result, a, b) __builtin_mul_overflow(a, b, result)
