#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\glibc\\time\\bits\\types\\time_t.h"
#ifndef __time_t_defined
#define __time_t_defined 1

#include <bits/types.h>

/* Returned by `time'.  */
#ifdef __USE_TIME64_REDIRECTS
typedef __time64_t time_t;
#else
typedef __time_t time_t;
#endif

#endif
