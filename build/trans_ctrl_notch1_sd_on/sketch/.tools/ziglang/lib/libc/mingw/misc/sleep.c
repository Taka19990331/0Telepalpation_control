#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\sleep.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <windows.h>

unsigned int sleep (unsigned int seconds)
{
  Sleep (seconds * 1000);
  return 0;
}
