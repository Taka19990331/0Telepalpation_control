#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\fegetround.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#include <internal.h>

/* 7.6.3.1
   The fegetround function returns the value of the rounding direction
   macro representing the current rounding direction.  */

int fegetround(void)
{
    return __mingw_controlfp(0, 0) & _MCW_RC;
}
