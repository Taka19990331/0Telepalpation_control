#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\ucrt_amsg_exit.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#undef __MSVCRT_VERSION__
#define _UCRT

#include <stdio.h>
#include <stdlib.h>
#include <internal.h>

void __cdecl __MINGW_ATTRIB_NORETURN _amsg_exit(int ret)
{
  fprintf(stderr, "runtime error %d\n", ret);
  _exit(255);
}
void __cdecl (*__MINGW_IMP_SYMBOL(_amsg_exit))(int) = _amsg_exit;
