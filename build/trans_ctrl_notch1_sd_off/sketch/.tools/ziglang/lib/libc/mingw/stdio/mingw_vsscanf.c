#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\mingw_vsscanf.c"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mingw_sformat.h"

int
__mingw_vsscanf (const char *s, const char *format, va_list argp)
{
  _IFP ifp = { .str = s, .is_string = 1 };
  return __mingw_sformat (&ifp, format, argp);
}
