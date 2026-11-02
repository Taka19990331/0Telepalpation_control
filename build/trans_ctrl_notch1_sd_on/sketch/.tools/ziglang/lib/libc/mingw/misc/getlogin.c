#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\getlogin.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <windows.h>

char *getlogin(void);

char *getlogin(void)
{
  static char user_name[MAX_PATH];
  DWORD  length = sizeof (user_name);

  if (GetUserName (user_name, &length))
    return user_name;
  return NULL;
}

