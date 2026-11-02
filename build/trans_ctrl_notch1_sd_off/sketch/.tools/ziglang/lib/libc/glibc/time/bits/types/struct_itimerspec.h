#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\glibc\\time\\bits\\types\\struct_itimerspec.h"
#ifndef __itimerspec_defined
#define __itimerspec_defined 1

#include <bits/types.h>
#include <bits/types/struct_timespec.h>

/* POSIX.1b structure for timer start values and intervals.  */
struct itimerspec
  {
    struct timespec it_interval;
    struct timespec it_value;
  };

#endif
