#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\stdio\\mingw_sscanf.c"
#include <stdarg.h>
#include <stdlib.h>

extern int __mingw_vsscanf (const char *buf, const char *format, va_list argp);

int __mingw_sscanf (const char *buf, const char *format, ...);

int
__mingw_sscanf (const char *buf, const char *format, ...)
{
  va_list argp;
  int r;

  va_start (argp, format);
  r = __mingw_vsscanf (buf, format, argp);
  va_end (argp);

  return r;
}

