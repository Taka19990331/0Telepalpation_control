#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\misc\\ucrt_at_quick_exit.c"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#undef __MSVCRT_VERSION__
#define _UCRT

#include <stdlib.h>
#include <corecrt_startup.h>

extern char __mingw_module_is_dll;

int __cdecl at_quick_exit(void (__cdecl *func)(void))
{
  // In a DLL, we can't register a function with _crt_at_quick_exit, because
  // we can't unregister it when the DLL is unloaded. This matches how
  // at_quick_exit/quick_exit work with MSVC with a dynamically linked CRT.
  if (__mingw_module_is_dll)
    return 0;
  return _crt_at_quick_exit(func);
}
int __cdecl (*__MINGW_IMP_SYMBOL(at_quick_exit))(void (__cdecl *)(void)) = at_quick_exit;
