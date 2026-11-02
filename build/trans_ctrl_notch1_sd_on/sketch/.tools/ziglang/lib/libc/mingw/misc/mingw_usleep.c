#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\mingw_usleep.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sys/types.h>
#include <errno.h>

int __cdecl usleep (useconds_t);

int __cdecl
usleep (useconds_t us)
{
  if (us != 0)
    Sleep (us / 1000);

  return 0;
}

