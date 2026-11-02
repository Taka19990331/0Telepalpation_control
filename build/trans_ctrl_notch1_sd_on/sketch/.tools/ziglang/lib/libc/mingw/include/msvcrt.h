#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\include\\msvcrt.h"
#ifndef __LIBMSVCRT_OS__
#error "This file should only be used in libmsvcrt-os.a"
#endif

#ifndef MSVCRT_H
#define MSVCRT_H

#include <winbase.h>

static inline HMODULE __mingw_get_msvcrt_handle(void)
{
    return GetModuleHandleA("msvcrt.dll");
}

#endif
